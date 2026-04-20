#include "ch32v30x.h"
#include "buttons.h"
#include "usb_device.h"

static uint8_t last_report;

void NMI_Handler(void) __attribute__((interrupt("machine")));
void NMI_Handler(void) {}

void HardFault_Handler(void) __attribute__((interrupt("machine")));
void HardFault_Handler(void)
{
    while (1);
}

/*
 * TIM2 ISR: scan buttons at 16kHz (oversample 2x the USB rate).
 * Send a HID report whenever the button state changes.
 */
void TIM2_IRQHandler(void) __attribute__((interrupt("machine")));
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

        uint8_t buttons = Buttons_Scan();

        if (buttons != last_report) {
            last_report = buttons;
            USB_Device_SendReport(&last_report, 1);
        }
    }
}
