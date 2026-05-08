#include "../inc/MarlinConfig.h"

#if ENABLED(SERVO0_MOTION_GUARD)

#include "servo_motion_guard.h"

#include "../gcode/gcode.h"
#include "../module/motion.h"
#include "../module/planner.h"

namespace servo_motion_guard {

  static bool servo0_is_down = false;
  static bool pending_move = false;
  static xyze_pos_t pending_destination;
  static feedRate_t pending_feedrate = 0;

  static inline bool in_blocked_zone(const float x, const float y) {
    return WITHIN(x, SERVO0_BLOCKED_X_MIN, SERVO0_BLOCKED_X_MAX)
        && WITHIN(y, SERVO0_BLOCKED_Y_MIN, SERVO0_BLOCKED_Y_MAX);
  }

  bool allow_servo0_set(const int angle) {
    if (angle >= SERVO0_DOWN_ANGLE) {
      // If a move is in progress (or queued), wait first so this check uses
      // the real final toolhead location, not an in-flight state.
      planner.synchronize();

      if (in_blocked_zone(motion.current_position.x, motion.current_position.y)) {
        // Auto-relocate to the wait point before allowing servo-down.
        motion.destination = motion.current_position;
        motion.destination.x = SERVO0_WAIT_POS_X;
        motion.destination.y = SERVO0_WAIT_POS_Y;
        SERIAL_ECHOLNPGM("Servo down: moving to waiting position first");
        motion.prepare_line_to_destination();
        planner.synchronize();
      }
    }
    return true;
  }

  bool move_redirected(xyze_pos_t &dest, const feedRate_t fr_mm_s) {
    if (!servo0_is_down || !in_blocked_zone(dest.x, dest.y)) return false;

    pending_destination = dest;
    pending_feedrate = fr_mm_s;
    pending_move = true;

    dest = motion.current_position;
    dest.x = SERVO0_WAIT_POS_X;
    dest.y = SERVO0_WAIT_POS_Y;

    SERIAL_ECHOLNPGM("Redirected to waiting position");
    return true;
  }

  void on_servo0_set(const int angle) {
    if (angle >= SERVO0_DOWN_ANGLE) {
      servo0_is_down = true;
      return;
    }

    if (angle <= SERVO0_UP_ANGLE) {
      servo0_is_down = false;

      if (pending_move) {
        motion.destination = pending_destination;
        if (pending_feedrate > 0) motion.feedrate_mm_s = pending_feedrate;
        pending_move = false;
        SERIAL_ECHOLNPGM("Resuming redirected move");
        motion.prepare_line_to_destination();
      }
    }
  }

}

#endif
