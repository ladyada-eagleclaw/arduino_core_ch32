# Metro CH32V4x7 peripherals

Helpers for the Adafruit Metro CH32V407/467 variants:

- `metroNeoPixelWrite(red, green, blue)` drives the onboard PC14 NeoPixel.
  `metroNeoPixelWriteOnPin(pin, red, green, blue)` uses the same implementation
  on another pin. Timing supports the Metro's 100 and 200 MHz CPU options.
- `MetroPSRAM::begin()` initializes the CH32V467's in-package 4 MiB PSRAM.
  `data()` returns its mapped address; the memory is separate from the heap.
  The CH32V407 has no PSRAM, so its `begin()` returns false.
- `MetroSDIO` implements SdFat's block-device interface for the microSD socket.
  `begin()` selects four-bit operation by default; `begin(false)` selects one bit.

Install **SdFat - Adafruit Fork** for the SDIO interface. The PSRAM configuration
requires the variants' 200 MHz system PLL with 100 or 200 MHz HCLK. These drivers do not change
the Metro header numbering or the installed TinyUF2 bootloader.

Adapted from the Metro helpers in ch32-riscv-ug/ArduinoCore-CH32RV at
`0d7de606b7436bad15f9d1f31968a6c3ecf00c81`. WCH-derived PSRAM initialization retains
its WCH-only license notice in the source.
