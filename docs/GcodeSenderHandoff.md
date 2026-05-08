# G-code Sender Handoff

This document is for the team implementing the logic that sends G-code to the machine.

It describes the firmware-side behavior that already exists, and the command sequencing assumptions that the sender should follow.

## Scope

This handoff covers:
- indexed XY positioning with `M820`
- pickup activation on `FAN0`
- guarded pickup cycle execution with `M810`
- related helper commands `M811`, `M812`, and `M813`

## Firmware Contract

The firmware currently guarantees the following behavior:

1. `M820 S<index>` moves the toolhead to a configured XY grid position.
2. `FAN0` activation is only allowed at grid positions `0`, `1`, and `2`.
3. If `M106 P0 S...` is sent while the machine is still moving, firmware waits for queued motion to finish first.
4. After motion completes, firmware samples the real stepper position before deciding whether `FAN0` may turn on.
5. While `M810` is executing, XY motion away from the pickup point is not allowed.
6. When `M810` is complete, the firmware clears the motion lock.
7. `FAN0` remains on after `M810` if the macro leaves it on.

## Grid Model

Current grid configuration:
- `GRID_INDEX_X_OFFSET = 0.0`
- `GRID_INDEX_Y_OFFSET = 0.0`
- `GRID_INDEX_X_SPACING = 50.0`
- `GRID_INDEX_Y_SPACING = 50.0`
- `GRID_INDEX_X_COUNT = 5`
- `GRID_INDEX_Y_COUNT = 5`

Row-major indexing:

$$index = y \cdot GRID\_INDEX\_X\_COUNT + x$$

Current allowed pickup indices:
- `0`
- `1`
- `2`

These are controlled in firmware by:
- `FAN0_PICKUP_MIN_INDEX = 0`
- `FAN0_PICKUP_MAX_INDEX = 2`

## Commands the Sender Can Use

### `M820 S<index> [F<mm/min>]`
Move to a grid position by index.

Examples:
- `M820 S0`
- `M820 S1`
- `M820 S2`
- `M820 S24 F12000`

Use this as the normal way to move between fixture positions.

### `M810`
Execute the pickup macro.

Current firmware startup definition:
- `M810 M106 P0 S255|M226 P108 S1|M226 P108 S0`

Meaning:
1. Turn on `FAN0` / vacuum air.
2. Wait for input `P108` to go high.
3. Wait for input `P108` to go low.

Operationally, this is treated as the guarded pickup cycle.

Important sender assumption:
- treat `M810` as synchronous
- do not assume pickup is complete until the firmware returns `ok`

### `M811`
Turn `FAN0` off.

Current firmware startup definition:
- `M811 M106 P0 S0`

### `M812`
Move servo 0 to the configured up position.

Current firmware startup definition:
- `M812 M280 P0 S10`

### `M813`
Move servo 0 to the configured down position.

Current firmware startup definition:
- `M813 M280 P0 S90`

## Required Sender Behavior

The sender logic should follow these rules:

1. Only attempt pickup at indices `0`, `1`, or `2`.
2. Move to the pickup location first, using `M820` or another completed XY move.
3. Wait for `ok` from the motion command before assuming the head is in place.
4. Send `M810` to run the pickup cycle.
5. Wait for `ok` from `M810` before sending any move away from the pickup point.
6. Do not rely on overlapping commands to save time during pickup; firmware intentionally serializes this for safety.
7. If a release / air-off step is needed later, use `M811` at the correct part of the process.

## Recommended Pickup Sequence

Recommended pattern for a pickup at grid index `1`:

1. `M820 S1`
2. Wait for `ok`
3. `M810`
4. Wait for `ok`
5. Continue with next move

Example:

```gcode
M820 S1
M810
M820 S10
```

The sender should still wait for `ok` between those commands, even if it internally pipelines commands.

## What Happens if Commands Are Sent Too Early

### `M106 P0 S255` sent during motion
Firmware behavior:
- waits for queued motion to finish
- reads the actual machine position from steppers
- only turns on `FAN0` if the final real position is one of the allowed pickup positions

This prevents the vacuum head from dropping mid-travel.

### Move command sent while `M810` is still running
Firmware behavior:
- rejects movement away from the current pickup point while the `M810` guarded cycle is active

Even though firmware blocks this, the sender should still avoid doing it.

## Sender-Side Error Handling

The sender should treat these as command failures / invalid operations:
- `FAN0 pickup only allowed at grid positions 0..2`
- `Move blocked while M810 pickup cycle is active`

Recommended response:
- stop the current step
- surface the firmware error to the operator / upstream controller
- do not silently retry with a different move unless your higher-level state machine explicitly allows it

## Practical Guidance for the Integration Team

- Use `M820` instead of raw `G1 X... Y...` whenever moving among known fixture positions.
- Use `M810` as the canonical pickup action.
- Treat `ok` after `M810` as the signal that the pickup sequence is complete and movement may resume.
- If release behavior is needed, explicitly model it with `M811` and any required motion.
- Keep the sender state machine simple: `move -> wait -> pickup -> wait -> move`.

## Current Firmware References

Primary references:
- [docs/Customizations.md](Customizations.md)
- [Marlin/Configuration.h](../Marlin/Configuration.h)
- [Marlin/Configuration_adv.h](../Marlin/Configuration_adv.h)
- [Marlin/src/feature/fan0_pickup_guard.cpp](../Marlin/src/feature/fan0_pickup_guard.cpp)
- [Marlin/src/gcode/motion/M820.cpp](../Marlin/src/gcode/motion/M820.cpp)
- [Marlin/src/gcode/feature/macro/M810-M819.cpp](../Marlin/src/gcode/feature/macro/M810-M819.cpp)
- [Marlin/src/gcode/temp/M106_M107.cpp](../Marlin/src/gcode/temp/M106_M107.cpp)
