#pragma once

#include <cassert>
#include <cstdint>

namespace math_api
{
// 0 for no fit
// 1 for fit the ratio of current screen
// 2 for fit the ratio of src screen
static void window_fit(int &dst_w, int &dst_h, int src_w, int src_h, int max_w, int max_h, uint8_t fit = 0)
{
  assert(src_w > 0 && src_h > 0);
  if ((max_w < 0 || max_h < 0) && fit == 1) fit = 2;

  dst_h = src_h;
  dst_w = src_w;
  switch (fit) {
  case 1:
    if (dst_h > max_h) {
      dst_h = max_h;
      dst_w = dst_h * max_w / max_h;
    }
    if (dst_w > max_w) {
      dst_w = max_w;
      dst_h = dst_w * max_h / max_w;
    }
    break;
  case 2:
    if (dst_h > max_h) {
      dst_h = max_h;
      dst_w = dst_h * src_w / src_h;
    }
    if (dst_w > max_w) {
      dst_w = max_w;
      dst_h = dst_w * src_h / src_w;
    }
    break;
  default:
    break;
  }
}

static bool point_in_area(int px, int py, int startx, int starty, int w, int h) 
{
  return px > startx && py > starty && px < startx + w && py < starty + h;
}

}  // namespace math_api
