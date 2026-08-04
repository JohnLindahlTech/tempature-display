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

### m5/temperature/[0-3]

Subscription, one topic per screen quadrant: `0` upper left, `1` upper right, `2` lower left, `3` lower right.

Payload: `signed number|name` - pipe separated temperature as a signed floating point (or `-` for no value) and the name to display. Examples: `-3.5|Kitchen`, `0.0|Bedroom`, `1.0|Entry`, `-|Backside`.

**Publish these retained.** The broker then replays the last value the instant the display subscribes, so the screen is correct after a reboot or a WiFi drop without any handshake. The display subscribes at QoS 1.

A quadrant that receives nothing for `STALE_TIMEOUT_MS` (default 15 minutes) is redrawn greyed out, so a dead sensor looks dead instead of showing a stale number forever. The same grey is used for a `-` reading.

### m5/status/availability

Publish, retained: `online` when the display connects, and `offline` published by the broker as the Last Will if the connection drops without a clean disconnect. Lets the rest of your system tell a quiet display from a dead one.

### m5/status/sleep

Subscription: When a message is received on this topic, the screen will go to sleep, updates will continue in the background, this is to save the display, but does not conserve battery.

### m5/status/wake

Subscription: When a message is received on this topic, the screen will wake and repaint from the values it already holds. No republish needed from your side.

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

## See more

- [https://platformio.org/](https://platformio.org/) - Platformio DX
- [https://github.com/m5stack/M5Unified](https://github.com/m5stack/M5Unified) - Github repo, including examples.
- [https://docs.m5stack.com/en/arduino/m5unified/helloworld](https://docs.m5stack.com/en/arduino/m5unified/helloworld) - Docs for M5Unified library.
- [https://github.com/govorox/SSLClient](https://github.com/govorox/SSLClient) - Github repo for SSLClient to consume the TLS (for MQTT).
- [https://github.com/knolleary/pubsubclient](https://github.com/knolleary/pubsubclient) - Github repo for the MQTT client.
