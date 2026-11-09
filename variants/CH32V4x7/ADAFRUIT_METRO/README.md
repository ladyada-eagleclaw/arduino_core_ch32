# Adafruit Metro CH32V407 / CH32V467

These variants target the rev A PCB with a separate Ethernet transformer and
RJ45 jack. Both use the same pin numbering and a 25 MHz crystal. CPU and bus
clocks are 200 MHz.

| Arduino pins | MCU pins, in the same order |
| --- | --- |
| D0–D7 | PB7, PB6, PB8, PB9, PC0, PC1, PC2, PC3 |
| D8–D13 | PA0, PA1, PA2, PA8, PA9, PB12 |
| A0–A5 | PA4, PA3, PB0, PB1, PC4, PC5 |
| SDA, SCL | PB11, PB10 |
| MOSI, MISO, SCK | PA7, PA6, PA5 |
| NeoPixel, USER button | PC14, PC15 |

`Serial` / `SerialUSB` is USB CDC; `Serial1` uses D0 RX and D1 TX.
The USER button is active low. A0 supports DAC output. Pin remapping is selected
through the variant's peripheral tables; the header UART uses USART1's PB6/PB7
remap. `Wire` uses an open-drain software controller because PB10/PB11 are I3C
pads on this family. External I2C pull-ups are required; controller mode only.

## TinyUF2

Install the matching Metro TinyUF2 bootloader once with a debug probe. Sketches
start at flash address `0x8000`, preserving its first 32 KiB. The two variants
use the CH32V4x7 UF2 family ID `0x7b8c32a7`.

Build/export produces a `.uf2` file. Double-tap the Metro's RESET button, then
copy that file to `METRO407` or `METRO467`. The upload recipe also accepts the
mounted drive as an explicit port, for example:

```
arduino-cli upload --fqbn <package>:ch32v:ADAFRUIT_METRO_CH32V467 --port D: --input-dir <build-output>
```

Replace the package name, drive and output directory with your installation's
values. Automatic serial-port-to-drive discovery is not implemented. The USB
port supports a 1200-baud DTR drop to request the bootloader.

The TinyUSB submodule revision includes the CH32V4x7 USBHS1 device port. That
port is a dependency of these board definitions.
