#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <SSLClient.h>
#include <functional>
#include <set>
#include <string>
#include "Config.h"

class MQTT
{
public:
  MQTT(SSLClient &sslClient);

  void init(const char *server, uint16_t port, const char *clientId,
            const char *user, const char *password);

  void setCallback(std::function<void(char *, uint8_t *, unsigned int)> callback);

  // Services the connection and reconnects with a fixed backoff.
  // Returns true while connected. Also returns, via `justConnected`, whether
  // this particular call is the one that (re)established the session.
  bool loop(bool *justConnected = nullptr);

  // Subscriptions are remembered and re-sent automatically after a reconnect,
  // which is what makes retained messages repopulate the screen.
  bool subscribe(const char *topic);
  bool unsubscribe(const char *topic);

  bool publish(const char *topic, const char *payload, bool retained = false);

  // Drops the session so the next loop() reconnects immediately. Resubscribing
  // makes the broker replay every retained topic - this is the manual refresh.
  void forceReconnect();

  int status();
  const char *printStatus(int status);
  int32_t statusColor();

private:
  bool connect();

  PubSubClient _client;
  const char *_server;
  uint16_t _port;
  const char *_clientId;
  const char *_user;
  const char *_password;
  uint32_t _lastReconnectAttempt;
  // Keyed by topic text, not by pointer, and owns a copy of each topic.
  std::set<std::string> _subscriptionTopics;
};
