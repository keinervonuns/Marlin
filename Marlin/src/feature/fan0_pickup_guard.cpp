#include "../inc/MarlinConfig.h"

#if ENABLED(FAN0_PICKUP_GUARD) && ENABLED(GRID_INDEX_MOVE)

#include "fan0_pickup_guard.h"

#include "../module/planner.h"

namespace fan0_pickup_guard {

  static bool m810_active = false;

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
    if (!m810_active) return true;

    if (near_position(motion.logical_x(dest.x), motion.logical_x(motion.position.x))
      && near_position(motion.logical_y(dest.y), motion.logical_y(motion.position.y)))
      return true;

    SERIAL_ERROR_MSG("Move blocked while M810 pickup cycle is active");
    return false;
  }

  void on_m810_start() {
    m810_active = true;
  }

  void on_m810_end() {
    m810_active = false;
  }

}

#endif