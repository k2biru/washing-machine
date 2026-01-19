# Washing Machine Controller (C/C++)

A robust, state-machine based washing machine controller designed for the LGT8F328P (Arduino-compatible) microcontroller. This project features a modular design, separating the core logic from hardware abstraction and providing a standalone C simulation for local development.

## Project Structure

- `lib/wm_control/`: Core washing machine logic (ANSI C).
- `src/`: MCU firmware logic.
    - `main.cpp`: Entry point (Arduino setup/loop).
    - `app.cpp`: Application logic and hardware abstraction.
- `test/`:
    - `test_wm_control.c`: Unit tests for the core state machine.
    - `simulation.c`: Standalone PC simulation of the wash cycle.
- `include/`: Common utilities and logging macros.

## Getting Started

### Prerequisites

- GCC (for local simulation and tests)
- [PlatformIO Core](https://docs.platformio.org/en/latest/core/index.html) (for MCU build)

### Local Development (PC)

Use the provided `Makefile` to run tests and simulations on your machine:

```bash
# Run unit tests
make test

# Run full wash cycle simulation
make run-wm-simulation

# Clean build artifacts
make clean
```

### Microcontroller (LGT8F328P)

The project is configured for the LGT8F328P board using PlatformIO.

```bash
# Build firmware
make pio-build

# Upload to board
make pio-upload

# Open serial monitor
make pio-monitor
```

## Features

- **Standardized Agitation**: Fixed 5-second intervals with alternating directions (CW/CCW).
- **Configurable Tick Rate**: Adjustable timing granularity (currently 500ms).
- **Safety Interlocks**: Robust actuator enforcement to prevent illegal states (e.g., motor running during fill).
- **Relay HAL**: Designed for active-low relay modules.
