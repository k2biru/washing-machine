#include "../lib/buzzer/buzzer.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int song_id = 0;
    if (argc > 1) {
        song_id = atoi(argv[1]);
    }

    // Note: We don't print anything to stdout here because
    // it will be piped to aplay as raw PCM data.
    // Error messages should go to stderr.

    buzzer_init(0); // Pin doesn't matter on Linux
    buzzer_play_song((song_id_t)song_id);

    return 0;
}
