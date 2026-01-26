#ifndef UTILS_H // Check if UTILS_H is NOT defined
#define UTILS_H // Define UTILS_H so the next include skips this file

#include <stdarg.h>

// --- LOG_PRINTF MACRO ---
#ifdef ARDUINO
#include <Arduino.h>

#ifdef __cplusplus
// Helper to bridge C stdio to Arduino Serial
static int serial_putchar(char c, FILE *f) {
    if (c == '\n')
        Serial.write('\r');
    return Serial.write(c) == 1 ? 0 : 1;
}

static void SerialPrintf(const char *fmt, ...) {
    static FILE serial_out;
    static bool initialized = false;
    if (!initialized) {
        fdev_setup_stream(&serial_out, serial_putchar, NULL, _FDEV_SETUP_WRITE);
        initialized = true;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf_P(&serial_out, fmt, args); // _P version reads fmt from Flash
    va_end(args);
}

// PSTR(fmt) keeps the string in Flash memory instead of SRAM
#define LOG_PRINTF(fmt, ...) SerialPrintf(PSTR(fmt), ##__VA_ARGS__)
#else
// Fallback for C on Arduino (direct logging is not easily bridged without C++ Serial)
#define LOG_PRINTF(fmt, ...) ((void)0)
#endif

// --- MS_DELAY MACRO ---
#define MS_DELAY(ms) delay(ms)

#else
// --- PC / x86 logic ---
#include <stdio.h>
#define LOG_PRINTF(fmt, ...) printf(fmt, ##__VA_ARGS__)

#ifdef _WIN32
#include <windows.h>
#define MS_DELAY(ms) Sleep(ms)
#else
#include <unistd.h>
#define MS_DELAY(ms) usleep((ms) * 1000)
#endif
#endif

#endif // UTILS_H
