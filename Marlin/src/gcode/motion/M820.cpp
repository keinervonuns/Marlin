/**
 * Marlin 3D Printer Firmware
 */

#include "../../inc/MarlinConfig.h"

#if ENABLED(GRID_INDEX_MOVE)

#include "../../MarlinCore.h"
#include "../gcode.h"
#include "../../feature/fan0_pickup_guard.h"
#include "../../module/motion.h"

/**
 * M820: Move to configured XY grid index
 *
 *   S<index>   Required. 0 .. (GRID_INDEX_X_COUNT * GRID_INDEX_Y_COUNT - 1)
 *   F<mm/min>  Optional move feedrate
 */
void GcodeSuite::M820() {
  if (!MOTION_CONDITIONS) return;

  if (!parser.seenval('S')) {
    SERIAL_ERROR_MSG("M820 requires S<index>");
    return;
  }

  const int32_t index = parser.value_long(),
                x_count = GRID_INDEX_X_COUNT,
                y_count = GRID_INDEX_Y_COUNT,
                total = x_count * y_count;

  if (index < 0 || index >= total) {
    SERIAL_ERROR_MSG("M820 index out of range (0..", total - 1, ")");
    return;
  }

  const int32_t gx = index % x_count,
                gy = index / x_count;

  const float tx = GRID_INDEX_X_OFFSET + gx * GRID_INDEX_X_SPACING,
              ty = GRID_INDEX_Y_OFFSET + gy * GRID_INDEX_Y_SPACING;

  motion.destination = motion.current_position;
  motion.destination.x = motion.raw_x(tx);
  motion.destination.y = motion.raw_y(ty);

  #if ENABLED(FAN0_PICKUP_GUARD)
    if (!fan0_pickup_guard::move_allowed(motion.destination)) return;
  #endif

  if (parser.seenval('F'))
    motion.feedrate_mm_s = parser.value_feedrate();

  motion.prepare_line_to_destination();
}

#endif // GRID_INDEX_MOVE
