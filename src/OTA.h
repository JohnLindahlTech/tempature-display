#pragma once

#include <Arduino.h>
#include <functional>

// Over-the-air firmware updates.
//
// The partition table (default_16MB.csv) already has two 6.25 MB app slots and
// an otadata partition, so this needs no flash layout change - only the code
// to accept an image over the network.
//
// Shape matches StickyWiFi and MQTT: init() once, loop() every iteration.
class OTA
{
public:
  OTA();

  // Records the settings. The underlying service is not started until the
  // first loop() that sees a live WiFi association, because ArduinoOTA::begin()
  // needs an IP address to bind its listener and register mDNS.
  void init(const char *hostname, const char *password);

  // Services a pending update. Pass the current WiFi state; with no
  // association there is nothing to listen on and this returns immediately.
  void loop(bool wifiConnected);

  // True from the first received packet until the device reboots or the
  // transfer fails. While this holds, loop() in main.cpp skips its normal work.
  bool inProgress() const { return _inProgress; }

  // Fired once, before the first packet is written. Use it to shut the broker
  // connection down cleanly - see the note in OTA.cpp about the Last Will.
  void onStart(std::function<void()> callback);

  // Fired on whole-percent changes only. Redrawing per packet would slow the
  // transfer down measurably; the panel is a slow SPI device.
  void onProgress(std::function<void(uint8_t)> callback);

  // Fired if the update fails. On success the device reboots instead, so there
  // is no matching "finished" callback to hook.
  void onError(std::function<void(const char *)> callback);

private:
  void begin();

  const char *_hostname;
  const char *_password;
  bool _begun;
  bool _inProgress;
  uint8_t _lastPercent;

  std::function<void()> _onStart;
  std::function<void(uint8_t)> _onProgress;
  std::function<void(const char *)> _onError;
};
