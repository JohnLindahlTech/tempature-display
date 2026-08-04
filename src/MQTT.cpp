#include "MQTT.h"
#include <M5Unified.h>
#include <PubSubClient.h>
#include <SSLClient.h>
#include "Log.h"

// Subscribe at QoS 1. PubSubClient does acknowledge inbound QoS 1 publishes,
// and it means a temperature update survives a marginal WiFi link instead of
// being silently dropped.
static const uint8_t SUBSCRIBE_QOS = 1;

MQTT::MQTT(SSLClient &sslClient)
    : _client(sslClient),
      _server(nullptr),
      _port(0),
      _clientId(nullptr),
      _user(nullptr),
      _password(nullptr),
      _lastReconnectAttempt(0)
{
}

void MQTT::init(const char *server, uint16_t port, const char *clientId,
                const char *user, const char *password)
{
  _server = server;
  _port = port;
  _clientId = clientId;
  _user = user;
  _password = password;
  _client.setServer(_server, _port);
  _client.setKeepAlive(MQTT_KEEPALIVE_SECONDS);
  _subscriptionTopics.clear();

  // Backdate the last attempt so the first loop() connects straight away
  // instead of idling for one backoff interval.
  _lastReconnectAttempt = millis() - (MQTT_RETRY_INTERVAL_MS + 1);
}

void MQTT::setCallback(std::function<void(char *, uint8_t *, unsigned int)> callback)
{
  _client.setCallback(callback);
}

bool MQTT::connect()
{
  LOG("mqtt: connecting to %s:%u as %s", _server, _port, _clientId);

  // Last Will: if this connection dies without a clean DISCONNECT, the broker
  // publishes "offline" on our behalf, retained, so the rest of the system can
  // tell a quiet display from a dead one.
  bool connected = _client.connect(_clientId, _user, _password,
                                   AVAILABILITY_TOPIC, 0, true, AVAILABILITY_OFFLINE);
  if (!connected)
  {
    LOG("mqtt: %s (%d)", printStatus(_client.state()), _client.state());
    return false;
  }

  _client.publish(AVAILABILITY_TOPIC, AVAILABILITY_ONLINE, true);

  // Re-apply subscriptions directly; calling subscribe() here would insert into
  // the same set we are iterating over. The broker replays every retained
  // topic in response, which is what repopulates the screen.
  for (const std::string &topic : _subscriptionTopics)
  {
    _client.subscribe(topic.c_str(), SUBSCRIBE_QOS);
  }

  LOG("mqtt: connected, %u subscription(s) restored",
      (unsigned)_subscriptionTopics.size());
  return true;
}

bool MQTT::loop(bool *justConnected)
{
  if (justConnected != nullptr)
  {
    *justConnected = false;
  }

  _client.loop();

  bool connected = _client.connected();
  if (connected)
  {
    _lastReconnectAttempt = 0;
    return true;
  }

  // Unsigned, so the subtraction below stays correct across the millis()
  // rollover at ~49 days of uptime.
  uint32_t now = millis();
  if ((now - _lastReconnectAttempt) <= MQTT_RETRY_INTERVAL_MS)
  {
    return false;
  }

  _lastReconnectAttempt = now;
  connected = connect();
  if (connected && justConnected != nullptr)
  {
    *justConnected = true;
  }
  return connected;
}

bool MQTT::subscribe(const char *topic)
{
  _subscriptionTopics.insert(topic);
  if (_client.connected())
  {
    return _client.subscribe(topic, SUBSCRIBE_QOS);
  }
  return false;
}

bool MQTT::unsubscribe(const char *topic)
{
  _subscriptionTopics.erase(topic);
  if (_client.connected())
  {
    return _client.unsubscribe(topic);
  }
  return false;
}

bool MQTT::publish(const char *topic, const char *payload, bool retained)
{
  if (!_client.connected())
  {
    return false;
  }
  return _client.publish(topic, payload, retained);
}

void MQTT::forceReconnect()
{
  LOG("mqtt: forced reconnect");
  // A clean DISCONNECT, so the broker does not fire the Last Will.
  _client.disconnect();
  _lastReconnectAttempt = millis() - (MQTT_RETRY_INTERVAL_MS + 1);
}

int MQTT::status()
{
  return _client.state();
}

int32_t MQTT::statusColor()
{
  switch (_client.state())
  {
  case MQTT_CONNECTED:
  case MQTT_CONNECTION_TIMEOUT:
    return TFT_GREEN;
  case MQTT_CONNECTION_LOST:
  case MQTT_CONNECT_FAILED:
  case MQTT_DISCONNECTED:
  case MQTT_CONNECT_UNAVAILABLE:
    return TFT_ORANGE;
  case MQTT_CONNECT_BAD_PROTOCOL:
  case MQTT_CONNECT_BAD_CREDENTIALS:
  case MQTT_CONNECT_UNAUTHORIZED:
    return TFT_DARKCYAN;
  case MQTT_CONNECT_BAD_CLIENT_ID:
    return TFT_YELLOW;
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
