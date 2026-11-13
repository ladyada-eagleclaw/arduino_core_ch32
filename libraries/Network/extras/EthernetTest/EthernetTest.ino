#include <Network.h>
#include <malloc.h>

// Ethernet-only fixture. USB commands: t = checks, ? = status, r = restart,
// u + little-endian uint32 length + payload = exact binary USB echo.
NetworkServer echoServer(4242, 2);
NetworkClient clients[2];
NetworkUDP udp;
uint8_t buffer[1472];
bool initialized, serving, mdnsReady;
uint32_t usbRemaining, tcpBytes, udpPackets, ioErrors;
unsigned failures;

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(1000);
  initialized = Ethernet.begin();
}

void loop() {
  Network.poll();
  if (Ethernet.hasIP() && !serving) {
    echoServer.begin();
    echoServer.setNoDelay(true);
    serving = udp.begin(4243) && (bool)echoServer;
    mdnsReady = MDNS.begin("metro-ethernet-test");
    mdnsReady = mdnsReady && MDNS.addService("metroecho", "tcp", 4242);
  }
  serviceUSB();
  serviceTCP();
  serviceUDP();
}

void check(const char *name, bool pass) {
  Serial.print(pass ? "PASS " : "FAIL ");
  Serial.print(name);
  Serial.print("\n\r");
  if (!pass) failures++;
}

void runTests() {
  failures = 0;
  Serial.print("Ethernet F_CPU="); Serial.print(F_CPU); Serial.print("\n\r");
  check("MAC/PHY initialization", initialized);
  uint32_t started = millis();
  while (!Ethernet.hasIP() && millis() - started < 20000) {
    Network.poll();
    delay(10);
  }
  check("Ethernet link", Ethernet.linkUp());
  check("DHCP address", Ethernet.hasIP());
  if (!Ethernet.hasIP()) {
    Serial.print("Ethernet tests finished.\n\r");
    return;
  }
  IPAddress address;
  // An uncached lookup may finish after its caller gives up; it must not
  // overwrite the result of the subsequent request for another name.
  Network.hostByName("example.net", address, 0);
  check("DNS after an abandoned lookup", Network.hostByName("example.com", address) == 1);
  NetworkClient client;
  bool connected = client.connect(address, 80);
  check("TCP client connects", connected);
  if (connected) {
    client.setTimeout(10000);
    client.print("GET / HTTP/1.0\r\nHost: example.com\r\nConnection: close\r\n\r\n");
    String status = client.readStringUntil('\n');
    Serial.print("HTTP: "); Serial.print(status); Serial.print("\n\r");
    check("HTTP response", status.startsWith("HTTP/1.1 200") || status.startsWith("HTTP/1.0 200"));
  }
  client.stop();
  // Repeated destruction must release the server's pending-client array.
  { NetworkServer warmup(4342, 2); warmup.begin(); }
  int before = mallinfo().uordblks;
  bool servers = true;
  for (unsigned i = 0; i < 100; i++) {
    NetworkServer temporary(4342, 2);
    temporary.begin();
    servers = servers && (bool)temporary;
  }
  check("server lifecycle and heap recovery", servers && mallinfo().uordblks == before);
  printStatus();
  Serial.print("Failures="); Serial.print(failures);
  Serial.print("\n\rEthernet tests finished.\n\r");
}

void printStatus() {
  Serial.print("IP="); Serial.print(Ethernet.localIP());
  Serial.print(" link="); Serial.print(Ethernet.linkUp());
  Serial.print(" speed="); Serial.print(Ethernet.linkSpeed());
  Serial.print(" full="); Serial.print(Ethernet.fullDuplex());
  Serial.print(" mdns="); Serial.print(mdnsReady);
  Serial.print(" tcp="); Serial.print(tcpBytes);
  Serial.print(" udp="); Serial.print(udpPackets);
  Serial.print(" errors="); Serial.print(ioErrors);
  Serial.print(" rx_errors="); Serial.print(Ethernet.receiveErrors());
  Serial.print(" links="); Serial.print(Ethernet.linkChanges());
  Serial.print("\n\r");
}

void serviceUSB() {
  if (usbRemaining) {
    int count = Serial.available();
    if (count > 512) count = 512;
    if ((uint32_t)count > usbRemaining) count = usbRemaining;
    if (count <= 0) return;
    count = Serial.read(buffer, count);
    if (Serial.write(buffer, count) != (size_t)count) ioErrors++;
    usbRemaining -= count;
    return;
  }
  if (!Serial.available()) return;
  char command = Serial.read();
  if (command == 't') runTests();
  if (command == '?') printStatus();
  if (command == 'u') {
    uint8_t length[4];
    if (Serial.readBytes(length, 4) == 4) {
      memcpy(&usbRemaining, length, sizeof(usbRemaining));
      if (usbRemaining > 1024 * 1024) usbRemaining = 0;
    }
  }
  if (command == 'r') {
    for (auto &client : clients) client.stop();
    echoServer.end();
    udp.stop();
    MDNS.end();
    Ethernet.end();
    serving = false;
    initialized = Ethernet.begin();
    Serial.print("Ethernet restarted.\n\r");
  }
}

void serviceTCP() {
  if (!serving) return;
  if (echoServer.hasClient()) {
    for (auto &client : clients) {
      if (!client.connected() && !client.available()) {
        client = echoServer.accept();
        client.setNoDelay(true);
        break;
      }
    }
  }
  for (auto &client : clients) {
    int count = client.available();
    if (count > 512) count = 512;
    if (count <= 0) continue;
    count = client.read(buffer, count);
    if (count <= 0) { ioErrors++; continue; }
    size_t written = client.write(buffer, count);
    tcpBytes += written;
    if (written != (size_t)count) ioErrors++;
  }
}

void serviceUDP() {
  int count = udp.parsePacket();
  if (!count) return;
  if (count > (int)sizeof(buffer)) { udp.clear(); ioErrors++; return; }
  IPAddress sender = udp.remoteIP();
  uint16_t port = udp.remotePort();
  count = udp.read(buffer, count);
  if (count <= 0) { ioErrors++; return; }
  bool sent = udp.beginPacket(sender, port);
  sent = sent && udp.write(buffer, count) == (size_t)count;
  sent = sent && udp.endPacket();
  if (!sent) ioErrors++;
  udpPackets++;
}
