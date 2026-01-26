CC      := gcc
CXX     := g++
CFLAGS  := -std=c99 -Wall -Wextra -Wpedantic -O2 -Ilib/wm_control -Isrc -Iinclude
CXXFLAGS:= -std=c++11 -Wall -Wextra -O2 -Ilib/wm_control -Isrc -Iinclude
BUILD_DIR := build

TARGET      := test/simulation
TEST_TARGET := test/test_wm

# Simulation Sources
SIM_SRCS_C   := test/simulation.c src/hal.c lib/wm_control/wm_control.c src/app.c
SIM_SRCS_CXX :=

# Unit Test Sources (Pure C tests, mocking app perhaps? No, test_wm_control only tests logic)
TEST_SRCS := test/test_wm_control.c lib/wm_control/wm_control.c

# Object Files
SIM_OBJS     := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SIM_SRCS_C)) \
                $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SIM_SRCS_CXX))
TEST_OBJS    := $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SRCS))

all: $(TARGET) $(TEST_TARGET)

# Link Simulation (Use CC as it is now pure C)
$(TARGET): $(SIM_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

# Link Unit Tests (Pure C)
$(TEST_TARGET): $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

# Compile C Sources
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C++ Sources
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

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

# --- PlatformIO ---
pio-build:
	pio run

pio-upload:
	pio run --target upload

pio-monitor:
	pio device monitor

# --- Linux Buzzer Sound Test ---
BUZZER_TEST_SRC := test/test_buzzer_linux.c lib/buzzer/buzzer.c src/hal.c
BUZZER_TEST_TARGET := build/test_buzzer_linux

$(BUZZER_TEST_TARGET): $(BUZZER_TEST_SRC) lib/buzzer/music.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DLINUX_SOUND -o $@ $(BUZZER_TEST_SRC) -lm

play-buzzer-linux: $(BUZZER_TEST_TARGET)
	./$(BUZZER_TEST_TARGET) | aplay -r 8000 -f U8

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TEST_TARGET) $(GEN_TARGET) $(BUZZER_TEST_TARGET)

.PHONY: all test clean run-wm-simulation pio-build pio-upload pio-monitor generate-music play-buzzer-linux
