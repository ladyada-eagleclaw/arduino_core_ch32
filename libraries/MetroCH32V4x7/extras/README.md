# Metro engineering bring-up

Target: Metro CH32V407 rev A PCB with a separate Ethernet transformer/RJ45,
populated with CH32V407RET6 or CH32V467RET6. This sketch also needs the ported
`Network` library, Adafruit SPIFlash, SdFat - Adafruit Fork, Sensirion I2C STC42,
and their dependencies.

With power disconnected, fit jumpers D0–D1, D2–D3, D4–D5, D6–D7, D8–D9,
D10–D11, D12–D13, A0–A1, A2–A3, A4–A5, and SPI-header MOSI–MISO. Insert a
microSD card, connect an STC42 at I2C address 0x2B, and attach Ethernet to DHCP.
Do not jumper power pins. D13 blinks and the NeoPixel cycles RGB without waiting
for a serial monitor. Tests run only when commanded over USB serial.

| Command | Check |
| --- | --- |
| `t` | Ten GPIO pairs, 100 EXTI edges, UART loopback, ADC rails, four SPI modes, TIM6 |
| `m` | Fourteen PWM routes, 1 kHz at 25/50/75% duty |
| `a` | A0 DAC to A1 ADC at quarter/half/three-quarter scale |
| `n` | Same NeoPixel sender on D13; TIM1/DMA captures all 24 bits through D12 |
| `r` | All 4 MiB of 467 PSRAM with address/inverse patterns; byte/halfword checks |
| `s` | Four-bit SDIO, repeated sector reads, CRC and filesystem mount |
| `w` | Create a new, exclusive 32 KiB METROxx.TST file; reopen/read/verify; retain it |
| `i` | I2C scan, STC42 identity and three CRC-checked measurements |
| `f` | External-flash identity, capacity and complete read |
| `p` | Only on a completely blank flash: program/verify first and last 4 KiB, then erase/verify |
| `e` | Ethernet link, DHCP, DNS and HTTP response from example.com |
| `b` | Arm debounced USER/card-detect counters |
| `v` | Report counters, current input levels and uptime |

The `n` check uses D12/PA9 as TIM1 channel 2 input. PWM-input mode captures HIGH
width on channel 1 and period on channel 2; DMA1 channels 2/3 record every edge
while the sender masks interrupts. The first period is omitted because it
includes the initial idle interval. It tests 0x00, 0xFF and 0xA5 frames, checks
every decoded bit and pulse width, and releases the timer afterward. Mapping:
WCH CH32V407 reference manual v1.2, Figure 11-1; configuration follows the
vendor's TIM/Input_Capture example. This verifies header-pad timing; visual RGB
confirmation also checks the onboard pixel's separate pad and LED.

## Bench evidence, 2026-11-09

Physical CH32V467, community core with WCH GCC 8.2.0 and 200 MHz SYSCLK/HCLK:

- `t`, `m`, `a`, `n`, `r`, `s`, `w`, `i`, `f`, `p`, `e` passed.
- GPIO IRQs: 100/100; TIM6: 99 ticks in a 100 ms window.
- PWM: approximately 1000 us periods and 251/502/753 us HIGH times.
- DAC/ADC: 1024/2048/3072 requested, 1023/2048/3073 measured.
- NeoPixel: 295 ns zero HIGH, 745 ns one HIGH, 1215–1250 ns periods; all 24
  captures per frame correct. User confirmed RGB cycling and D13 blink.
- PSRAM: zero word-pattern errors over 4 MiB; byte/halfword tests passed.
- SDIO: 10 MHz, four-bit, FAT32; 32 KiB exclusive-file round trip passed.
- STC42: correct product ID 0x08030103 and CRC-checked responses. Sensor status
  flags were reported as 0x2A; this is a bus/communication check, not gas calibration.
- External flash: GD25Q32, C8 40 16, 4 MiB. Test sectors restored to blank.
- Ethernet: 100 Mbps full duplex; DHCP, DNS and HTTP 200; zero RX errors in the run.
- Three USER presses/releases, one card removal/insertion, and physical RESET
  were observed. USB reconnected after RESET.
- Separate BlinkUSB firmware passed ten rounds of byte-exact CDC echo from
  1 through 16384 bytes. 1200-baud bootloader entry and explicit-drive Arduino
  UF2 upload passed. The first 32 KiB remain reserved for TinyUF2.

Both variants compile. Peripherals on a physical 407, USB full-speed fallback,
formal USB compliance, Ethernet throughput/load, UDP/mDNS/server APIs, and
voltage/temperature margin testing are not covered by these results.
