#ifndef WM_CONTROL_H
#define WM_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

/* ---------- Sensors ---------- */
/* Water level states from empty to high */
typedef enum { WATER_EMPTY = 0, WATER_LOW, WATER_MED, WATER_HIGH } water_level_t;

typedef struct {
    water_level_t water_level;
    bool drain_check; /* Returns true if any water is detected in the drum */
} wm_sensors_t;

/* ---------- Actuators ---------- */
/* Buzzer beep patterns */
typedef enum { BUZZER_OFF = 0, BUZZER_START, BUZZER_FINISH, BUZZER_ERROR } wm_buzzer_mode_t;

/* Motor directions including stop state */
typedef enum { MOTOR_STOP = 0, MOTOR_CW = 1, MOTOR_CCW = 2 } wm_motor_dir_t;

typedef struct {
    bool inlet_valve;         /* Control water intake */
    bool soap_pump;           /* Inject soap during wash phase */
    bool drain_pump;          /* Remove water from drum */
    wm_motor_dir_t motor_dir; /* Current motor direction */
    wm_buzzer_mode_t buzzer;  /* Feedback sound output */
} wm_actuators_t;

/* ---------- Program Config ---------- */
/* Configuration defining how a specific wash program behaves */
typedef struct {
    uint8_t wash_count;  /* Number of wash cycles */
    uint8_t rinse_count; /* Number of rinse cycles */
    bool spin_enable;    /* Whether to perform final spin */

    uint16_t soap_time_sec;          /* Duration for soap injection in seconds */
    uint16_t wash_agitate_time_sec;  /* Total agitation time during WASH (seconds) */
    uint16_t rinse_agitate_time_sec; /* Total agitation time during RINSE (seconds) */
    uint8_t agitate_run_sec;         /* Duration the motor is ON in each 5s interval */
    uint8_t agitate_pause_sec;       /* (Legacy) Previously used for pauses */
    uint16_t water_fill_timeout_sec; /* Max time allowed to reach HIGH level */
    uint16_t drain_timeout_sec;      /* Max time allowed to reach EMPTY level */
    uint8_t ticks_per_second;        /* Tick frequency (e.g., 1 for 1s, 2 for 500ms) */
} wm_program_t;

/* ---------- States ---------- */
/* Main state machine stages */
typedef enum {
    WM_IDLE = 0, /* Waiting for start signal */
    WM_START,    /* Triggering start-up sequence/buzzer */
    WM_FILL,     /* Filling water until HIGH level reached */
    WM_SOAP,     /* Wash phase: injecting soap */
    WM_AGITATE,  /* Wash/Rinse phase: alternating motor rotation */
    WM_DRAIN,    /* Draining water until EMPTY level reached */
    WM_SPIN,     /* High speed rotation to dry clothes */
    WM_PAUSED,   /* User paused the timer */
    WM_COMPLETE, /* Finished successfully */
    WM_ERROR     /* Safety or hardware failure occurred */
} wm_state_t;

typedef enum {
    WM_ERR_NONE = 0,
    WM_ERR_TIMEOUT_FILL,
    WM_ERR_TIMEOUT_DRAIN,
    WM_ERR_INVALID_PROGRAM
} wm_error_t;

/* ---------- Controller ---------- */
typedef struct {
    wm_state_t state;
    wm_state_t prev_state;

    bool is_wash_phase; /* distinguish wash vs rinse */
    uint8_t wash_done;
    uint8_t rinse_done;
    uint16_t state_time;

    wm_program_t program;
    wm_error_t error_code;
} wm_controller_t;

/* ---------- API ---------- */
void wm_init(wm_controller_t *ctrl, wm_sensors_t *sens, wm_actuators_t *act, wm_program_t program);

void wm_start(wm_controller_t *ctrl);
void wm_pause(wm_controller_t *ctrl);
void wm_resume(wm_controller_t *ctrl);
void wm_abort(wm_controller_t *ctrl);
uint16_t wm_get_time_remaining_sec(wm_controller_t *ctrl);

void wm_tick(wm_controller_t *ctrl, wm_sensors_t *sens, wm_actuators_t *act);

const char *wm_state_str(wm_state_t s);
const char *wm_error_str(wm_error_t err);

#endif
