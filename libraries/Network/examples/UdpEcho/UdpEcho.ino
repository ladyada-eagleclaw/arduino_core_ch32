#include <Network.h>

NetworkUDP udp;
bool listening = false;
uint8_t packet[1472];

void setup() {
  Serial.begin(115200);
  // Wait for the Serial Monitor; remove this wait for standalone operation.
  while (!Serial) delay(10);
  delay(250);

  Serial.print("Adafruit Metro CH32 Ethernet UDP echo example\n\r");

  if (!Ethernet.begin()) Serial.print("Ethernet initialization failed.\n\r");
}

void loop() {
  Network.poll();
  if (!Ethernet.hasIP()) return;
  if (!listening) {
    listening = udp.begin(4243);
    if (!listening) return;
    Serial.print("Send a UDP packet to port 4243 at ");
    Serial.print(Ethernet.localIP());
    Serial.print("\n\r");
  }
  int length = udp.parsePacket();
  if (!length) return;
  if (length > (int)sizeof(packet)) {
    udp.clear();
    return;
  }
  IPAddress sender = udp.remoteIP();
  uint16_t port = udp.remotePort();
  int received = udp.read(packet, length);
  if (received <= 0) return;
  if (!udp.beginPacket(sender, port)) return;
  udp.write(packet, received);
  if (!udp.endPacket()) Serial.print("UDP response failed.\n\r");
}
