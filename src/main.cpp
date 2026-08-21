#include <M5Unified.h>
#include <SSLClient.h>
#include <WiFi.h>
#include "Config.h"
#include "Log.h"
#include "MQTT.h"
#include "OTA.h"
#include "Printer.h"
#include "State.h"
#include "StickyWiFi.h"
#include "Version.h"

Printer printer;
State state(&printer);
StickyWiFi swifi;

WiFiClient wifi_transport_layer;
SSLClient secure_presentation_layer(&wifi_transport_layer);
MQTT mqtt(secure_presentation_layer);
OTA ota;

// Centre dot: a traffic light for faults, a blip for activity.
//
// While connected and idle it is painted in the base colour, i.e. invisible.
// Every inbound message flashes it green for ACTIVITY_BLIP_MS. Anything wrong
// with WiFi or the broker shows as a persistent fault colour instead.
static uint32_t activityAt = 0;
static bool activityPending = false;

void mqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  activityAt = millis();
  activityPending = true;
  state.update(topic, payload, length);
}

static void paintDot(bool wifiConnected, bool mqttConnected)
{
  if (!wifiConnected)
  {
    printer.dot(swifi.statusColor());
    return;
  }
  if (!mqttConnected)
  {
    printer.dot(mqtt.statusColor());
    return;
  }

  // Unsigned subtraction, so this survives the millis() rollover.
  if (activityPending && (millis() - activityAt) < ACTIVITY_BLIP_MS)
  {
    printer.dot(TFT_GREEN);
    return;
  }

  activityPending = false;
  printer.dot(M5.Display.getBaseColor());
}

void setup()
{
  LOG_BEGIN();
  LOG("firmware %s", firmwareVersion());

  auto cfg = M5.config();
  cfg.led_brightness = 0;
  M5.begin(cfg);

  printer.begin();
  printer.clear(TFT_GOLD);
  printer.dot(TFT_WHITE);

  secure_presentation_layer.setCACert(CA_CERT);

  mqtt.init(MQTT_SERVER, MQTT_PORT, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
  mqtt.setCallback(mqttCallback);
  for (uint8_t i = 0; i < SLOT_COUNT; i += 1)
  {
    mqtt.subscribe(TEMPERATURE_TOPICS[i]);
  }
  mqtt.subscribe(WAKE);
  mqtt.subscribe(SLEEP);

  swifi.init(WIFI_SSID, WIFI_PASSPHRASE);

#if OTA_ENABLED
  ota.init(OTA_HOSTNAME, OTA_PASSWORD);

  ota.onStart([]() {
    // Writing the image blocks this loop for far longer than
    // MQTT_KEEPALIVE_SECONDS, so the broker would otherwise decide we died and
    // fire the Last Will. Say "offline" deliberately and disconnect cleanly, so
    // an update looks like an update rather than a crash.
    mqtt.publish(AVAILABILITY_TOPIC, AVAILABILITY_OFFLINE, true);
    mqtt.forceReconnect();

    M5.Display.wakeup();
    printer.banner("Updating", "do not power off", TFT_GOLD);
  });

  ota.onProgress([](uint8_t percent) {
    printer.progress(percent, TFT_GREEN);
  });

  ota.onError([](const char *reason) {
    printer.banner("Update failed", reason, TFT_RED);
    delay(3000);
    // Back to the temperature grid; the values themselves were never lost.
    printer.clear(TFT_GOLD);
    state.redraw();
  });
#endif
}

void loop()
{
  delay(1);
  M5.update();

  // --- Connectivity --------------------------------------------------------
  wl_status_t status = swifi.loop();
  bool wifiConnected = (status == WL_CONNECTED);

#if OTA_ENABLED
  ota.loop(wifiConnected);
  if (ota.inProgress())
  {
    // The panel belongs to the update screen and the broker connection is
    // already closed. Nothing else should touch either until we reboot.
    return;
  }
#endif

  bool mqttConnected = false;
  if (wifiConnected)
  {
    // The broker replays every retained topic when we resubscribe, so a fresh
    // connection repopulates the screen on its own. No update request needed.
    // justConnected fires on the call that (re)established the session, which
    // is the moment to restate anything the broker should hold retained.
    bool justConnected = false;
    mqttConnected = mqtt.loop(&justConnected);
    if (justConnected)
    {
      mqtt.publish(VERSION_TOPIC, firmwareVersion(), true);
    }
  }
  paintDot(wifiConnected, mqttConnected);

  // Grey out quadrants whose sensor has gone quiet.
  state.tick();

  // --- Buttons -------------------------------------------------------------
  if (M5.BtnA.wasClicked())
  {
    LOG("btn A: wake");
    M5.Display.wakeup();
    state.redraw();
  }

  if (M5.BtnB.wasClicked())
  {
    // Manual refresh: reconnecting resubscribes, and the broker replays the
    // retained values.
    LOG("btn B: refresh");
    mqtt.forceReconnect();
  }

  if (M5.BtnC.wasClicked())
  {
    LOG("btn C: sleep");
    M5.Display.sleep();
  }
}
