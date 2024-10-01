#include "Printer.h"
#include <M5GFX.h>
#include <M5Unified.h>

const int framePadding = 5;
const int frameRadius = 10;
const int textTopPadding = framePadding * 2;

Printer::Printer()
{
}

void Printer::init(char *ul, char *ur, char *ll, char *lr, char *val, int32_t color)
{
  _width = M5.Display.width();
  _height = M5.Display.height();
  upperLeft(ul, val, color);
  upperRight(ur, val, color);
  lowerLeft(ll, val, color);
  lowerRight(lr, val, color);
};

void Printer::base(int32_t color)
{
  char *empty = (char *)"";
  init(empty, empty, empty, empty, empty, color);
}

void Printer::upperLeft(char *txt, char *temperature, int32_t color)
{
  int x = 0;
  int y = 0;
  box(x, y, color);
  text(txt, temperature, x, y, color);
};

void Printer::upperRight(char *txt, char *temperature, int32_t color)
{
  int x = _width / 2;
  int y = 0;
  box(x, y, color);
  text(txt, temperature, x, y, color);
};

void Printer::lowerLeft(char *txt, char *temperature, int32_t color)
{
  int x = 0;
  int y = _height / 2;
  box(x, y, color);
  text(txt, temperature, x, y, color);
};

void Printer::lowerRight(char *txt, char *temperature, int32_t color)
{
  int x = _width / 2;
  int y = _height / 2;
  box(x, y, color);
  text(txt, temperature, x, y, color);
};

void Printer::dot(int32_t color)
{
  M5.Display.fillCircle(_width / 2, _height / 2, frameRadius / 2, color);
};

void Printer::box(int32_t x, int32_t y, int32_t color)
{
  int32_t xPadded = x + framePadding;
  int32_t yPadded = y + framePadding;
  int32_t w = _width / 2 - 2 * framePadding;
  int32_t h = _height / 2 - 2 * framePadding;

  M5.Display.fillRoundRect(xPadded, yPadded, w, h, frameRadius, M5.Display.getBaseColor());
  M5.Display.drawRoundRect(xPadded, yPadded, w, h, frameRadius, color);
}

void Printer::text(char *txt, char *temperature, int32_t x, int32_t y, int32_t color)
{
  int xPadded = x + (_width / 4);
  int yTxt = y + textTopPadding;
  int yTmp = y + (_height / 4);
  M5.Display.setTextColor(color);
  M5.Display.setTextDatum(TC_DATUM);
  M5.Display.drawString(txt, xPadded, yTxt, &FreeSans12pt7b);
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.drawString(temperature, xPadded, yTmp, &FreeSansBold24pt7b);
}
