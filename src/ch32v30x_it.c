#include "ch32v30x.h"
#include "buttons.h"
#include "usb_device.h"

void NMI_Handler(void) __attribute__((interrupt("machine")));
void NMI_Handler(void) {}

void HardFault_Handler(void) __attribute__((interrupt("machine")));
void HardFault_Handler(void)
{
    while (1);
}

/*
 * TIM2 ISR: scan buttons at 100kHz.
 * Updates latest_report which the USB handler sends every microframe.
 */
void TIM2_IRQHandler(void) __attribute__((interrupt("machine")));
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        uint8_t buttons = Buttons_Scan();
        latest_report = buttons;
    }
}
