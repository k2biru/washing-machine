# Washing Machine Controller (C/C++)

A robust, state-machine based washing machine controller designed for the LGT8F328P (Arduino-compatible) microcontroller. This project features a modular design, separating the core logic from hardware abstraction and providing a standalone C simulation for local development.

### Project Context
> [!NOTE]
> **Vibe Coding Demonstration**: This repository serves as a practical demonstration of "Vibe Coding" principles applied to C-based MCU development. It showcases the capability to write, simulate, and verify embedded firmware entirely on a host machine (Unit Tests & PC Simulation) before deploying to hardware, emphasizing rapid iteration and vibe-based flow states.

## Key Features

-   **Modular Parameter Selection**: Multi-stage menu for selecting Program (Normal, Short, Express), Water Level (Low, Med, High), and Power (Normal, Strong).
-   **High-Precision Agitation**: Decisecond-level control (100ms ticks) with configurable run/stop pulses (e.g., 1.6s run for Normal power).
-   **Target Water Level**: Intelligent filling logic that stops at the user-specified level (Low, Med, or High).
-   **Encapsulated State**: OOP-style `App` structure removes global variables, enabling cleaner integration and multiple instances.
-   **Safety Interlocks**: Strictly enforced hardware constraints (e.g., Motor inhibited during Fill; Inlet inhibited during Drain).
-   **Real-time Feedback**: Logic-driven buzzer notifications for Start, Completion, and Errors.
-   **Cross-Platform Core**: The exact same C logic runs on the MCU and the Linux simulator.

## System Architecture

The system is designed with a strict separation of concerns, ensuring portability and testability.

### Hardware Interface (HAL)
The application interacts with physical components through a Hardware Abstraction Layer (`hal.h`). This allows the logic to remain oblivious to whether it is controlling 12V relays or a terminal simulation.

#### Actuators (Outputs)
| Actuator ID | Description | Sim / Linux Equivalent | MCU / Hardware Equivalent |
| :--- | :--- | :--- | :--- |
| `HAL_ACT_MOTOR_POWER` | Controls main motor power relay. | Console Log & State Variable | Relay Pin (Active Low/High Configurable) |
| `HAL_ACT_MOTOR_DIR` | Controls motor direction (CW/CCW). | Console Display (`CW`/`CCW`/`STOP`) | H-Bridge or Direction Relay |
| `HAL_ACT_INLET` | Water inlet valve solenoid. | Increases `sim_water_level` | Solenoid Valve Pin |
| `HAL_ACT_DRAIN` | Water drain pump. | Decreases `sim_water_level` | Pump Relay Pin |
| `HAL_ACT_SOAP` | Soap dispenser pump/actuator. | Console Log | Peristaltic Pump / Solenoid |
| `HAL_BUZZER` | Piezo buzzer for status tones. | MIDI/Console Output | PWM / Tone Pin |

#### Sensors (Inputs)
| Sensor ID | Description | Sim / Linux Equivalent | MCU / Hardware Equivalent |
| :--- | :--- | :--- | :--- |
| `Water Level` | Analog/Digital level sensor. | `key_up`/`key_down` (Simulated physics) | Pressure Switch / Float Sensor |
| `Drain Check` | Safety sensor detecting water presence. | Derived from `water_level > 0` | Continuity / Flow Sensor |
| `Buttons (A, B, C)` | User Interface inputs. | Keyboard Keys (`a`, `b`, `c`) | Tactile Pushbuttons (Debounced) |

### Simulation Layer
In the Linux build, the HAL is implemented to interact with `test/simulation.c`. This file acts as a "Physics Engine," responding to actuator states (e.g., if Drain Pump is ON, decrement water level variable) and feeding sensor data back to the core logic.

#### UI Simulation (Controls)
-   **'a' (BTN_A)**: Select/OK / Start / Pause.
-   **'b' (BTN_B)**: Next item in menu.
-   **'c' (BTN_C)**: Abort current cycle.

#### Modular Presets
-   **Programs**: Normal (15m/15m), Short (10m/10m), Express (7m/7m).
-   **Power Modes**:
    -   *Normal*: 1.6s run, 3.4s stop (per 5s pulse).
    -   *Strong*: 4.0s run, 1.0s stop (per 5s pulse).

## Project Structure

- `lib/wm_control/`: Core washing machine logic (ANSI C).
- `lib/buzzer/`: Buzzer music player and tunes.
- `src/`: MCU firmware logic.
    - `main.cpp`: Entry point (Arduino setup/loop).
    - `app.c`: Application logic and hardware abstraction (C99).
- `test/`:
    - `test_wm_control.c`: Unit tests for the core state machine.
    - `simulation.c`: Standalone PC simulation of the wash cycle.
- `include/`: Common utilities and logging macros.

## Getting Started

### Prerequisites
To run the local simulation and tests, ensure your host environment (Linux/WSL) has the following installed:
*   **Build System**: `make`
*   **Compiler**: `gcc` (GNU Compiler Collection)
*   **Audio**: `aplay` (Optional, for Linux sound tests)

### Local Development (PC)

Use the provided `Makefile` to run tests and simulations on your machine:

```bash
# Clean build artifacts
make clean

# Run unit tests
make test

# Run full wash cycle simulation
make run-wm-simulation
```

## Unit Test Suite
The project includes a comprehensive suite of unit tests (`test/test_wm_control.c`) to verify the state machine logic under various conditions.

| Test Name | Description | Expected Outcome |
| :--- | :--- | :--- |
| `test_init_state` | Verifies initial values of the controller struct. | State is `WM_IDLE`, all counters 0. |
| `test_start_to_fill` | Checks transition from Start to Fill. | `WM_START` -> `WM_FILL`, `inlet_valve` ON. |
| `test_fill_to_soap` | Checks transition from Fill to Soap injection. | Sensor HIGH triggers transition to `WM_SOAP`. |
| `test_soap_to_agitate` | Verifies Soap timer and transition to Agitate. | After time elapses, state becomes `WM_AGITATE`. |
| `test_pause_resume` | Tests Pause/Resume functionality. | State entering `WM_PAUSED` and restoring `prev_state`. |
| `test_drain_sensor` | Ensures logic waits for drain sensor. | State remains `WM_DRAIN` until sensor reports empty. |
| `test_drain_timeout` | Simulator timeout condition for drain. | Triggers `WM_ERROR` (TIMEOUT_DRAIN). |
| `test_invalid_program` | Tests config validation. | Invalid params trigger `WM_ERR_INVALID_PROGRAM`. |
| `test_complete_buzzer` | Verifies completion behavior. | `WM_COMPLETE` state triggers `BUZZER_FINISH`. |
| `test_safety_mechanisms` | Checks critical safety interlocks. | Motor forced STOP during FILL; Inlet forced OFF during DRAIN. |
| `test_full_standard_cycle` | Simulates a complete Wash-Rinse-Spin cycle. | Controller navigates all states sequentially to `WM_COMPLETE`. |
| `test_spin_logic` | Verifies specific behavior in Spin state. | Motor spins CW/CCW, Drain Pump is OFF (gravity drain assumption or model specific). |

## Microcontroller (LGT8F328P)

The project is configured for the LGT8F328P board using PlatformIO.

```bash
# Build firmware
make pio-build

# Upload to board
make pio-upload
```
