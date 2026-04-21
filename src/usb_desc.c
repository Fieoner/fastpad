#include "usb_desc.h"
#include "adp_reports.h"

/*
 * HID Report Descriptor: analog-dance-pad compatible
 * Multi-report descriptor with all ADP report IDs.
 *
 * Report IDs:
 *   0x01 - Input:   uint16 buttons + uint16[32] sensors
 *   0x02 - Feature: pad configuration (thresholds, mapping)
 *   0x03 - Output:  reset
 *   0x04 - Output:  save configuration
 *   0x05 - Feature: device name
 *   0x06 - Input:   unused joystick (StepMania compat)
 *   0x07 - Feature: light rule (stub)
 *   0x08 - Output:  factory reset
 *   0x09 - Feature: identification
 *   0x0A - Feature: LED mapping (stub)
 *   0x0B - Feature: set property
 *   0x0C - Feature: sensor config
 *   0x0E - Feature: identification v2
 */
const uint8_t GamepadReportDesc[] = {
    0x05, 0x01,              /* Usage Page (Generic Desktop) */
    0x09, 0x04,              /* Usage (Joystick) */
    0xA1, 0x01,              /* Collection (Application) */

    /* --- Report 0x01: Input (buttons + sensors) --- */
    0x85, REPORT_ID_INPUT,   /*   Report ID (1) */
    0x05, 0x09,              /*   Usage Page (Button) */
    0x19, 0x01,              /*   Usage Minimum (1) */
    0x29, ADP_BUTTON_COUNT,  /*   Usage Maximum (16) */
    0x15, 0x00,              /*   Logical Minimum (0) */
    0x25, 0x01,              /*   Logical Maximum (1) */
    0x75, 0x01,              /*   Report Size (1) */
    0x95, ADP_BUTTON_COUNT,  /*   Report Count (16) */
    0x81, 0x02,              /*   Input (Data, Var, Abs) */
    0x06, 0x00, 0xFF,        /*   Usage Page (Vendor 0xFF00) */
    0x09, 0x01,              /*   Usage (1) */
    0xA1, 0x00,              /*   Collection (Physical) */
    0x09, 0x01,              /*     Usage (1) */
    0x15, 0x00,              /*     Logical Minimum (0) */
    0x26, 0xFF, 0x00,        /*     Logical Maximum (255) */
    0x75, 0x08,              /*     Report Size (8) */
    0x95, ADP_SENSOR_COUNT*2,/*     Report Count (64) */
    0x81, 0x02,              /*     Input (Data, Var, Abs) */
    0xC0,                    /*   End Collection */

    /* --- Report 0x02: Feature (pad configuration) --- */
    0x85, REPORT_ID_PAD_CONFIG,
    0x06, 0x00, 0xFF,
    0x09, 0x02,
    0xA1, 0x00,
    0x09, 0x02,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, PAD_CONFIG_REPORT_SIZE,
    0xB1, 0x42,              /*   Feature (Data, Var, Abs, Non-Volatile) */
    0xC0,

    /* --- Report 0x03: Output (reset) --- */
    0x85, REPORT_ID_RESET,
    0x06, 0x00, 0xFF,
    0x09, 0x02,
    0x91, 0x42,              /*   Output (Data, Var, Abs, Non-Volatile) */

    /* --- Report 0x04: Output (save configuration) --- */
    0x85, REPORT_ID_SAVE,
    0x06, 0x00, 0xFF,
    0x09, 0x02,
    0x91, 0x42,

    /* --- Report 0x05: Feature (name) --- */
    0x85, REPORT_ID_NAME,
    0x06, 0x00, 0xFF,
    0x09, 0x02,
    0xA1, 0x00,
    0x09, 0x02,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, NAME_REPORT_SIZE,
    0xB1, 0x42,
    0xC0,

    /* --- Report 0x06: Input (unused joystick for StepMania) --- */
    0x85, REPORT_ID_JOYSTICK,
    0x05, 0x01,
    0x09, 0x04,
    0xA1, 0x00,
    0x09, 0x30,              /*   Usage (X) */
    0x16, 0x00, 0x00,        /*   Logical Minimum (0) 16-bit */
    0x26, 0x7F, 0x00,        /*   Logical Maximum (127) */
    0x36, 0x00, 0x00,        /*   Physical Minimum (0) */
    0x46, 0x7F, 0x00,        /*   Physical Maximum (127) */
    0x95, 0x01,
    0x75, 0x08,
    0x81, 0x02,
    0xC0,

    /* --- Report 0x07: Feature (light rule - stub) --- */
    0x85, REPORT_ID_LIGHT_RULE,
    0x06, 0x00, 0xFF,
    0x09, 0x02,
    0xA1, 0x00,
    0x09, 0x02,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, LIGHT_RULE_REPORT_SIZE,
    0xB1, 0x42,
    0xC0,

    /* --- Report 0x08: Output (factory reset) --- */
    0x85, REPORT_ID_FACTORY_RESET,
    0x06, 0x00, 0xFF,
    0x09, 0x02,
    0x91, 0x42,

    /* --- Report 0x09: Feature (identification) --- */
    0x85, REPORT_ID_IDENTIFICATION,
    0x06, 0x00, 0xFF,
    0x09, 0x02,
    0xA1, 0x00,
    0x09, 0x02,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, IDENTIFICATION_REPORT_SIZE,
    0xB1, 0x42,
    0xC0,

    /* --- Report 0x0A: Feature (LED mapping - stub) --- */
    0x85, REPORT_ID_LED_MAPPING,
    0x06, 0x00, 0xFF,
    0x09, 0x02,
    0xA1, 0x00,
    0x09, 0x02,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, LED_MAPPING_REPORT_SIZE,
    0xB1, 0x42,
    0xC0,

    /* --- Report 0x0B: Feature (set property) --- */
    0x85, REPORT_ID_SET_PROPERTY,
    0x06, 0x00, 0xFF,
    0x09, 0x02,
    0xA1, 0x00,
    0x09, 0x02,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, SET_PROPERTY_REPORT_SIZE,
    0xB1, 0x42,
    0xC0,

    /* --- Report 0x0C: Feature (sensor config) --- */
    0x85, REPORT_ID_SENSOR,
    0x06, 0x00, 0xFF,
    0x09, 0x02,
    0xA1, 0x00,
    0x09, 0x02,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, SENSOR_REPORT_SIZE,
    0xB1, 0x42,
    0xC0,

    /* --- Report 0x0E: Feature (identification v2) --- */
    0x85, REPORT_ID_IDENTIFICATION_V2,
    0x06, 0x00, 0xFF,
    0x09, 0x02,
    0xA1, 0x00,
    0x09, 0x02,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, IDENTIFICATION_V2_REPORT_SIZE,
    0xB1, 0x42,
    0xC0,

    0xC0                     /* End Collection (Application) */
};

/* Device Descriptor */
const uint8_t GamepadDeviceDesc[] = {
    0x12,
    0x01,              /* bDescriptorType: Device */
    0x00, 0x02,        /* bcdUSB: 2.00 */
    0x00, 0x00, 0x00,
    0x40,              /* bMaxPacketSize0: 64 */
    (uint8_t)(DEF_USB_VID), (uint8_t)(DEF_USB_VID >> 8),
    (uint8_t)(DEF_USB_PID), (uint8_t)(DEF_USB_PID >> 8),
    0x01, 0x00,        /* bcdDevice: 0.01 */
    0x01, 0x02, 0x03,
    0x01
};

/* Configuration Descriptor - High Speed */
const uint8_t GamepadCfgDesc_HS[] = {
    /* Configuration */
    0x09, 0x02,
    0x22, 0x00,        /* wTotalLength: 34 */
    0x01, 0x01, 0x00,
    0x80, 0x32,

    /* Interface */
    0x09, 0x04,
    0x00, 0x00, 0x01,
    0x03, 0x00, 0x00, 0x00,

    /* HID */
    0x09, 0x21,
    0x11, 0x01,        /* bcdHID: 1.11 */
    0x00, 0x01, 0x22,
    (uint8_t)(sizeof(GamepadReportDesc)),
    (uint8_t)(sizeof(GamepadReportDesc) >> 8),

    /* Endpoint 1 IN - Interrupt */
    0x07, 0x05,
    0x81, 0x03,
    (uint8_t)(DEF_USB_EP1_HS_SIZE),
    (uint8_t)(DEF_USB_EP1_HS_SIZE >> 8),
    0x01               /* bInterval: 1 (125us = 8000Hz) */
};

/* Configuration Descriptor - Full Speed */
const uint8_t GamepadCfgDesc_FS[] = {
    0x09, 0x02,
    0x22, 0x00,
    0x01, 0x01, 0x00,
    0x80, 0x32,

    0x09, 0x04,
    0x00, 0x00, 0x01,
    0x03, 0x00, 0x00, 0x00,

    0x09, 0x21,
    0x11, 0x01,
    0x00, 0x01, 0x22,
    (uint8_t)(sizeof(GamepadReportDesc)),
    (uint8_t)(sizeof(GamepadReportDesc) >> 8),

    0x07, 0x05,
    0x81, 0x03,
    (uint8_t)(DEF_USB_EP1_FS_SIZE),
    (uint8_t)(DEF_USB_EP1_FS_SIZE >> 8),
    0x01
};

/* Device Qualifier */
const uint8_t GamepadQualifierDesc[] = {
    0x0A, 0x06,
    0x00, 0x02,
    0x00, 0x00, 0x00,
    0x40, 0x01, 0x00
};

/* String Descriptors */
const uint8_t GamepadStringLangID[] = {
    0x04, 0x03, 0x09, 0x04
};

const uint8_t GamepadStringVendor[] = {
    0x10, 0x03,
    'D', 0, 'D', 0, 'R', 0, '-', 0, 'E', 0, 'X', 0, 'P', 0
};

const uint8_t GamepadStringProduct[] = {
    0x1A, 0x03,
    'F', 0, 'S', 0, 'R', 0, ' ', 0, 'M', 0, 'i', 0, 'n', 0,
    'i', 0, ' ', 0, 'p', 0, 'a', 0, 'd', 0
};

const uint8_t GamepadStringSerial[] = {
    0x0A, 0x03,
    '0', 0, '0', 0, '0', 0, '1', 0
};

const uint16_t GamepadDeviceDescLen     = sizeof(GamepadDeviceDesc);
const uint16_t GamepadCfgDesc_HS_Len    = sizeof(GamepadCfgDesc_HS);
const uint16_t GamepadCfgDesc_FS_Len    = sizeof(GamepadCfgDesc_FS);
const uint16_t GamepadReportDescLen     = sizeof(GamepadReportDesc);
const uint16_t GamepadQualifierDescLen  = sizeof(GamepadQualifierDesc);
