#include "main.h"
#include "SEGGER_RTT.h"

// SEGGER RTT: workaround for large memory mcu`s
#ifdef DEBUG
SEGGER_RTT_CB _SEGGER_RTT;
char seggerRttUpBuffer[BUFFER_SIZE_UP];
char seggerRttDownBuffer[BUFFER_SIZE_DOWN];
#endif

extern TIM_HandleTypeDef htim4;

void utils_init_microseconds_timer() {
    HAL_TIM_Base_Start(&htim4);
}

uint32_t utils_get_microseconds_timer() {
    return TIM4->CNT;
}

uint32_t utils_get_milliseconds_tick() {
    return HAL_GetTick();
}
