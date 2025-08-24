#ifndef _LED_H
#define _LED_H

#include "main.h"

#define LED_DURATION 25 

void leds_test_blink(uint8_t numblinks);
void led_red_on(void);
void led_red_off(void);
void led_blue_on(void);
void led_process(void);

#endif
