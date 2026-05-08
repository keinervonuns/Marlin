#pragma once

#include "../inc/MarlinConfigPre.h"

#if ENABLED(SERVO0_MOTION_GUARD)

#include "../module/motion.h"

namespace servo_motion_guard {

  bool move_redirected(xyze_pos_t &dest, const feedRate_t fr_mm_s);
  bool allow_servo0_set(const int angle);
  void on_servo0_set(const int angle);

}

#endif
