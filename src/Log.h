#pragma once

#include <Arduino.h>
#include "Config.h"

// Serial logging, plus an encrypted live stream for anyone attached over the
// network. Previously the only way to see what the firmware was doing was the
// colour of a 5px dot, which made anything network-related painful to diagnose.
// LOG_* compiles to nothing when DEBUG is 0.
//
// This used to stamp __DATE__ / __TIME__ at boot. Those freeze when their
// including translation unit is compiled, so editing any other .cpp left the
// boot line reporting the previous build - a stamp that is right most of the
// time, which is worse than none. main.cpp logs firmwareVersion() instead; see
// src/Version.h.

#if DEBUG

#define LOG_BEGIN() logBegin()
#define LOG(fmt, ...) logPrintf(fmt, ##__VA_ARGS__)
// Call every iteration with the current WiFi state; starts the listener while
// associated and tears the session down when the association goes away.
#define LOG_NETWORK(connected) logNetwork(connected)

void logBegin();
// The format attribute makes the compiler check LOG() arguments the same way it
// checks printf, which is worth having on a device with no debugger.
void logPrintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
void logNetwork(bool wifiConnected);

#else

#define LOG_BEGIN() \
  do                \
  {                 \
  } while (0)
#define LOG(fmt, ...) \
  do                  \
  {                   \
  } while (0)
#define LOG_NETWORK(connected) \
  do                           \
  {                            \
  } while (0)

#endif
