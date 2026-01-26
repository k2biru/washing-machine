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

/* Program Parameters */
static const struct {
    const char *name;
    uint16_t wash_min;
    uint16_t rinse_min;
    uint8_t rinse_count;
} programs[] = {{"Normal", 15, 15, 2}, {"Short", 10, 10, 2}, {"Express", 7, 7, 1}};
static const int num_programs = 3;

/* Water Level Parameters */
static const struct {
    const char *name;
    water_level_t level;
} levels[] = {{"Low", WATER_LOW}, {"Med", WATER_MED}, {"High", WATER_HIGH}};
static const int num_levels = 3;

/* Power Parameters */
static const struct {
    const char *name;
    uint16_t run_ms;
    uint16_t cycle_ms;
} powers[] = {{"Normal", 1600, 5000}, {"Strong", 4000, 5000}};
static const int num_powers = 2;

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

void app_init(App *app) {
    hal_init();
    app->ui_state = UI_STARTUP;
    app->menu_step = 0;
    app->sel_program = 0;
    app->sel_level = 0;
    app->sel_power = 0;
    app->last_tick_time = hal_millis();

    LOG_PRINTF("\n%s\n", "=== Washing Machine Menu ===");
    LOG_PRINTF("Program: %s (B: Next, A: OK)\n", programs[app->sel_program].name);
}

void app_loop(App *app) {
    uint32_t now = hal_millis();

    /* --- Input Handling --- */
    bool btnA = is_just_pressed(HAL_BTN_A);
    bool btnB = is_just_pressed(HAL_BTN_B);
    bool btnC = is_just_pressed(HAL_BTN_C);

    switch (app->ui_state) {
    case UI_STARTUP:
        if (btnB) {
            if (app->menu_step == 0)
                app->sel_program = (app->sel_program + 1) % num_programs;
            else if (app->menu_step == 1)
                app->sel_level = (app->sel_level + 1) % num_levels;
            else if (app->menu_step == 2)
                app->sel_power = (app->sel_power + 1) % num_powers;

            if (app->menu_step == 0)
                LOG_PRINTF("Program: %s\n", programs[app->sel_program].name);
            else if (app->menu_step == 1)
                LOG_PRINTF("Water Level: %s\n", levels[app->sel_level].name);
            else if (app->menu_step == 2)
                LOG_PRINTF("Power: %s\n", powers[app->sel_power].name);
        }
        if (btnA) {
            app->menu_step++;
            if (app->menu_step == 1) {
                LOG_PRINTF("Water Level: %s (B: Next, A: OK)\n", levels[app->sel_level].name);
            } else if (app->menu_step == 2) {
                LOG_PRINTF("Power: %s (B: Next, A: OK)\n", powers[app->sel_power].name);
            } else {
                /* All selections done, build program and start */
                wm_program_t prog = {
                    .wash_count = 1,
                    .rinse_count = programs[app->sel_program].rinse_count,
                    .spin_enable = true,
                    .soap_time_sec = 20, /* Default soap for wash */
                    .wash_agitate_time_sec = programs[app->sel_program].wash_min * 60,
                    .rinse_agitate_time_sec = programs[app->sel_program].rinse_min * 60,
                    .agitate_run_ms = powers[app->sel_power].run_ms,
                    .agitate_cycle_ms = powers[app->sel_power].cycle_ms,
                    .target_water_level = levels[app->sel_level].level,
                    .water_fill_timeout_sec = 600, /* 10 mins */
                    .drain_timeout_sec = 300,      /* 5 mins */
                    .ticks_per_second = 10         /* 100ms resolution */
                };

                wm_init(&app->ctrl, &app->sensors, &app->actuators, prog);
                wm_start(&app->ctrl);
                app->ui_state = UI_RUNNING;
                LOG_PRINTF("\nStarting cycle: %s, %s Level, %s Power...\n",
                           programs[app->sel_program].name, levels[app->sel_level].name,
                           powers[app->sel_power].name);
            }
        }
        break;

    case UI_RUNNING:
        if (btnA) {
            if (app->ctrl.state == WM_PAUSED) {
                wm_resume(&app->ctrl);
                LOG_PRINTF("\n%s\n", "Resumed.");
            } else {
                wm_pause(&app->ctrl);
                LOG_PRINTF("\n%s\n", "Paused.");
            }
        }
        if (btnC) {
            wm_pause(&app->ctrl);
            app->ui_state = UI_ABORT;
            LOG_PRINTF("\n%s\n", "Abort? (A: YES, C: NO/RESUME)");
        }

        /* Auto-transition to SLEEP if finished */
        if (app->ctrl.state == WM_COMPLETE || app->ctrl.state == WM_ERROR) {
            // Provide slow feedback or just wait a bit
            // Since loop runs fast, we use a simple counter or timer?
            // Original code used counter with 50ms delay
            // Let's rely on time to avoid frame-rate dependency
            static uint32_t hold_timer = 0;
            if (hold_timer == 0)
                hold_timer = now;

            if (now - hold_timer > 2000) { // 2 seconds hold
                hold_timer = 0;
                app->ui_state = UI_SLEEP;
                LOG_PRINTF("\n%s\n", "=== CYCLE ENDED ===");
                LOG_PRINTF("Press A to WAKE UP\n");
            }
        }
        break;

    case UI_ABORT:
        if (btnA) {
            wm_abort(&app->ctrl);
            app->ui_state = UI_RUNNING; // Let the state machine finish the drain
            LOG_PRINTF("\n%s\n", "Aborting... Draining Water...");
        }
        if (btnC) {
            wm_resume(&app->ctrl);
            app->ui_state = UI_RUNNING;
            LOG_PRINTF("\n%s\n", "Aborted cancelled. Resuming...");
        }
        break;

    case UI_SLEEP:
        if (btnA) {
            app->ui_state = UI_STARTUP;
            app->menu_step = 0;
            LOG_PRINTF("\n%s\n", "Waking up...");
            LOG_PRINTF("Program: %s (B: Next, A: OK)\n", programs[app->sel_program].name);
        }
        break;
    }

    /* --- Controller Simulation & Ticking --- */
    if (app->ui_state == UI_SLEEP || app->ui_state == UI_STARTUP) {
        // No logic tick, just return
        return;
    }

    /* Run controller at ticks_per_second */
    uint32_t tick_period_ms = 1000 / app->ctrl.program.ticks_per_second;
    if (now - app->last_tick_time >= tick_period_ms) {
        app->last_tick_time = now;

        /* READ PHYSICAL SENSORS (Abstracted by HAL) */
        /* Note: In Simulation, these values are injected by simulation.c via hal_sim_set_sensors */
        /* On real hardware, hal_sensors_read would read actual pins */
        int water_raw = 0;
        bool drain_check = false;
        hal_sensors_read(&drain_check, &water_raw);

        app->sensors.water_level = (water_level_t)water_raw;
        app->sensors.drain_check = drain_check;

        /* Tick Controller */
        wm_tick(&app->ctrl, &app->sensors, &app->actuators);
        wm_actuators(&app->actuators);

        if (app->ui_state == UI_RUNNING) {
            /* Display progress */
            uint16_t rem = wm_get_time_remaining_sec(&app->ctrl);
            LOG_PRINTF("Phase: %-5s | Status: %-10s | Time Rem: %02d:%02d | Level: %-6s | "
                       "Inlet:%d Soap:%d "
                       "Drain:%d Motor:%s\n",
                       app->ctrl.is_wash_phase ? "WASH" : "RINSE", wm_state_str(app->ctrl.state),
                       rem / 60, rem % 60, water_str(app->sensors.water_level),
                       app->actuators.inlet_valve, app->actuators.soap_pump,
                       app->actuators.drain_pump, motor_str(app->actuators.motor_dir));
        }
    }
}
