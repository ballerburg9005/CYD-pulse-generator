# CYD bench waveform generator

Standalone bench logic generator and onboard speaker audio tester for
ESP32-2432S022C. **Not a medical device: do not connect to people, animals,
electrodes or stimulation hardware.** USB/scope grounds are not isolated.

## Build (ESP-IDF 5.4.4)

```sh
git clone https://github.com/ballerburg9005/CYD-pulse-generator
cd CYD-pulse-generator
. ~/esp/esp-idf/export.sh
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

## Modes and controls

Tap the mode name to cycle. All modes have 0.5–40.0 Hz repetition,
with 0.1 Hz and 5 Hz steps.

| Mode | Adjustable parameters | Output |
|---|---|---|
| Logic pulse | high time 10–10000 us, steps 10/100 us | GPIO23 |
| Speaker sine | source level | GPIO26 DAC and onboard amp |
| Speaker triangle | source level | GPIO26 DAC and onboard amp |
| Speaker square | source level | GPIO26 DAC and onboard amp |
| Sine burst | 200–2000 Hz audio carrier, 10–90% gate | onboard speaker |
| Square burst | same | onboard speaker |
| Noise burst | gate percentage (carrier setting has no effect) | onboard speaker |

Audio source level is a small digital DAC swing, not calibrated current or
voltage. Audio sampling uses the task-dispatched 10 kHz esp_timer technique
from the clock; jitter and aliasing must be measured. RMT logic uses nominal
1 us timing resolution. No output starts automatically on boot. START
runs approximately 15 seconds; release the START button then tap anywhere
to stop. Parameters can be changed while stopped. Timeout is a software
feature, not a hardware interlock.

The onboard amplifier may filter out low frequencies or convert square
edges into brief transients. HF here means audio-frequency carrier, not
radio-frequency generation. It is not a DC-coupled arbitrary waveform
instrument. Keep the stock speaker connected; do not attach external
stimulation circuits or loads. **Never attach an earth-referenced scope
ground clip to either speaker terminal** unless the exact topology is
verified; use a correctly rated differential probe for speaker output.

## Board configuration

Reused driver from [CYD 100% Clock](https://github.com/ballerburg9005/cyd-100-percent-clock).
ST7789 i80 display and CST816S touch; no composite, clock, Wi-Fi, RTC, TF,
media player or contact code.

| Function | GPIO |
|---|---|
| LCD D0..D7 | 15,13,12,14,27,25,33,32 |
| LCD WR/DC/CS/RD | 4/16/17/2 |
| Backlight | 0 |
| CST816S touch (0x15) | SDA21/SCL22 |
| Speaker DAC | 26 |
| SD (unused) | CS5, CLK18, MISO19, MOSI23 |
| Logic test signal | GPIO23 |

GPIO25 is occupied by LCD. Remove the SD card before powering on.
Physical access to GPIO23 is unverified: identify its pad on the actual
board schematic or by continuity with power off. Use high-impedance
bench measurement only, not an external load.

Landscape 320x240, 180-degree rotation, 112% touch edge compensation,
20 px horizontal button margins, visible touch dot. Orientation and
compensation are configurable in menuconfig. Edge expansion is a heuristic.

[Research notes](RESEARCH.md) are background, not clinical settings.
No waveform or amplifier output has been measured on the physical board.
GitHub Actions builds the ESP-IDF firmware; check the Actions tab for
actual build status before flashing.
