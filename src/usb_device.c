#include "usb_device.h"
#include "usb_desc.h"
#include "adp_reports.h"
#include "ch32v30x.h"
#include "debug.h"
#include <stddef.h>

/* Endpoint buffers - must be 4-byte aligned.
 * EP1 TX buffer padded to 4-byte boundary so DMA word reads don't
 * overwrite adjacent ep0_buf. */
#define EP1_TX_BUF_SIZE ((DEF_INPUT_REPORT_WIRE_SIZE + 3) & ~3)
__attribute__((aligned(4))) static uint8_t ep0_buf[64];
__attribute__((aligned(4))) static uint8_t ep1_tx_buf[EP1_TX_BUF_SIZE];

/* Control transfer state */
static const uint8_t *setup_desc_ptr;
static uint16_t setup_desc_len;
static uint8_t  setup_addr;

volatile uint8_t usb_device_state;
volatile uint8_t usb_device_speed;
volatile uint8_t latest_buttons;

/* HID idle rate */
static uint8_t hid_idle_rate;

/* --- ADP configuration state (stored in RAM) --- */

static PadConfiguration pad_config;
static NameReport       pad_name;

static void ADP_Config_Init(void)
{
    for (int i = 0; i < ADP_SENSOR_COUNT; i++) {
        pad_config.sensorThresholds[i] = 512;
        pad_config.sensorToButtonMapping[i] = (int8_t)(i < 8 ? i : -1);
    }
    /* 0.5f in IEEE 754 LE = 0x3F000000 */
    pad_config.releaseMultiplier[0] = 0x00;
    pad_config.releaseMultiplier[1] = 0x00;
    pad_config.releaseMultiplier[2] = 0x00;
    pad_config.releaseMultiplier[3] = 0x3F;

    pad_name.size = 7;
    pad_name.name[0] = 'f'; pad_name.name[1] = 'a'; pad_name.name[2] = 's';
    pad_name.name[3] = 't'; pad_name.name[4] = 'p'; pad_name.name[5] = 'a';
    pad_name.name[6] = 'd';
}

/* --- Feature report handlers --- */

static uint8_t feature_buf[128];
static IdentificationReport ident_report;
static IdentificationV2Report ident_v2_report;

static void Build_Identification(void)
{
    ident_report.firmwareVersionMajor = 1;
    ident_report.firmwareVersionMinor = 0;
    ident_report.buttonCount = 8;
    ident_report.sensorCount = ADP_SENSOR_COUNT;
    ident_report.ledCount = 0;
    ident_report.maxSensorValue = ADP_MAX_SENSOR_VALUE;
    const char *bt = "CH32V307-fastpad";
    for (int i = 0; i < 32; i++)
        ident_report.boardType[i] = bt[i] ? bt[i] : 0;
}

__attribute__((noinline))
static int Handle_Feature_Get(uint8_t report_id, uint16_t wLength)
{
    const uint8_t *src = NULL;
    uint16_t len = 0;

    switch (report_id) {
    case REPORT_ID_PAD_CONFIG:
        src = (const uint8_t *)&pad_config;
        len = PAD_CONFIG_REPORT_SIZE;
        break;
    case REPORT_ID_NAME:
        src = (const uint8_t *)&pad_name;
        len = NAME_REPORT_SIZE;
        break;
    case REPORT_ID_IDENTIFICATION:
        Build_Identification();
        src = (const uint8_t *)&ident_report;
        len = IDENTIFICATION_REPORT_SIZE;
        break;
    case REPORT_ID_IDENTIFICATION_V2:
        Build_Identification();
        {
            const uint8_t *s = (const uint8_t *)&ident_report;
            uint8_t *d = (uint8_t *)&ident_v2_report.parent;
            for (uint16_t i = 0; i < sizeof(IdentificationReport); i++)
                d[i] = s[i];
        }
        ident_v2_report.features = 0;
        src = (const uint8_t *)&ident_v2_report;
        len = IDENTIFICATION_V2_REPORT_SIZE;
        break;
    default:
        return -1;
    }

    feature_buf[0] = report_id;
    uint16_t copy_len = len;
    if (copy_len > sizeof(feature_buf) - 1)
        copy_len = sizeof(feature_buf) - 1;
    for (uint16_t i = 0; i < copy_len; i++)
        feature_buf[1 + i] = src[i];

    return 1 + copy_len;
}

__attribute__((noinline))
static void Handle_Feature_Set(uint8_t report_id, const uint8_t *data, uint16_t len)
{
    switch (report_id) {
    case REPORT_ID_PAD_CONFIG:
        if (len >= PAD_CONFIG_REPORT_SIZE) {
            uint8_t *d = (uint8_t *)&pad_config;
            for (uint16_t i = 0; i < PAD_CONFIG_REPORT_SIZE; i++)
                d[i] = data[i];
        }
        break;
    case REPORT_ID_NAME:
        if (len >= NAME_REPORT_SIZE) {
            uint8_t *d = (uint8_t *)&pad_name;
            for (uint16_t i = 0; i < NAME_REPORT_SIZE; i++)
                d[i] = data[i];
        }
        break;
    default:
        break;
    }
}

/* --- USB HS peripheral init --- */

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
    USBHSD->ENDP_CONFIG = USBHS_UEP0_T_EN | USBHS_UEP0_R_EN | USBHS_UEP1_T_EN;
    USBHSD->ENDP_TYPE   = 0;
    USBHSD->BUF_MODE    = 0;

    USBHSD->UEP0_MAX_LEN = DEF_USB_EP0_HS_SIZE;
    USBHSD->UEP0_DMA     = (uint32_t)(uintptr_t)ep0_buf;

    USBHSD->UEP1_MAX_LEN = EP1_TX_BUF_SIZE;
    USBHSD->UEP1_TX_DMA  = (uint32_t)(uintptr_t)ep1_tx_buf;

    USBHSD->UEP0_TX_LEN  = 0;
    USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
    USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_RES_ACK;

    /* EP1: NAK until SET_CONFIGURATION arms it */
    USBHSD->UEP1_TX_LEN  = 0;
    USBHSD->UEP1_TX_CTRL = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_AUTO;
    USBHSD->UEP1_RX_CTRL = USBHS_UEP_R_RES_NAK;
}

void USB_Device_Init(void)
{
    ADP_Config_Init();
    USBHS_Clock_Init();

    USBHSD->CONTROL = USBHS_UC_CLR_ALL | USBHS_UC_RESET_SIE;
    Delay_Us(10);
    USBHSD->CONTROL = 0;
    Delay_Us(10);

    USBHSD->HOST_CTRL = USBHS_UH_PHY_SUSPENDM;
    USBHSD->CONTROL   = USBHS_UC_DMA_EN | USBHS_UC_INT_BUSY | USBHS_UC_SPEED_HIGH;
    USBHSD->INT_EN    = USBHS_UIE_SETUP_ACT | USBHS_UIE_TRANSFER
                       | USBHS_UIE_DETECT    | USBHS_UIE_SUSPEND;
    USBHSD->DEV_AD    = 0x00;

    USBHS_Endp_Init();

    USBHSD->CONTROL |= USBHS_UC_DEV_PU_EN;

    usb_device_state = USB_STATE_DEFAULT;
    usb_device_speed = USB_SPEED_HIGH;

    NVIC_EnableIRQ(USBHS_IRQn);
}

/* --- EP0 helpers --- */

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

/* State for SET_REPORT data phase */
static uint8_t ep0_feature_active;

__attribute__((noinline))
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
    ep0_feature_active = 0;

    /* Standard device requests */
    if ((bmRequestType & 0x60) == 0x00) {
        switch (bRequest) {
        case 0x06: /* GET_DESCRIPTOR */
            switch (wValue >> 8) {
            case 0x01:
                EP0_Send(GamepadDeviceDesc, GamepadDeviceDescLen, wLength);
                break;
            case 0x02:
                if (usb_device_speed == USB_SPEED_HIGH)
                    EP0_Send(GamepadCfgDesc_HS, GamepadCfgDesc_HS_Len, wLength);
                else
                    EP0_Send(GamepadCfgDesc_FS, GamepadCfgDesc_FS_Len, wLength);
                break;
            case 0x03:
                switch (wValue & 0xFF) {
                case 0: EP0_Send(GamepadStringLangID, GamepadStringLangID[0], wLength); break;
                case 1: EP0_Send(GamepadStringVendor, GamepadStringVendor[0], wLength); break;
                case 2: EP0_Send(GamepadStringProduct, GamepadStringProduct[0], wLength); break;
                case 3: EP0_Send(GamepadStringSerial, GamepadStringSerial[0], wLength); break;
                default: EP0_Stall(); break;
                }
                break;
            case 0x06:
                EP0_Send(GamepadQualifierDesc, GamepadQualifierDescLen, wLength);
                break;
            case 0x22:
                EP0_Send(GamepadReportDesc, GamepadReportDescLen, wLength);
                break;
            case 0x07:
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
            if (usb_device_state == USB_STATE_CONFIGURED) {
                /* Arm EP1 with empty report */
                ep1_tx_buf[0] = REPORT_ID_INPUT;
                for (int i = 1; i < DEF_INPUT_REPORT_WIRE_SIZE; i++)
                    ep1_tx_buf[i] = 0;
                USBHSD->UEP1_TX_LEN  = DEF_INPUT_REPORT_WIRE_SIZE;
                USBHSD->UEP1_TX_CTRL = USBHS_UEP_T_RES_ACK | USBHS_UEP_T_TOG_AUTO;
            } else {
                USBHSD->UEP1_TX_LEN  = 0;
                USBHSD->UEP1_TX_CTRL = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_AUTO;
            }
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
        {
            uint8_t report_type = (uint8_t)(wValue >> 8);
            uint8_t report_id   = (uint8_t)(wValue & 0xFF);

            if (report_type == 0x03) {
                int flen = Handle_Feature_Get(report_id, wLength);
                if (flen > 0)
                    EP0_Send(feature_buf, (uint16_t)flen, wLength);
                else
                    EP0_Stall();
            } else if (report_type == 0x01) {
                ep0_buf[0] = 0;
                EP0_Send(ep0_buf, 1, wLength);
            } else {
                EP0_Stall();
            }
            break;
        }

        case 0x09: /* SET_REPORT */
        {
            uint8_t report_type = (uint8_t)(wValue >> 8);
            uint8_t report_id   = (uint8_t)(wValue & 0xFF);

            if (report_type == 0x03) {
                ep0_feature_active = report_id;
                USBHSD->UEP0_TX_LEN  = 0;
                USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_ACK | USBHS_UEP_T_TOG_DATA1;
                USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_RES_ACK;
            } else if (report_type == 0x02) {
                USBHSD->UEP0_TX_LEN  = 0;
                USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_ACK | USBHS_UEP_T_TOG_DATA1;
            } else {
                EP0_Stall();
            }
            break;
        }

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
        case 0x22:
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

__attribute__((noinline, used))
static void USBHS_IRQ_Body(void)
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
                    /* Toggle DATA0/DATA1 for multi-packet transfers */
                    USBHSD->UEP0_TX_CTRL = ((USBHSD->UEP0_TX_CTRL ^ USBHS_UEP_T_TOG_DATA1)
                                          & ~USBHS_UEP_T_RES_MASK)
                                          | USBHS_UEP_T_RES_ACK;
                    setup_desc_ptr += tx_len;
                    setup_desc_len -= tx_len;
                } else {
                    USBHSD->UEP0_TX_LEN  = 0;
                    USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
                }
            }
            else if (token == USBHS_UIS_TOKEN_OUT) {
                uint16_t rx_len = USBHSD->RX_LEN;

                if (ep0_feature_active && rx_len > 0) {
                    Handle_Feature_Set(ep0_feature_active, ep0_buf + 1, rx_len - 1);
                    ep0_feature_active = 0;
                }

                USBHSD->UEP0_TX_LEN  = 0;
                USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
                USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_RES_ACK;
            }
        }
        else if (endp == 1) {
            if (token == USBHS_UIS_TOKEN_IN) {
                /* EP1 IN complete: build wire-format report from latest_buttons */
                uint8_t btn = latest_buttons;
                ep1_tx_buf[0] = REPORT_ID_INPUT;
                ep1_tx_buf[1] = btn;
                ep1_tx_buf[2] = 0;
                for (int i = 0; i < ADP_SENSOR_COUNT; i++) {
                    uint16_t val = (i < 8 && ((btn >> i) & 1)) ? ADP_MAX_SENSOR_VALUE : 0;
                    ep1_tx_buf[3 + i*2]     = (uint8_t)val;
                    ep1_tx_buf[3 + i*2 + 1] = (uint8_t)(val >> 8);
                }
                USBHSD->UEP1_TX_LEN  = DEF_INPUT_REPORT_WIRE_SIZE;
                USBHSD->UEP1_TX_CTRL = (USBHSD->UEP1_TX_CTRL & ~USBHS_UEP_T_RES_MASK)
                                      | USBHS_UEP_T_RES_ACK;
            }
        }

        USBHSD->INT_FG = USBHS_UIF_TRANSFER;
    }
    else if (intflag & USBHS_UIF_BUS_RST) {
        USBHSD->DEV_AD = 0;
        usb_device_state = USB_STATE_DEFAULT;

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

/* Naked ISR wrapper: saves only integer regs, no FP overhead. */
void __attribute__((naked)) USBHS_IRQHandler(void)
{
    __asm__ volatile (
        "addi sp, sp, -64\n"
        "sw ra,  60(sp)\n"
        "sw t0,  56(sp)\n"
        "sw t1,  52(sp)\n"
        "sw t2,  48(sp)\n"
        "sw a0,  44(sp)\n"
        "sw a1,  40(sp)\n"
        "sw a2,  36(sp)\n"
        "sw a3,  32(sp)\n"
        "sw a4,  28(sp)\n"
        "sw a5,  24(sp)\n"
        "sw a6,  20(sp)\n"
        "sw a7,  16(sp)\n"
        "sw t3,  12(sp)\n"
        "sw t4,   8(sp)\n"
        "sw t5,   4(sp)\n"
        "sw t6,   0(sp)\n"
        "call USBHS_IRQ_Body\n"
        "lw ra,  60(sp)\n"
        "lw t0,  56(sp)\n"
        "lw t1,  52(sp)\n"
        "lw t2,  48(sp)\n"
        "lw a0,  44(sp)\n"
        "lw a1,  40(sp)\n"
        "lw a2,  36(sp)\n"
        "lw a3,  32(sp)\n"
        "lw a4,  28(sp)\n"
        "lw a5,  24(sp)\n"
        "lw a6,  20(sp)\n"
        "lw a7,  16(sp)\n"
        "lw t3,  12(sp)\n"
        "lw t4,   8(sp)\n"
        "lw t5,   4(sp)\n"
        "lw t6,   0(sp)\n"
        "addi sp, sp, 64\n"
        "mret\n"
    );
}
