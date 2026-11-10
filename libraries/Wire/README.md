# Wire

## CH32V407 / CH32V467 I3C controller

Boards can select `WIRE_I3C` and `OPT_I2C_SLAVE=0` to use the I3C peripheral
for hardware I2C transfers on PB11 (SDA) and PB10 (SCL). External pull-ups are
required. The normal I2C peripheral driver remains the default for other boards.

This backend currently supports controller mode, these default pins, 100 MHz
HCLK, and the 400 kHz clock setting. `Wire.begin()` selects that setting.
The timing registers select 1.5 us low and 1.1 us high, keeping the actual bus
rate below 400 kHz. The 8-bit timing fields cannot represent 100 kHz at this
HCLK. At 200 MHz even the longest low period is shorter than Fast-mode's
1.3 us minimum, so transfers are disabled at that clock. This backend does
not change the system clock or provide a software fallback.

The CH32V407 reference manual, section 19.4.1, requires `MEND=1` and nonzero
`DCNT` for legacy I2C messages. Consequently:

* `endTransmission(false)` returns 4 without sending the message.
* A zero-byte `endTransmission()` probe returns 4 without touching the bus.
* `requestFrom(..., false)` and the combined internal-address read overload
  return zero bytes without touching the bus.
* `endTransmission()` returns 2 for address NACK and 3 for data NACK.
  Other errors, including timeout, return 4; failed reads return zero bytes.
* Unsupported `setClock()` values disable transfers until a supported setting
  is restored. `setClock()` itself has no return value in the Arduino API.

Devices that accept STOP-separated command writes and reads can use ordinary
`Wire` calls. Devices requiring repeated START, zero-byte detection, or a
100 kHz bus are not supported by this backend. Unsupported operations are
never converted into a different bus transaction.

Transfers have a bounded timeout. Errors reset the controller and its FIFOs
before the next transfer. No bit-banged bus recovery is performed.

References: [WCH CH32V407 reference manual](https://www.wch.cn/downloads/CH32V407RM_PDF.html),
sections 19.4.1 and 19.4.16–18, and
[WCH AN16001](https://github.com/openwch/ch32_application_notes/blob/main/zh/AN16001/AN16001.md).
