# CH32V407 / CH32V467 vendor support

Source: [CH32 RISC-V User Group's WCH SDK mirror](https://github.com/ch32-riscv-ug/CH32V407/tree/afee2d144b184176e5ca21202714ca9add1dd46f),
commit `afee2d144b184176e5ca21202714ca9add1dd46f`.
`SRC` comes from `EVT/EXAM/SRC`; `USER` comes from
`EVT/EXAM/FLASH/FLASH_Program/User`, excluding `main.c`.

The original WCH notices apply: this software, modified or not, is for WCH
microcontrollers. It is not relicensed under the Arduino core's license.

Local adaptations:

- Call newlib's C++ constructor dispatcher before `main`.
- Allow a board-selected flash origin and sizes; reserve the SDK's first
  1 KiB of SRAM for hardware context stacking.
- Spell vector CSRs numerically for the core's packaged GCC 8.2 assembler.
- Use the SDK PSRAM example's 25 MHz HSE × 8 configuration, with SYSCLK and
  HCLK both 200 MHz. `F_CPU=100000000` selects HCLK / 2 while retaining the
  200 MHz system PLL, allowing compliant legacy-I2C timing on I3C. The original
  default used SYSCLK 400 MHz / HCLK 200 MHz.

The Metro variants select a 32 KiB bootloader reservation. These settings
target the RET6 devices fitted to the Metro boards.
