# CH32 Ethernet

IPv4 networking for the CH32V407/467 Ethernet MAC and internal 10/100 PHY,
using lwIP 2.2.1. The board needs the Ethernet transformer, connector, and
25 MHz crystal. Both 100 MHz and 200 MHz CPU settings are supported.

## Getting started

Include `<Network.h>`, call `Ethernet.begin()` in `setup()`, and call
`Network.poll()` frequently in `loop()` and while waiting for a link or address.
`begin()` initializes the controller; `Ethernet.linkUp()` reports the cable
link and `Ethernet.hasIP()` reports when an address is available.

DHCP is the default. For a static address, call
`Ethernet.config(local, gateway, subnet, dns)` before `Ethernet.begin()`.
Use `Ethernet.localIP()`, `Ethernet.gatewayIP()`, and `Ethernet.dnsIP()` to read
the configuration. `Ethernet.setHostname()` and `Ethernet.setMacAddress()`
must be called before `begin()`. The default MAC is locally administered and
derived from the chip's unique ID.

The examples show DHCP address reporting, an HTTP request, and UDP echo:

- **DhcpAddress** prints the address, MAC, and negotiated link speed.
- **WebClient** uses `NetworkClient` to fetch a page over plain HTTP.
- **UdpEcho** echoes datagrams received on port 4243.

## API and limitations

- `NetworkClient` implements Arduino `Client`/`Stream` for TCP connections.
- `NetworkServer` listens for TCP connections; `accept()` returns a client.
- `NetworkUDP` implements Arduino `UDP`, including multicast.
- `Network.hostByName()` resolves IPv4 addresses through DNS.
- `MDNS.begin("name")` advertises `name.local`; `MDNS.addService()` advertises
  services. mDNS queries are not implemented.
- `Ethernet.end()` stops the interface; `begin()` can start it again.

The interface follows the ESP32 Network API, but is not a complete ESP32 API
implementation. The global interface is named `Ethernet` because `ETH` is
already a WCH SDK register macro. This is not the SPI/Wiznet `Ethernet.h` library.

The stack runs in foreground code without an operating system. Call its APIs
from one foreground context, never from an interrupt. Blocking TCP and DNS
operations service the stack while waiting; ordinary `delay()` does not.
Long operations that omit `Network.poll()` delay network traffic and timeouts.
IPv6, TLS, USB networking, and SNTP are not provided. The stack's static pools
use about 60 KiB of RAM; each open TCP client also needs a receive buffer.

## Source and installation

Boards Manager release archives include lwIP. For a Git installation, initialize
the submodules with `git submodule update --init --recursive`.

The Arduino interfaces and MAC driver are adapted from
[ch32-riscv-ug/ArduinoCore-CH32RV at 0d7de60](https://github.com/ch32-riscv-ug/ArduinoCore-CH32RV/tree/0d7de606b7436bad15f9d1f31968a6c3ecf00c81/libraries/Network),
under the [MIT license](LICENSE). Imported backend paths for other chips remain
guarded by that core's defines; this community-core port enables CH32V407/467.

The unmodified [lwIP submodule](https://github.com/lwip-tcpip/lwip/tree/77dcd25a72509eb83f72b033d219b1d40cd8eb95)
is pinned to `STABLE-2_2_1_RELEASE` (`77dcd25a72509eb83f72b033d219b1d40cd8eb95`)
and retains its [BSD license](lwip/COPYING). Configuration and the C wrappers
that select its compiled sources live in `src/`, outside the submodule.
