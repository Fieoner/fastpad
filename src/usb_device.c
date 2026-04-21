#include "usb_device.h"
#include "usb_desc.h"
#include "ch32v30x.h"
#include "debug.h"
#include <stddef.h>

/* Endpoint buffers - must be 4-byte aligned */
__attribute__((aligned(4))) static uint8_t ep0_buf[64];
__attribute__((aligned(4))) static uint8_t ep1_tx_buf[64];

/* Control transfer state */
static const uint8_t *setup_desc_ptr;
static uint16_t setup_desc_len;
static uint8_t  setup_addr;

volatile uint8_t usb_device_state;
volatile uint8_t usb_device_speed;
volatile uint8_t ep1_tx_busy;
volatile uint8_t latest_report;

/* HID idle rate */
static uint8_t hid_idle_rate;

static void USBHS_Clock_Init(void)
{
    RCC_USBCLK48MConfig(RCC_USBCLK48MCLKSource_USBPHY);
    RCC_USBHSPLLCLKConfig(RCC_HSBHSPLLCLKSource_HSE);
    RCC_USBHSConfig(RCC_USBPLL_Div2);
    RCC_USBHSPLLCKREFCLKConfig(RCC_USBHSPLLCKREFCLK_4M);
    RCC_USBHSPHYPLLALIVEcmd(ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBHS, ENABLE);
}

static void USBHS_Endp_Init(void)
{
    /* EP0: bidirectional control, EP1: TX only (HID IN) */
    USBHSD->ENDP_CONFIG = USBHS_UEP0_T_EN | USBHS_UEP0_R_EN | USBHS_UEP1_T_EN;
    USBHSD->ENDP_TYPE   = 0;
    USBHSD->BUF_MODE    = 0;

    USBHSD->UEP0_MAX_LEN = DEF_USB_EP0_HS_SIZE;
    USBHSD->UEP0_DMA     = (uint32_t)(uintptr_t)ep0_buf;

    USBHSD->UEP1_MAX_LEN = DEF_USB_EP1_HS_SIZE;
    USBHSD->UEP1_TX_DMA  = (uint32_t)(uintptr_t)ep1_tx_buf;

    /* EP0: NAK TX, ACK RX */
    USBHSD->UEP0_TX_LEN  = 0;
    USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
    USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_RES_ACK;

    /* EP1: auto toggle, preload with zero report so first poll gets data */
    latest_report = 0;
    ep1_tx_buf[0] = 0;
    USBHSD->UEP1_TX_LEN  = DEF_GAMEPAD_REPORT_SIZE;
    USBHSD->UEP1_TX_CTRL = USBHS_UEP_T_RES_ACK | USBHS_UEP_T_TOG_AUTO;
    USBHSD->UEP1_RX_CTRL = USBHS_UEP_R_RES_NAK;
}

void USB_Device_Init(void)
{
    USBHS_Clock_Init();

    /* Reset USB SIE */
    USBHSD->CONTROL = USBHS_UC_CLR_ALL | USBHS_UC_RESET_SIE;
    Delay_Us(10);
    USBHSD->CONTROL = 0;
    Delay_Us(10);

    /* Configure as HS device */
    USBHSD->HOST_CTRL = USBHS_UH_PHY_SUSPENDM;
    USBHSD->CONTROL   = USBHS_UC_DMA_EN | USBHS_UC_INT_BUSY | USBHS_UC_SPEED_HIGH;
    USBHSD->INT_EN    = USBHS_UIE_SETUP_ACT | USBHS_UIE_TRANSFER
                       | USBHS_UIE_DETECT    | USBHS_UIE_SUSPEND;
    USBHSD->DEV_AD    = 0x00;

    USBHS_Endp_Init();

    /* Enable pull-up to signal device presence */
    USBHSD->CONTROL |= USBHS_UC_DEV_PU_EN;

    usb_device_state = USB_STATE_DEFAULT;
    usb_device_speed = USB_SPEED_HIGH;

    NVIC_EnableIRQ(USBHS_IRQn);
}

void USB_Device_SendReport(uint8_t *data, uint8_t len)
{
    /* Just update the latest state - the EP1 IN complete handler
     * continuously reloads from latest_report every microframe */
    latest_report = data[0];
}

/* --- EP0 Setup Request Handling --- */

static void EP0_Send(const uint8_t *data, uint16_t len, uint16_t max_len)
{
    if (len > max_len) len = max_len;
    setup_desc_ptr = data;
    setup_desc_len = len;

    uint16_t tx_len = (len > DEF_USB_EP0_HS_SIZE) ? DEF_USB_EP0_HS_SIZE : len;
    for (uint16_t i = 0; i < tx_len; i++)
        ep0_buf[i] = setup_desc_ptr[i];

    USBHSD->UEP0_TX_LEN  = tx_len;
    USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_ACK | USBHS_UEP_T_TOG_DATA1;

    setup_desc_ptr += tx_len;
    setup_desc_len -= tx_len;
}

static void EP0_Stall(void)
{
    USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_STALL;
    USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_RES_STALL;
}

static void Handle_Setup(void)
{
    uint8_t bmRequestType = ep0_buf[0];
    uint8_t bRequest      = ep0_buf[1];
    uint16_t wValue       = ep0_buf[2] | (ep0_buf[3] << 8);
    uint16_t wIndex       = ep0_buf[4] | (ep0_buf[5] << 8);
    uint16_t wLength      = ep0_buf[6] | (ep0_buf[7] << 8);

    (void)wIndex;
    setup_desc_ptr = NULL;
    setup_desc_len = 0;
    setup_addr = 0;

    /* Standard device requests */
    if ((bmRequestType & 0x60) == 0x00) {
        switch (bRequest) {
        case 0x06: /* GET_DESCRIPTOR */
            switch (wValue >> 8) {
            case 0x01: /* Device */
                EP0_Send(GamepadDeviceDesc, GamepadDeviceDescLen, wLength);
                break;
            case 0x02: /* Configuration */
                if (usb_device_speed == USB_SPEED_HIGH)
                    EP0_Send(GamepadCfgDesc_HS, GamepadCfgDesc_HS_Len, wLength);
                else
                    EP0_Send(GamepadCfgDesc_FS, GamepadCfgDesc_FS_Len, wLength);
                break;
            case 0x03: /* String */
                switch (wValue & 0xFF) {
                case 0: EP0_Send(GamepadStringLangID, GamepadStringLangID[0], wLength); break;
                case 1: EP0_Send(GamepadStringVendor, GamepadStringVendor[0], wLength); break;
                case 2: EP0_Send(GamepadStringProduct, GamepadStringProduct[0], wLength); break;
                case 3: EP0_Send(GamepadStringSerial, GamepadStringSerial[0], wLength); break;
                default: EP0_Stall(); break;
                }
                break;
            case 0x06: /* Device Qualifier */
                EP0_Send(GamepadQualifierDesc, GamepadQualifierDescLen, wLength);
                break;
            case 0x22: /* HID Report Descriptor (via interface request) */
                EP0_Send(GamepadReportDesc, GamepadReportDescLen, wLength);
                break;
            case 0x07: /* Other Speed Configuration */
                if (usb_device_speed == USB_SPEED_HIGH)
                    EP0_Send(GamepadCfgDesc_FS, GamepadCfgDesc_FS_Len, wLength);
                else
                    EP0_Send(GamepadCfgDesc_HS, GamepadCfgDesc_HS_Len, wLength);
                break;
            default:
                EP0_Stall();
                break;
            }
            break;

        case 0x05: /* SET_ADDRESS */
            setup_addr = (uint8_t)(wValue & 0x7F);
            USBHSD->UEP0_TX_LEN  = 0;
            USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_ACK | USBHS_UEP_T_TOG_DATA1;
            break;

        case 0x09: /* SET_CONFIGURATION */
            usb_device_state = (wValue) ? USB_STATE_CONFIGURED : USB_STATE_ADDRESSED;
            USBHSD->UEP0_TX_LEN  = 0;
            USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_ACK | USBHS_UEP_T_TOG_DATA1;
            break;

        case 0x08: /* GET_CONFIGURATION */
            ep0_buf[0] = (usb_device_state == USB_STATE_CONFIGURED) ? 1 : 0;
            EP0_Send(ep0_buf, 1, wLength);
            break;

        case 0x00: /* GET_STATUS */
            ep0_buf[0] = 0x00;
            ep0_buf[1] = 0x00;
            EP0_Send(ep0_buf, 2, wLength);
            break;

        case 0x01: /* CLEAR_FEATURE */
        case 0x03: /* SET_FEATURE */
            USBHSD->UEP0_TX_LEN  = 0;
            USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_ACK | USBHS_UEP_T_TOG_DATA1;
            break;

        default:
            EP0_Stall();
            break;
        }
    }
    /* HID class requests */
    else if ((bmRequestType & 0x60) == 0x20) {
        switch (bRequest) {
        case 0x01: /* GET_REPORT */
            ep0_buf[0] = 0;
            EP0_Send(ep0_buf, DEF_GAMEPAD_REPORT_SIZE, wLength);
            break;

        case 0x02: /* GET_IDLE */
            ep0_buf[0] = hid_idle_rate;
            EP0_Send(ep0_buf, 1, wLength);
            break;

        case 0x0A: /* SET_IDLE */
            hid_idle_rate = (uint8_t)(wValue >> 8);
            USBHSD->UEP0_TX_LEN  = 0;
            USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_ACK | USBHS_UEP_T_TOG_DATA1;
            break;

        case 0x0B: /* SET_PROTOCOL */
            USBHSD->UEP0_TX_LEN  = 0;
            USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_ACK | USBHS_UEP_T_TOG_DATA1;
            break;

        default:
            EP0_Stall();
            break;
        }
    }
    /* Interface GET_DESCRIPTOR for HID report descriptor */
    else if (bmRequestType == 0x81 && bRequest == 0x06) {
        switch (wValue >> 8) {
        case 0x22: /* HID Report Descriptor */
            EP0_Send(GamepadReportDesc, GamepadReportDescLen, wLength);
            break;
        default:
            EP0_Stall();
            break;
        }
    }
    else {
        EP0_Stall();
    }
}

/* --- USB HS IRQ Handler --- */

void USBHS_IRQHandler(void) __attribute__((interrupt("machine")));
void USBHS_IRQHandler(void)
{
    uint8_t intflag = USBHSD->INT_FG;
    uint8_t intst   = USBHSD->INT_ST;

    if (intflag & USBHS_UIF_SETUP_ACT) {
        Handle_Setup();
        USBHSD->INT_FG = USBHS_UIF_SETUP_ACT;
    }
    else if (intflag & USBHS_UIF_TRANSFER) {
        uint8_t endp  = intst & USBHS_UIS_ENDP_MASK;
        uint8_t token = intst & USBHS_UIS_TOKEN_MASK;

        if (endp == 0) {
            if (token == USBHS_UIS_TOKEN_IN) {
                /* EP0 IN: continue sending descriptor data or apply address */
                if (setup_addr) {
                    USBHSD->DEV_AD = setup_addr;
                    usb_device_state = USB_STATE_ADDRESSED;
                    setup_addr = 0;
                }

                if (setup_desc_len > 0) {
                    uint16_t tx_len = (setup_desc_len > DEF_USB_EP0_HS_SIZE)
                                    ? DEF_USB_EP0_HS_SIZE : setup_desc_len;
                    for (uint16_t i = 0; i < tx_len; i++)
                        ep0_buf[i] = setup_desc_ptr[i];
                    USBHSD->UEP0_TX_LEN  = tx_len;
                    USBHSD->UEP0_TX_CTRL = (USBHSD->UEP0_TX_CTRL & ~USBHS_UEP_T_RES_MASK)
                                          | USBHS_UEP_T_RES_ACK;
                    setup_desc_ptr += tx_len;
                    setup_desc_len -= tx_len;
                } else {
                    USBHSD->UEP0_TX_LEN  = 0;
                    USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
                }
            }
            else if (token == USBHS_UIS_TOKEN_OUT) {
                /* EP0 OUT: status stage complete */
                USBHSD->UEP0_TX_LEN  = 0;
                USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
                USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_RES_ACK;
            }
        }
        else if (endp == 1) {
            if (token == USBHS_UIS_TOKEN_IN) {
                /* EP1 IN complete: immediately reload with current state
                 * so the next host poll always gets fresh data */
                ep1_tx_buf[0] = latest_report;
                USBHSD->UEP1_TX_LEN  = DEF_GAMEPAD_REPORT_SIZE;
                USBHSD->UEP1_TX_CTRL = (USBHSD->UEP1_TX_CTRL & ~USBHS_UEP_T_RES_MASK)
                                      | USBHS_UEP_T_RES_ACK;
            }
        }

        USBHSD->INT_FG = USBHS_UIF_TRANSFER;
    }
    else if (intflag & USBHS_UIF_BUS_RST) {
        /* Bus reset */
        USBHSD->DEV_AD = 0;
        usb_device_state = USB_STATE_DEFAULT;

        /* Check speed after reset */
        if (USBHSD->SPEED_TYPE & 0x01)
            usb_device_speed = USB_SPEED_HIGH;
        else
            usb_device_speed = USB_SPEED_FULL;

        USBHS_Endp_Init();
        USBHSD->INT_FG = USBHS_UIF_BUS_RST;
    }
    else if (intflag & USBHS_UIF_SUSPEND) {
        USBHSD->INT_FG = USBHS_UIF_SUSPEND;
    }
}
