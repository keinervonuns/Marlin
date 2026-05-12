#include "../inc/MarlinConfig.h"

#if ENABLED(FAN0_PICKUP_GUARD) && ENABLED(GRID_INDEX_MOVE)

#include "fan0_pickup_guard.h"

#include "../HAL/shared/Delay.h"
#include "../module/planner.h"
#include "../module/stepper.h"

namespace fan0_pickup_guard {

  constexpr millis_t PICKUP_LATCH_GRACE_MS = 800;

  static bool pickup_cycle_active = false;
  static bool pickup_release_pending = false;
  static bool movement_latched = false;
  static bool sensor_initialized = false;
  static bool latch_reported = false;
  static millis_t ignore_latch_until_ms = 0;

  static inline void ensure_sensor_initialized() {
    if (sensor_initialized) return;
    SET_INPUT(FAN0_PICKUP_SENSOR_PIN);
    sensor_initialized = true;
  }

  static inline bool pickup_sensor_active() {
    ensure_sensor_initialized();
    return READ(FAN0_PICKUP_SENSOR_PIN) == FAN0_PICKUP_SENSOR_ACTIVE_STATE;
  }

  static void latch_motion_lock() {
    if (movement_latched) return;

    movement_latched = true;
    stepper.quick_stop();
    planner.quick_stop();

    if (!latch_reported) {
      SERIAL_ERROR_MSG("Pickup safety latch triggered on sensor pin. Motion locked until board restart.");
      latch_reported = true;
    }
  }

  static inline bool near_position(const float a, const float b) {
    return ABS(a - b) <= FAN0_PICKUP_POSITION_TOLERANCE;
  }

  static bool at_grid_index(const int32_t index) {
    const int32_t x_count = GRID_INDEX_X_COUNT;
    const int32_t gx = index % x_count,
                  gy = index / x_count;

    const float tx = GRID_INDEX_X_OFFSET + gx * GRID_INDEX_X_SPACING,
                ty = GRID_INDEX_Y_OFFSET + gy * GRID_INDEX_Y_SPACING;

    return near_position(motion.logical_x(motion.position.x), tx)
      && near_position(motion.logical_y(motion.position.y), ty);
  }

  static bool at_allowed_pickup_position() {
    const int32_t total = GRID_INDEX_X_COUNT * GRID_INDEX_Y_COUNT;
    for (int32_t index = FAN0_PICKUP_MIN_INDEX; index <= FAN0_PICKUP_MAX_INDEX; ++index) {
      if (WITHIN(index, 0, total - 1) && at_grid_index(index)) return true;
    }
    return false;
  }

  bool allow_fan0_speed(const uint16_t speed) {
    monitor_and_latch();
    if (movement_latched) return false;

    // In this hardware profile, pickup state is FAN0 S0.
    // Guard only that state to indices FAN0_PICKUP_MIN_INDEX..MAX_INDEX.
    if (speed == 0) {
      planner.synchronize();
      motion.set_current_from_steppers_for_axis(ALL_AXES_ENUM);
    }

    if (speed != 0 || at_allowed_pickup_position()) return true;

    SERIAL_ERROR_MSG(
      "FAN0 pickup only allowed at grid positions ",
      FAN0_PICKUP_MIN_INDEX,
      "..",
      FAN0_PICKUP_MAX_INDEX
    );
    return false;
  }

  bool move_allowed(const xyze_pos_t &dest) {
    monitor_and_latch();

    if (movement_latched) {
      SERIAL_ERROR_MSG("Move blocked: pickup safety latch is active (restart board to clear)");
      return false;
    }

    if (pickup_release_pending) {
      if (pickup_sensor_active()) {
        SERIAL_ERROR_MSG("Move blocked: waiting for pickup sensor to return idle after grab");
        return false;
      }

      pickup_release_pending = false;
      pickup_cycle_active = false;
      ignore_latch_until_ms = millis() + PICKUP_LATCH_GRACE_MS;
    }

    if (!pickup_cycle_active) return true;

    if (near_position(motion.logical_x(dest.x), motion.logical_x(motion.position.x))
      && near_position(motion.logical_y(dest.y), motion.logical_y(motion.position.y)))
      return true;

    SERIAL_ERROR_MSG("Move blocked while M810 pickup cycle is active");
    return false;
  }

  void monitor_and_latch() {
    const millis_t now = millis();

    // During pickup sequence, sensor activity is expected.
    // Suppress latching while M810 runs, while waiting for the pickup sensor
    // to return to idle after a grab, shortly after that transition, and while
    // parked at allowed pickup indices.
    if (movement_latched || pickup_cycle_active || pickup_release_pending || !ELAPSED(now, ignore_latch_until_ms) || at_allowed_pickup_position()) return;
    if (!pickup_sensor_active()) return;

    // Re-sync position before latching so pickup-position checks use the true
    // instantaneous coordinates and not stale planner state.
    motion.set_current_from_steppers_for_axis(ALL_AXES_ENUM);
    if (pickup_cycle_active || pickup_release_pending || at_allowed_pickup_position()) return;

    latch_motion_lock();
  }

  bool latched() {
    return movement_latched;
  }

  void on_m810_start() {
    pickup_cycle_active = true;
    pickup_release_pending = false;
    ignore_latch_until_ms = millis() + PICKUP_LATCH_GRACE_MS;
  }

  void on_m810_end() {
    pickup_release_pending = true;
    ignore_latch_until_ms = millis() + PICKUP_LATCH_GRACE_MS;
  }

}

#endif