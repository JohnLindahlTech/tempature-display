#pragma once

#include <Arduino.h>
#include "Config.h"

// The display is a 2x2 grid. Slots are numbered in reading order:
//
//   0 | 1
//   --+--
//   2 | 3
//
// which matches TEMPERATURE_0..TEMPERATURE_3.
#define SLOT_COUNT 4

class Printer
{
public:
  Printer();

  // Must be called after M5.begin(), it reads the panel dimensions.
  void begin();

  // Paints the grid background: four empty boxes in `color`.
  void clear(int32_t color);

  // Repaints one slot. Caller decides when something actually changed;
  // repainting is a visible flash, so don't call this on every message.
  void slot(uint8_t index, const char *name, const char *temperature, int32_t color);

  // Connection indicator in the middle of the grid. Redraws only on change.
  void dot(int32_t color);

  // Full-screen takeover, used while a firmware update is running. The grid is
  // meaningless at that point and the panel is the only progress indicator the
  // device has. Follow it with progress() to draw the bar.
  void banner(const char *title, const char *detail, int32_t color);

  // Fills the bar drawn by banner(). Only ever grows, so repeated calls are
  // cheap: each one paints just the newly completed sliver.
  void progress(uint8_t percent, int32_t color);

  // Forgets the last dot colour, so the next dot() repaints unconditionally.
  // Needed after a panel sleep/wake, where the pixels are gone but the colour
  // has not changed - without this the indicator would stay blank until the
  // connection status happened to change.
  void invalidateDot();

private:
  void origin(uint8_t index, int32_t &x, int32_t &y) const;
  void box(int32_t x, int32_t y, int32_t color);
  void text(const char *name, const char *temperature, int32_t x, int32_t y, int32_t color);

  int32_t _width;
  int32_t _height;
  int32_t _dotColor;
  int32_t _progressFilled; // px of the progress bar already painted
};
