#ifndef __IAP_H
#define __IAP_H

#include <stdint.h>

/* IAP commands (byte 0 of SET_REPORT data, after report ID) */
#define IAP_CMD_BEGIN   0x01
#define IAP_CMD_DATA    0x02
#define IAP_CMD_FINISH  0x03

/* IAP status */
#define IAP_STATUS_IDLE   0x00
#define IAP_STATUS_READY  0x01
#define IAP_STATUS_ERROR  0x03
#define IAP_STATUS_DONE   0x04

/* Max firmware size we can buffer in RAM.
 * RAM is 32KB, firmware is ~6KB, we use ~3KB for stack+BSS.
 * Leave 24KB for the firmware buffer. */
#define IAP_MAX_FW_SIZE   (24 * 1024)

extern volatile uint8_t iap_status;

void IAP_Handle_Set(const uint8_t *data, uint16_t len);
void IAP_Get_Status(uint8_t *buf);

/* Called from main loop when iap_status == IAP_STATUS_DONE.
 * Disables interrupts, erases flash, writes from RAM, resets.
 * DOES NOT RETURN. */
void IAP_Flash_And_Reset(void) __attribute__((noreturn));

#endif
