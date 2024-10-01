#include "Arduino.h"
#include <WiFi.h>

#ifndef stickywifi_h
#define stickywifi_h

class StickyWiFi
{
public:
  StickyWiFi();
  wl_status_t init(char *ssid, char *passphrase);
  wl_status_t loop();
  const char *printStatus(wl_status_t status);
  int32_t statusColor();

private:
  char *_ssid;
  char *_passphrase;
  wl_status_t _status;
  long _lastReconnectAttempt;
};

#endif
