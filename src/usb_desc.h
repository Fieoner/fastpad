#ifndef __USB_DESC_H
#define __USB_DESC_H

#include <stdint.h>

#define DEF_USB_VID    0x1209  /* pid.codes open-source VID */
#define DEF_USB_PID    0xB196  /* analog-dance-pad PID */

#define DEF_USB_EP0_HS_SIZE   64
#define DEF_USB_EP0_FS_SIZE   64

/* Input report size: report ID (1) + buttons (2) + sensors (64) = 67 */
#define DEF_INPUT_REPORT_WIRE_SIZE  67

#define DEF_USB_EP1_HS_SIZE   DEF_INPUT_REPORT_WIRE_SIZE
#define DEF_USB_EP1_FS_SIZE   DEF_INPUT_REPORT_WIRE_SIZE

#define DEF_USB_EP1_IN_ADDR   0x81

extern const uint8_t GamepadDeviceDesc[];
extern const uint8_t GamepadCfgDesc_HS[];
extern const uint8_t GamepadCfgDesc_FS[];
extern const uint8_t GamepadReportDesc[];
extern const uint8_t GamepadQualifierDesc[];
extern const uint8_t GamepadStringLangID[];
extern const uint8_t GamepadStringVendor[];
extern const uint8_t GamepadStringProduct[];
extern const uint8_t GamepadStringSerial[];

extern const uint16_t GamepadDeviceDescLen;
extern const uint16_t GamepadCfgDesc_HS_Len;
extern const uint16_t GamepadCfgDesc_FS_Len;
extern const uint16_t GamepadReportDescLen;
extern const uint16_t GamepadQualifierDescLen;

#endif
