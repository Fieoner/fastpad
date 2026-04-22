#ifndef __USB_DEVICE_H
#define __USB_DEVICE_H

#include <stdint.h>
#include "adp_reports.h"
#include "usb_desc.h"

/* USB device states */
#define USB_STATE_DEFAULT       0
#define USB_STATE_ADDRESSED     1
#define USB_STATE_CONFIGURED    2

/* USB speed after enumeration */
#define USB_SPEED_FULL          0
#define USB_SPEED_HIGH          1

void USB_Device_Init(void);

extern volatile uint8_t usb_device_state;
extern volatile uint8_t usb_device_speed;
extern volatile uint8_t latest_buttons;
extern volatile uint8_t bootloader_request;

#endif
