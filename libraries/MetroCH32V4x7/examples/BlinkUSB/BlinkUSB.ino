#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
}

void loop() {
  static uint32_t lastBlink;
  static bool led;
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    led = !led;
    digitalWrite(LED_BUILTIN, led);
  }
  uint8_t buffer[512];
  int count = Serial.available();
  if (count > (int)sizeof(buffer)) count = sizeof(buffer);
  if (count > 0) {
    count = Serial.read(buffer, count);
    Serial.write(buffer, count);
  }
}
