CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Wpedantic -O2 -Ilib/wm_control
BUILD_DIR := build

TARGET      := test/simulation
TEST_TARGET := test/test_wm

SRCS      := test/simulation.c lib/wm_control/wm_control.c
TEST_SRCS := test/test_wm_control.c lib/wm_control/wm_control.c

# Map source files to object files in the build directory
OBJS      := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))
TEST_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SRCS))

all: $(TARGET) $(TEST_TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_TARGET): $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

# Rule for object files, maintaining directory structure under BUILD_DIR
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

run-wm-simulation: $(TARGET)
	./$(TARGET)

# --- MIDI Generator ---
GEN_SRC := tools/midi_generator/generator.c
GEN_TARGET := build/midi_gen
$(GEN_TARGET): $(GEN_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DSTANDALONE_GENERATOR -o $@ $^ -lm

generate-music: $(GEN_TARGET)
	./$(GEN_TARGET) lib/buzzer/music.h \
		song_start:tools/midi_generator/input/start.mid \
		song_finished:tools/midi_generator/input/finish.mid \
		song_error:tools/midi_generator/input/error.mid

# --- Existing targets ---
SRCS      := test/simulation.c lib/wm_control/wm_control.c lib/buzzer/buzzer.c
TEST_SRCS := test/test_wm_control.c lib/wm_control/wm_control.c

pio-build:
	pio run

pio-upload:
	pio run --target upload

pio-monitor:
	pio device monitor

# --- Linux Buzzer Sound Test ---
BUZZER_TEST_SRC := test/test_buzzer_linux.c lib/buzzer/buzzer.c
BUZZER_TEST_TARGET := build/test_buzzer_linux

$(BUZZER_TEST_TARGET): $(BUZZER_TEST_SRC) lib/buzzer/music.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DLINUX_SOUND -o $@ $(BUZZER_TEST_SRC) -lm

play-buzzer-linux: $(BUZZER_TEST_TARGET)
	./$(BUZZER_TEST_TARGET) | aplay -r 8000 -f U8

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TEST_TARGET) $(GEN_TARGET) $(BUZZER_TEST_TARGET)

.PHONY: all test clean run-wm-simulation pio-build pio-upload pio-monitor generate-music play-buzzer-linux
