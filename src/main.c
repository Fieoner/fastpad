#include "ch32v30x.h"
#include "debug.h"
#include "buttons.h"
#include "usb_device.h"

/*
 * Timer 2: fires at 100kHz for button scanning.
 * System clock = 144MHz, APB1 timer clock = 144MHz.
 * PSC=0 -> 144MHz tick, Period=1439 -> 100kHz interrupt.
 */
static void Timer_Init(void)
{
    TIM_TimeBaseInitTypeDef tim;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    tim.TIM_Prescaler     = 0;
    tim.TIM_Period        = 1439;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    tim.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &tim);

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);

    NVIC_EnableIRQ(TIM2_IRQn);
}

int main(void)
{
    SystemCoreClockUpdate();
    Delay_Init();

    Buttons_Init();
    USB_Device_Init();
    Timer_Init();

    while (1) {
        __WFI();
    }
}
