#include "app.h"
#include "../include/utils.h"
#include "../lib/buzzer/buzzer.h"
#include <Arduino.h>

/*
 * Hardware Abstraction Layer (HAL) for Washing Machine
 *
 * Pins are defined based on physical wiring to the relay module.
 * Relays are typically ACTIVE-LOW:
 * - digitalWrite(pin, LOW) -> RELAY ON (Actuator Active)
 * - digitalWrite(pin, HIGH) -> RELAY OFF (Actuator Inactive)
 */
const int motorPin = 12;         /* RELAY: Controls motor POWER (Enable) */
const int motorRotationPin = 10; /* RELAY: Controls direction (LOW=CW, HIGH=CCW) */
const int inletPin = 5;          /* RELAY: Controls water inlet valve */
const int drainPin = 6;          /* RELAY: Controls drain pump */
const int soapPin = 7;           /* RELAY: Controls detergent pump */
const int buzzerPin = 13;        /* BUZZER: Feedback sound output */

const int btnAPin = 2; /* BUTTON: Start/Pause/OK */
const int btnBPin = 3; /* BUTTON: Next */
const int btnCPin = 4; /* BUTTON: ESC */

enum ui_state_t { UI_STARTUP, UI_RUNNING, UI_ABORT, UI_SLEEP };

static wm_program_t presets[] = {
    /* Standard: 1 wash, 2 rinse, 3s agitate */
    {1, 2, true, 3, 20, 20, 3, 0, 30, 15, 2},
    /* Quick: 1 wash, 1 rinse, 4s agitate */
    {1, 1, true, 2, 10, 10, 4, 0, 20, 10, 2},
    /* Heavy: 2 wash, 2 rinse, 5s agitate */
    {2, 2, true, 5, 30, 30, 5, 0, 40, 20, 2}};
static const char *preset_names[] = {"Standard", "Quick", "Heavy Duty"};
static const int num_presets = 3;

/* Initialize all actuator pins as OUTPUT and set them to OFF (HIGH) */
int wm_actuators_init(void) {
    pinMode(motorPin, OUTPUT);
    pinMode(motorRotationPin, OUTPUT);
    pinMode(inletPin, OUTPUT);
    pinMode(drainPin, OUTPUT);
    pinMode(soapPin, OUTPUT);

    pinMode(btnAPin, INPUT_PULLUP);
    pinMode(btnBPin, INPUT_PULLUP);
    pinMode(btnCPin, INPUT_PULLUP);
    buzzer_init(buzzerPin);

    // Initial state: all OFF (Active-low relays assumed HIGH)
    digitalWrite(motorPin, HIGH);
    digitalWrite(motorRotationPin, HIGH);
    digitalWrite(inletPin, HIGH);
    digitalWrite(drainPin, HIGH);
    digitalWrite(soapPin, HIGH);
    return 0;
}

/* Bridges the logic state (struct) to the physical pins */
static int wm_actuators(wm_actuators_t *act) {
    bool motor_on = (act->motor_dir != MOTOR_STOP);

    /* motorPin is Enable (Active-low: LOW turns it ON) */
    digitalWrite(motorPin, !motor_on);

    /* Direction Control: CCW if pin is HIGH, CW if pin is LOW */
    digitalWrite(motorRotationPin, (act->motor_dir == MOTOR_CCW) ? HIGH : LOW);

    /* Actuators are active-low (logical TRUE -> pin LOW) */
    digitalWrite(inletPin, !act->inlet_valve);
    digitalWrite(drainPin, !act->drain_pump);
    digitalWrite(soapPin, !act->soap_pump);

    /* Buzzer Control: Play song on state change */
    static wm_buzzer_mode_t last_buzzer = BUZZER_OFF;
    if (act->buzzer != BUZZER_OFF && act->buzzer != last_buzzer) {
        switch (act->buzzer) {
        case BUZZER_START:
            buzzer_play_song(SONG_START);
            break;
        case BUZZER_FINISH:
            buzzer_play_song(SONG_FINISHED);
            break;
        case BUZZER_ERROR:
            buzzer_play_song(SONG_ERROR);
            break;
        default:
            break;
        }
    }
    last_buzzer = act->buzzer;

    return 0;
}

/* --- Logging Helpers --- */

static const char *water_str(water_level_t w) {
    switch (w) {
    case WATER_EMPTY:
        return "EMPTY";
    case WATER_LOW:
        return "LOW";
    case WATER_MED:
        return "MED";
    case WATER_HIGH:
        return "HIGH";
    default:
        return "?";
    }
}

static const char *motor_str(wm_motor_dir_t d) {
    switch (d) {
    case MOTOR_STOP:
        return "STOP";
    case MOTOR_CW:
        return "CW";
    case MOTOR_CCW:
        return "CCW";
    default:
        return "?";
    }
}

/* --- Main Washing Program --- */

/* Helper for non-blocking button edge detection */
static bool is_just_pressed(int pin) {
    static uint32_t last_time[15] = {0}; // simple debounce
    static bool last_state[15] = {true, true, true, true, true, true, true, true,
                                  true, true, true, true, true, true, true};

    bool state = digitalRead(pin);
    uint32_t now = millis();

    if (state != last_state[pin] && (now - last_time[pin] > 50)) {
        last_time[pin] = now;
        last_state[pin] = state;
        if (state == LOW)
            return true; // Pressed (pulled down)
    }
    return false;
}

int wm_program_ex(void) {
    ui_state_t ui_state = UI_STARTUP;
    int current_preset = 0;

    wm_controller_t ctrl;
    wm_sensors_t sensors;
    wm_actuators_t actuators;

    /* Initialize controller once (it will be re-init on start) */
    wm_init(&ctrl, &sensors, &actuators, presets[current_preset]);

    uint32_t last_tick_time = 0;

    LOG_PRINTF("\n=== Washing Machine Menu ===\n");
    LOG_PRINTF("Preset: %s (Press B for Next, A to START)\n", preset_names[current_preset]);

    while (1) {
        uint32_t now = millis();

        /* --- Input Handling --- */
        bool btnA = is_just_pressed(btnAPin);
        bool btnB = is_just_pressed(btnBPin);
        bool btnC = is_just_pressed(btnCPin);

        switch (ui_state) {
        case UI_STARTUP:
            if (btnB) {
                current_preset = (current_preset + 1) % num_presets;
                LOG_PRINTF("Preset: %s (Press B for Next, A to START)\n",
                           preset_names[current_preset]);
            }
            if (btnA) {
                wm_init(&ctrl, &sensors, &actuators, presets[current_preset]);
                wm_start(&ctrl);
                ui_state = UI_RUNNING;
                LOG_PRINTF("\nStarting %s...\n", preset_names[current_preset]);
            }
            break;

        case UI_RUNNING:
            if (btnA) {
                if (ctrl.state == WM_PAUSED) {
                    wm_resume(&ctrl);
                    LOG_PRINTF("\nResumed.\n");
                } else {
                    wm_pause(&ctrl);
                    LOG_PRINTF("\nPaused.\n");
                }
            }
            if (btnC) {
                wm_pause(&ctrl);
                ui_state = UI_ABORT;
                LOG_PRINTF("\nAbort? (A: YES, C: NO/RESUME)\n");
            }

            /* Auto-transition to SLEEP if finished */
            if (ctrl.state == WM_COMPLETE || ctrl.state == WM_ERROR) {
                static int hold = 0;
                if (++hold > 4) {
                    hold = 0;
                    ui_state = UI_SLEEP;
                    LOG_PRINTF("\n=== CYCLE ENDED ===\n");
                    LOG_PRINTF("Press A to WAKE UP\n");
                }
            }
            break;

        case UI_ABORT:
            if (btnA) {
                wm_abort(&ctrl);
                ui_state = UI_RUNNING; // Let the state machine finish the drain
                LOG_PRINTF("\nAborting... Draining Water...\n");
            }
            if (btnC) {
                wm_resume(&ctrl);
                ui_state = UI_RUNNING;
                LOG_PRINTF("\nAborted cancelled. Resuming...\n");
            }
            break;

        case UI_SLEEP:
            if (btnA) {
                ui_state = UI_STARTUP;
                LOG_PRINTF("\nWaking up...\n");
                LOG_PRINTF("Preset: %s (Press B for Next, A to START)\n",
                           preset_names[current_preset]);
            }
            break;
        }

        /* --- Controller Simulation & Ticking --- */
        if (ui_state == UI_SLEEP) {
            MS_DELAY(100);
            continue;
        }

        /* Run controller at presets[current_preset].ticks_per_second */
        uint32_t tick_period_ms = 1000 / presets[current_preset].ticks_per_second;
        if (now - last_tick_time >= tick_period_ms) {
            last_tick_time = now;

            /* Simulated Sensor Behavior */
            if (actuators.inlet_valve) {
                if (sensors.water_level < WATER_HIGH)
                    sensors.water_level = (water_level_t)((int)sensors.water_level + 1);
            }
            if (actuators.drain_pump) {
                if (sensors.water_level > WATER_EMPTY)
                    sensors.water_level = (water_level_t)((int)sensors.water_level - 1);
            }
            sensors.drain_check = (sensors.water_level != WATER_EMPTY);

            /* Tick Controller */
            wm_tick(&ctrl, &sensors, &actuators);
            wm_actuators(&actuators);

            if (ui_state == UI_RUNNING) {
                /* Display progress */
                uint16_t rem = wm_get_time_remaining_sec(&ctrl);
                LOG_PRINTF("Phase: %-5s | Status: %-10s | Time Rem: %02d:%02d | Level: %-6s | "
                           "Inlet:%d Soap:%d "
                           "Drain:%d Motor:%s\n",
                           ctrl.is_wash_phase ? "WASH" : "RINSE", wm_state_str(ctrl.state),
                           rem / 60, rem % 60, water_str(sensors.water_level),
                           actuators.inlet_valve, actuators.soap_pump, actuators.drain_pump,
                           motor_str(actuators.motor_dir));
            }
        }

        MS_DELAY(50); // Fast enough for button response
    }

    return 0;
}
