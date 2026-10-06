# matter-relay

ESP32-C6 Matter relay board, built with PlatformIO on the Arduino + ESP-IDF
hybrid framework. It exposes two relays to any Matter controller (Apple Home,
Google Home, Home Assistant, SmartThings) and keeps working over a plain
Wi-Fi link.

Written and tested on the **ESP32-C6 Super Mini**.

## Hardware

| Function | GPIO | Notes |
| --- | --- | --- |
| Relay 1 | 2 | Momentary (pulse) relay — e.g. a PC power button |
| Relay 2 | 3 | Latching relay — state is kept across reboots |
| Button | 9 | The on-board **BOOT** button |

Both relay outputs are active-low, i.e. `LOW` = closed/on.

> GPIO2 and GPIO3 are strapping pins on the ESP32-C6. The firmware sets them
> as outputs after boot, which works fine, but if you see the board
> misbehaving at power-up on your own hardware, move the relays to free GPIOs.

## Behaviour

- **Relay 1 — pulse mode.** Any change of its On/Off toggle in the Matter app
  closes the contacts for `PULSE_MS` (500 ms) and opens them again. That is
  what a momentary switch input expects, so a Matter "turn on" becomes a short
  press of the PC's power button. The pulse is driven from `loop()` without a
  blocking `delay()`, so Wi-Fi and Matter keep being serviced.
- **Relay 2 — latching mode.** The requested state is written straight to the
  pin and persisted to NVS (`Preferences`, namespace `relays`, key `r2`), so
  the relay comes back in the same state after a power cut.
- **Physical button (BOOT)** on GPIO9, active-low with the internal pull-up.
- **Wi-Fi watchdog.** If the link drops, the firmware re-issues `Wi-Fi.begin()`
  every 10 seconds until it is back.
- **NTP sync** (`pool.ntp.org`, `time.nist.gov`) runs right after the first
  successful association — Matter needs a real clock and will otherwise log
  `GetClock_RealTimeMS()` errors.
- **Commissioning.** On first boot the serial console prints the manual pairing
  code and the QR-code URL for onboarding.

## Device type

The firmware creates two `MatterOnOffPlugin` endpoints. The Arduino Matter
library has changed how the device type is passed between versions, so
`src/main.cpp` keeps all three supported spellings behind `#define`s at the
top of the file:

```cpp
#define DEVICE_TYPE_IN_CONSTRUCTOR  // MatterOnOffPlugin relay(MATTER_ON_OFF_PLUG_IN_UNIT);
#define DEVICE_TYPE_IN_BEGIN        // relay.begin(state, MATTER_ON_OFF_PLUG_IN_UNIT);
#define DEVICE_TYPE_SET_METHOD      // relay.setDeviceType(MATTER_ON_OFF_PLUG_IN_UNIT);
```

All three are commented out by default, which means `relay.begin(state)` — the
plain signature. Uncomment the one your installed library version wants.

## Getting the code

```bash
git clone <this repo>
cd matter-relay
```

### Wi-Fi credentials

```bash
cp src/secrets.h.example src/secrets.h
# then edit src/secrets.h
```

`secrets.h` is gitignored.

### Vendored libraries

The project needs two libraries that ship with the ESP32 Arduino core 3.3.10
and are **not** committed here (they are upstream Espressif code):

- `lib/Matter` — [arduino-esp32 `libraries/Matter`](https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter), v3.3.10
- `lib/Network` — [arduino-esp32 `libraries/Network`](https://github.com/espressif/arduino-esp32/tree/master/libraries/Network), v3.3.10

Either copy them out of your local `arduino-esp32` checkout, or let PlatformIO
pull the matching core and point `lib_extra_dirs` at it.

`managed_components/` is populated automatically by the ESP-IDF component
manager on the first build and is also gitignored.

## Build and flash

```bash
pio run -t upload
pio device monitor
```

The board runs on `esp32-c6-devkitm-1` with the `huge_app.csv` partition table,
which is required because Matter does not fit in the default app partition.

> Building from Git Bash: unset `MSYSTEM` first, otherwise `idf_tools.py`
> refuses to run.

## Commissioning

Watch the serial monitor (115200 baud) on first boot. It prints:

```
Ручной код: <pairing code>
QR URL: <onboarding url>
```

Enter the code in your Matter controller, or scan the URL as a QR code. Once
commissioned, the two relays appear as separate On/Off plug-in units.

## Files

```
├── platformio.ini                # ESP32-C6 Super Mini, huge_app partition table
├── sdkconfig.defaults            # ESP-IDF defaults used by the hybrid build
├── huge_app.csv                  # partition table
├── CMakeLists.txt                # ESP-IDF project glue
├── src/
│   ├── main.cpp                  # Wi-Fi, relays, Matter endpoints, button
│   ├── CMakeLists.txt
│   └── secrets.h.example         # Wi-Fi template
└── main/
    ├── CMakeLists.txt
    └── dummy.c                   # keeps the IDF component model happy
```

## Notes and limitations

- The pulse relay leaves its Matter attribute in the "on" state after the
  pulse. The library in use did not expose a way to write the attribute back,
  so the code documents the `relay1.setState(false)` call to add if yours does.
- No OTA support.
- Wi-Fi transmit power is capped at 8.5 dBm to avoid brownouts on weak USB
  supplies. Remove that line if your board sits far from the router.

## License

No license file is included. Contact the author before redistributing.
