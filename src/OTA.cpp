#include "OTA.h"
#include <ArduinoOTA.h>
#include <esp_ota_ops.h>
#include "Config.h"
#include "Log.h"

// Keep the pending rollback alive past initArduino().
//
// This is a weak symbol in the Arduino core (esp32-hal-misc.c). Left alone, the
// core calls esp_ota_mark_app_valid_cancel_rollback() from initArduino() -
// which runs BEFORE setup() - so a freshly flashed image is marked permanently
// good before a single line of our code executes, and a panic in setup() would
// boot-loop forever with rollback never firing.
//
// extern "C" is load-bearing: the core declares this in a .c file, so a C++
// definition would mangle its name, silently fail to override the weak symbol,
// and leave the old behaviour in place with nothing to show for it.
extern "C" bool verifyRollbackLater()
{
  return true;
}

OTA::OTA()
    : _hostname(nullptr),
      _password(nullptr),
      _begun(false),
      _rollbackPending(false),
      _inProgress(false),
      _lastPercent(0)
{
}

void OTA::init(const char *hostname, const char *password)
{
  _hostname = hostname;
  _password = password;

  // Only true on the first boot after an OTA push. A normal reboot runs with
  // the image already marked valid, and a serial flash never sets the state at
  // all, so on both of those this is false and confirmProgress() does nothing.
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  _rollbackPending = (esp_ota_get_state_partition(running, &state) == ESP_OK &&
                      state == ESP_OTA_IMG_PENDING_VERIFY);

  if (_rollbackPending)
  {
    LOG("ota: image on trial, confirming after %lums of uptime",
        (unsigned long)OTA_VALIDATE_AFTER_MS);
  }
}

void OTA::confirmImage()
{
  // The bar is "can this image still be updated remotely", not "is everything
  // working". Rollback exists to recover firmware that cannot be fixed over the
  // air, so a live OTA listener is the whole test - gating on the broker
  // instead would roll a perfectly good image back during a routine broker
  // outage, which is worse than the problem being solved.
  if (!_rollbackPending || !_begun)
  {
    return;
  }

  // Grace period: survive a while before vouching for the image, so firmware
  // that comes up cleanly and then panics a few seconds into loop() still
  // reboots with the rollback armed. Nothing is blocked while this runs - the
  // display and the broker work normally; only the otadata write is deferred.
  if (millis() < OTA_VALIDATE_AFTER_MS)
  {
    return;
  }

  _rollbackPending = false;
  if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
  {
    LOG("ota: image confirmed, rollback cancelled");
  }
  else
  {
    LOG("ota: WARNING failed to confirm image, it will roll back on reboot");
  }
}

void OTA::onStart(std::function<void()> callback)
{
  _onStart = callback;
}

void OTA::onProgress(std::function<void(uint8_t)> callback)
{
  _onProgress = callback;
}

void OTA::onError(std::function<void(const char *)> callback)
{
  _onError = callback;
}

void OTA::begin()
{
  ArduinoOTA.setHostname(_hostname);
  ArduinoOTA.setPort(OTA_PORT);

  // An empty password means an unauthenticated listener on the LAN. Config.h
  // already emits a build warning; this is the runtime half of it.
  if (_password != nullptr && _password[0] != '\0')
  {
    ArduinoOTA.setPassword(_password);
  }
  else
  {
    LOG("ota: WARNING no password set, updates are unauthenticated");
  }

  ArduinoOTA.onStart([this]() {
    _inProgress = true;
    _lastPercent = 255; // Forces the first onProgress through the != check.
    LOG("ota: update started");
    if (_onStart)
    {
      _onStart();
    }
  });

  ArduinoOTA.onProgress([this](unsigned int done, unsigned int total) {
    if (total == 0)
    {
      return;
    }
    uint8_t percent = (uint8_t)((done * 100UL) / total);
    if (percent == _lastPercent)
    {
      return;
    }
    _lastPercent = percent;
    if (_onProgress)
    {
      _onProgress(percent);
    }
  });

  ArduinoOTA.onEnd([this]() {
    // The reboot happens inside ArduinoOTA once this returns, so this is the
    // last code that runs on the old firmware.
    LOG("ota: update complete, rebooting");
  });

  ArduinoOTA.onError([this](ota_error_t error) {
    _inProgress = false;
    const char *text = "unknown";
    switch (error)
    {
    case OTA_AUTH_ERROR:
      text = "auth failed";
      break;
    case OTA_BEGIN_ERROR:
      text = "begin failed";
      break;
    case OTA_CONNECT_ERROR:
      text = "connect failed";
      break;
    case OTA_RECEIVE_ERROR:
      text = "receive failed";
      break;
    case OTA_END_ERROR:
      text = "end failed";
      break;
    }
    LOG("ota: error - %s (%u)", text, (unsigned)error);
    if (_onError)
    {
      _onError(text);
    }
  });

  ArduinoOTA.begin();
  _begun = true;
  LOG("ota: listening as %s on port %u", _hostname, (unsigned)OTA_PORT);
}

void OTA::loop(bool wifiConnected)
{
  if (!wifiConnected)
  {
    return;
  }

  // Started lazily, and only once. ArduinoOTA keeps its socket across a WiFi
  // drop and reassociation, so calling begin() again on every reconnect would
  // re-register mDNS for no benefit.
  if (!_begun)
  {
    begin();
  }

  ArduinoOTA.handle();
  confirmImage();
}
