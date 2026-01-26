#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "wm_control.h"

/**
 * @brief Application State Structure
 */
typedef struct {
    int ui_state;
    int menu_step;
    int sel_program;
    int sel_level;
    int sel_power;

    wm_controller_t ctrl;
    wm_sensors_t sensors;
    wm_actuators_t actuators;
    uint32_t last_tick_time;
} App;

/**
 * @brief Initialize the application (HAL, State Machine, etc).
 * @param app Pointer to App structure
 */
void app_init(App *app);

/**
 * @brief Main application loop.
 * Should be called repeatedly.
 * @param app Pointer to App structure
 */
void app_loop(App *app);

#ifdef __cplusplus
}
#endif

#endif // APP_H
