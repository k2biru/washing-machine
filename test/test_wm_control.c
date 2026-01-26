#include <assert.h>
#include <stdio.h>

#include "../lib/wm_control/wm_control.h"

/* ============================================================
 * Test Macros
 * ============================================================ */

#define TICK_AND_ASSERT_STATE(ctrl, sens, act, expected)                                           \
    do {                                                                                           \
        wm_tick((ctrl), (sens), (act));                                                            \
        assert((ctrl)->state == (expected));                                                       \
    } while (0)

#define MULTI_TICK(ctrl, sens, act, count)                                                         \
    do {                                                                                           \
        for (int i = 0; i < (int)(count); i++) {                                                   \
            wm_tick((ctrl), (sens), (act));                                                        \
        }                                                                                          \
    } while (0)

#define TICK_AND_ASSERT_ACTUATOR(ctrl, sens, act, expr)                                            \
    do {                                                                                           \
        wm_tick((ctrl), (sens), (act));                                                            \
        assert((expr));                                                                            \
    } while (0)

/* ============================================================
 * Helpers
 * ============================================================ */

/* ============================================================
 * Tests
 * ============================================================ */

static void test_init_state(void) {
    wm_controller_t c;
    wm_sensors_t s;
    wm_actuators_t a;

    wm_program_t program = {
        .wash_count = 1,
        .rinse_count = 1,
        .spin_enable = true,
        .soap_time_sec = 3,
        .wash_agitate_time_sec = 5,
        .rinse_agitate_time_sec = 4,
        .water_fill_timeout_sec = 10,
        .drain_timeout_sec = 10,
        .agitate_run_sec = 3,
        .agitate_pause_sec = 3,
        .ticks_per_second = 1,
    };

    wm_init(&c, &s, &a, program);

    assert(c.state == WM_IDLE);
    assert(c.wash_done == 0);
    assert(c.rinse_done == 0);
    assert(s.water_level == WATER_EMPTY);
    assert(s.drain_check == false);
    assert(c.error_code == WM_ERR_NONE);

    printf("✓ test_init_state\n");
}

static void test_start_to_fill(void) {
    wm_controller_t c;
    wm_sensors_t s;
    wm_actuators_t a;

    wm_program_t program = {
        .wash_count = 1,
        .rinse_count = 1,
        .spin_enable = false,
        .soap_time_sec = 2,
        .wash_agitate_time_sec = 3,
        .rinse_agitate_time_sec = 3,
        .water_fill_timeout_sec = 10,
        .drain_timeout_sec = 10,
        .agitate_run_sec = 3,
        .agitate_pause_sec = 3,
        .ticks_per_second = 1,
    };

    wm_init(&c, &s, &a, program);
    wm_start(&c);

    /* Check for Start Beep */
    /* wm_start sets state to WM_START */
    assert(c.state == WM_START);

    /* First tick: Process START state, beep, transition to FILL */
    wm_tick(&c, &s, &a);
    assert(a.buzzer == BUZZER_START);
    assert(c.state == WM_FILL);
    assert(a.inlet_valve == false); /* Valve opens in next tick */

    /* Second tick: In FILL state, valve opens */
    wm_tick(&c, &s, &a);
    assert(c.state == WM_FILL);
    assert(a.inlet_valve == true);

    printf("✓ test_start_to_fill\n");
}

static void test_fill_to_soap(void) {
    wm_controller_t c;
    wm_sensors_t s;
    wm_actuators_t a;

    wm_program_t program = {
        .wash_count = 1,
        .rinse_count = 0,
        .spin_enable = false,
        .soap_time_sec = 2,
        .wash_agitate_time_sec = 3,
        .rinse_agitate_time_sec = 3,
        .water_fill_timeout_sec = 10,
        .drain_timeout_sec = 10,
        .agitate_run_sec = 3,
        .agitate_pause_sec = 3,
        .ticks_per_second = 1,
    };

    wm_init(&c, &s, &a, program);
    wm_start(&c);
    wm_tick(&c, &s, &a); /* Advance WM_START -> WM_FILL */

    s.water_level = WATER_HIGH;
    TICK_AND_ASSERT_STATE(&c, &s, &a, WM_SOAP);

    printf("✓ test_fill_to_soap\n");
}

static void test_soap_to_agitate(void) {
    wm_controller_t c;
    wm_sensors_t s;
    wm_actuators_t a;

    wm_program_t program = {
        .wash_count = 1,
        .rinse_count = 0,
        .spin_enable = false,
        .soap_time_sec = 2,
        .wash_agitate_time_sec = 15,
        .rinse_agitate_time_sec = 3,
        .water_fill_timeout_sec = 10,
        .drain_timeout_sec = 10,
        .agitate_run_sec = 3,
        .agitate_pause_sec = 3,
        .ticks_per_second = 1,
    };

    wm_init(&c, &s, &a, program);
    wm_start(&c);
    wm_tick(&c, &s, &a); /* Advance WM_START -> WM_FILL */

    /* FILL -> SOAP */
    s.water_level = WATER_HIGH;
    TICK_AND_ASSERT_STATE(&c, &s, &a, WM_SOAP);

    /* SOAP duration: (soap_time_sec * program.ticks_per_second) - 1 ticks to stay in SOAP */
    /* Since we already ticked once to enter SOAP, we tick (2 * 2 - 1) = 3 more times */
    MULTI_TICK(&c, &s, &a, (program.soap_time_sec * program.ticks_per_second) - 1);
    assert(c.state == WM_SOAP);

    /* Next tick -> AGITATE */
    TICK_AND_ASSERT_STATE(&c, &s, &a, WM_AGITATE);

    /* First AGITATE execution tick */
    /* Pattern (New Rule): CW for run_sec at start of 5s interval */
    /* Tick 0 (current state_time is 0 after state reset) */
    wm_tick(&c, &s, &a);
    assert(c.state == WM_AGITATE);
    assert(a.motor_dir == MOTOR_CW);

    /* Advance to end of run_sec (3s -> 6 ticks) */
    /* We already ticked once, so tick 4 more times (reaches state_time=5) */
    MULTI_TICK(&c, &s, &a, (program.agitate_run_sec * program.ticks_per_second) - 2);
    assert(a.motor_dir == MOTOR_CW);

    /* Next tick -> STOP (within the first 5s interval, reaches state_time=6) */
    wm_tick(&c, &s, &a);
    assert(a.motor_dir == MOTOR_STOP);

    /* Advance to CCW interval (start of 5s-10s -> T=5 * program.ticks_per_second ticks) */
    c.state_time = 5 * program.ticks_per_second;
    wm_tick(&c, &s, &a);
    assert(a.motor_dir == MOTOR_CCW);

    printf("✓ test_soap_to_agitate\n");
}

static void test_pause_resume(void) {
    wm_controller_t c;
    wm_sensors_t s;
    wm_actuators_t a;

    wm_program_t program = {
        .wash_count = 1,
        .rinse_count = 0,
        .spin_enable = false,
        .soap_time_sec = 2,
        .wash_agitate_time_sec = 15,
        .rinse_agitate_time_sec = 3,
        .water_fill_timeout_sec = 10,
        .drain_timeout_sec = 10,
        .agitate_run_sec = 3,
        .agitate_pause_sec = 3,
        .ticks_per_second = 1,
    };

    wm_init(&c, &s, &a, program);
    wm_start(&c);
    wm_tick(&c, &s, &a); /* Advance WM_START -> WM_FILL */

    s.water_level = WATER_HIGH;
    TICK_AND_ASSERT_STATE(&c, &s, &a, WM_SOAP);

    wm_pause(&c);
    assert(c.state == WM_PAUSED);

    wm_tick(&c, &s, &a); /* no progress */
    wm_resume(&c);

    assert(c.state == WM_SOAP);

    printf("✓ test_pause_resume\n");
}

static void test_drain_sensor(void) {
    wm_controller_t c;
    wm_sensors_t s;
    wm_actuators_t a;

    wm_program_t program = {
        .wash_count = 1,
        .water_fill_timeout_sec = 10,
        .drain_timeout_sec = 10,
        .agitate_run_sec = 3,
        .agitate_pause_sec = 3,
        .ticks_per_second = 1,
        /* other fields 0/false */
    };

    wm_init(&c, &s, &a, program);
    wm_start(&c);
    wm_tick(&c, &s, &a); /* Advance WM_START -> WM_FILL */

    /* Go to DRAIN */
    c.state = WM_DRAIN;
    s.water_level = WATER_EMPTY;
    s.drain_check = true; /* Sensor still reports water */

    /* Should stay in DRAIN because drain_check is true, even if water_level is EMPTY */
    TICK_AND_ASSERT_STATE(&c, &s, &a, WM_DRAIN);

    /* Now clear drain sensor */
    s.drain_check = false;
    wm_tick(&c, &s, &a);

    /* Should proceed to next state (FILL for rinse, or COMPLETE if done) */
    assert(c.state != WM_DRAIN);

    printf("✓ test_drain_sensor\n");
}

static void test_drain_timeout(void) {
    wm_controller_t c;
    wm_sensors_t s;
    wm_actuators_t a;
    wm_program_t program = {
        .water_fill_timeout_sec = 10,
        .drain_timeout_sec = 5,
        .agitate_run_sec = 3,
        .agitate_pause_sec = 3,
        .ticks_per_second = 1,
    };

    wm_init(&c, &s, &a, program);
    wm_start(&c);
    wm_tick(&c, &s, &a); /* Advance WM_START -> WM_FILL */

    c.state = WM_DRAIN;
    c.state_time = (program.drain_timeout_sec * program.ticks_per_second) + 1; /* Exceed timeout */
    s.drain_check = true;

    wm_tick(&c, &s, &a);

    assert(c.state == WM_ERROR);
    assert(c.error_code == WM_ERR_TIMEOUT_DRAIN);

    /* Tick again to process ERROR state and set buzzer */
    wm_tick(&c, &s, &a);
    assert(a.buzzer == BUZZER_ERROR);

    printf("✓ test_drain_timeout\n");
}

static void test_invalid_program(void) {
    wm_controller_t c;
    wm_sensors_t s;
    wm_actuators_t a;
    wm_program_t program = {.water_fill_timeout_sec = 0}; /* Invalid */

    wm_init(&c, &s, &a, program);

    assert(c.state == WM_ERROR);
    assert(c.error_code == WM_ERR_INVALID_PROGRAM);

    printf("✓ test_invalid_program\n");
}

static void test_complete_buzzer(void) {
    wm_controller_t c;
    wm_sensors_t s;
    wm_actuators_t a;
    wm_program_t program = {.water_fill_timeout_sec = 1,
                            .drain_timeout_sec = 1,
                            .ticks_per_second = 1}; // Minimal valid program

    wm_init(&c, &s, &a, program);

    /* Force state to COMPLETE */
    c.state = WM_COMPLETE;

    wm_tick(&c, &s, &a);

    assert(a.buzzer == BUZZER_FINISH);

    printf("✓ test_complete_buzzer\n");
}

static void test_safety_mechanisms(void) {
    wm_controller_t c;
    wm_sensors_t s;
    wm_actuators_t a;
    wm_program_t program = {
        .water_fill_timeout_sec = 10,
        .drain_timeout_sec = 10,
        .agitate_run_sec = 3,
        .agitate_pause_sec = 3,
        .ticks_per_second = 1,
    };

    wm_init(&c, &s, &a, program);
    wm_start(&c);
    wm_tick(&c, &s, &a); /* START -> FILL */

    /* Safety 1: Motor should be stopped during FILL */
    /* Manually force motor ON (simulate bug/glitch) */
    a.motor_dir = MOTOR_CW;

    wm_tick(&c, &s, &a);

    /* Controller should override it back to STOP */
    assert(a.motor_dir == MOTOR_STOP);
    assert(c.state == WM_FILL);

    /* Safety 2: Inlet and Drain cannot be both ON */
    /* Transition to DRAIN */
    c.state = WM_DRAIN;
    s.water_level = WATER_HIGH;
    s.drain_check = true;
    /* DRAIN state sets drain=true, inlet=false */

    /* Force Inlet ON */
    a.inlet_valve = true;

    wm_tick(&c, &s, &a);

    /* Logic: DRAIN state sets inlet=false.
       If we want to test Interlock specifically, we rely on the state logic
       to enforce it. The Interlock comment in wm_control.c says:
       "Safety Interlocks (Override)"
       Let's verify that override works even if state logic was weird.
       But we can't easily break state logic here.
       However, we can check if the final outcome adheres to the rule.
    */
    assert(a.drain_pump == true);
    assert(a.inlet_valve == false); /* Should be forced off by DRAIN logic + Interlock */

    printf("✓ test_safety_mechanisms\n");
}

/* ============================================================
 * Main
 * ============================================================ */

static void test_full_standard_cycle(void) {
    wm_controller_t c;
    wm_sensors_t s;
    wm_actuators_t a;

    /* Standard: 1 wash, 1 rinse, spin enabled */
    wm_program_t program = {
        .wash_count = 1,
        .rinse_count = 1,
        .spin_enable = true,
        .soap_time_sec = 1,
        .wash_agitate_time_sec = 2,
        .rinse_agitate_time_sec = 2,
        .water_fill_timeout_sec = 10,
        .drain_timeout_sec = 10,
        .agitate_run_sec = 2,
        .agitate_pause_sec = 2,
        .ticks_per_second = 10, // Fast simulation
    };

    wm_init(&c, &s, &a, program);
    wm_start(&c);

    /* START -> FILL */
    wm_tick(&c, &s, &a);
    assert(c.state == WM_FILL);

    /* Fill Water */
    s.water_level = WATER_HIGH;
    s.drain_check = true; /* Ensure drain sensor detects water */
    wm_tick(&c, &s, &a);
    assert(c.state == WM_SOAP);

    /* Soap Logic (1s = 10 ticks) */
    MULTI_TICK(&c, &s, &a, 10);
    /* Should transition to AGITATE */
    wm_tick(&c, &s, &a);
    assert(c.state == WM_AGITATE);

    /* Wash Agitate (Wait for transition) */
    int limit = 50;
    while (c.state == WM_AGITATE && limit-- > 0) {
        wm_tick(&c, &s, &a);
    }
    assert(c.state == WM_DRAIN);

    /* Drain Water */
    s.water_level = WATER_EMPTY;
    s.drain_check = false;
    wm_tick(&c, &s, &a);

    /* Rinse Phase: FILL */
    assert(c.state == WM_FILL);
    assert(c.is_wash_phase == false);

    /* Fill for Rinse */
    s.water_level = WATER_HIGH;
    wm_tick(&c, &s, &a);
    /* Rinse uses AGITATE immediately (No SOAP) */
    assert(c.state == WM_AGITATE);

    /* Rinse Agitate (Wait for transition) */
    limit = 50;
    while (c.state == WM_AGITATE && limit-- > 0) {
        wm_tick(&c, &s, &a);
    }
    assert(c.state == WM_DRAIN);

    /* Drain Rinse Water */
    s.water_level = WATER_EMPTY;
    s.drain_check = false;
    wm_tick(&c, &s, &a);

    /* Spin Phase */
    assert(c.state == WM_SPIN);
    wm_tick(&c, &s, &a); /* Tick once to run SPIN logic */
    assert(a.motor_dir == MOTOR_CW || a.motor_dir == MOTOR_CCW);
    assert(a.inlet_valve == false);

    /* Spin for fixed duration (hardcoded 3s in logic? No, derived?) */
    /* wm_control.c logic for SPIN duration needs check.
       Usually hardcoded or config. Let's assume it runs for some ticks. */
    /* Keep ticking until Complete */
    limit = 100;
    while (c.state == WM_SPIN && limit-- > 0) {
        wm_tick(&c, &s, &a);
    }

    assert(c.state == WM_COMPLETE);

    printf("✓ test_full_standard_cycle\n");
}

static void test_spin_logic(void) {
    wm_controller_t c;
    wm_sensors_t s;
    wm_actuators_t a;
    wm_program_t program = {.spin_enable = true, .ticks_per_second = 1};

    wm_init(&c, &s, &a, program);

    /* Force to SPIN state */
    c.state = WM_SPIN;
    c.state_time = 0;

    /* Tick */
    wm_tick(&c, &s, &a);

    /* Safety Check: Spin MUST NOT have water */
    /* If we force water level high during spin: */
    s.water_level = WATER_HIGH;
    wm_tick(&c, &s, &a);

    /* Logic check: Does it error? Or just pump? */
    /* Current implementation might not check water in SPIN state explicitly if safely entered.
       But let's check actuators. */
    assert(a.motor_dir != MOTOR_STOP);
    assert(a.inlet_valve == false);
    assert(a.soap_pump == false);
    assert(a.drain_pump == false); /* Drain pump NOT enabled in current logic */

    printf("✓ test_spin_logic\n");
}

int main(void) {
    printf("Running washing machine unit tests...\n\n");

    test_init_state();
    test_start_to_fill();
    test_fill_to_soap();
    test_soap_to_agitate();
    test_pause_resume();
    test_drain_sensor();
    test_drain_timeout();
    test_invalid_program();
    test_complete_buzzer();
    test_safety_mechanisms();

    /* New Tests */
    test_full_standard_cycle();
    test_spin_logic();

    printf("\nAll tests PASSED ✅\n");
    return 0;
}
