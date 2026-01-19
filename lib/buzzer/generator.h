#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdint.h>
#include <stdio.h>

/**
 * @brief Generate music.h from a MIDI file
 * @param midi_path Path to the input MIDI file
 * @param output_path Path to the output music.h header
 * @return 0 on success, non-zero on error
 */
int generate_music_header(const char *midi_path, const char *output_path);

#endif // GENERATOR_H
