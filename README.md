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

> Put build flags in the `[env:...]` block, not in `[common]`. PlatformIO does
> not merge `[common]` automatically - an env has to pull it in explicitly with
> `build_flags = ${common.build_flags}`, and none currently do. Flags placed
> there are silently ignored, which is why `VERSION` is not actually defined in
> the firmware today.

## OTA updates

The board's default partition table (`default_16MB.csv`) already provides two
6.25 MB app slots and an `otadata` partition, so nothing about the flash layout
had to change - the firmware is ~1.13 MB, or 17% of one slot.

### Setup, once

Add the password to `src/credentials.h`, and export the same value in whichever
shell you upload from:

```cpp
#define OTA_PASSWORD "..."     // src/credentials.h (gitignored)
```

```sh
export OTA_PASSWORD='...'      # must match the above
```

Leave the macro out and the build warns - an unauthenticated listener lets
anyone who can reach the device push firmware to it. The shell variable is read
by `platformio.ini` via `${sysenv.OTA_PASSWORD}`, so the secret stays out of the
tracked file. Forget the `export` and espota reports a bare
`Authentication Failed`, which looks like a wrong password rather than a missing
one.

### The first flash has to be serial

A device cannot receive an update it does not yet know how to accept. Flash once
over USB, then OTA works from then on:

```sh
pio run -t upload                        # serial, the default env
pio run -e m5stack-core2-ota -t upload   # over the air
```

Serial is deliberately the default env: a first flash, or recovering a device
that boot-loops, should never require editing `platformio.ini`.

Confirm the listener came up - it starts lazily, because `ArduinoOTA::begin()`
needs an IP to bind and register mDNS:

```
[    3421] ota: listening as m5-temperature-display on port 3232
```

During the transfer the panel shows a progress bar, and `loop()` returns early
so nothing else touches the display or the broker. On failure you get a red
banner with the reason and the temperature grid comes back; the readings were
never lost.

### What happens if it dies at 50%

**A failed transfer is safe.** The image is written to the *inactive* app slot,
and the boot pointer is only moved at the very end, after the MD5 that espota
supplied has been verified (`Updater.cpp`, `_verifyEnd()` is the only caller of
`esp_ota_set_boot_partition`). Power loss, a WiFi drop or a truncated transfer
therefore leaves `otadata` untouched: the device reboots into the current
firmware, and the half-written image is inert until the next attempt overwrites
it.

**Firmware that flashes cleanly and then crashes at boot** is handled by an
armed rollback. The Arduino core would otherwise call
`esp_ota_mark_app_valid_cancel_rollback()` from `initArduino()` - *before*
`setup()` - marking a new image permanently good before any of this project's
code runs, so a panic in `setup()` would boot-loop forever with rollback never
firing. `src/OTA.cpp` overrides the core's weak `verifyRollbackLater()` to keep
the image on trial instead, and confirms it only once two things hold:

1. the OTA listener is up, and
2. the device has been running for `OTA_VALIDATE_AFTER_MS` (default 60 s).

Fail either and the next reboot rolls back to the previous image automatically.

The bar is deliberately **"can this image still be updated remotely"**, not "is
everything working". Rollback exists to recover firmware that cannot be fixed
over the air, so a live listener is the whole test. Gating on the broker instead
would roll a perfectly good image back during a routine broker outage - solving
nothing and breaking something.

> `extern "C"` on that override is load-bearing. The core declares
> `verifyRollbackLater()` in a `.c` file, so a C++ definition mangles its name,
> silently fails to override the weak symbol, and leaves the old behaviour in
> place with no error anywhere. Verify with
> `nm firmware.elf | grep verifyRollbackLater`: `T` means the override took, `W`
> means the core's version is still winning.

None of this affects normal operation. `ESP_OTA_IMG_PENDING_VERIFY` is only ever
set on the first boot after an OTA push - an ordinary reboot runs with the image
already valid, and a serial flash never sets it at all. Even during the window
nothing is blocked or delayed: the display and broker behave exactly as usual,
and the only deferred action is a one-time flag write to `otadata`.

The cost is at the edges. Too short a window and firmware that panics a few
seconds into `loop()` gets confirmed before it fails; too long and an ordinary
power cut during the window rolls back an image that was fine. This display runs
on grid power with no battery, so that second case is real if unlikely - which
is the argument for 60 seconds rather than ten minutes. Set
`OTA_VALIDATE_AFTER_MS` to `0` to confirm as soon as the listener is up.

Serial shows which path a boot took:

```
[    1204] ota: image on trial, confirming after 60000ms of uptime
[   60012] ota: image confirmed, rollback cancelled
```

### Over the network: mDNS, IPs and VLANs

`upload_port` takes an mDNS name purely as a convenience. Anywhere `.local` does
not resolve - across VLANs, most commonly - put the address in directly and
change nothing else:

```ini
upload_port = 10.20.30.40
```

The part that catches firewalls out is that the transfer is **bidirectional**:

| Direction | Protocol | Port | Purpose |
| --------- | -------- | ---- | ------- |
| host -> device | **UDP** | 3232 | invitation: host port, image size, MD5 |
| device -> host | **TCP** | `--host_port` | the device connects back and pulls the image |

Two consequences. A rule permitting only workstation -> device is never enough,
and the invitation is UDP, not TCP. And espota otherwise picks the callback port
at random from 10000-60000 on every run, which no firewall rule can follow -
hence the pinned `--host_port=45678` in `platformio.ini`.

**On an IoT VLAN this is the thing that breaks.** Such segments are usually
configured to block sessions initiated toward the trusted LAN, which is exactly
the direction espota needs. Either allow that one inbound TCP port from the IoT
subnet, or upload from a host already on it. MQTT is unaffected, being outbound
to the broker.

Also check NTP is reachable from the new segment: without it `StickyWiFi` never
marks the clock synced, mbedTLS cannot check the broker certificate's validity
dates, and TLS fails in a way that reads as a broker fault rather than a
firewall one.

Two failure modes look identical from the terminal. `No response from the ESP`
after ten dots means the UDP invitation never landed - wrong address, or UDP
3232 blocked. A hang *after* `Authenticating... OK` means the callback TCP
connection is blocked. Opposite sides of the rule, same-looking symptom.

On a multi-homed workstation (VPN, Docker bridges, two NICs) the device connects
back to whatever source address the invitation came from, which the kernel picks
by routing and may get wrong. `--host_ip=<your address on that subnet>` forces
it; the symptom is a stall right after the invitation succeeds.

The password itself is never sent in clear - it is an MD5 challenge-response
against a server nonce. The firmware image is plaintext TCP, which is fine on a
trusted LAN and worth knowing if it ever crosses something less trusted.

### You lose the serial log

`pio device monitor` is a serial connection. Stop plugging in USB and `LOG()`
output has nowhere to go, and `monitor_filters = esp32_exception_decoder` can no
longer turn a panic backtrace into `file:line`. Combined with the boot-crash gap
above, that is the argument for keeping the cable reachable until network
logging (telnet or syslog) exists.

### The update looks like a crash unless you handle the Will

Writing the image blocks `loop()` for far longer than `MQTT_KEEPALIVE_SECONDS`
(default 30). Left alone, the broker concludes the display died and fires the
Last Will, so `m5/status/availability` flips to `offline` in the middle of what
is actually a healthy update.

`ota.onStart` therefore publishes `offline` deliberately and disconnects
cleanly, before the first packet is written. The topic ends up in the same state
either way - the difference is that it happens on purpose, at a predictable
moment, instead of ~45 seconds later as a timeout.

The device publishes `online` again on its own when it reboots and reconnects.

## WiFi credentials are WIFI_SSID / WIFI_PASSPHRASE

`SSID` was a landmine. The Arduino core's `WiFi.h` declares
`WiFiSTAClass::SSID()`, so a `#define SSID "..."` that reaches the preprocessor
first rewrites that declaration into a string literal - and the compiler then
reports a syntax error *inside* `WiFi.h`, with nothing visibly wrong at the
point of use. It only ever built because `M5Unified.h` happens to include
`WiFi.h` before `Config.h` in every translation unit. The first new `.cpp` to
include a project header first (`src/OTA.cpp`) broke the build in a way that
looked nothing like its cause.

So `src/credentials.h` uses the prefixed names:

```cpp
#define WIFI_SSID       "your-network"
#define WIFI_PASSPHRASE "your-passphrase"
```

The short spelling is not accepted. Defining `SSID` anywhere now trips an
`#error` in `Config.h` naming the fix, rather than emitting the original
error inside `WiFi.h`. That check cannot produce a false positive: in any
translation unit that already included `WiFi.h`, `SSID` is a member function
name and not a macro at all.

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
