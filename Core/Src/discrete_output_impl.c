#include "main.h"
#include "discrete_output.h"

bool
discrete_output_ll_init(const DiscreteOutput_t* pDiscreteOutput) {
    /* Initialized by CubeMX */
    (void) pDiscreteOutput;
    return true;
}

void
discrete_output_ll_set_level(const DiscreteOutput_t* pDiscreteOutput, const bool level) {
    if (pDiscreteOutput) {
        GPIO_PinState outputNewState = (level) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        switch (pDiscreteOutput->id) {
            case LED_RED_ID:
                HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, outputNewState);
                break;

            case LED_BLUE_ID:
                HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, outputNewState);
                break;

            default:
                /* error ID */
                break;
        }
    }
}

uint32_t
discrete_output_ll_get_tick(void) {
    return HAL_GetTick();
}
