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
| `j` | Same read-only SDIO checks in one-bit mode |
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

## Concurrent load test

`MetroSoak` is a separate CH32V467 engineering sketch. Keep D2–D3 and A0–A1
connected, along with Ethernet and the microSD card. It starts without a serial
monitor, drives D2 PWM at 1 kHz, counts rising edges through D3, and compares a
1 kHz TIM7 interrupt counter against uptime. A0 holds half-scale DAC output and
A1 checks it every 10 ms. The NeoPixel cycles RGB while the sketch continuously
verifies and changes address-dependent patterns across all 4 MiB of PSRAM.

The sketch reads the existing 32 KiB `METRO04.TST` pattern file created by
`MetroBringup`; change that filename to the preserved test file on your card.
It opens the file read-only and repeatedly verifies every block. It does not
create, erase, or overwrite card files.

Host interfaces:

- USB CDC echoes raw bytes. Use request/reply transfers of at most 4096 bytes
  or read and write concurrently; a blocking large write followed by a read can
  deadlock against the device's bounded echo buffers.
- TCP port 4242 echoes data for two simultaneous clients.
- UDP port 4243 echoes datagrams up to 1472 bytes. Include a sequence number
  and distinguish late/lost packets from incorrect payloads.
- UDP port 4244 returns JSON counters in response to `status`. Sending
  `lifecycle` additionally creates, starts, and destroys 1000 temporary TCP
  servers, reporting heap growth and bind failures.
- mDNS advertises `Metro467Soak._metroecho._tcp.local.` on port 4242, with a
  `board=CH32V467` TXT field and hostname `metro467-community.local`.

After an aborted streaming test, restart the sketch before beginning a new
sequence so queued bytes from the interrupted stream cannot contaminate it.

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

Additional unattended testing:

- Mixed load ran for 49.59 minutes: 3,016,798,208 USB bytes and 5,266,861,726 TCP
  bytes across two connections matched exactly. UDP verified 263,113 replies,
  with 59 timeouts (0.0224%) and four late replies. No payload corruption was
  observed. A missed UDP status reply stopped the host run before its one-hour
  target; this is not reported as a completed one-hour pass. The board remained
  running, responded to a subsequent status request, and passed another USB echo.
- During the logged interval, PSRAM completed at least 1467 full-memory checks,
  SD completed 2270 full-file checks, and ADC completed 290528 half-scale checks,
  all with zero errors. GPIO and TIM7 each tracked approximately 2.91 million
  1 kHz events. Heap use remained bounded at 13484–14964 bytes during traffic.
- Thirty fresh mDNS discoveries passed while traffic was running. Thirty
  full-MTU ICMP requests (1472-byte payload, no fragmentation) received replies
  with no loss and 0–1 ms reported round-trip time on this bench network.
- Creating, starting, and destroying 1000 NetworkServer objects leaked 16000
  bytes before the destructor fix and zero afterward; every bind succeeded.
- A subsequent 30-minute test passed 8334 TCP connect/echo/close cycles and
  8334 UDP echoes, with zero failures or timeouts. Heap use was 14964 bytes both
  before and after. A final 1000-server allocation test again showed zero growth.
  PSRAM added 1874 full-memory passes and SD added 1408 full-file passes, with
  no peripheral errors. Total uninterrupted board uptime reached 83 minutes.
- Ordinary Serial sketches without an explicit TinyUSB include now compile.
  Regression builds pass on both Metros, V203 USBFS, V307 USBHS, and a non-USB
  V003 blink. The independent fix also builds against upstream's pinned TinyUSB.
- The unmodified SimpleSerialRead sketch then passed eight minutes on the 467,
  with 4777 byte-exact responses and no disconnect. Five successive 1200-baud
  TinyUF2 entries, UF2 reloads, and USB reconnects passed afterward.
- Closing checks on the final build passed PSRAM followed by the complete GPIO,
  EXTI, UART, ADC, SPI and TIM6 test; NeoPixel DMA capture; and both one-bit and
  four-bit SDIO initialization, repeated reads, CRC and FAT32 mount at 10 MHz.
  Ethernet reinitialization, DHCP, DNS and HTTP 200 also passed after the reloads.
  The board was left running MetroBringup with Ethernet and its blink/RGB loop.

Both variants compile. Physical 407 testing, formal USB compliance, and
voltage/temperature margin testing remain pending. An experimental forced
full-speed USB build did not produce a CDC response; those experimental driver
changes were reverted. Full-speed fallback remains unresolved. These checks do
not establish complete network API coverage or maximum Ethernet throughput.
