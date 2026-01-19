#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

typedef struct {
    uint16_t freq;     // Frequency in Hz (0 for rest)
    uint32_t duration; // Duration in ms
} note_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SONG_START = 0, SONG_FINISHED, SONG_ERROR } song_id_t;

/**
 * @brief Initialize the buzzer pin
 * @param pin The Arduino pin number
 */
void buzzer_init(uint8_t pin);

/**
 * @brief Play a specific song by ID
 * @param song_id The ID of the song to play
 */
void buzzer_play_song(song_id_t song_id);

/**
 * @brief Play a custom sequence of notes
 * @param notes Pointer to an array of notes in PROGMEM
 * @param note_count Number of notes in the array
 */
void buzzer_play_sequence(const note_t *notes, uint16_t note_count);

#ifdef __cplusplus
}
#endif

#endif // BUZZER_H
