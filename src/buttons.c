#include "buttons.h"
#include "ch32v30x.h"

/*
 * Pin masks per port, per button.
 * Index by [button][port] where port: 0=A, 1=B, 2=C, 3=D, 4=E
 */
#define PORT_A 0
#define PORT_B 1
#define PORT_C 2
#define PORT_D 3
#define PORT_E 4
#define NUM_PORTS 5

static const uint16_t button_pins[8][NUM_PORTS] = {
    /* Button 0: PE2, PE3, PE4, PE5 */
    [0] = { [PORT_E] = (1<<2)|(1<<3)|(1<<4)|(1<<5) },
    /* Button 1: PC14, PC15, PC0, PC1 */
    [1] = { [PORT_C] = (1<<14)|(1<<15)|(1<<0)|(1<<1) },
    /* Button 2: PA0, PA1, PA2, PA3 */
    [2] = { [PORT_A] = (1<<0)|(1<<1)|(1<<2)|(1<<3) },
    /* Button 3: PA6, PA7, PC4, PC5 */
    [3] = { [PORT_A] = (1<<6)|(1<<7), [PORT_C] = (1<<4)|(1<<5) },
    /* Button 4: PB7, PB6, PB5, PB4 */
    [4] = { [PORT_B] = (1<<4)|(1<<5)|(1<<6)|(1<<7) },
    /* Button 5: PD6, PD5, PD4, PD3 */
    [5] = { [PORT_D] = (1<<3)|(1<<4)|(1<<5)|(1<<6) },
    /* Button 6: PD0, PC12, PC11, PC10 */
    [6] = { [PORT_D] = (1<<0), [PORT_C] = (1<<10)|(1<<11)|(1<<12) },
    /* Button 7: PA13, PA12, PA11, PA10 */
    [7] = { [PORT_A] = (1<<10)|(1<<11)|(1<<12)|(1<<13) },
};

/* Precomputed: union of all pins used per port, for GPIO init */
static const uint16_t port_used_pins[NUM_PORTS] = {
    /* PA: 0,1,2,3,6,7,10,11,12,13 */
    (1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<6)|(1<<7)|(1<<10)|(1<<11)|(1<<12)|(1<<13),
    /* PB: 4,5,6,7 */
    (1<<4)|(1<<5)|(1<<6)|(1<<7),
    /* PC: 0,1,4,5,10,11,12,14,15 */
    (1<<0)|(1<<1)|(1<<4)|(1<<5)|(1<<10)|(1<<11)|(1<<12)|(1<<14)|(1<<15),
    /* PD: 0,3,4,5,6 */
    (1<<0)|(1<<3)|(1<<4)|(1<<5)|(1<<6),
    /* PE: 2,3,4,5 */
    (1<<2)|(1<<3)|(1<<4)|(1<<5),
};

static GPIO_TypeDef * const gpio_ports[NUM_PORTS] = {
    GPIOA, GPIOB, GPIOC, GPIOD, GPIOE
};

static const uint32_t port_clocks[NUM_PORTS] = {
    RCC_APB2Periph_GPIOA,
    RCC_APB2Periph_GPIOB,
    RCC_APB2Periph_GPIOC,
    RCC_APB2Periph_GPIOD,
    RCC_APB2Periph_GPIOE,
};

static uint8_t press_counters[8];
static uint8_t release_counters[8];
static uint8_t debounced_state;

void Buttons_Init(void)
{
    GPIO_InitTypeDef gpio;

    /* Enable clocks for all used ports */
    uint32_t clocks = 0;
    for (int p = 0; p < NUM_PORTS; p++)
        clocks |= port_clocks[p];
    RCC_APB2PeriphClockCmd(clocks, ENABLE);

    /* Configure only the pins we actually use as input pull-up */
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    for (int p = 0; p < NUM_PORTS; p++) {
        if (port_used_pins[p]) {
            gpio.GPIO_Pin = port_used_pins[p];
            GPIO_Init(gpio_ports[p], &gpio);
        }
    }

    debounced_state = 0;
    for (int i = 0; i < 8; i++) {
        press_counters[i] = 0;
        release_counters[i] = 0;
    }
}

uint8_t Buttons_Scan(void)
{
    /* Read all 5 ports once */
    uint16_t port_state[NUM_PORTS];
    for (int p = 0; p < NUM_PORTS; p++)
        port_state[p] = (uint16_t)(gpio_ports[p]->INDR);

    for (int i = 0; i < 8; i++) {
        /* Check if ANY sensor for this button is active (low) */
        uint8_t raw_pressed = 0;
        for (int p = 0; p < NUM_PORTS; p++) {
            if (button_pins[i][p] && (~port_state[p] & button_pins[i][p])) {
                raw_pressed = 1;
                break;
            }
        }

        uint8_t currently_pressed = (debounced_state >> i) & 1;

        if (raw_pressed && !currently_pressed) {
            press_counters[i]++;
            if (press_counters[i] >= DEBOUNCE_PRESS) {
                debounced_state |= (1 << i);
                press_counters[i] = 0;
            }
            release_counters[i] = 0;
        } else if (!raw_pressed && currently_pressed) {
            release_counters[i]++;
            if (release_counters[i] >= DEBOUNCE_RELEASE) {
                debounced_state &= ~(1 << i);
                release_counters[i] = 0;
            }
            press_counters[i] = 0;
        } else {
            press_counters[i] = 0;
            release_counters[i] = 0;
        }
    }

    return debounced_state;
}
