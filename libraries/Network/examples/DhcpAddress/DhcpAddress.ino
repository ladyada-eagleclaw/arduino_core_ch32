#include <Network.h>

bool hadAddress = false;

void setup() {
  Serial.begin(115200);
  // Wait for the Serial Monitor; remove this wait for standalone operation.
  while (!Serial) delay(10);
  delay(250);

  Serial.print("Adafruit Metro CH32 Ethernet DHCP example\n\r");

  if (!Ethernet.begin()) {
    Serial.print("Ethernet initialization failed.\n\r");
    return;
  }
  Serial.print("Connect the RJ45 cable to a network with DHCP.\n\r");
}

void loop() {
  Network.poll();
  bool hasAddress = Ethernet.hasIP();
  if (hasAddress && !hadAddress) {
    Serial.print("IP address: ");
    Serial.print(Ethernet.localIP());
    Serial.print("\n\rMAC address: ");
    Serial.print(Ethernet.macAddress());
    Serial.print("\n\rLink speed in Mbit/s: ");
    Serial.print(Ethernet.linkSpeed());
    Serial.print("\n\r");
  } else if (!hasAddress && hadAddress) {
    Serial.print("Network connection lost; waiting for reconnection.\n\r");
  }
  hadAddress = hasAddress;
}
