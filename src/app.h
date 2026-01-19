#ifndef APP_H
#define APP_H

extern "C" {
#include "wm_control.h"
}

/**
 * @brief Initializes the hardware actuators (Relays).
 * Should be called once during setup.
 */
int wm_actuators_init(void);

/**
 * @brief Executes the washing machine program simulation/logic.
 * Contains the main tick loop for the state machine.
 */
int wm_program_ex(void);

#endif // APP_H
