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
  HCLK both 200 MHz. The original default used SYSCLK 400 MHz / HCLK 200 MHz.
- Update the reported clock after initialization, including HSI fallback when
  the crystal does not start.
- Correct DMA channel 8–13 status/clear masks and extended-register selection
  to match the WCH CH32V407 reference manual v1.2, sections 11.3.1–11.3.8.
  Clear flags with direct writes and remove software bank selectors from them.
- Fix DVP FIFO-ready, I3C interrupt-enable, EXTI pending-clear, and Ethernet
  process-state/MMC interrupt masks; report Ethernet timestamp timeout failures.
- Initialize SPI NSS, preserve USART initialization arguments and reset its
  Mark/Space selection, and preserve previously enabled debug-stop bits.
- Correct ADC divider constants and the PSRAM low-power-exit bit; declare the
  existing PSRAM and USART helpers in their public headers.
- Use an unsigned UTMI2 shift and document the watchdog's LSI readiness wait.

The Metro variants select a 32 KiB bootloader reservation. These settings
target the RET6 devices fitted to the Metro boards.

## Register regression tests

From the repository root on Linux, compile the real SDK drivers against mapped
RAM. This checks register accesses, initialization, timeout handling, and all 24
DMA channels. It does not emulate peripheral timing, write-one-to-clear effects,
or the RISC-V debug CSR; the latter needs a target build or hardware check.

```bash
sdk=system/CH32V4x7
testdir=$(mktemp -d)
gcc -std=gnu11 -O2 -g -ffunction-sections -fdata-sections \
  -Werror=implicit-function-declaration \
  -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
  -fsanitize=undefined -fno-sanitize-recover=undefined \
  -I"$sdk/SRC/Peripheral/inc" -I"$sdk/SRC/Core" \
  -I"$sdk/SRC/Debug" -I"$sdk/USER" tests/ch32v4x7_sdk.c \
  "$sdk"/SRC/Peripheral/src/ch32v4x7_{dma,dvp,eth,exti,gpio,i3c,iwdg,psram,rcc,spi,usart}.c \
  "$sdk/USER/system_ch32v4x7.c" -Wl,--gc-sections -o "$testdir/sdk-test"
"$testdir/sdk-test"
```
