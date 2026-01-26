#include "app.h"
#include "../include/utils.h"
#include "hal.h"

/*
 * Hardware Abstraction Layer (HAL) for Washing Machine
 *
 * NOTE: The low-level pin definitions and Arduino dependencies
 * have been moved to hal.c / hal.h.
 */

typedef enum { UI_STARTUP, UI_RUNNING, UI_ABORT, UI_SLEEP } ui_state_t;

static wm_program_t presets[] = {
    /* Standard: 1 wash, 2 rinse, 3s agitate */
    {1, 2, true, 3, 20, 20, 3, 0, 30, 15, 2},
    /* Quick: 1 wash, 1 rinse, 4s agitate */
    {1, 1, true, 2, 10, 10, 4, 0, 20, 10, 2},
    /* Heavy: 2 wash, 2 rinse, 5s agitate */
    {2, 2, true, 5, 30, 30, 5, 0, 40, 20, 2}};
static const char *preset_names[] = {"Standard", "Quick", "Heavy Duty"};
static const int num_presets = 3;

/* Initialize all actuator pins via HAL */
int wm_actuators_init(void) {
    hal_init();
    return 0;
}

/* Bridges the logic state (struct) to the physical pins via HAL */
static int wm_actuators(wm_actuators_t *act) {
    bool motor_on = (act->motor_dir != MOTOR_STOP);

    /* motorPin is Enable */
    hal_actuator_write(HAL_ACT_MOTOR_POWER, motor_on);

    /* Direction Control: CCW if pin is HIGH, CW if pin is LOW (handled by HAL logic primarily,
       but here we pass boolean.
       We need to be consistent with HAL implementation:
       HAL: true -> CCW (HIGH), false -> CW (LOW)
    */
    bool is_ccw = (act->motor_dir == MOTOR_CCW);
    hal_actuator_write(HAL_ACT_MOTOR_DIR, is_ccw);

    /* Actuators */
    hal_actuator_write(HAL_ACT_INLET, act->inlet_valve);
    hal_actuator_write(HAL_ACT_DRAIN, act->drain_pump);
    hal_actuator_write(HAL_ACT_SOAP, act->soap_pump);

    /* Buzzer Control via HAL */
    static wm_buzzer_mode_t last_buzzer = BUZZER_OFF;
    if (act->buzzer != BUZZER_OFF && act->buzzer != last_buzzer) {
        switch (act->buzzer) {
        case BUZZER_START:
            hal_sound_play(HAL_SONG_START);
            break;
        case BUZZER_FINISH:
            hal_sound_play(HAL_SONG_FINISHED);
            break;
        case BUZZER_ERROR:
            hal_sound_play(HAL_SONG_ERROR);
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

/* --- Main Washing Program --- */

/* Helper for non-blocking button edge detection using HAL */
static bool is_just_pressed(hal_button_t btn) {
    // Map HAL button enum to a simple index for debounce array (0-2)
    int idx = (int)btn;
    if (idx < 0 || idx >= 3)
        return false;

    static uint32_t last_time[3] = {0};
    static bool last_state[3] = {false, false, false};

    bool state = hal_button_read(btn);
    uint32_t now = hal_millis();

    if (state != last_state[idx] && (now - last_time[idx] > 50)) {
        last_time[idx] = now;
        last_state[idx] = state;
        if (state == true) // Pressed
            return true;
    }
    return false;
}

// Global App State
static ui_state_t g_ui_state = UI_STARTUP;
static int g_current_preset = 0;
static wm_controller_t g_ctrl;
static wm_sensors_t g_sensors;
static wm_actuators_t g_actuators;
static uint32_t g_last_tick_time = 0;

void app_init(void) {
    hal_init();
    wm_init(&g_ctrl, &g_sensors, &g_actuators, presets[g_current_preset]);
    g_ui_state = UI_STARTUP;
    g_last_tick_time = hal_millis();

    LOG_PRINTF("\n%s\n", "=== Washing Machine Menu ===");
    LOG_PRINTF("Preset: %s (Press B for Next, A to START)\n", preset_names[g_current_preset]);
}

void app_loop(void) {
    uint32_t now = hal_millis();

    /* --- Input Handling --- */
    bool btnA = is_just_pressed(HAL_BTN_A);
    bool btnB = is_just_pressed(HAL_BTN_B);
    bool btnC = is_just_pressed(HAL_BTN_C);

    switch (g_ui_state) {
    case UI_STARTUP:
        if (btnB) {
            g_current_preset = (g_current_preset + 1) % num_presets;
            LOG_PRINTF("Preset: %s (Press B for Next, A to START)\n",
                       preset_names[g_current_preset]);
        }
        if (btnA) {
            wm_init(&g_ctrl, &g_sensors, &g_actuators, presets[g_current_preset]);
            wm_start(&g_ctrl);
            g_ui_state = UI_RUNNING;
            LOG_PRINTF("\nStarting %s...\n", preset_names[g_current_preset]);
        }
        break;

    case UI_RUNNING:
        if (btnA) {
            if (g_ctrl.state == WM_PAUSED) {
                wm_resume(&g_ctrl);
                LOG_PRINTF("\n%s\n", "Resumed.");
            } else {
                wm_pause(&g_ctrl);
                LOG_PRINTF("\n%s\n", "Paused.");
            }
        }
        if (btnC) {
            wm_pause(&g_ctrl);
            g_ui_state = UI_ABORT;
            LOG_PRINTF("\n%s\n", "Abort? (A: YES, C: NO/RESUME)");
        }

        /* Auto-transition to SLEEP if finished */
        if (g_ctrl.state == WM_COMPLETE || g_ctrl.state == WM_ERROR) {
            // Provide slow feedback or just wait a bit
            // Since loop runs fast, we use a simple counter or timer?
            // Original code used counter with 50ms delay
            // Let's rely on time to avoid frame-rate dependency
            static uint32_t hold_timer = 0;
            if (hold_timer == 0)
                hold_timer = now;

            if (now - hold_timer > 2000) { // 2 seconds hold
                hold_timer = 0;
                g_ui_state = UI_SLEEP;
                LOG_PRINTF("\n%s\n", "=== CYCLE ENDED ===");
                LOG_PRINTF("Press A to WAKE UP\n");
            }
        }
        break;

    case UI_ABORT:
        if (btnA) {
            wm_abort(&g_ctrl);
            g_ui_state = UI_RUNNING; // Let the state machine finish the drain
            LOG_PRINTF("\n%s\n", "Aborting... Draining Water...");
        }
        if (btnC) {
            wm_resume(&g_ctrl);
            g_ui_state = UI_RUNNING;
            LOG_PRINTF("\n%s\n", "Aborted cancelled. Resuming...");
        }
        break;

    case UI_SLEEP:
        if (btnA) {
            g_ui_state = UI_STARTUP;
            LOG_PRINTF("\n%s\n", "Waking up...");
            LOG_PRINTF("Preset: %s (Press B for Next, A to START)\n",
                       preset_names[g_current_preset]);
        }
        break;
    }

    /* --- Controller Simulation & Ticking --- */
    if (g_ui_state == UI_SLEEP) {
        // No logic tick, just return
        return;
    }

    /* Run controller at presets[current_preset].ticks_per_second */
    uint32_t tick_period_ms = 1000 / presets[g_current_preset].ticks_per_second;
    if (now - g_last_tick_time >= tick_period_ms) {
        g_last_tick_time = now;

        /* READ PHYSICAL SENSORS (Abstracted by HAL) */
        /* Note: In Simulation, these values are injected by simulation.c via hal_sim_set_sensors */
        /* On real hardware, hal_sensors_read would read actual pins */
        int water_raw = 0;
        bool drain_check = false;
        hal_sensors_read(&drain_check, &water_raw);

        g_sensors.water_level = (water_level_t)water_raw;
        g_sensors.drain_check = drain_check;

        /* Tick Controller */
        wm_tick(&g_ctrl, &g_sensors, &g_actuators);
        wm_actuators(&g_actuators);

        if (g_ui_state == UI_RUNNING) {
            /* Display progress */
            uint16_t rem = wm_get_time_remaining_sec(&g_ctrl);
            LOG_PRINTF("Phase: %-5s | Status: %-10s | Time Rem: %02d:%02d | Level: %-6s | "
                       "Inlet:%d Soap:%d "
                       "Drain:%d Motor:%s\n",
                       g_ctrl.is_wash_phase ? "WASH" : "RINSE", wm_state_str(g_ctrl.state),
                       rem / 60, rem % 60, water_str(g_sensors.water_level),
                       g_actuators.inlet_valve, g_actuators.soap_pump, g_actuators.drain_pump,
                       motor_str(g_actuators.motor_dir));
        }
    }
}
