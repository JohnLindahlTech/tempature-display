#include "Arduino.h"

#ifndef printer_h
#define printer_h

class Printer
{
public:
  Printer();
  void init(char *ul, char *ur, char *ll, char *lr, char *val, int32_t color);
  void base(int32_t color);

  void upperLeft(char *txt, char *temperature, int32_t color);
  void upperRight(char *txt, char *temperature, int32_t color);
  void lowerLeft(char *txt, char *temperature, int32_t color);
  void lowerRight(char *txt, char *temperature, int32_t color);
  void dot(int32_t color);
  void box(int32_t x, int32_t y, int32_t color);
  void text(char *txt, char *temperature, int32_t x, int32_t y, int32_t color);

private:
  int _height;
  int _width;
};

#endif
