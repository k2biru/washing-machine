#include "app.h"
#include "utils.h"
#include <Arduino.h>

/* --- Main Entry Point for MCU --- */
static App app;

void setup() {
    /* Initialize App (HAL + Logic) */
    app_init(&app);

    Serial.begin(115200);
    while (!Serial) {
        ;
    }
    LOG_PRINTF("System Initialized.\n");
}

void loop() {
    /* Run App Logic Loop */
    app_loop(&app);
}
