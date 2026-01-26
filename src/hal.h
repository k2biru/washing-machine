#ifndef HAL_H
#define HAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Actuator IDs corresponding to the physical relays/devices
typedef enum {
    HAL_ACT_MOTOR_POWER,
    HAL_ACT_MOTOR_DIR, // 0/LOW = CW, 1/HIGH = CCW (or vice versa depending on wiring)
    HAL_ACT_INLET,
    HAL_ACT_DRAIN,
    HAL_ACT_SOAP
} hal_actuator_t;

// Button IDs
typedef enum {
    HAL_BTN_A, // Start/Pause/OK
    HAL_BTN_B, // Next
    HAL_BTN_C  // ESC
} hal_button_t;

// Song IDs
typedef enum { HAL_SONG_START, HAL_SONG_FINISHED, HAL_SONG_ERROR } hal_song_t;

/**
 * @brief Initialize all hardware pins and peripherals.
 */
void hal_init(void);

/**
 * @brief Get system time in milliseconds.
 */
uint32_t hal_millis(void);

/**
 * @brief Blocking delay.
 * @param ms Milliseconds to wait
 */
void hal_delay(uint32_t ms);

/**
 * @brief Control an actuator (Relay).
 * @param act Actuator ID
 * @param active true = ON/ACTIVE, false = OFF/INACTIVE
 */
void hal_actuator_write(hal_actuator_t act, bool active);

/**
 * @brief Read button state.
 * @param btn Button ID
 * @return true if button is currently pressed (active), false otherwise.
 */
bool hal_button_read(hal_button_t btn);

/**
 * @brief Play a defined song/tune on the buzzer.
 * @param song_id Song ID to play
 */
void hal_sound_play(hal_song_t song_id);

/**
 * @brief Read all sensors.
 * @param drain_check Output pointer for drain sensor state (true=water detected).
 * @param water_level_raw Output pointer for raw water level sensor value (0-100 or enum).
 */
void hal_sensors_read(bool *drain_check, int *water_level_raw);

#ifndef ARDUINO
/* --- Simulation Hooks --- */
/* These allow the PC simulation to inject sensor state and read actuator state */

void hal_sim_set_sensors(bool drain_check, int water_level_raw);
void hal_sim_set_button(hal_button_t btn, bool pressed);

typedef struct {
    bool motor_power;
    bool motor_ccw;
    bool inlet;
    bool drain;
    bool soap;
} hal_sim_actuators_t;

hal_sim_actuators_t hal_sim_get_actuators(void);

#endif

#ifdef __cplusplus
}
#endif

#endif // HAL_H
