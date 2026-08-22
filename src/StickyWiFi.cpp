#include "StickyWiFi.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <time.h>
#include "Log.h"

// Anything past this means the clock has been set by SNTP rather than left at
// the 1970 epoch the ESP32 boots with.
static const time_t SANE_EPOCH = 1700000000; // 2023-11-14

StickyWiFi::StickyWiFi()
    : _ssid(nullptr),
      _passphrase(nullptr),
      _status(WL_IDLE_STATUS),
      _lastReconnectAttempt(0),
      _timeSynced(false),
      _timeRequested(false)
{
}

wl_status_t StickyWiFi::init(const char *ssid, const char *passphrase)
{
  _ssid = ssid;
  _passphrase = passphrase;
  LOG("wifi: connecting to %s", _ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect();
  _lastReconnectAttempt = millis();
  _status = WiFi.begin(_ssid, _passphrase);
  return _status;
}

void StickyWiFi::onConnected()
{
  if (_timeSynced)
  {
    return;
  }

  time_t now = time(nullptr);
  if (now > SANE_EPOCH)
  {
    _timeSynced = true;
    LOG("wifi: clock synced (epoch %ld)", (long)now);
    return;
  }

  // Kick SNTP off exactly once per association and let loop() notice when the
  // clock lands. configTime() does sntp_stop() + sntp_init() internally, so
  // calling it every iteration restarts the query before any reply arrives -
  // the clock would never sync at all.
  if (_timeRequested)
  {
    return;
  }
  _timeRequested = true;

  // Without this the device sits at 1970 and mbedTLS cannot meaningfully check
  // the broker certificate's notBefore/notAfter dates.
  LOG("wifi: requesting time from %s", NTP_SERVER);
  configTime(0, 0, NTP_SERVER);
}

wl_status_t StickyWiFi::loop()
{
  wl_status_t previous = _status;
  _status = WiFi.status();

  if (_status == WL_CONNECTED)
  {
    if (previous != WL_CONNECTED)
    {
      LOG("wifi: connected, ip %s, rssi %d dBm",
          WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
    }
    _lastReconnectAttempt = 0;
    onConnected();
    return _status;
  }

  // The drop itself is worth a line. Without this the first sign of trouble is
  // the retry below, up to WIFI_RETRY_INTERVAL_MS later, which makes a brief
  // wobble and a real outage look identical in the log.
  if (previous == WL_CONNECTED)
  {
    LOG("wifi: link lost (%s)", printStatus(_status));
  }

  // Unsigned, so the subtraction below stays correct across the millis()
  // rollover at ~49 days of uptime.
  uint32_t now = millis();
  if ((now - _lastReconnectAttempt) > WIFI_RETRY_INTERVAL_MS)
  {
    // Not connected, and enough time has passed to make another try.
    // Disconnect first to clean up.
    LOG("wifi: %s, retrying", printStatus(_status));
    WiFi.disconnect();
    // Re-request the time after reassociating; the previous query died with
    // the old association. Already-synced clocks stay synced.
    _timeRequested = false;
    _lastReconnectAttempt = now;
    _status = WiFi.begin(_ssid, _passphrase);
  }

  return _status;
}

int32_t StickyWiFi::statusColor()
{
  switch (WiFi.status())
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
  case WL_CONNECTION_LOST:
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
