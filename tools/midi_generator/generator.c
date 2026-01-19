#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EVENTS 20000
#define MIDI_MAGIC 0x4D546864
#define TRACK_MAGIC 0x4D54726B

typedef struct {
    uint32_t abs_tick;
    uint8_t type; // 1: Note On, 0: Note Off, 2: Tempo
    uint8_t note;
    uint32_t tempo; // For tempo events
} midi_event_t;

typedef struct {
    uint16_t format;
    uint16_t ntracks;
    uint16_t division;
} midi_header_t;

typedef struct {
    uint16_t freq;
    uint32_t duration;
} note_t;

static uint32_t read_be32(FILE *f) {
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4)
        return 0;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

static uint16_t read_be16(FILE *f) {
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2)
        return 0;
    return (uint16_t)((b[0] << 8) | b[1]);
}

static uint32_t read_varlen(FILE *f) {
    uint32_t val = 0;
    uint8_t b;
    do {
        if (fread(&b, 1, 1, f) != 1)
            break;
        val = (val << 7) | (b & 0x7F);
    } while (b & 0x80);
    return val;
}

static float note_to_freq(uint8_t note) {
    if (note == 0)
        return 0;
    return 440.0f * powf(2.0f, (note - 69.0f) / 12.0f);
}

int compare_events(const void *a, const void *b) {
    midi_event_t *ea = (midi_event_t *)a;
    midi_event_t *eb = (midi_event_t *)b;
    if (ea->abs_tick != eb->abs_tick)
        return ea->abs_tick - eb->abs_tick;
    // Process Note Off before Note On at same tick
    return ea->type - eb->type;
}

int process_midi(const char *path, const char *song_name, FILE *out) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("Failed to open MIDI");
        return 1;
    }

    if (read_be32(f) != MIDI_MAGIC) {
        fclose(f);
        return 1;
    }

    read_be32(f); // hlen
    midi_header_t header;
    header.format = read_be16(f);
    header.ntracks = read_be16(f);
    header.division = read_be16(f);

    midi_event_t *events = malloc(sizeof(midi_event_t) * MAX_EVENTS);
    int event_count = 0;

    for (int t = 0; t < header.ntracks; t++) {
        if (read_be32(f) != TRACK_MAGIC) {
            uint32_t tlen = read_be32(f);
            fseek(f, tlen, SEEK_CUR);
            continue;
        }
        uint32_t tlen = read_be32(f);
        long track_end = ftell(f) + tlen;
        uint32_t current_tick = 0;
        uint8_t running_status = 0;

        while (ftell(f) < track_end) {
            current_tick += read_varlen(f);
            uint8_t status;
            if (fread(&status, 1, 1, f) != 1)
                break;

            if (!(status & 0x80)) {
                fseek(f, -1, SEEK_CUR);
                status = running_status;
            } else {
                running_status = status;
            }

            uint8_t type = status & 0xF0;
            if (type == 0x90 || type == 0x80) {
                uint8_t note, vel;
                fread(&note, 1, 1, f);
                fread(&vel, 1, 1, f);
                if (type == 0x90 && vel == 0)
                    type = 0x80;

                if (event_count < MAX_EVENTS) {
                    events[event_count].abs_tick = current_tick;
                    events[event_count].type = (type == 0x90) ? 1 : 0;
                    events[event_count].note = note;
                    event_count++;
                }
            } else if (status == 0xFF) {
                uint8_t meta_type;
                fread(&meta_type, 1, 1, f);
                uint32_t meta_len = read_varlen(f);
                if (meta_type == 0x51) {
                    uint8_t b[3];
                    fread(b, 1, 3, f);
                    uint32_t tempo = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2];
                    if (event_count < MAX_EVENTS) {
                        events[event_count].abs_tick = current_tick;
                        events[event_count].type = 2;
                        events[event_count].tempo = tempo;
                        event_count++;
                    }
                } else {
                    fseek(f, meta_len, SEEK_CUR);
                }
            } else {
                int skip = (type == 0xC0 || type == 0xD0) ? 1 : 2;
                if (type == 0xF0) { // SysEx
                    fseek(f, -1, SEEK_CUR);
                    read_varlen(f); // Should handle properly but skipping
                } else {
                    fseek(f, skip, SEEK_CUR);
                }
            }
        }
    }
    fclose(f);

    qsort(events, event_count, sizeof(midi_event_t), compare_events);

    fprintf(out, "const note_t %s_data[] PROGMEM_ATTR = {\n", song_name);

    uint32_t tempo = 500000;
    uint32_t current_tick = 0;
    float current_ms = 0;
    int note_on_count = 0;
    uint8_t last_note = 0;
    float last_note_ms = 0;
    int count = 0;

    for (int i = 0; i < event_count; i++) {
        uint32_t delta_ticks = events[i].abs_tick - current_tick;
        current_ms += (float)delta_ticks * (tempo / 1000.0f) / header.division;
        current_tick = events[i].abs_tick;

        if (events[i].type == 2) {
            tempo = events[i].tempo;
        } else if (events[i].type == 1) { // Note On
            if (note_on_count == 0) {
                uint32_t rest_dur = (uint32_t)(current_ms - last_note_ms);
                if (rest_dur > 20 && rest_dur <= 5000) {
                    fprintf(out, "    {0, %" PRIu32 "},\n", rest_dur);
                    count++;
                }
                last_note = events[i].note;
                last_note_ms = current_ms;
            }
            note_on_count++;
        } else if (events[i].type == 0) { // Note Off
            if (note_on_count > 0)
                note_on_count--;
            if (note_on_count == 0 || (events[i].note == last_note && note_on_count > 0)) {
                uint32_t dur = (uint32_t)(current_ms - last_note_ms);
                if (dur > 20) {
                    fprintf(out, "    {%u, %" PRIu32 "}, // Note %d\n",
                            (uint16_t)roundf(note_to_freq(last_note)), dur, last_note);
                    count++;
                }
                last_note_ms = current_ms;
                // If there are other notes still on, pick the most recent/highest one?
                // For now, simplicity: wait for next Note On
            }
        }
    }

    fprintf(out, "};\nconst uint16_t %s_length = %d;\n\n", song_name, count);
    free(events);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s <output.h> <name1:in1.mid> [name2:in2.mid] ...\n", argv[0]);
        return 1;
    }

    FILE *out = fopen(argv[1], "w");
    if (!out)
        return 1;

    fprintf(out, "#ifndef MUSIC_H\n#define MUSIC_H\n\n#include \"buzzer.h\"\n\n");
    fprintf(out, "#ifdef ARDUINO\n#include <avr/pgmspace.h>\n#define PROGMEM_ATTR "
                 "PROGMEM\n#else\n#define PROGMEM_ATTR\n#endif\n\n");

    for (int i = 2; i < argc; i++) {
        char *colon = strchr(argv[i], ':');
        if (!colon)
            continue;
        *colon = '\0';
        char *name = argv[i];
        char *path = colon + 1;
        printf("Processing %s from %s...\n", name, path);
        process_midi(path, name, out);
    }

    fprintf(out, "#endif // MUSIC_H\n");
    fclose(out);
    return 0;
}
