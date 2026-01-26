#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "wm_control.h"

/**
 * @brief Initialize the application (HAL, State Machine, etc).
 */
void app_init(void);

/**
 * @brief Main application loop.
 * Should be called repeatedly.
 */
void app_loop(void);

#ifdef __cplusplus
}
#endif

#endif // APP_H
