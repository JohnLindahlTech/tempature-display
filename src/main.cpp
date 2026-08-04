#include <M5Unified.h>
#include <SSLClient.h>
#include <WiFi.h>
#include "Config.h"
#include "Log.h"
#include "MQTT.h"
#include "Printer.h"
#include "State.h"
#include "StickyWiFi.h"

Printer printer;
State state(&printer);
StickyWiFi swifi;

WiFiClient wifi_transport_layer;
SSLClient secure_presentation_layer(&wifi_transport_layer);
MQTT mqtt(secure_presentation_layer);

void mqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  state.update(topic, payload, length);
}

void setup()
{
  LOG_BEGIN();

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

  swifi.init(SSID, PASSPHRASE);
}

void loop()
{
  delay(1);
  M5.update();

  // --- Connectivity --------------------------------------------------------
  wl_status_t status = swifi.loop();
  if (status == WL_CONNECTED)
  {
    // The broker replays every retained topic when we resubscribe, so a fresh
    // connection repopulates the screen on its own. No update request needed.
    mqtt.loop();
    printer.dot(mqtt.statusColor());
  }
  else
  {
    printer.dot(swifi.statusColor());
  }

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
