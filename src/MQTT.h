#include "Arduino.h"
#include <SSLClient.h>
#include <PubSubClient.h>
#include <set>

#ifndef MQTT_h
#define MQTT_h

class MQTT
{
public:
  MQTT(SSLClient &sslClient);
  void init(char *server, uint16_t port, char *clientId, char *user, char *password);
  void setCallback(std::function<void(char *, uint8_t *, unsigned int)> callback);
  boolean loop();
  boolean subscribe(char *topic);
  boolean unsubscribe(char *topic);
  boolean publish(const char *topic, const char *payload);
  int status();

  const char *printStatus(int status);
  int32_t statusColor();

private:
  PubSubClient _client;
  char *_server;
  uint16_t _port;
  char *_clientId;
  char *_user;
  char *_password;
  std::function<void(char *, uint8_t *, unsigned int)> _callback;
  long _lastReconnectAttempt;
  std::set<char *> _subscriptionTopics;
};

#endif
