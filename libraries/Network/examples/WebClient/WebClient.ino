#include <Network.h>

NetworkClient client;

void setup() {
  Serial.begin(115200);
  // Wait for the Serial Monitor; remove this wait for standalone operation.
  while (!Serial) delay(10);
  delay(250);

  Serial.print("Adafruit Metro CH32 Ethernet web client example\n\r");

  if (!Ethernet.begin()) {
    halt("Ethernet initialization failed.");
  }
  uint32_t started = millis();
  while (!Ethernet.hasIP()) {
    Network.poll();
    if (millis() - started >= 20000) halt("No DHCP address. Check the cable and router.");
    delay(10);
  }
  if (!client.connect("example.com", 80)) {
    halt("Could not connect to example.com.");
  }
  // HTTP requires CRLF line endings on the wire.
  client.print("GET / HTTP/1.0\r\nHost: example.com\r\nConnection: close\r\n\r\n");
}

void loop() {
  Network.poll();
  while (client.available()) Serial.write(client.read());
  if (!client.connected()) {
    client.stop();
    halt("HTTP connection closed.");
  }
}

void halt(const char *message) {
  Serial.print(message);
  Serial.print("\n\r");
  while (true) {
    Network.poll();
    delay(10);
  }
}
