#include "OTA.h"
#include <ArduinoOTA.h>
#include "Config.h"
#include "Log.h"

OTA::OTA()
    : _hostname(nullptr),
      _password(nullptr),
      _begun(false),
      _inProgress(false),
      _lastPercent(0)
{
}

void OTA::init(const char *hostname, const char *password)
{
  _hostname = hostname;
  _password = password;
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
}
