#include "led.h"
#include "stm32f0xx_hal.h"

// Private variables
static uint32_t led_blue_laston   = 0;
static uint32_t led_green_laston  = 0;
static uint32_t led_blue_lastoff  = 0;
static uint32_t led_green_lastoff = 0;

__STATIC_INLINE void internal_led_blue_on(void) { HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET); }

__STATIC_INLINE void internal_led_blue_off(void) { HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET); }

__STATIC_INLINE void internal_led_red_on(void) { HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET); }

__STATIC_INLINE void internal_led_red_off(void) { HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET); }

void led_red_on(void) {
    // Make sure the LED has been off for at least LED_DURATION before turning on
    // again This prevents a solid status LED on a busy canbus
    if (led_green_laston == 0 && HAL_GetTick() - led_green_lastoff > LED_DURATION) {
        // Invert LED
        internal_led_red_on();
        led_green_laston = HAL_GetTick();
    }
}

void led_red_off(void) { internal_led_red_off(); }

// Blink blue LED (blocking)
void leds_test_blink(uint8_t numblinks) {
    uint8_t i;
    for (i = 0; i < numblinks; i++) {
        internal_led_blue_on();
        HAL_Delay(100);
        internal_led_blue_off();
        HAL_Delay(100);
    }
    for (i = 0; i < numblinks; i++) {
        internal_led_red_on();
        HAL_Delay(100);
        internal_led_red_off();
        HAL_Delay(100);
    }
}

// Attempt to turn on status LED
void led_blue_on(void) {
    // Make sure the LED has been off for at least LED_DURATION before turning on
    // again This prevents a solid status LED on a busy canbus
    if (led_blue_laston == 0 && HAL_GetTick() - led_blue_lastoff > LED_DURATION) {
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
        led_blue_laston = HAL_GetTick();
    }
}

// Process time-based LED events
void led_process(void) {
    // If LED has been on for long enough, turn it off
    if (led_blue_laston > 0 && HAL_GetTick() - led_blue_laston > LED_DURATION) {
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
        led_blue_laston  = 0;
        led_blue_lastoff = HAL_GetTick();
    }

    // If LED has been on for long enough, turn it off
    if (led_green_laston > 0 && HAL_GetTick() - led_green_laston > LED_DURATION) {
        // Invert LED
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
        led_green_laston  = 0;
        led_green_lastoff = HAL_GetTick();
    }
}
