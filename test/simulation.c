#define _DEFAULT_SOURCE
#include "../lib/wm_control/wm_control.h" // For water_level_t enum
#include "../src/app.h"
#include "../src/hal.h"
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

/* --- Physics Simulation --- */
static int sim_water_level = 0; // 0=EMPTY, 1=LOW, 2=MED, 3=HIGH
static uint64_t last_physics_tick = 0;

static uint64_t get_now_ms(void) {
    struct timeval now_tv;
    gettimeofday(&now_tv, NULL);
    return (uint64_t)now_tv.tv_sec * 1000 + (now_tv.tv_usec / 1000);
}

static void run_physics(void) {
    // Read actuators from HAL
    hal_sim_actuators_t acts = hal_sim_get_actuators();

    // Simple timing for water fill/drain simulation
    uint64_t now = get_now_ms();

    // Fill/Drain every 2000ms ?? Actually app presets ticks_per_sec is variable.
    // Let's make it relatively fast for simulation comfort.
    // App ticks are driven by actual time in app_loop (1s, 0.5s).
    // Let's make water move every 500ms for responsiveness.
    if (now - last_physics_tick > 500) {
        last_physics_tick = now;

        if (acts.inlet) {
            if (sim_water_level < WATER_HIGH) {
                sim_water_level++;
            }
        }
        if (acts.drain) {
            if (sim_water_level > WATER_EMPTY) {
                sim_water_level--;
            }
        }
    }

    // Update HAL Sensors
    bool drain_check = (sim_water_level > WATER_EMPTY);
    hal_sim_set_sensors(drain_check, sim_water_level);
}

int main(void) {
    set_conio_terminal_mode();

    printf("\n=== Washing Machine Simulation ===\n");
    printf("Controls: 'a' = Start/Pause/OK, 'b' = Next, 'c' = ESC/Abort\n");

    // Initialize Application
    App app;
    app_init(&app);

    while (1) {
        // 1. Input Handling -> Simulate Buttons
        int key = get_key();

        // Reset all buttons first
        hal_sim_set_button(HAL_BTN_A, false);
        hal_sim_set_button(HAL_BTN_B, false);
        hal_sim_set_button(HAL_BTN_C, false);

        if (key == 'a')
            hal_sim_set_button(HAL_BTN_A, true);
        if (key == 'b')
            hal_sim_set_button(HAL_BTN_B, true);
        if (key == 'c')
            hal_sim_set_button(HAL_BTN_C, true);

        // 2. Run Physics (Water Level)
        run_physics();

        // 3. Run Application Loop
        app_loop(&app);

        // 4. Sleep to spare CPU
        usleep(50000); // 50ms
    }

    return 0;
}
