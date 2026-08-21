#pragma once

#include <Arduino.h>
#include "Config.h"

// Serial logging. Previously the only way to see what the firmware was doing
// was the colour of a 5px dot, which made anything network-related painful to
// diagnose. LOG_* compiles to nothing when DEBUG is 0.
//
// This used to stamp __DATE__ / __TIME__ here. Those freeze when THIS file's
// including translation unit is compiled, so editing any other .cpp left the
// boot line reporting the previous build - a stamp that is right most of the
// time, which is worse than none. main.cpp logs firmwareVersion() instead; see
// src/Version.h.

#if DEBUG

#define LOG_BEGIN()             \
  do                            \
  {                             \
    Serial.begin(SERIAL_BAUD);  \
    delay(50);                  \
    Serial.println();           \
    Serial.println("[boot]");                                 \
  } while (0)

#define LOG(fmt, ...)                                       \
  do                                                        \
  {                                                         \
    Serial.printf("[%8lu] " fmt "\n", (unsigned long)millis(), ##__VA_ARGS__); \
  } while (0)

#else

#define LOG_BEGIN() \
  do                \
  {                 \
  } while (0)
#define LOG(fmt, ...) \
  do                  \
  {                   \
  } while (0)

#endif
