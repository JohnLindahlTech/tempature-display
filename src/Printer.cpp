#include "Printer.h"
#include <M5GFX.h>
#include <M5Unified.h>

static const int32_t framePadding = 5;
static const int32_t frameRadius = 10;
static const int32_t textTopPadding = framePadding * 2;

// -1 is not a valid 16/24-bit colour, so the first dot() always paints.
Printer::Printer() : _width(0), _height(0), _dotColor(-1), _progressFilled(0) {}

void Printer::begin()
{
  _width = M5.Display.width();
  _height = M5.Display.height();
}

void Printer::clear(int32_t color)
{
  for (uint8_t i = 0; i < SLOT_COUNT; i += 1)
  {
    slot(i, "", "", color);
  }
}

void Printer::origin(uint8_t index, int32_t &x, int32_t &y) const
{
  x = (index % 2) * (_width / 2);
  y = (index / 2) * (_height / 2);
}

void Printer::slot(uint8_t index, const char *name, const char *temperature, int32_t color)
{
  if (index >= SLOT_COUNT)
  {
    return;
  }

  int32_t x = 0;
  int32_t y = 0;
  origin(index, x, y);

  // Hold the SPI transaction across the erase + redraw so the panel doesn't
  // show a half-drawn box.
  M5.Display.startWrite();
  box(x, y, color);
  text(name, temperature, x, y, color);
  M5.Display.endWrite();
}

void Printer::dot(int32_t color)
{
  if (color == _dotColor)
  {
    return;
  }
  _dotColor = color;
  M5.Display.fillCircle(_width / 2, _height / 2, frameRadius / 2, color);
}

void Printer::invalidateDot()
{
  _dotColor = -1;
}

void Printer::box(int32_t x, int32_t y, int32_t color)
{
  int32_t xPadded = x + framePadding;
  int32_t yPadded = y + framePadding;
  int32_t w = _width / 2 - 2 * framePadding;
  int32_t h = _height / 2 - 2 * framePadding;

  M5.Display.fillRoundRect(xPadded, yPadded, w, h, frameRadius, M5.Display.getBaseColor());
  M5.Display.drawRoundRect(xPadded, yPadded, w, h, frameRadius, color);
}

void Printer::text(const char *name, const char *temperature, int32_t x, int32_t y, int32_t color)
{
  int32_t xCentre = x + (_width / 4);
  int32_t yName = y + textTopPadding;
  int32_t yTemperature = y + (_height / 4);

  M5.Display.setTextColor(color);
  M5.Display.setTextDatum(TC_DATUM);
  M5.Display.drawString(name, xCentre, yName, &FreeSans12pt7b);
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.drawString(temperature, xCentre, yTemperature, &FreeSansBold24pt7b);
}

// --- Firmware update screen ------------------------------------------------

static const int32_t barHeight = 24;

void Printer::banner(const char *title, const char *detail, int32_t color)
{
  M5.Display.startWrite();
  M5.Display.fillScreen(M5.Display.getBaseColor());

  M5.Display.setTextColor(color);
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.drawString(title, _width / 2, _height / 3, &FreeSansBold24pt7b);
  M5.Display.drawString(detail, _width / 2, _height / 3 + 40, &FreeSans12pt7b);

  int32_t x = framePadding * 2;
  int32_t y = _height - framePadding * 2 - barHeight;
  M5.Display.drawRect(x, y, _width - framePadding * 4, barHeight, color);
  M5.Display.endWrite();

  _progressFilled = 0;

  // The grid is gone, so the cached dot colour no longer describes the panel.
  invalidateDot();
}

void Printer::progress(uint8_t percent, int32_t color)
{
  if (percent > 100)
  {
    percent = 100;
  }

  int32_t x = framePadding * 2;
  int32_t y = _height - framePadding * 2 - barHeight;
  int32_t inner = _width - framePadding * 4 - 4;
  int32_t target = (inner * percent) / 100;

  if (target <= _progressFilled)
  {
    return;
  }

  // Paint only the new sliver. Redrawing the whole bar every percent would add a
  // visible flicker and slow the transfer.
  M5.Display.fillRect(x + 2 + _progressFilled, y + 2,
                      target - _progressFilled, barHeight - 4, color);
  _progressFilled = target;
}
