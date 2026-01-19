#define _DEFAULT_SOURCE
#include "../lib/wm_control/wm_control.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

/* --- Utility for Non-blocking Keyboard Input --- */
static void set_conio_terminal_mode(void) {
    struct termios new_termios;
    tcgetattr(0, &new_termios);
    new_termios.c_lflag &= ~ICANON;
    new_termios.c_lflag &= ~ECHO;
    tcsetattr(0, TCSANOW, &new_termios);
}

static int get_key(void) {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    if (select(1, &fds, NULL, NULL, &tv)) {
        return getchar();
    }
    return -1;
}

/* --- UI Representation --- */
typedef enum { UI_STARTUP, UI_RUNNING, UI_ABORT, UI_SLEEP } ui_state_t;

static wm_program_t presets[] = {{1, 2, true, 3, 20, 20, 3, 0, 30, 15, 2},
                                 {1, 1, true, 2, 10, 10, 4, 0, 20, 10, 2},
                                 {2, 2, true, 5, 30, 30, 5, 0, 40, 20, 2}};
static const char *preset_names[] = {"Standard", "Quick", "Heavy Duty"};
static const int num_presets = 3;

static const char *water_str(water_level_t w) {
    static const char *strs[] = {"EMPTY", "LOW", "MED", "HIGH"};
    return strs[w];
}

static const char *motor_str(wm_motor_dir_t d) {
    static const char *strs[] = {"STOP", "CW", "CCW"};
    return strs[d];
}

int main(void) {
    set_conio_terminal_mode();
    ui_state_t ui_state = UI_STARTUP;
    int current_preset = 0;

    wm_controller_t ctrl;
    wm_sensors_t sensors;
    wm_actuators_t actuators;

    wm_init(&ctrl, &sensors, &actuators, presets[current_preset]);

    printf("\n=== Washing Machine Simulation ===\n");
    printf("Controls: 'a' = Start/Pause/OK, 'b' = Next, 'c' = ESC/Abort\n");
    printf("Current Preset: %s\n", preset_names[current_preset]);

    while (1) {
        int key = get_key();

        /* UI Input Handling */
        if (ui_state == UI_STARTUP) {
            if (key == 'b') {
                current_preset = (current_preset + 1) % num_presets;
                printf("\rPreset: %-15s (Press 'a' to START)          ",
                       preset_names[current_preset]);
                fflush(stdout);
            }
            if (key == 'a') {
                wm_init(&ctrl, &sensors, &actuators, presets[current_preset]);
                wm_start(&ctrl);
                ui_state = UI_RUNNING;
                printf("\nStarting %s...\n", preset_names[current_preset]);
            }
        } else if (ui_state == UI_RUNNING) {
            if (key == 'a') {
                if (ctrl.state == WM_PAUSED) {
                    wm_resume(&ctrl);
                    printf("\n--- Resumed ---\n");
                } else {
                    wm_pause(&ctrl);
                    printf("\n--- Paused ---\n");
                }
            }
            if (key == 'c') {
                wm_pause(&ctrl);
                ui_state = UI_ABORT;
                printf("\n--- Abort? (a: Confirm, c: Cancel) ---\n");
            }
        } else if (ui_state == UI_ABORT) {
            if (key == 'a') {
                wm_abort(&ctrl);
                ui_state = UI_RUNNING;
                printf("\n--- Aborting... ---\n");
            }
            if (key == 'c') {
                wm_resume(&ctrl);
                ui_state = UI_RUNNING;
                printf("\n--- Abort cancelled. ---\n");
            }
        } else if (ui_state == UI_SLEEP) {
            if (key == 'a') {
                ui_state = UI_STARTUP;
                printf("\n--- Waking up... ---\n");
                printf("Preset: %s (Press 'a' to START)\n", preset_names[current_preset]);
            }
        }

        /* Controller Tick & Simulation (Run at ~ticks_per_second) */
        if (ui_state == UI_SLEEP) {
            usleep(100000);
            continue;
        }

        static uint64_t last_tick = 0;
        struct timeval now_tv;
        gettimeofday(&now_tv, NULL);
        uint64_t now_ms = (uint64_t)now_tv.tv_sec * 1000 + (now_tv.tv_usec / 1000);

        uint32_t period = 1000 / presets[current_preset].ticks_per_second;
        if (now_ms - last_tick >= period) {
            last_tick = now_ms;

            /* Simulated Sensors */
            if (actuators.inlet_valve) {
                if (sensors.water_level < WATER_HIGH)
                    sensors.water_level++;
            }
            if (actuators.drain_pump) {
                if (sensors.water_level > WATER_EMPTY)
                    sensors.water_level--;
            }
            sensors.drain_check = (sensors.water_level != WATER_EMPTY);

            /* Logic Tick */
            wm_tick(&ctrl, &sensors, &actuators);

            if (ui_state == UI_RUNNING) {
                uint16_t rem = wm_get_time_remaining_sec(&ctrl);
                printf("\rPhase: %-5s | Status: %-10s | Time: %02d:%02d | Level: %-6s | Valve:%d "
                       "Soap:%d Pump:%d "
                       "Motor:%s    ",
                       ctrl.is_wash_phase ? "WASH" : "RINSE", wm_state_str(ctrl.state), rem / 60,
                       rem % 60, water_str(sensors.water_level), actuators.inlet_valve,
                       actuators.soap_pump, actuators.drain_pump, motor_str(actuators.motor_dir));
                fflush(stdout);

                if (ctrl.state == WM_COMPLETE || ctrl.state == WM_ERROR) {
                    static int hold = 0;
                    if (++hold > 4) {
                        hold = 0;
                        ui_state = UI_SLEEP;
                        printf("\n\n=== Cycle Finished ===\n");
                        printf("ENTERING SLEEP MODE. Press 'a' to wake up.\n");
                    }
                }
            }
        }

        usleep(50000); // 50ms sleep
    }

    return 0;
}
