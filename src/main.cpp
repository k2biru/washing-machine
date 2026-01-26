#include "app.h"
#include "utils.h"
#include <Arduino.h>

/* --- Main Entry Point for MCU --- */
void setup() {
    /* Initialize App (HAL + Logic) */
    app_init();

    Serial.begin(115200);
    while (!Serial) {
        ;
    }
    LOG_PRINTF("System Initialized.\n");
}

void loop() {
    /* Run App Logic Loop */
    app_loop();
}
