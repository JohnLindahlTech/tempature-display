# temperature-display

The new and improved codebase for the (M5Stack Core2) temperature display.

## Development

> _N.B_ Platformio can not be run with the WSL runtime on Windows, so make sure to start VS Code from a windows context (i.e. not WSL terminal, but `cmd` or `PowerShell`)

- Install VS Code
- Install [platformio extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)
- Install [programming USB drivers](https://docs.m5stack.com/en/arduino/m5core2/program)
  - [CP210x_VCP_Windows](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/drivers/CP210x_VCP_Windows.zip) - This is probably the one you want.
  - [CH9102_VCP_SER_Windows](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/drivers/CH9102_VCP_SER_Windows.exe)
  - Will probably require a reboot of you computer.
- Create a `src/credentials.h` which you fill with the `#define`'s from [src/Config.h](./src/Config.h)
  - Important to make sure the `CA_CERT` has the correct formatting (`\n` in the string and trailing `\` on each line)
    ```cpp
    #define CA_CERT "-----BEGIN CERTIFICATE-----\n"                                 \
               "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n" \
               ...
               "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n" \
               "-----END CERTIFICATE-----";
    ```
- (Optional) Create a `src/overrides.h` if you need custom MQTT topics, timings or layout.

Both files are **partial**: define only the macros you want to change, everything else falls back to the default in [src/Config.h](./src/Config.h). That file is the single place where every compile-time setting is declared and documented.

Serial logging is on by default at 115200 baud (`pio device monitor`). Build with `-D DEBUG=0` to compile it out.

## MQTT

Every topic the display touches, at a glance:

| Topic                    | Direction  | Retain | Payload                        | Read by the device? |
| ------------------------ | ---------- | ------ | ------------------------------ | ------------------- |
| `m5/temperature/[0-3]`   | subscribe  | **yes**| `<temperature>\|<name>`         | yes, parsed         |
| `m5/status/sleep`        | subscribe  | **no** | anything - ignored             | no, topic only      |
| `m5/status/wake`         | subscribe  | **no** | anything - ignored             | no, topic only      |
| `m5/status/availability` | publish    | yes    | `online` / `offline` (the Will)| n/a                 |

The display subscribes at QoS 1.

### Payload size limit: 256 bytes, silently enforced

This one has bitten us, so it goes first.

PubSubClient reads each message into a fixed buffer, `MQTT_MAX_PACKET_SIZE`, which defaults to **256 bytes for the whole MQTT packet** - not just the payload. Anything larger is **discarded before the callback runs**:

```c
// PubSubClient.cpp, readPacket()
if (!this->stream && idx > this->bufferSize) {
    len = 0; // This will cause the packet to be ignored.
}
```

There is no error, no log line and no status-dot blip. The message simply never happened as far as the device is concerned, while `mosquitto_sub` on the same broker shows it arriving perfectly. Every topic keeps working except the one whose payload grew.

The budget, for a QoS 1 publish:

```
1 (header) + 1-2 (remaining length) + 2 (topic length) + topic + 2 (packet id) + payload  <=  256
```

For `m5/status/sleep` (15 characters) that leaves **234 bytes of payload**. As a rule of thumb, keep any payload under ~200 bytes and you will never think about this again.

> **Do not forward a whole zigbee2mqtt message onto these topics.** A Hue motion sensor's JSON is 239 bytes, which makes a 261-byte packet: five bytes over, silently dropped, sleep and wake both dead. In Node-RED put a `change` node before the `mqtt out` node setting `msg.payload` to a timestamp. That keeps the message useful for debugging and costs 13 bytes.

If a topic ever genuinely needs a large payload, raising `setBufferSize()` only moves the cliff. The real fix is a bounded payload at the publisher: these are control topics, and control topics should not carry unbounded state.

### m5/temperature/[0-3]

Subscription, one topic per screen quadrant: `0` upper left, `1` upper right, `2` lower left, `3` lower right.

Payload: `signed number|name` - pipe separated temperature as a signed floating point (or `-` for no value) and the name to display. Examples: `-3.5|Kitchen`, `0.0|Bedroom`, `1.0|Entry`, `-|Backside`.

**Publish these retained.** The broker then replays the last value the instant the display subscribes, so the screen is correct after a reboot or a WiFi drop without any handshake. The display subscribes at QoS 1.

A quadrant that receives nothing for `STALE_TIMEOUT_MS` (default 90 minutes) is redrawn greyed out, so a dead sensor looks dead instead of showing a stale number forever. The same grey is used for a `-` reading.

### m5/status/availability

Publish, retained: `online` when the display connects, and `offline` published by the broker as the Last Will if the connection drops without a clean disconnect. Lets the rest of your system tell a quiet display from a dead one.

### m5/status/sleep and m5/status/wake

Subscription. A message on `sleep` blanks the screen (updates continue in the background; this saves the display but does not conserve battery). A message on `wake` wakes it and repaints from the values it already holds - no republish needed from your side.

**The payload is ignored entirely.** `State::update` routes on the topic and never looks at the bytes, so `1`, a timestamp or an empty message all behave identically. A millisecond timestamp is the useful choice: it costs 13 bytes and lets you line up the broker's clock against the device's when something looks out of order. What the payload must not be is *large* - see the 256-byte limit above.

> **Do not publish these retained.** These two topics are *events*, not *state*. A retained message is replayed to every client the moment it subscribes, so a retained `sleep` puts the display to sleep on every single connect and reconnect - which looks exactly like a device that boots to a black screen and hangs. Press button A to wake it, then clear the retained message:
>
> ```sh
> mosquitto_pub -h <broker> -t m5/status/sleep -r -n   # -n = empty payload, clears the retain
> mosquitto_pub -h <broker> -t m5/status/wake  -r -n
> ```
>
> In Node-RED that is the `mqtt out` node with **Retain** left as `false`. Retain belongs on `m5/temperature/[0-3]`, where replaying the last value is exactly what you want.

### m5/request/update (removed)

Previously the display published `true` here to ask for a refresh, because values were not retained. Retained messages replace it: button B now forces a reconnect, which resubscribes and makes the broker replay everything. If your publisher still listens on this topic, it is safe to leave it - the display no longer uses it.

## Status dot

The dot in the centre of the grid is a traffic indicator, not a status light:

- **Dark** - connected and idle. Nothing to report.
- **Green blip** - a message arrived. Lasts `ACTIVITY_BLIP_MS` (default 250 ms).
- **Persistent colour** - something is wrong. Red/brown/magenta come from the WiFi state, orange/cyan/yellow from the broker state; see `statusColor()` in [src/StickyWiFi.cpp](./src/StickyWiFi.cpp) and [src/MQTT.cpp](./src/MQTT.cpp).

So a steady colour always means a fault, and a healthy display sits dark and winks on traffic.

## Buttons

| Button | Action                                                      |
| ------ | ----------------------------------------------------------- |
| A      | Wake the display and repaint                                 |
| B      | Refresh: reconnect, resubscribe and pull retained values     |
| C      | Put the display to sleep                                     |

## Debugging MQTT

Watch everything the display cares about, with sizes, and you can usually see the fault directly:

```sh
mosquitto_sub -h <broker> -p 8883 -u <user> -P <pass> --capath /etc/ssl/certs \
  -v -t 'm5/#' -F '%I %t (%l bytes) %p'
```

`%l` is the payload length. Add `-R` to hide retained replay, or drop it to inspect exactly what a fresh subscriber - i.e. the display after a reboot - would be handed.

> **The trap.** Testing a topic by hand with `-m 1` sends a 20-byte packet, which always works. That exonerates the firmware and sends you hunting upstream, where the flow also looks fine because the broker really is receiving and delivering the message. **Reproduce with a payload the same size as the real one**, or you will not see the failure at all.

When sleep or wake does nothing, in order:

1. **Is the message on the broker?** If not, it is the publisher; the display is innocent.
2. **How big is it?** Over ~234 bytes for these topics and it is being dropped silently. This is the most likely answer.
3. **Is it retained?** `mosquitto_sub -v -t 'm5/status/#'` right after connecting. A retained `sleep` re-sleeps the display on every reconnect; clear it with `-r -n`.
4. **Is the device connected at all?** `m5/status/availability` should read `online`. Remember it is retained, so a stale `online` is possible if the broker never fired the Will.
5. **Only now suspect the firmware.** Serial at 115200 logs `state: sleep` / `state: wake` on every handled message; silence there with the message confirmed on the broker means it never reached the callback, which points back at step 2.

## See more

- [https://platformio.org/](https://platformio.org/) - Platformio DX
- [https://github.com/m5stack/M5Unified](https://github.com/m5stack/M5Unified) - Github repo, including examples.
- [https://docs.m5stack.com/en/arduino/m5unified/helloworld](https://docs.m5stack.com/en/arduino/m5unified/helloworld) - Docs for M5Unified library.
- [https://github.com/govorox/SSLClient](https://github.com/govorox/SSLClient) - Github repo for SSLClient to consume the TLS (for MQTT).
- [https://github.com/knolleary/pubsubclient](https://github.com/knolleary/pubsubclient) - Github repo for the MQTT client.
