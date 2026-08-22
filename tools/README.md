# m5log

Client for the display's encrypted log stream. The device listens; this attaches
to it - the ESPHome model, where the client subscribes rather than the device
pushing somewhere.

```sh
export LOG_NOISE_PSK='<same value as ../src/credentials.h>'
uv run --directory tools m5log m5-temperature-display.local
```

The key is read from the environment, never a flag: a secret on the command line
ends up in shell history and in `ps` output for every user on the machine.

## Protocol

`Noise_NNpsk0_25519_ChaChaPoly_SHA256`, client as initiator, device as
responder. Every frame is a 2-byte big-endian length followed by that many
bytes: two handshake messages, then a one-way stream of encrypted log lines.

Not compatible with ESPHome's native API despite sharing the port and the cipher
suite - theirs carries protobuf, this carries log text.

## Tests

```sh
uv run --directory tools pytest
```

The tests round-trip a real handshake against an in-process responder, which
covers the framing and the NNpsk0 message flow without needing hardware. What
they cannot cover is the device's C implementation agreeing with this one.
