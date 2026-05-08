#pragma once

#include "../inc/MarlinConfigPre.h"

#if ENABLED(FAN0_PICKUP_GUARD) && ENABLED(GRID_INDEX_MOVE)

#include "../module/motion.h"

namespace fan0_pickup_guard {

  bool allow_fan0_speed(const uint16_t speed);
  bool move_allowed(const xyze_pos_t &dest);
  void on_m810_start();
  void on_m810_end();

}

#endif