#include "wm_control.h"

void wm_init(wm_controller_t *c, wm_sensors_t *s, wm_actuators_t *a, wm_program_t program) {
    *c = (wm_controller_t){0};
    *a = (wm_actuators_t){0};

    c->state = WM_IDLE;
    c->program = program;
    c->error_code = WM_ERR_NONE;

    /* Validation */
    if (program.water_fill_timeout_sec == 0 || program.drain_timeout_sec == 0 ||
        program.ticks_per_second == 0) {
        c->state = WM_ERROR;
        c->error_code = WM_ERR_INVALID_PROGRAM;
    }

    s->water_level = WATER_EMPTY;
    s->drain_check = false;
}

void wm_start(wm_controller_t *c) {
    if (c->state == WM_IDLE) {
        c->is_wash_phase = true;
        c->state = WM_START;
        c->state_time = 0;
    }
}

void wm_pause(wm_controller_t *c) {
    if (c->state != WM_PAUSED && c->state != WM_COMPLETE) {
        c->prev_state = c->state;
        c->state = WM_PAUSED;
    }
}

void wm_resume(wm_controller_t *c) {
    if (c->state == WM_PAUSED) {
        c->state = c->prev_state;
    }
}

void wm_abort(wm_controller_t *c) {
    /*
     * Abort Logic:
     * If there is water (or we are in any active state), force a transition to DRAIN.
     * Configure the controller so that after DRAIN it goes to COMPLETE/IDLE.
     */
    if (c->state == WM_IDLE || c->state == WM_COMPLETE || c->state == WM_ERROR) {
        return;
    }

    c->is_wash_phase = false;
    c->rinse_done = c->program.rinse_count;
    c->wash_done = c->program.wash_count;
    c->program.spin_enable = false; /* Don't spin after aborting */

    c->state = WM_DRAIN;
    c->state_time = 0;
}

uint16_t wm_get_time_remaining_sec(wm_controller_t *c) {
    if (c->state == WM_IDLE || c->state == WM_COMPLETE || c->state == WM_ERROR) {
        return 0;
    }

    uint32_t total_sec = 0;
    uint32_t current_state_target = 0;

    /* 1. Remaining time in CURRENT state */
    switch (c->state) {
    case WM_FILL:
        current_state_target = c->program.water_fill_timeout_sec;
        break;
    case WM_SOAP:
        current_state_target = c->program.soap_time_sec;
        break;
    case WM_AGITATE:
        current_state_target =
            c->is_wash_phase ? c->program.wash_agitate_time_sec : c->program.rinse_agitate_time_sec;
        break;
    case WM_DRAIN:
        current_state_target = c->program.drain_timeout_sec;
        break;
    case WM_SPIN:
        current_state_target = 7; // Fixed spin time
        break;
    default:
        current_state_target = 0;
        break;
    }

    uint32_t elapsed_sec = c->state_time / c->program.ticks_per_second;
    if (elapsed_sec < current_state_target) {
        total_sec += (current_state_target - elapsed_sec);
    }

    /* 2. Future states in CURRENT cycle (WASH or RINSE) */
    /* Note: This is an estimate as fill/drain times vary. We use timeouts as rough estimates. */

    // If in FILL, add following steps in current cycle
    if (c->state == WM_FILL) {
        if (c->is_wash_phase) {
            total_sec += c->program.soap_time_sec + c->program.wash_agitate_time_sec +
                         c->program.drain_timeout_sec;
        } else {
            total_sec += c->program.rinse_agitate_time_sec + c->program.drain_timeout_sec;
        }
    } else if (c->state == WM_SOAP) {
        total_sec += c->program.wash_agitate_time_sec + c->program.drain_timeout_sec;
    } else if (c->state == WM_AGITATE) {
        total_sec += c->program.drain_timeout_sec;
    }

    /* 3. Future cycles */
    uint32_t wash_remaining = 0;
    if (c->is_wash_phase && c->wash_done < c->program.wash_count) {
        wash_remaining = c->program.wash_count - c->wash_done - 1;
    }

    uint32_t rinse_remaining = 0;
    if (c->is_wash_phase) {
        rinse_remaining = c->program.rinse_count;
    } else if (c->rinse_done < c->program.rinse_count) {
        rinse_remaining = c->program.rinse_count - c->rinse_done - 1;
    }

    /* Standard cycle: Fill -> (Soap) -> Agitate -> Drain */
    uint32_t standard_wash_sec = c->program.water_fill_timeout_sec + c->program.soap_time_sec +
                                 c->program.wash_agitate_time_sec + c->program.drain_timeout_sec;
    uint32_t standard_rinse_sec = c->program.water_fill_timeout_sec +
                                  c->program.rinse_agitate_time_sec + c->program.drain_timeout_sec;

    total_sec += wash_remaining * standard_wash_sec;
    total_sec += rinse_remaining * standard_rinse_sec;

    /* 4. Final Spin */
    if (c->program.spin_enable && (c->state != WM_SPIN)) {
        total_sec += 7;
    }

    return (uint16_t)total_sec;
}

void wm_tick(wm_controller_t *c, wm_sensors_t *s, wm_actuators_t *a) {
    /* Reset all outputs every tick */
    *a = (wm_actuators_t){0};

    if (c->state == WM_PAUSED)
        return;

    c->state_time++;

    /*
     * Main State Machine Logic
     * Runs once every tick (defined by the frequency of the caller).
     */
    switch (c->state) {

    case WM_IDLE:
        a->motor_dir = MOTOR_STOP;
        a->inlet_valve = false;
        a->drain_pump = false;
        break;

    case WM_START:
        a->buzzer = BUZZER_START;
        c->state = WM_FILL;
        c->state_time = 0;
        break;

    case WM_FILL:
        a->inlet_valve = true;

        /* Exit FILL state once target water level is achieved */
        if (s->water_level >= c->program.target_water_level) {
            c->state = c->is_wash_phase ? WM_SOAP : WM_AGITATE;
            c->state_time = 0;
        } else if (c->state_time >=
                   (uint32_t)c->program.water_fill_timeout_sec * c->program.ticks_per_second) {
            /* Error if filling takes too long */
            c->state = WM_ERROR;
            c->error_code = WM_ERR_TIMEOUT_FILL;
        }
        break;

    case WM_SOAP:
        a->soap_pump = true;
        /* Only inject soap during the soap phase of the wash */
        if (c->state_time >= (uint32_t)c->program.soap_time_sec * c->program.ticks_per_second) {
            c->state = WM_AGITATE;
            c->state_time = 0;
        }
        break;

    case WM_AGITATE: {
        /*
         * Agitation Logic (Configurable Window):
         * Motor turns ON at the start of every half-cycle (agitate_cycle_ms).
         * The motor stays ON for 'run_ticks' and then STOPS.
         * The full cycle is 2 * agitate_cycle_ms: First half (CW), Second half (CCW).
         */
        uint32_t run_ticks =
            (uint32_t)c->program.agitate_run_ms * c->program.ticks_per_second / 1000;
        uint32_t half_cycle_ticks =
            (uint32_t)c->program.agitate_cycle_ms * c->program.ticks_per_second / 1000;
        uint32_t full_cycle_ticks = 2 * half_cycle_ticks;

        uint32_t cycle_time = c->state_time % full_cycle_ticks;

        if (cycle_time < half_cycle_ticks) {
            /* First half cycle: Clockwise rotation */
            a->motor_dir = (cycle_time < run_ticks) ? MOTOR_CW : MOTOR_STOP;
        } else {
            /* Second half cycle: Counter-clockwise rotation */
            a->motor_dir = (cycle_time - half_cycle_ticks < run_ticks) ? MOTOR_CCW : MOTOR_STOP;
        }

        /* Determine total agitation time based on whether we are Washing or Rinsing */
        uint32_t target_ticks = (uint32_t)(c->is_wash_phase ? c->program.wash_agitate_time_sec
                                                            : c->program.rinse_agitate_time_sec) *
                                c->program.ticks_per_second;

        if (c->state_time >= target_ticks) {
            c->state = WM_DRAIN;
            c->state_time = 0;
        }
        break;
    }

    case WM_DRAIN:
        a->drain_pump = true;

        if (s->drain_check == false) {

            if (c->is_wash_phase && ++c->wash_done < c->program.wash_count) {
                c->state = WM_FILL;
            } else if (!c->is_wash_phase && ++c->rinse_done < c->program.rinse_count) {
                c->state = WM_FILL;
            } else if (c->is_wash_phase) {
                c->is_wash_phase = false;
                c->state = WM_FILL;
            } else if (c->program.spin_enable) {
                c->state = WM_SPIN;
            } else {
                c->state = WM_COMPLETE;
            }

            c->state_time = 0;
        } else if (c->state_time >=
                   (uint32_t)c->program.drain_timeout_sec * c->program.ticks_per_second) {
            c->state = WM_ERROR;
            c->error_code = WM_ERR_TIMEOUT_DRAIN;
        }
        break;

    case WM_SPIN:
        a->motor_dir = MOTOR_CW;
        /* Spin fixed duration: 7 seconds */
        if (c->state_time >= (uint32_t)7 * c->program.ticks_per_second) {
            c->state = WM_COMPLETE;
        }
        break;

    case WM_COMPLETE:
        a->buzzer = BUZZER_FINISH;
        break;

    case WM_ERROR:
        a->buzzer = BUZZER_ERROR;
        break;

    default:
        break;
    }

    /* Safety Interlocks (Robust Output Enforcement) */

    /* 1. Inlet Valve: Only allowed in FILL state */
    if (c->state != WM_FILL && a->inlet_valve) {
        a->inlet_valve = false;
    }

    /* 2. Soap Pump: Only allowed in SOAP state */
    if (c->state != WM_SOAP && a->soap_pump) {
        a->soap_pump = false;
    }

    /* 3. Drain Pump: Allowed in DRAIN and SPIN states */
    if (c->state != WM_DRAIN && c->state != WM_SPIN && a->drain_pump) {
        a->drain_pump = false;
    }

    /* 4. Motor: Only allowed in AGITATE and SPIN states */
    if (c->state != WM_AGITATE && c->state != WM_SPIN && a->motor_dir != MOTOR_STOP) {
        a->motor_dir = MOTOR_STOP;
    }

    /* 5. Mutual Exclusion: Inlet and Drain cannot be on together */
    if (a->inlet_valve && a->drain_pump) {
        a->inlet_valve = false;
    }
}

const char *wm_state_str(wm_state_t s) {
    switch (s) {
    case WM_IDLE:
        return "IDLE";
    case WM_START:
        return "START";
    case WM_FILL:
        return "FILL";
    case WM_SOAP:
        return "SOAP";
    case WM_AGITATE:
        return "AGITATE";
    case WM_DRAIN:
        return "DRAIN";
    case WM_SPIN:
        return "SPIN";
    case WM_PAUSED:
        return "PAUSED";
    case WM_COMPLETE:
        return "COMPLETE";
    case WM_ERROR:
        return "ERROR";
    default:
        return "?";
    }
}

const char *wm_error_str(wm_error_t err) {
    switch (err) {
    case WM_ERR_NONE:
        return "NONE";
    case WM_ERR_TIMEOUT_FILL:
        return "TIMEOUT_FILL";
    case WM_ERR_TIMEOUT_DRAIN:
        return "TIMEOUT_DRAIN";
    case WM_ERR_INVALID_PROGRAM:
        return "INVALID_PROGRAM";
    default:
        return "UNKNOWN_ERROR";
    }
}
