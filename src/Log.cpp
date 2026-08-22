// Config.h first: it decides LOG_NOISE_ENABLED, which gates the includes below.
// With the usual "project headers last" order the noise headers were skipped
// and every Noise type came back as an undeclared identifier.
#include "Config.h"

#include <WiFi.h>

#if LOG_NOISE_ENABLED
#include <mbedtls/base64.h>
#include <noise/protocol.h>
#endif

#include <stdarg.h>
#include <stdio.h>

#include "Log.h"

#if DEBUG

namespace
{
#if LOG_NOISE_ENABLED
// openssl rand -base64 32 produces 44 characters plus a null terminator. A
// mistyped or truncated key would otherwise show up as a handshake that never
// completes, which is a miserable thing to debug.
static_assert(sizeof(LOG_NOISE_PSK) == 45,
              "LOG_NOISE_PSK must be exactly 32 bytes base64 - openssl rand -base64 32");

WiFiServer server(LOG_NOISE_PORT);
WiFiClient watcher;
bool listening = false;

NoiseHandshakeState *handshake = nullptr;
NoiseCipherState *sending = nullptr;
uint32_t handshakeStartedAt = 0;

// Receive buffer for the client's handshake messages. Log traffic is one-way
// after that, so nothing else is ever read.
uint8_t inbox[LOG_NOISE_MAX_FRAME];
size_t inboxLength = 0;
#endif

#if LOG_NOISE_ENABLED

void closeSession()
{
  // Only a session that finished its handshake is worth announcing. Serial
  // directly rather than LOG(), because LOG() ends up back in here.
  bool wasAttached = (sending != nullptr);

  if (handshake != nullptr)
  {
    noise_handshakestate_free(handshake);
    handshake = nullptr;
  }
  if (sending != nullptr)
  {
    noise_cipherstate_free(sending);
    sending = nullptr;
  }
  inboxLength = 0;
  if (watcher)
  {
    watcher.stop();
  }

  if (wasAttached)
  {
    Serial.printf("[%8lu] log: watcher detached\n", (unsigned long)millis());
  }
}

// Every frame is a 2-byte big-endian length followed by that many bytes. Both
// handshake messages and encrypted log lines use it.
bool sendFrame(const uint8_t *payload, size_t length)
{
  if (!watcher || !watcher.connected())
  {
    return false;
  }
  // WiFiClient does not override Print::availableForWrite(), which is a
  // hardcoded "return 0" in the core. Guarding on it here meant this function
  // returned false for every frame ever sent, so the handshake reply never
  // left the device and the session was torn down instead. Check what write()
  // actually reported instead - it is the only honest signal available.
  uint8_t header[2] = {(uint8_t)(length >> 8), (uint8_t)(length & 0xff)};
  if (watcher.write(header, sizeof(header)) != sizeof(header))
  {
    return false;
  }
  if (watcher.write(payload, length) != length)
  {
    return false;
  }
  return true;
}

// Returns true when a whole frame is buffered, leaving it at inbox[2..].
bool receiveFrame(size_t &length)
{
  while (watcher.available() > 0 && inboxLength < sizeof(inbox))
  {
    int byte = watcher.read();
    if (byte < 0)
    {
      break;
    }
    inbox[inboxLength++] = (uint8_t)byte;
  }

  if (inboxLength < 2)
  {
    return false;
  }
  size_t expected = ((size_t)inbox[0] << 8) | inbox[1];
  if (expected > sizeof(inbox) - 2)
  {
    // Garbage, or someone speaking a different protocol at us.
    closeSession();
    return false;
  }
  if (inboxLength < expected + 2)
  {
    return false;
  }

  length = expected;
  return true;
}

void consumeFrame(size_t length)
{
  size_t total = length + 2;
  memmove(inbox, inbox + total, inboxLength - total);
  inboxLength -= total;
}

bool startHandshake()
{
  // noise-c is built with NOISE_USE_LIBSODIUM, so noise_rand_bytes() is
  // libsodium's randombytes_buf(). That needs sodium_init() first, and
  // noise_init_framework() is the only thing that calls it. Without this the
  // responder's ephemeral key generation fails inside write_message, which
  // looked exactly like a wrong PSK from the client's side.
  static bool frameworkReady = false;
  if (!frameworkReady)
  {
    int initErr = noise_init_framework();
    if (initErr != NOISE_ERROR_NONE)
    {
      Serial.printf("log: noise_init_framework failed (%d)\n", initErr);
      return false;
    }
    frameworkReady = true;
  }

  uint8_t psk[32];
  size_t decoded = 0;
  if (mbedtls_base64_decode(psk, sizeof(psk), &decoded,
                            (const uint8_t *)LOG_NOISE_PSK,
                            sizeof(LOG_NOISE_PSK) - 1) != 0 ||
      decoded != sizeof(psk))
  {
    Serial.println("log: LOG_NOISE_PSK is not valid base64 for 32 bytes");
    return false;
  }

  // The device is the responder: the client speaks first. NNpsk0 mixes the
  // pre-shared key in before anything else, so an attacker without it cannot
  // even complete the first message, and the ephemeral keys give each session
  // forward secrecy.
  if (noise_handshakestate_new_by_name(&handshake,
                                       "Noise_NNpsk0_25519_ChaChaPoly_SHA256",
                                       NOISE_ROLE_RESPONDER) != NOISE_ERROR_NONE)
  {
    handshake = nullptr;
    return false;
  }
  if (noise_handshakestate_set_pre_shared_key(handshake, psk, sizeof(psk)) != NOISE_ERROR_NONE ||
      noise_handshakestate_start(handshake) != NOISE_ERROR_NONE)
  {
    noise_handshakestate_free(handshake);
    handshake = nullptr;
    return false;
  }

  memset(psk, 0, sizeof(psk));
  handshakeStartedAt = millis();
  return true;
}

void pumpHandshake()
{
  if (handshake == nullptr)
  {
    return;
  }

  // Unsigned, so this survives the millis() rollover.
  if ((millis() - handshakeStartedAt) > LOG_NOISE_HANDSHAKE_TIMEOUT_MS)
  {
    Serial.println("log: handshake timed out");
    closeSession();
    return;
  }

  uint8_t scratch[LOG_NOISE_MAX_FRAME];
  NoiseBuffer buffer;

  int action = noise_handshakestate_get_action(handshake);
  if (action == NOISE_ACTION_READ_MESSAGE)
  {
    size_t length = 0;
    if (!receiveFrame(length))
    {
      return;
    }
    noise_buffer_set_input(buffer, inbox + 2, length);
    int err = noise_handshakestate_read_message(handshake, &buffer, nullptr);
    consumeFrame(length);
    if (err != NOISE_ERROR_NONE)
    {
      // Wrong key, or not a Noise client. Both look the same from here, which
      // is the point.
      Serial.printf("log: handshake rejected (noise error %d)\n", err);
      closeSession();
      return;
    }
  }
  else if (action == NOISE_ACTION_WRITE_MESSAGE)
  {
    noise_buffer_set_output(buffer, scratch, sizeof(scratch));
    int err = noise_handshakestate_write_message(handshake, &buffer, nullptr);
    if (err != NOISE_ERROR_NONE)
    {
      Serial.printf("log: handshake write failed (noise error %d)\n", err);
      closeSession();
      return;
    }
    if (!sendFrame(buffer.data, buffer.size))
    {
      Serial.println("log: handshake reply could not be sent");
      closeSession();
      return;
    }
  }
  else if (action == NOISE_ACTION_SPLIT)
  {
    // Only the send direction is kept: after the handshake this is a one-way
    // stream and the device never reads from the client again.
    NoiseCipherState *receiving = nullptr;
    int err = noise_handshakestate_split(handshake, &sending, &receiving);
    if (err != NOISE_ERROR_NONE)
    {
      Serial.printf("log: handshake split failed (noise error %d)\n", err);
      closeSession();
      return;
    }
    if (receiving != nullptr)
    {
      noise_cipherstate_free(receiving);
    }
    noise_handshakestate_free(handshake);
    handshake = nullptr;
    Serial.printf("[%8lu] log: watcher attached\n", (unsigned long)millis());
  }
}

void serve()
{
  if (!listening)
  {
    server.begin();
    server.setNoDelay(true);
    listening = true;
  }

  if (server.hasClient())
  {
    WiFiClient incoming = server.available();
    // One watcher at a time. Displace rather than refuse, so a stale session
    // from a closed laptop cannot lock everyone else out until it times out.
    closeSession();
    watcher = incoming;
    watcher.setNoDelay(true);
    if (!startHandshake())
    {
      closeSession();
    }
  }

  if (watcher && !watcher.connected())
  {
    closeSession();
    return;
  }

  pumpHandshake();
}

void tell(const char *line, size_t length)
{
  // Dropped rather than queued while the handshake is still running: a log
  // buffer is not worth the memory on a device whose whole job is four numbers.
  if (sending == nullptr || !watcher || !watcher.connected())
  {
    return;
  }
  if (length + 16 > LOG_NOISE_MAX_FRAME)
  {
    return;
  }

  uint8_t frame[LOG_NOISE_MAX_FRAME];
  memcpy(frame, line, length);

  NoiseBuffer buffer;
  noise_buffer_set_inout(buffer, frame, length, sizeof(frame));
  if (noise_cipherstate_encrypt(sending, &buffer) != NOISE_ERROR_NONE)
  {
    // A failed encrypt means the nonce is exhausted or the state is broken;
    // either way the session cannot continue safely.
    closeSession();
    return;
  }

  sendFrame(buffer.data, buffer.size);
}
#endif

} // namespace

void logBegin()
{
  Serial.begin(SERIAL_BAUD);
  delay(50);
  Serial.println();
  Serial.println("[boot]");
}

void logNetwork(bool wifiConnected)
{
  if (!wifiConnected)
  {
#if LOG_NOISE_ENABLED
    closeSession();
    if (listening)
    {
      server.end();
      listening = false;
    }
#endif
    return;
  }

#if LOG_NOISE_ENABLED
  serve();
#endif
}

void logPrintf(const char *format, ...)
{
  char line[LOG_BUFFER_SIZE];

  int prefix = snprintf(line, sizeof(line), "[%8lu] ", (unsigned long)millis());
  if (prefix < 0)
  {
    return;
  }

  va_list args;
  va_start(args, format);
  int written = vsnprintf(line + prefix, sizeof(line) - prefix, format, args);
  va_end(args);
  if (written < 0)
  {
    return;
  }

  // vsnprintf returns what it *would* have written, so clamp before using it as
  // a length or an over-long line would read past the buffer.
  size_t length = (size_t)prefix + (size_t)written;
  if (length > sizeof(line) - 1)
  {
    length = sizeof(line) - 1;
  }

  Serial.write((const uint8_t *)line, length);
  Serial.write('\n');

  // tell() does not call LOG(), so there is no recursion to guard against - the
  // reason this transport is simpler than publishing log lines over MQTT.
#if LOG_NOISE_ENABLED
  tell(line, length);
#endif
}

#endif
