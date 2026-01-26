#define _POSIX_C_SOURCE 199309L // for clock_gettime
#define _DEFAULT_SOURCE         // for usleep
#include "hal.h"

#ifdef ARDUINO
#include "../lib/buzzer/buzzer.h"
#include <Arduino.h>

/* Pin Definitions */
static const int PIN_MOTOR = 12;     /* RELAY: Controls motor POWER */
static const int PIN_MOTOR_ROT = 10; /* RELAY: Controls direction */
static const int PIN_INLET = 5;      /* RELAY: Controls water inlet valve */
static const int PIN_DRAIN = 6;      /* RELAY: Controls drain pump */
static const int PIN_SOAP = 7;       /* RELAY: Controls detergent pump */
static const int PIN_BUZZER = 13;    /* BUZZER: Feedback sound output */

static const int PIN_BTN_A = 2; /* BUTTON: Start/Pause/OK */
static const int PIN_BTN_B = 3; /* BUTTON: Next */
static const int PIN_BTN_C = 4; /* BUTTON: ESC */

void hal_init(void) {
    pinMode(PIN_MOTOR, OUTPUT);
    pinMode(PIN_MOTOR_ROT, OUTPUT);
    pinMode(PIN_INLET, OUTPUT);
    pinMode(PIN_DRAIN, OUTPUT);
    pinMode(PIN_SOAP, OUTPUT);

    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
    pinMode(PIN_BTN_C, INPUT_PULLUP);

    buzzer_init(PIN_BUZZER);

    // Initial state: all OFF (Active-low relays assumed HIGH means OFF)
    digitalWrite(PIN_MOTOR, HIGH);
    digitalWrite(PIN_MOTOR_ROT, HIGH);
    digitalWrite(PIN_INLET, HIGH);
    digitalWrite(PIN_DRAIN, HIGH);
    digitalWrite(PIN_SOAP, HIGH);
}

uint32_t hal_millis(void) { return millis(); }

void hal_delay(uint32_t ms) { delay(ms); }

void hal_actuator_write(hal_actuator_t act, bool active) {
    int pin = -1;
    // Logical 'active' means 'ON'.
    // Relays are Active-LOW: LOW -> ON, HIGH -> OFF.
    // So if active=true, we write LOW. If active=false, we write HIGH.
    int level = active ? LOW : HIGH;

    // Special case handling if needed, but standard relays follow above logic.
    // However, MOTOR_DIR might be different.
    // From original app.cpp: "LOW=CW, HIGH=CCW".
    // Let's assume hal_actuator_write(HAL_ACT_MOTOR_DIR, true) means toggle to CCW?
    // Or simpler: let the wrapper decide?
    // Let's look at original app.cpp:
    // digitalWrite(motorRotationPin, (act->motor_dir == MOTOR_CCW) ? HIGH : LOW);

    switch (act) {
    case HAL_ACT_MOTOR_POWER:
        pin = PIN_MOTOR;
        break;
    case HAL_ACT_MOTOR_DIR:
        pin = PIN_MOTOR_ROT;
        // For Direction: Let's define: false=CW (LOW), true=CCW (HIGH)
        level = active ? HIGH : LOW;
        break;
    case HAL_ACT_INLET:
        pin = PIN_INLET;
        break;
    case HAL_ACT_DRAIN:
        pin = PIN_DRAIN;
        break;
    case HAL_ACT_SOAP:
        pin = PIN_SOAP;
        break;
    }

    if (pin != -1) {
        digitalWrite(pin, level);
    }
}

bool hal_button_read(hal_button_t btn) {
    int pin = -1;
    switch (btn) {
    case HAL_BTN_A:
        pin = PIN_BTN_A;
        break;
    case HAL_BTN_B:
        pin = PIN_BTN_B;
        break;
    case HAL_BTN_C:
        pin = PIN_BTN_C;
        break;
    }

    if (pin != -1) {
        // Active LOW buttons (INPUT_PULLUP)
        // Pressed -> logic LOW.
        // Return true if pressed (LOW).
        return (digitalRead(pin) == LOW);
    }
    return false;
}

void hal_sound_play(hal_song_t song_id) {
    switch (song_id) {
    case HAL_SONG_START:
        buzzer_play_song(SONG_START);
        break;
    case HAL_SONG_FINISHED:
        buzzer_play_song(SONG_FINISHED);
        break;
    case HAL_SONG_ERROR:
        buzzer_play_song(SONG_ERROR);
        break;
    }
}

void hal_sensors_read(bool *drain_check, int *water_level_raw) {
    // In a real Arduino scenario, this would read pins.
    // For now, if we don't have physical sensors wired, we might return defaults
    // or rely on a global variable if we are mocking it on hardware too.
    // Assumption: Real hardware checks would go here.
    if (drain_check)
        *drain_check = false;
    if (water_level_raw)
        *water_level_raw = 0;
    // To support "Hardware Simulation" on the actual MCU, we'd need more logic.
    // But request implies simulation is on PC.
}

#else // LINUX / DUMMY

#include <stddef.h> // for NULL
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h> // for usleep

// Simulation State
static struct {
    bool drain_check;
    int water_level;
    bool buttons[3]; // A, B, C

    // Actuators
    bool act_motor_pwr;
    bool act_motor_ccw;
    bool act_inlet;
    bool act_drain;
    bool act_soap;
} sim_state = {0};

void hal_init(void) {
    // Dummy init
    // printf("[HAL] Init\n");
    memset(&sim_state, 0, sizeof(sim_state));
}

uint32_t hal_millis(void) {
    // Monotonic clock for linux
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

void hal_delay(uint32_t ms) { usleep(ms * 1000); }

void hal_actuator_write(hal_actuator_t act, bool active) {
    switch (act) {
    case HAL_ACT_MOTOR_POWER:
        sim_state.act_motor_pwr = active;
        break;
    case HAL_ACT_MOTOR_DIR:
        sim_state.act_motor_ccw = active;
        break;
    case HAL_ACT_INLET:
        sim_state.act_inlet = active;
        break;
    case HAL_ACT_DRAIN:
        sim_state.act_drain = active;
        break;
    case HAL_ACT_SOAP:
        sim_state.act_soap = active;
        break;
    }
}

bool hal_button_read(hal_button_t btn) {
    if (btn >= 0 && btn < 3) {
        return sim_state.buttons[btn];
    }
    return false;
}

void hal_sound_play(hal_song_t song_id) {
    // printf("[HAL] Play Song: %d\n", song_id);
    (void)song_id;
}

void hal_sensors_read(bool *drain_check, int *water_level_raw) {
    if (drain_check)
        *drain_check = sim_state.drain_check;
    if (water_level_raw)
        *water_level_raw = sim_state.water_level;
}

/* --- Simulation Hooks --- */
void hal_sim_set_sensors(bool drain_check, int water_level_raw) {
    sim_state.drain_check = drain_check;
    sim_state.water_level = water_level_raw;
}

void hal_sim_set_button(hal_button_t btn, bool pressed) {
    if (btn >= 0 && btn < 3) {
        sim_state.buttons[btn] = pressed;
    }
}

hal_sim_actuators_t hal_sim_get_actuators(void) {
    hal_sim_actuators_t acts;
    acts.motor_power = sim_state.act_motor_pwr;
    acts.motor_ccw = sim_state.act_motor_ccw;
    acts.inlet = sim_state.act_inlet;
    acts.drain = sim_state.act_drain;
    acts.soap = sim_state.act_soap;
    return acts;
}

#endif
