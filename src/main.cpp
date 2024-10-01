#include <M5Unified.h>
#include <WiFi.h>
#include <SSLClient.h>
#include "Printer.h"
#include "StickyWiFi.h"
#include "State.h"
#include "MQTT.h"

#if defined __has_include
#if __has_include("credentials.h")
// If using the credentials.h file, all the #defines from below needs to be available.
#include "credentials.h"

#else

#define MQTT_SERVER "mqtt.home.arpa"
#define MQTT_PORT 8883
#define MQTT_USER "user"
#define MQTT_PASSWORD "password"
#define MQTT_CLIENT_IT "myMqttClient"
#define SSID "Wifi-SSID"
#define PASSPHRASE "Wifi-passphrase"

// Notice the format for each row: Inside the string use \n and end of line needs a \ (backslash)
#define CA_CERT "-----BEGIN CERTIFICATE-----\n" \
                "asfefgwgegwegeg\n"             \
                "-----END CERTIFICATE-----"

#endif
#endif

const char *mqttServer = MQTT_SERVER;
const int mqttPort = MQTT_PORT;
const char *mqttUser = MQTT_USER;
const char *mqttPassword = MQTT_PASSWORD;
const char *mqttClientId = MQTT_CLIENT_IT;
const char *ca_cert = CA_CERT;
const char *ssid = SSID;
const char *password = PASSPHRASE;

Printer printer = Printer();
State state = State(&printer);
StickyWiFi swifi = StickyWiFi();

WiFiClient wifi_transport_layer;
SSLClient secure_presentation_layer(&wifi_transport_layer);
MQTT mqtt(secure_presentation_layer);

boolean requestedUpdate = false;

void mqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  state.update(topic, payload, length);
}

void setup()
{
  // Serial.begin(9600);
  // Serial.println("Hello Moto?");
  auto cfg = M5.config();
  cfg.led_brightness = 0;
  M5.begin(cfg);

  printer.base(TFT_GOLD);
  printer.dot(TFT_WHITE);

  secure_presentation_layer.setCACert(ca_cert);
  mqtt.init((char *)mqttServer, mqttPort, (char *)mqttClientId, (char *)mqttUser, (char *)mqttPassword);

  mqtt.subscribe((char *)TEMPERATURE_0);
  mqtt.subscribe((char *)TEMPERATURE_1);
  mqtt.subscribe((char *)TEMPERATURE_2);
  mqtt.subscribe((char *)TEMPERATURE_3);
  mqtt.subscribe((char *)WAKE);
  mqtt.subscribe((char *)SLEEP);
  mqtt.setCallback(mqttCallback);

  wl_status_t status = swifi.init((char *)ssid, (char *)password);
  if (status == WL_CONNECTED)
  {
    printer.dot(swifi.statusColor());
  }
}

void loop()
{
  delay(1);
  M5.update();

  /**
   *
   * MQTT Ctrl
   *
   */
  wl_status_t status = swifi.loop();
  if (status == WL_CONNECTED)
  {
    boolean mqttConnect = mqtt.loop();
    if (mqttConnect && !requestedUpdate)
    {
      printer.dot(TFT_GREEN);
      mqtt.publish(REQUEST_UPDATE, "true");
      requestedUpdate = true;
    }
    else if (!mqttConnect)
    {
      requestedUpdate = false;
      printer.dot(mqtt.statusColor());
    }
    // else, connected but nothing needs to be done
  }
  else
  {
    // Not connected
    printer.dot(swifi.statusColor());
  }
  /**
   *
   * Button Ctrl
   *
   */
  if (M5.BtnA.wasClicked())
  {
    M5.Display.wakeup();
  }
  if (M5.BtnB.wasClicked())
  {
    if (status == WL_CONNECTED && mqtt.status() == MQTT_CONNECTED)
    {
      printer.dot(TFT_GREEN);
      mqtt.publish(REQUEST_UPDATE, "true");
    }
  }
  if (M5.BtnC.wasClicked())
  {
    M5.Display.sleep();
  }
}
