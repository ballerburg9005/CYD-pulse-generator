# CYD pulse generator

A standalone **bench-only digital pulse-train generator** for the Sunton
ESP32-2432S022C (ESP32-WROOM-32, ST7789 i80, CST816S). It is not a medical
device or a stimulator. Do not connect it to people, animals, electrodes,
or wearable stimulation hardware. USB/scope grounds are not isolated.
Output is ordinary 3.3 V GPIO logic, not a calibrated current source.

## Build
ESP-IDF v5.4.x (original clock used 5.4.4).

```sh
git clone https://github.com/ballerburg9005/CYD-pulse-generator
cd CYD-pulse-generator
. ~/esp/esp-idf/export.sh
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

## Bench controls
Frequency 1–1000 Hz; independent high time 10–10000 us in 10 us steps,
automatically limited to less than the period. RMT hardware repeats the
encoded period at nominal 1 us tick resolution without software toggling.
The screen shows the requested frequency and quantized period, high time
and duty cycle. Default is a generic 100 Hz / 1000 us test signal.
START runs for at most 15 seconds; tap anywhere to stop early (after
releasing START). Output is idle low, never starts on boot, and edits
are possible only while stopped. The timeout is software-controlled,
not a hardware safety interlock.

## Board notes
Ported display/touch implementation from
[CYD 100% Clock](https://github.com/ballerburg9005/cyd-100-percent-clock).
No clock, RTC, Wi-Fi, SD filesystem, audio or composite video code.

| Function | GPIO |
|---|---|
| ST7789 D0..D7 | 15, 13, 12, 14, 27, 25, 33, 32 |
| LCD WR/DC/CS/RD | 4/16/17/2 |
| Backlight | 0 |
| CST816S I2C | SDA21/SCL22, address 0x15 |
| Onboard speaker amplifier (unused) | 26 |
| microSD (unused) | CS5, CLK18, MISO19, MOSI23 |
| Bench output | GPIO23 (SD MOSI) |

GPIO25 is part of the LCD data bus. GPIO26 is connected to the onboard
amplifier and is not a clean dedicated output. GPIO23 is chosen because
the SD interface is unused in this firmware; remove the SD card before
powering on. Confirm the board's actual MOSI test point or connector
against its schematic/continuity before probing; access to this signal
has not been physically verified. Use a high-impedance scope or logic
analyzer only, with the board's logic ground. No external power or loads
should be connected to the output. The device does not measure actual
output voltage, current, timing or impedance; screen values are calculated.

Landscape orientation retains the original 180-degree flip (TF/button at
top) and 112% touch expansion. Controls are inset at least 20 px on both
horizontal edges. The touch transformation is configurable in menuconfig.
Touch edge expansion is a heuristic, not a measured calibration.

## Research context
Published work uses low-frequency pulse trains, including the 10–12 Hz
median nerve studies and 20 Hz auricular studies. See [research notes](RESEARCH.md)
for references, not clinical settings or instructions. This project is
for digital signal measurement only.

## Validation
Host timing tests: `cc -std=c11 -Wall -Wextra -Werror -I main tests/test_timing.c -o /tmp/cyd-timing && /tmp/cyd-timing`.
CI builds ESP-IDF firmware. Actual hardware verification is still required:
confirm orientation, touch, signal access, idle-low behavior, timing,
stop behavior and UI responsiveness using only bench instruments.
