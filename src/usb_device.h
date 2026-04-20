#ifndef __USB_DEVICE_H
#define __USB_DEVICE_H

#include <stdint.h>

/* USB device states */
#define USB_STATE_DEFAULT       0
#define USB_STATE_ADDRESSED     1
#define USB_STATE_CONFIGURED    2

/* USB speed after enumeration */
#define USB_SPEED_FULL          0
#define USB_SPEED_HIGH          1

void USB_Device_Init(void);
void USB_Device_SendReport(uint8_t *data, uint8_t len);

extern volatile uint8_t usb_device_state;
extern volatile uint8_t usb_device_speed;
extern volatile uint8_t ep1_tx_busy;

#endif
