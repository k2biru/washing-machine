#include "app.h"
#include "utils.h"
#include <Arduino.h>

void setup() {
    /* Initialize hardware once */
    wm_actuators_init();

    Serial.begin(115200);
    while (!Serial) {
        ;
    }
    LOG_PRINTF("System Initialized.\n");
}

void loop() {
    LOG_PRINTF("Starting Wash Program...\n");

    /* Run the wash program logic */
    wm_program_ex();

    LOG_PRINTF("Program Cycle Finished. Cooling down...\n");
    MS_DELAY(5000); /* Wait between cycles */
}
