#include "StickyWifi.h"
#include <WiFi.h>
#include <M5Unified.h>

const long backOffTimeout = 5000;

StickyWiFi::StickyWiFi()
{
  _status = WL_IDLE_STATUS;
  _lastReconnectAttempt = 0;
}

wl_status_t StickyWiFi::init(char *ssid, char *passphrase)
{
  _ssid = ssid;
  _passphrase = passphrase;
  WiFi.disconnect();
  _lastReconnectAttempt = millis();
  _status = WiFi.begin(_ssid, _passphrase);
  return _status;
};

wl_status_t StickyWiFi::loop()
{
  long now = millis();
  _status = WiFi.status();
  if (_status == WL_CONNECTED)
  {
    // Connected
    _lastReconnectAttempt = 0;
  }
  else if (now - _lastReconnectAttempt > backOffTimeout)
  {
    // Not connected, and enough time has passed to make another try.
    // Lets disconned to clean up
    WiFi.disconnect();
    _lastReconnectAttempt = now;
    _status = WiFi.begin(_ssid, _passphrase);
  }

  return _status;
}

int32_t StickyWiFi::statusColor()
{
  int status = WiFi.status();
  switch (status)
  {
  case WL_IDLE_STATUS:
    return TFT_WHITE;
  case WL_NO_SSID_AVAIL:
    return TFT_BROWN;
  case WL_SCAN_COMPLETED:
    return TFT_DARKGREEN;
  case WL_CONNECTED:
    return TFT_GREEN;
  case WL_CONNECT_FAILED:
    return TFT_RED;
  case WL_CONNECTION_LOST:
    return TFT_RED;
  case WL_DISCONNECTED:
    return TFT_RED;
  default:
    return TFT_MAGENTA;
  }
}

const char *StickyWiFi::printStatus(wl_status_t status)
{
  switch (status)
  {
  case WL_IDLE_STATUS:
    return "Wifi is Idle";
  case WL_NO_SSID_AVAIL:
    return "Wifi no ssid available";
  case WL_SCAN_COMPLETED:
    return "Wifi scan completed";
  case WL_CONNECTED:
    return "Wifi is connected";
  case WL_CONNECT_FAILED:
    return "Wifi connection failed";
  case WL_CONNECTION_LOST:
    return "Wifi connection lost";
  case WL_DISCONNECTED:
    return "Wifi is Disconnected";
  default:
    return "Wifi is unknown state";
  }
}
