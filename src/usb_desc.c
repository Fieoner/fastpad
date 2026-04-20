#include "usb_desc.h"

/*
 * HID Report Descriptor: 8-button gamepad
 * Report = 1 byte, each bit is a button (1=pressed)
 */
const uint8_t GamepadReportDesc[] = {
    0x05, 0x01,        /* Usage Page (Generic Desktop) */
    0x09, 0x05,        /* Usage (Gamepad) */
    0xA1, 0x01,        /* Collection (Application) */
    0xA1, 0x00,        /*   Collection (Physical) */
    0x05, 0x09,        /*     Usage Page (Button) */
    0x19, 0x01,        /*     Usage Minimum (Button 1) */
    0x29, 0x08,        /*     Usage Maximum (Button 8) */
    0x15, 0x00,        /*     Logical Minimum (0) */
    0x25, 0x01,        /*     Logical Maximum (1) */
    0x75, 0x01,        /*     Report Size (1) */
    0x95, 0x08,        /*     Report Count (8) */
    0x81, 0x02,        /*     Input (Data, Variable, Absolute) */
    0xC0,              /*   End Collection */
    0xC0               /* End Collection */
};

/* Device Descriptor */
const uint8_t GamepadDeviceDesc[] = {
    0x12,              /* bLength */
    0x01,              /* bDescriptorType: Device */
    0x00, 0x02,        /* bcdUSB: 2.00 (HS) */
    0x00,              /* bDeviceClass: defined at interface */
    0x00,              /* bDeviceSubClass */
    0x00,              /* bDeviceProtocol */
    0x40,              /* bMaxPacketSize0: 64 */
    (uint8_t)(DEF_USB_VID), (uint8_t)(DEF_USB_VID >> 8),
    (uint8_t)(DEF_USB_PID), (uint8_t)(DEF_USB_PID >> 8),
    0x00, 0x01,        /* bcdDevice: 1.00 */
    0x01,              /* iManufacturer */
    0x02,              /* iProduct */
    0x03,              /* iSerialNumber */
    0x01               /* bNumConfigurations */
};

/* Configuration Descriptor - High Speed */
const uint8_t GamepadCfgDesc_HS[] = {
    /* Configuration */
    0x09,              /* bLength */
    0x02,              /* bDescriptorType: Configuration */
    0x22, 0x00,        /* wTotalLength: 34 */
    0x01,              /* bNumInterfaces */
    0x01,              /* bConfigurationValue */
    0x00,              /* iConfiguration */
    0x80,              /* bmAttributes: bus powered */
    0x32,              /* bMaxPower: 100mA */

    /* Interface */
    0x09,              /* bLength */
    0x04,              /* bDescriptorType: Interface */
    0x00,              /* bInterfaceNumber */
    0x00,              /* bAlternateSetting */
    0x01,              /* bNumEndpoints */
    0x03,              /* bInterfaceClass: HID */
    0x00,              /* bInterfaceSubClass: no boot */
    0x00,              /* bInterfaceProtocol: none */
    0x00,              /* iInterface */

    /* HID */
    0x09,              /* bLength */
    0x21,              /* bDescriptorType: HID */
    0x11, 0x01,        /* bcdHID: 1.11 */
    0x00,              /* bCountryCode */
    0x01,              /* bNumDescriptors */
    0x22,              /* bDescriptorType: Report */
    (uint8_t)(sizeof(GamepadReportDesc)),
    (uint8_t)(sizeof(GamepadReportDesc) >> 8),

    /* Endpoint 1 IN - Interrupt */
    0x07,              /* bLength */
    0x05,              /* bDescriptorType: Endpoint */
    0x81,              /* bEndpointAddress: EP1 IN */
    0x03,              /* bmAttributes: Interrupt */
    (uint8_t)(DEF_USB_EP1_HS_SIZE),
    (uint8_t)(DEF_USB_EP1_HS_SIZE >> 8),
    0x01               /* bInterval: 1 (125us microframe = 8000Hz) */
};

/* Configuration Descriptor - Full Speed (fallback) */
const uint8_t GamepadCfgDesc_FS[] = {
    /* Configuration */
    0x09,
    0x02,
    0x22, 0x00,
    0x01,
    0x01,
    0x00,
    0x80,
    0x32,

    /* Interface */
    0x09,
    0x04,
    0x00,
    0x00,
    0x01,
    0x03,
    0x00,
    0x00,
    0x00,

    /* HID */
    0x09,
    0x21,
    0x11, 0x01,
    0x00,
    0x01,
    0x22,
    (uint8_t)(sizeof(GamepadReportDesc)),
    (uint8_t)(sizeof(GamepadReportDesc) >> 8),

    /* Endpoint 1 IN */
    0x07,
    0x05,
    0x81,
    0x03,
    (uint8_t)(DEF_USB_EP1_FS_SIZE),
    (uint8_t)(DEF_USB_EP1_FS_SIZE >> 8),
    0x01               /* bInterval: 1ms (FS max = 1000Hz) */
};

/* Device Qualifier (for HS, describes FS capability) */
const uint8_t GamepadQualifierDesc[] = {
    0x0A,              /* bLength */
    0x06,              /* bDescriptorType: Device Qualifier */
    0x00, 0x02,        /* bcdUSB: 2.00 */
    0x00,
    0x00,
    0x00,
    0x40,              /* bMaxPacketSize0: 64 */
    0x01,              /* bNumConfigurations */
    0x00               /* bReserved */
};

/* String Descriptors */
const uint8_t GamepadStringLangID[] = {
    0x04, 0x03,
    0x09, 0x04         /* English (US) */
};

const uint8_t GamepadStringVendor[] = {
    0x0C, 0x03,
    'P', 0, 'a', 0, 'd', 0, 'M', 0, 'F', 0
};

const uint8_t GamepadStringProduct[] = {
    0x14, 0x03,
    'G', 0, 'a', 0, 'm', 0, 'e', 0, 'p', 0,
    'a', 0, 'd', 0, ' ', 0, '8', 0
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
