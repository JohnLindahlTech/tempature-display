#include "MQTT.h"
#include <M5Unified.h>
#include <SSLClient.h>
#include <PubSubClient.h>
#include <set>

const long backOffTimeout = 5000;

MQTT::MQTT(SSLClient &sslClient)
{
  _lastReconnectAttempt = 0;
  _client = PubSubClient(sslClient);
}

void MQTT::init(char *server, uint16_t port, char *clientId, char *user, char *password)
{
  _lastReconnectAttempt = 0;
  _server = server;
  _port = port;
  _clientId = clientId;
  _user = user;
  _password = password;
  _client.setServer(_server, _port);
  _subscriptionTopics = {};
}

void MQTT::setCallback(std::function<void(char *, uint8_t *, unsigned int)> callback)
{
  _client.setCallback(callback);
}

boolean MQTT::loop()
{
  _client.loop();
  long now = millis();
  boolean connected = _client.connected();
  if (connected)
  {
    // Connected
    _lastReconnectAttempt = 0;
  }
  else if (now - _lastReconnectAttempt > backOffTimeout)
  {
    // Not connected, but enough time has passed to try again.
    _lastReconnectAttempt = now;
    connected = _client.connect(_clientId, _user, _password);
    if (connected && !_subscriptionTopics.empty())
    {
      for (char *topic : _subscriptionTopics)
      {
        subscribe(topic);
      }
    }
  }
  return connected;
}

boolean MQTT::subscribe(char *topic)
{
  _subscriptionTopics.insert(topic);
  if (_client.connected())
  {
    return _client.subscribe(topic);
  }
  return false;
}

boolean MQTT::unsubscribe(char *topic)
{
  _subscriptionTopics.erase(topic);
  if (_client.connected())
  {
    return _client.unsubscribe(topic);
  }
  return false;
}

boolean MQTT::publish(const char *topic, const char *payload)
{
  if (_client.connected())
  {
    return _client.publish(topic, payload);
  }
  return false;
}

int MQTT::status()
{
  return _client.state();
}

int32_t MQTT::statusColor()
{
  int status = _client.state();
  switch (status)
  {
  case MQTT_CONNECTION_TIMEOUT:
    return TFT_GREEN;
  case MQTT_CONNECTION_LOST:
    return TFT_ORANGE;
  case MQTT_CONNECT_FAILED:
    return TFT_ORANGE;
  case MQTT_DISCONNECTED:
    return TFT_ORANGE;
  case MQTT_CONNECTED:
    return TFT_GREEN;
  case MQTT_CONNECT_BAD_PROTOCOL:
    return TFT_DARKCYAN;
  case MQTT_CONNECT_BAD_CLIENT_ID:
    return TFT_YELLOW;
  case MQTT_CONNECT_UNAVAILABLE:
    return TFT_ORANGE;
  case MQTT_CONNECT_BAD_CREDENTIALS:
    return TFT_DARKCYAN;
  case MQTT_CONNECT_UNAUTHORIZED:
    return TFT_DARKCYAN;
  default:
    return TFT_GOLD;
  }
}

const char *MQTT::printStatus(int status)
{
  switch (status)
  {
  case MQTT_CONNECTION_TIMEOUT:
    return "MQTT Connection timeout";
  case MQTT_CONNECTION_LOST:
    return "MQTT Connection lost";
  case MQTT_CONNECT_FAILED:
    return "MQTT Connection failed";
  case MQTT_DISCONNECTED:
    return "MQTT Disconnected";
  case MQTT_CONNECTED:
    return "MQTT Connected";
  case MQTT_CONNECT_BAD_PROTOCOL:
    return "MQTT Bad protocol";
  case MQTT_CONNECT_BAD_CLIENT_ID:
    return "MQTT Bad client id";
  case MQTT_CONNECT_UNAVAILABLE:
    return "MQTT Connection unavailable";
  case MQTT_CONNECT_BAD_CREDENTIALS:
    return "MQTT Bad credentials";
  case MQTT_CONNECT_UNAUTHORIZED:
    return "MQTT Unauthorized";
  default:
    return "MQTT unknown status";
  }
}
