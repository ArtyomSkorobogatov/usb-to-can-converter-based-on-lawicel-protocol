#ifndef UTILS_CONF_H
#define UTILS_CONF_H

#include "SEGGER_RTT.h"

#if defined(DEBUG) && !defined(NDEBUG)
#define debug_printf(...) SEGGER_RTT_printf(0, ##__VA_ARGS__)
#else
#define debug_printf(...)
#endif // DEBUG

#define DISABLE_IRQ() __disable_irq()
#define ENABLE_IRQ() __enable_irq()

#endif //UTILS_CONF_H
