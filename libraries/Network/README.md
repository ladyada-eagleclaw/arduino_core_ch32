# CH32 Ethernet port

Imported from ch32-riscv-ug/ArduinoCore-CH32RV at
0d7de606b7436bad15f9d1f31968a6c3ecf00c81 (MIT), retaining bundled lwIP notices.
This adaptation targets the community core's CH32V407/467 support.

Call `Network.poll()` in `loop()` and while waiting for DHCP/link changes.
Blocking TCP and DNS operations also service the stack. USB networking and
SNTP are not included in this port. The Ethernet interface is named `Ethernet`
to avoid colliding with the vendor SDK's `ETH` register definition.
