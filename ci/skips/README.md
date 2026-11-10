# Example skip files

Skip metadata for submodule examples lives here so it can be committed in the
CH32 core without editing the submodule. Mirror the example's path from the
repository root. For example, to skip TinyUSB's serial echo on CH32V20x, edit:

```text
ci/skips/libraries/Adafruit_TinyUSB_Arduino/examples/CDC/serial_echo/.skip.txt
```

Add one board family, build target, or Arduino board ID per line:

```text
# Skip one family:
CH32V20x
# Skip one VM00X build target:
CH32VM00X-CH32V006K8
# Upstream board IDs also work:
CH32V30x_EVT
```

`*` skips every board. Blank lines and comments beginning with `#` are ignored;
comments can also follow a board name. Comment-only files add no exclusions.
`.FAMILY.test.skip` and `.FAMILY.test.only` markers are also supported.

Rules in parent directories apply to all examples below them. The library-level
TinyUSB `.skip.txt` excludes V003/VM00X (no USB peripheral), V103 (its TinyUSB port
is marked non-working), and L103/X035 (no implemented MCU mapping in the current
CH32 TinyUSB port). Having a USB peripheral alone does not establish TinyUSB
support. Update this rule and `TINYUSB_OPTIONS` in `tools/build_examples.py` when
additional ports become available.

Each current TinyUSB example has its own `.skip.txt`. Host/dual-role, disabled
USB classes, platform-specific examples, and examples requiring optional
libraries absent from CI have documented exclusions. Install the optional
libraries and remove their exclusions to test those examples. An eligible
example is a compile candidate; these rules do not establish hardware operation.

The runner also honors markers shipped inside the original example directory.
Local rules add exclusions; they cannot override an upstream skip or allowlist.
New upstream examples automatically inherit library-level restrictions.

Metro examples are restricted to the two Metro board IDs. Their engineering
fixtures under `extras` require bench-specific dependencies and are excluded;
the Miscellaneous builds compile BlinkUSB and the ordinary SimpleSerialRead
example for both CH32V407 and CH32V467.

For eligible TinyUSB examples, V20x defaults to `usb=tinyusb_usbd` and V30x to
`usb=tinyusb_usbhs`. An explicitly selected USB mode is preserved; `usb=none`
skips TinyUSB examples. Other examples retain their configured FQBN. Existing
skip files next to examples outside submodules continue to work as before.
