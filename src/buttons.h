#ifndef __BUTTONS_H
#define __BUTTONS_H

#include <stdint.h>

/*
 * 32 digital sensor inputs across GPIO ports A, B, C, D, E → 8 buttons.
 *
 *   Button 0: PE2, PE3, PE4, PE5
 *   Button 1: PC14, PC15, PC0, PC1
 *   Button 2: PA0, PA1, PA2, PA3
 *   Button 3: PA6, PA7, PC4, PC5
 *   Button 4: PB7, PB6, PB5, PB4
 *   Button 5: PD6, PD5, PD4, PD3
 *   Button 6: PD0, PC12, PC11, PC10
 *   Button 7: PA13, PA12, PA11, PA10
 *
 * Eager debounce: press is reported immediately (zero added latency).
 * Release requires DEBOUNCE_RELEASE consecutive readings to confirm.
 */

#define DEBOUNCE_PRESS    2
#define DEBOUNCE_RELEASE  4

void Buttons_Init(void);

/* Call from timer ISR. Returns 8-bit button state. */
uint8_t Buttons_Scan(void);

#endif
