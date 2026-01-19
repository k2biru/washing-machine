#include "buzzer.h"
#include "music.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <avr/pgmspace.h>

/*
 * The LGT8F Arduino core defines tone/noTone with C++ linkage only.
 * We use __asm__ to alias our C declarations to the mangled C++ names
 * so that we can keep this file as .c while linking correctly.
 */
void tone(uint8_t _pin, unsigned int frequency, unsigned long duration) __asm__("_Z4tonehjm");
void noTone(uint8_t _pin) __asm__("_Z6noToneh");

static uint8_t buzzer_pin = 0;

void buzzer_init(uint8_t pin) {
    buzzer_pin = pin;
    pinMode(buzzer_pin, OUTPUT);
}

void buzzer_play_sequence(const note_t *notes, uint16_t note_count) {
    for (uint16_t i = 0; i < note_count; i++) {
        uint16_t freq = pgm_read_word(&(notes[i].freq));
        uint32_t duration = pgm_read_dword(&(notes[i].duration));

        if (freq == 0) {
            noTone(buzzer_pin);
            delay(duration);
        } else {
            tone(buzzer_pin, freq, duration);
            // tone() is non-blocking, so we wait for the duration
            // Plus a small gap between notes for better clarity
            delay(duration);
            noTone(buzzer_pin);
            delay(duration * 0.3);
        }
    }
    noTone(buzzer_pin);
}

void buzzer_play_song(song_id_t song_id) {
    switch (song_id) {
    case SONG_START:
        buzzer_play_sequence(song_start_data, song_start_length);
        break;
    case SONG_FINISHED:
        buzzer_play_sequence(song_finished_data, song_finished_length);
        break;
    case SONG_ERROR:
        buzzer_play_sequence(song_error_data, song_error_length);
        break;
    }
}

#else
// Mock implementation for PC simulation
#include <stdio.h>
#include <unistd.h>

void buzzer_init(uint8_t pin) {
#ifndef LINUX_SOUND
    printf("[BUZZER] Init on pin %d\n", pin);
#else
    (void)pin;
#endif
}

#ifdef LINUX_SOUND
static void play_pcm_tone(uint16_t freq, uint32_t duration_ms) {
    const uint32_t sample_rate = 8000;
    uint32_t num_samples = (sample_rate * duration_ms) / 1000;

    if (freq == 0) {
        for (uint32_t i = 0; i < num_samples; i++) {
            putchar(127); // Silence (middle of U8)
        }
    } else {
        uint32_t period = sample_rate / freq;
        if (period == 0)
            period = 1;
        for (uint32_t i = 0; i < num_samples; i++) {
            // Square wave
            if ((i % period) < (period / 2)) {
                putchar(200); // High
            } else {
                putchar(50); // Low
            }
        }
    }
    fflush(stdout);
}
#endif

void buzzer_play_sequence(const note_t *notes, uint16_t note_count) {
#ifndef LINUX_SOUND
    printf("[BUZZER] Playing sequence of %d notes...\n", note_count);
#endif

    for (uint16_t i = 0; i < note_count; i++) {
#ifdef LINUX_SOUND
        play_pcm_tone(notes[i].freq, notes[i].duration);
        // Small gap
        play_pcm_tone(0, 20);
#else
        if (notes[i].freq == 0) {
            printf("[BUZZER] Rest %d ms\n", notes[i].duration);
        } else {
            printf("[BUZZER] Tone %d Hz for %d ms\n", notes[i].freq, notes[i].duration);
        }
#endif
    }
}

void buzzer_play_song(song_id_t song_id) {
#ifndef LINUX_SOUND
    printf("[BUZZER] Playing song %d...\n", song_id);
#endif

    switch (song_id) {
    case SONG_START:
        buzzer_play_sequence(song_start_data, song_start_length);
        break;
    case SONG_FINISHED:
        buzzer_play_sequence(song_finished_data, song_finished_length);
        break;
    case SONG_ERROR:
        buzzer_play_sequence(song_error_data, song_error_length);
        break;
    }
}
#endif
