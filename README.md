# temperature-display

The new and improved codebase for the (M5Stack Core2) temperature display.

# Development

> _N.B_ Platformio can not be run with the WSL runtime on Windows, so make sure to start VS Code from a windows context (i.e. not WSL terminal, but `cmd` or `PowerShell`)

- Install VS Code
- Install [platformio extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)
- Install [programming USB drivers](https://docs.m5stack.com/en/arduino/m5core2/program)
  - [CP210x_VCP_Windows](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/drivers/CP210x_VCP_Windows.zip) - This is probably the one you want.
  - [CH9102_VCP_SER_Windows](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/drivers/CH9102_VCP_SER_Windows.exe)
  - Will probably require a reboot of you computer.
- Create a `src/credentials.h` which you fill with the `#define`'s from [src/main.h](./src/main.h)
- (Optional) Create a `src/overrides.h` which you fill with the `#defines`'s from [src/State.h](./src/State.h), if you need to use custom MQTT topics etc.

## See more

- [https://platformio.org/](https://platformio.org/) - Platformio DX
- [https://github.com/m5stack/M5Unified](https://github.com/m5stack/M5Unified) - Github repo, including examples.
- [https://docs.m5stack.com/en/arduino/m5unified/helloworld](https://docs.m5stack.com/en/arduino/m5unified/helloworld) - Docs for M5Unified library.
- [https://github.com/govorox/SSLClient](https://github.com/govorox/SSLClient) - Github repo for SSLClient to consume the TLS (for MQTT).
- [https://github.com/knolleary/pubsubclient](https://github.com/knolleary/pubsubclient) - Github repo for the MQTT client.
