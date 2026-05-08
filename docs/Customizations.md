# Custom Firmware Additions (Session Summary)

This document compiles the custom changes added in this workspace for the XY gantry machine.

## 1) Hardware / Core Machine Configuration

### Board / build target
- PlatformIO default environment: `STM32F446ZE_btt_usb_flash_drive`
- Serial monitor port: `/dev/ttyACM1`
- Monitor filters: `time, send_on_enter`

### Machine size
Configured in [Marlin/Configuration.h](../Marlin/Configuration.h):
- `X_BED_SIZE 750`
- `Y_BED_SIZE 600`
- `X_MAX_POS X_BED_SIZE`
- `Y_MAX_POS Y_BED_SIZE`

### Y2 on MOT3/Z2 connector (independent channel)
Configured in [Marlin/Configuration.h](../Marlin/Configuration.h):
- `Y2_DRIVER_TYPE TMC2209`
- `Y2_STEP_PIN PG4`
- `Y2_DIR_PIN PC1`
- `Y2_ENABLE_PIN PA0`
- `Y2_STOP_PIN PG11` (Z2-STOP / MOT3 DIAG)
- `Y2_CS_PIN PC7`
- `Y2_SERIAL_TX_PIN PC7`

These settings were added to keep Y2 on its own sensorless-homing path.

## 2) Homing / Sensorless Tuning

Configured in [Marlin/Configuration_adv.h](../Marlin/Configuration_adv.h):
- `HOMING_BUMP_MM { 0, 0 }`
- `HOMING_BUMP_DIVISOR { 2, 2 }`
- `HOMING_BACKOFF_POST_MM { 20, 20 }`
- `SENSORLESS_HOMING`
- `X_STALL_SENSITIVITY 80`
- `Y_STALL_SENSITIVITY 80`
- `Y2_STALL_SENSITIVITY Y_STALL_SENSITIVITY`
- `IMPROVE_HOMING_RELIABILITY`

## 3) Servo / Valve / Macro Workflow

### Servo enabled
Configured in [Marlin/Configuration.h](../Marlin/Configuration.h):
- `NUM_SERVOS 1`
- `SERVO_DELAY { 300 }`

### Startup macro initialization
Configured in [Marlin/Configuration_adv.h](../Marlin/Configuration_adv.h):
- `STARTUP_COMMANDS` defines:
  - `G1 F18000`
  - `M810 M106 P0 S255|M226 P108 S1|M226 P108 S0`
  - `M811 M106 P0 S0`
  - `M812 M280 P0 S10`
  - `M813 M280 P0 S90`
  - Immediate `M812`

### G-code macro support
Configured in [Marlin/Configuration_adv.h](../Marlin/Configuration_adv.h):
- `GCODE_MACROS`
- `GCODE_MACROS_SLOTS 5`
- `GCODE_MACROS_SLOT_SIZE 50`

### Pin control / pin debugging
Configured in [Marlin/Configuration_adv.h](../Marlin/Configuration_adv.h):
- `DIRECT_PIN_CONTROL` (M42)
- `PINS_DEBUGGING` (M43)

## 4) Custom Feature: Servo 0 Motion Guard

### Purpose
Prevent unsafe XY motion while Servo 0 is in the down position.

### Config block
Configured in [Marlin/Configuration.h](../Marlin/Configuration.h):
- `SERVO0_MOTION_GUARD`
- `SERVO0_UP_ANGLE 10`
- `SERVO0_DOWN_ANGLE 90`
- Blocked area:
  - `SERVO0_BLOCKED_X_MIN 0`
  - `SERVO0_BLOCKED_X_MAX X_MAX_POS`
  - `SERVO0_BLOCKED_Y_MIN 0`
  - `SERVO0_BLOCKED_Y_MAX 160`
- Wait point:
  - `SERVO0_WAIT_POS_X 5`
  - `SERVO0_WAIT_POS_Y Y_MAX_POS`

### New source files
- [Marlin/src/feature/servo_motion_guard.h](../Marlin/src/feature/servo_motion_guard.h)
- [Marlin/src/feature/servo_motion_guard.cpp](../Marlin/src/feature/servo_motion_guard.cpp)

### Integration points
- [Marlin/src/gcode/control/M280.cpp](../Marlin/src/gcode/control/M280.cpp)
  - Checks/handles servo-down and servo-up transitions.
- [Marlin/src/gcode/motion/G0_G1.cpp](../Marlin/src/gcode/motion/G0_G1.cpp)
  - Redirects blocked moves to the wait point and queues original destination.

### Runtime behavior
1. If Servo 0 is commanded down while head is in blocked zone, firmware first moves to wait point.
2. If a move enters blocked zone while Servo 0 is down, firmware redirects to wait point and queues the move.
3. When Servo 0 is raised, queued move is resumed automatically.

## 5) Custom Feature: Grid Index Move (`M820`)

### Purpose
Move to indexed XY positions on a configurable grid.

### Config block
Configured in [Marlin/Configuration.h](../Marlin/Configuration.h):
- `GRID_INDEX_MOVE`
- `GRID_INDEX_X_OFFSET   0.0f`
- `GRID_INDEX_Y_OFFSET   0.0f`
- `GRID_INDEX_X_SPACING 50.0f`
- `GRID_INDEX_Y_SPACING 50.0f`
- `GRID_INDEX_X_COUNT 5`
- `GRID_INDEX_Y_COUNT 5`

### Indexing model
Row-major indexing:

$$index = y \cdot GRID\_INDEX\_X\_COUNT + x$$

Target coordinate:

$$X = GRID\_INDEX\_X\_OFFSET + x \cdot GRID\_INDEX\_X\_SPACING$$
$$Y = GRID\_INDEX\_Y\_OFFSET + y \cdot GRID\_INDEX\_Y\_SPACING$$

### Command syntax
- `M820 S<index> [F<mm/min>]`
  - `S` is required.
  - `F` is optional move feedrate override.

### Source integration
- Declaration: [Marlin/src/gcode/gcode.h](../Marlin/src/gcode/gcode.h)
- Dispatch case: [Marlin/src/gcode/gcode.cpp](../Marlin/src/gcode/gcode.cpp)
- Implementation: [Marlin/src/gcode/motion/M820.cpp](../Marlin/src/gcode/motion/M820.cpp)

### Example indices for current 5x5 config
- `M820 S0` → first point `(x=0, y=0)`
- `M820 S4` → end of first row
- `M820 S5` → start of second row
- `M820 S24` → final point

## 6) Custom Feature: FAN0 Pickup Guard

### Purpose
Limit the toolhead vacuum actuator on `FAN0` to pickup positions `0`, `1`, and `2`, and prevent XY motion away from the pickup point while `M810` is still running.

### Config block
Configured in [Marlin/Configuration.h](../Marlin/Configuration.h):
- `FAN0_PICKUP_GUARD`
- `FAN0_PICKUP_MIN_INDEX 0`
- `FAN0_PICKUP_MAX_INDEX 2`
- `FAN0_PICKUP_POSITION_TOLERANCE 1.0f`

### New source files
- [Marlin/src/feature/fan0_pickup_guard.h](../Marlin/src/feature/fan0_pickup_guard.h)
- [Marlin/src/feature/fan0_pickup_guard.cpp](../Marlin/src/feature/fan0_pickup_guard.cpp)

### Integration points
- [Marlin/src/gcode/temp/M106_M107.cpp](../Marlin/src/gcode/temp/M106_M107.cpp)
  - Blocks `M106 P0 S...` if the current XY position is not grid index `0`, `1`, or `2`.
- [Marlin/src/gcode/feature/macro/M810-M819.cpp](../Marlin/src/gcode/feature/macro/M810-M819.cpp)
  - Marks macro slot `0` (`M810`) as the guarded pickup cycle window.
- [Marlin/src/gcode/motion/G0_G1.cpp](../Marlin/src/gcode/motion/G0_G1.cpp)
  - Blocks XY moves while `M810` is active.
- [Marlin/src/gcode/motion/M820.cpp](../Marlin/src/gcode/motion/M820.cpp)
  - Applies the same motion lock to indexed grid moves.

### Runtime behavior
1. `FAN0` can only be turned on when the toolhead has actually reached grid positions `0`, `1`, or `2`.
2. Running `M810` starts a guarded pickup cycle and keeps motion locked until the macro returns.
3. `FAN0` remains on after `M810` if the macro leaves it on; only the movement lock is cleared when `M810` completes.

Before approving `M106 P0 S...`, the firmware now waits for queued motion to finish and samples the real stepper position, so a mid-move command won't drop the vacuum head early.

## 7) Operational Notes

- Save runtime tuning with `M500` when desired.
- If serial monitor access fails on Linux, ensure user belongs to `dialout` and reconnect session.
- Custom behavior depends on compile-time defines above; disabling those flags removes related features.
- For the team implementing external command sequencing, see [docs/GcodeSenderHandoff.md](GcodeSenderHandoff.md).

## 8) Files Touched for Custom Features

- [Marlin/Configuration.h](../Marlin/Configuration.h)
- [Marlin/Configuration_adv.h](../Marlin/Configuration_adv.h)
- [Marlin/src/feature/fan0_pickup_guard.h](../Marlin/src/feature/fan0_pickup_guard.h)
- [Marlin/src/feature/fan0_pickup_guard.cpp](../Marlin/src/feature/fan0_pickup_guard.cpp)
- [Marlin/src/feature/servo_motion_guard.h](../Marlin/src/feature/servo_motion_guard.h)
- [Marlin/src/feature/servo_motion_guard.cpp](../Marlin/src/feature/servo_motion_guard.cpp)
- [Marlin/src/gcode/control/M280.cpp](../Marlin/src/gcode/control/M280.cpp)
- [Marlin/src/gcode/feature/macro/M810-M819.cpp](../Marlin/src/gcode/feature/macro/M810-M819.cpp)
- [Marlin/src/gcode/motion/G0_G1.cpp](../Marlin/src/gcode/motion/G0_G1.cpp)
- [Marlin/src/gcode/gcode.h](../Marlin/src/gcode/gcode.h)
- [Marlin/src/gcode/gcode.cpp](../Marlin/src/gcode/gcode.cpp)
- [Marlin/src/gcode/motion/M820.cpp](../Marlin/src/gcode/motion/M820.cpp)
- [Marlin/src/gcode/temp/M106_M107.cpp](../Marlin/src/gcode/temp/M106_M107.cpp)
- [platformio.ini](../platformio.ini)
