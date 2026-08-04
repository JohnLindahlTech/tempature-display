#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "Config.h"

class StickyWiFi
{
public:
  StickyWiFi();

  wl_status_t init(const char *ssid, const char *passphrase);

  // Services the connection and reconnects with a fixed backoff.
  wl_status_t loop();

  // True once NTP has set the system clock. Until then mbedTLS cannot check
  // the broker certificate's validity dates.
  bool timeSynced() const { return _timeSynced; }

  int32_t statusColor();
  const char *printStatus(wl_status_t status);

private:
  void onConnected();

  const char *_ssid;
  const char *_passphrase;
  wl_status_t _status;
  uint32_t _lastReconnectAttempt;
  bool _timeSynced;
};
