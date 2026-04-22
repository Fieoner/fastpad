/*
 * IAP (In-Application Programming) for CH32V307
 *
 * Receives firmware into a RAM buffer over USB HID report 0x0F.
 * When complete, erases flash and writes the new firmware from RAM,
 * then resets. The flash write runs from a small RAM-resident stub
 * since we can't execute from flash while erasing it.
 *
 * Risk: power loss during flash write = brick (recover via BOOT0+RST).
 */

#include "iap.h"
#include "ch32v30x.h"
#include "ch32v30x_flash.h"


volatile uint8_t iap_status;

/* Firmware buffer in RAM */
static uint8_t fw_buf[IAP_MAX_FW_SIZE] __attribute__((aligned(4)));
static uint32_t fw_total_size;
static uint32_t fw_expected_crc;
static uint32_t fw_received;

/* CRC32 (zlib polynomial) */
static uint32_t crc32_calc(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

void IAP_Handle_Set(const uint8_t *data, uint16_t len)
{
    if (len < 1) return;
    uint8_t cmd = data[0];

    switch (cmd) {
    case IAP_CMD_BEGIN:
        if (len >= 9) {
            fw_total_size = data[1] | (data[2] << 8) |
                           (data[3] << 16) | (data[4] << 24);
            fw_expected_crc = data[5] | (data[6] << 8) |
                             (data[7] << 16) | (data[8] << 24);
        } else {
            iap_status = IAP_STATUS_ERROR;
            break;
        }
        if (fw_total_size == 0 || fw_total_size > IAP_MAX_FW_SIZE) {
            iap_status = IAP_STATUS_ERROR;
            break;
        }
        fw_received = 0;
        iap_status = IAP_STATUS_READY;
        break;

    case IAP_CMD_DATA: {
        if (iap_status != IAP_STATUS_READY) break;
        /* data[1..2] = chunk index (unused, we just append) */
        const uint8_t *payload = &data[3];
        uint16_t payload_len = (len > 3) ? (len - 3) : 0;
        if (payload_len > 60) payload_len = 60;

        uint32_t space = fw_total_size - fw_received;
        if (payload_len > space) payload_len = (uint16_t)space;

        for (uint16_t i = 0; i < payload_len; i++)
            fw_buf[fw_received + i] = payload[i];
        fw_received += payload_len;
        break;
    }

    case IAP_CMD_FINISH:
        if (iap_status != IAP_STATUS_READY || fw_received != fw_total_size) {
            iap_status = IAP_STATUS_ERROR;
            break;
        }
        if (fw_expected_crc != 0) {
            uint32_t actual = crc32_calc(fw_buf, fw_total_size);
            if (actual != fw_expected_crc) {
                iap_status = IAP_STATUS_ERROR;
                break;
            }
        }
        iap_status = IAP_STATUS_DONE;
        break;

    default:
        break;
    }
}

void IAP_Get_Status(uint8_t *buf)
{
    buf[0] = 0x0F;  /* report ID */
    buf[1] = iap_status;
    buf[2] = (uint8_t)(fw_received);
    buf[3] = (uint8_t)(fw_received >> 8);
    buf[4] = (uint8_t)(fw_received >> 16);
    buf[5] = (uint8_t)(fw_received >> 24);
}

/*
 * Flash write stub — runs entirely from RAM.
 *
 * Uses the WCH fast flash mode (256-byte pages):
 *   - KEYR + MODEKEYR unlock
 *   - PAGE_ER (0x00020000) for erase
 *   - PAGE_PG (0x00010000) + PG_STRT (0x00200000) for program
 *
 * Addresses use 0x08000000 base (FLASH_BASE per vendor headers).
 */
__attribute__((section(".data"), noinline, used))
static void ram_flash_writer(uint32_t *src, uint32_t size)
{
    volatile uint32_t *KEYR     = (volatile uint32_t *)0x40022004;
    volatile uint32_t *STATR    = (volatile uint32_t *)0x4002200C;
    volatile uint32_t *CTLR     = (volatile uint32_t *)0x40022010;
    volatile uint32_t *ADDR     = (volatile uint32_t *)0x40022014;
    volatile uint32_t *MODEKEYR = (volatile uint32_t *)0x40022024;

    /* Unlock flash */
    *KEYR = 0x45670123;
    *KEYR = 0xCDEF89AB;
    /* Unlock fast mode */
    *MODEKEYR = 0x45670123;
    *MODEKEYR = 0xCDEF89AB;

    uint32_t base = 0x08000000;

    /* Erase 256-byte pages */
    uint32_t pages = (size + 255) / 256;
    for (uint32_t p = 0; p < pages; p++) {
        *CTLR |= 0x00020000;           /* PAGE_ER */
        *ADDR = base + p * 256;
        *CTLR |= 0x00000040;           /* STRT */
        while (*STATR & 0x01);          /* BSY */
        *CTLR &= ~(uint32_t)0x00020000;
    }

    /* Program 256-byte pages (64 words each) */
    uint32_t total_words = (size + 3) / 4;
    for (uint32_t p = 0; p < pages; p++) {
        volatile uint32_t *dst = (volatile uint32_t *)(base + p * 256);

        *CTLR |= 0x00010000;           /* PAGE_PG */
        while (*STATR & 0x01);          /* BSY */
        while (*STATR & 0x02);          /* WR_BSY */

        /* Load 64 words into page buffer */
        for (uint32_t w = 0; w < 64; w++) {
            uint32_t idx = p * 64 + w;
            dst[w] = (idx < total_words) ? src[idx] : 0xFFFFFFFF;
            while (*STATR & 0x02);      /* WR_BSY */
        }

        /* Commit */
        *CTLR |= 0x00200000;           /* PG_STRT */
        while (*STATR & 0x01);          /* BSY */
        *CTLR &= ~(uint32_t)0x00010000; /* clear PAGE_PG */
    }

    /* Lock */
    *CTLR |= 0x00008000;               /* FLOCK */
    *CTLR |= 0x00000080;               /* LOCK */

    /* System reset */
    volatile uint32_t *PFIC_CFGR = (volatile uint32_t *)0xE000E048;
    *PFIC_CFGR = 0xBEEF0080;
    while (1);
}

void IAP_Flash_And_Reset(void)
{
    __disable_irq();
    ram_flash_writer((uint32_t *)fw_buf, fw_total_size);
    while (1);
}
