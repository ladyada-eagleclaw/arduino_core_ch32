#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <HardwareTimer.h>
#include <MetroNeoPixel.h>
#include <MetroPSRAM.h>
#include <MetroSDIO.h>
#include <Network.h>
#include <malloc.h>

// Existing fixture: D2-D3 and A0-A1. METRO04.TST is the 32 KiB pattern file
// created by MetroBringup; this sketch only opens it read-only.
NetworkServer echoServer(4242, 2);
NetworkClient clients[2];
NetworkUDP echoUDP, diagnostics;
MetroPSRAM psram;
MetroSDIO card;
FsVolume volume;
FsFile cardFile;
HardwareTimer timer7(TIM7);
volatile uint32_t gpioEdges = 0, timerTicks = 0;
uint32_t testStartedMs = 0;
uint64_t usbBytes = 0, tcpBytes = 0;
uint32_t udpPackets = 0, tcpConnections = 0, ioErrors = 0;
uint32_t ramPasses = 0, ramErrors = 0, ramIndex = 0;
uint32_t sdPasses = 0, sdErrors = 0, sdBlock = 0;
uint32_t adcSamples = 0, adcErrors = 0;
int serverHeapDelta = 0;
uint32_t serverCreationFailures = 0;
bool ramReady = false, ramFilled = false, sdReady = false, networkReady = false;
bool mdnsReady = false;
uint8_t networkBuffer[1472];

void setup() {
  SerialUSB.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D3, INPUT);
  attachInterrupt(D3, GPIO_Mode_IN_FLOATING, gpioEdge, EXTI_Mode_Interrupt,
                  EXTI_Trigger_Rising);
  analogWriteResolution(8);
  analogWriteFrequency(1000);
  analogWrite(D2, 128);
  analogWriteResolution(12);
  analogReadResolution(12);
  analogWrite(A0, 2048);
  timer7.setOverflow(1000, HERTZ_FORMAT);
  timer7.attachInterrupt(timerTick);
  timer7.resume();
  testStartedMs = millis();
  ramReady = psram.begin();
  sdReady = card.begin() && volume.begin(&card) &&
            cardFile.open("METRO04.TST", O_RDONLY) && cardFile.fileSize() == 32768;
  RCC_PB2PeriphClockCmd(RCC_PB2Periph_AFIO | RCC_PB2Periph_GPIOD, ENABLE);
  AFIO->PCFR1 |= 1u << 31;
  GPIO_InitTypeDef gpio = {};
  gpio.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
  gpio.GPIO_Mode = GPIO_Mode_AF_PP;
  gpio.GPIO_Speed = GPIO_Speed_High;
  GPIO_Init(GPIOD, &gpio);
  Ethernet.begin();
}

void loop() {
  Network.poll();
  if (Ethernet.hasIP() && !networkReady) {
    echoServer.begin();
    echoServer.setNoDelay(true);
    networkReady = echoUDP.begin(4243) && diagnostics.begin(4244) && (bool)echoServer;
    mdnsReady = MDNS.begin("metro467-community");
    MDNS.setInstanceName("Metro467Soak");
    mdnsReady &= MDNS.addService("metroecho", "tcp", 4242);
    mdnsReady &= MDNS.addServiceTxt("metroecho", "tcp", "board", "CH32V467");
  }
  serviceUSB();
  serviceTCP();
  serviceUDP();
  serviceDiagnostics();
  checkRAM();
  checkSD();
  static uint32_t lastADC = 0, lastLED = 0;
  if (millis() - lastADC >= 10) {
    lastADC = millis();
    int sample = analogRead(A1);
    adcSamples++;
    if (abs(sample - 2048) > 64) adcErrors++;
  }
  if (millis() - lastLED >= 500) {
    lastLED = millis();
    static uint8_t color = 0;
    digitalWrite(LED_BUILTIN, (millis() / 500) % 2);
    metroNeoPixelWrite(color == 0 ? 12 : 0, color == 1 ? 12 : 0, color == 2 ? 12 : 0);
    color = (color + 1) % 3;
  }
}

void gpioEdge() { gpioEdges++; }
void timerTick() { timerTicks++; }

void serviceUSB() {
  uint8_t buffer[512];
  int count = SerialUSB.available();
  if (count > (int)sizeof(buffer)) count = sizeof(buffer);
  if (count <= 0) return;
  count = SerialUSB.read(buffer, count);
  size_t written = SerialUSB.write(buffer, count);
  usbBytes += written;
  if (written != (size_t)count) ioErrors++;
}

void serviceTCP() {
  if (!networkReady) return;
  if (echoServer.hasClient()) {
    for (unsigned i = 0; i < 2; i++) {
      if (!clients[i].connected()) {
        clients[i] = echoServer.accept();
        clients[i].setNoDelay(true);
        tcpConnections++;
        break;
      }
    }
  }
  for (auto &client : clients) {
    int count = client.available();
    if (count > 512) count = 512;
    if (count <= 0) continue;
    count = client.read(networkBuffer, count);
    if (count <= 0) { ioErrors++; continue; }
    size_t written = client.write(networkBuffer, count);
    tcpBytes += written;
    if (written != (size_t)count) ioErrors++;
  }
}

void serviceUDP() {
  int count = echoUDP.parsePacket();
  if (!count) return;
  IPAddress sender = echoUDP.remoteIP();
  uint16_t port = echoUDP.remotePort();
  if (count > (int)sizeof(networkBuffer)) { echoUDP.clear(); ioErrors++; return; }
  count = echoUDP.read(networkBuffer, count);
  if (count <= 0) { ioErrors++; return; }
  bool pass = echoUDP.beginPacket(sender, port);
  pass &= echoUDP.write(networkBuffer, count) == (size_t)count;
  pass &= echoUDP.endPacket();
  if (!pass) ioErrors++;
  udpPackets++;
}

void serviceDiagnostics() {
  if (!diagnostics.parsePacket()) return;
  IPAddress sender = diagnostics.remoteIP();
  uint16_t port = diagnostics.remotePort();
  char command[16] = {};
  diagnostics.read(command, sizeof(command) - 1);
  diagnostics.clear();
  if (strcmp(command, "lifecycle") == 0) {
    int before = mallinfo().uordblks;
    for (unsigned i = 0; i < 1000; i++) {
      NetworkServer temporary(4342, 2);
      temporary.begin();
      if (!temporary) serverCreationFailures++;
    }
    serverHeapDelta = mallinfo().uordblks - before;
  }
  struct mallinfo heap = mallinfo();
  char status[800];
  snprintf(status, sizeof(status),
    "{\"ms\":%lu,\"started\":%lu,\"usb_mib\":%lu,\"tcp_mib\":%lu,"
    "\"udp_packets\":%lu,\"connections\":%lu,\"io_errors\":%lu,"
    "\"ram_ready\":%u,\"ram_passes\":%lu,\"ram_errors\":%lu,"
    "\"sd_ready\":%u,\"sd_passes\":%lu,\"sd_errors\":%lu,"
    "\"adc_samples\":%lu,\"adc_errors\":%lu,\"gpio_edges\":%lu,"
    "\"timer_ticks\":%lu,\"rx_errors\":%lu,\"tx_busy\":%lu,\"heap_used\":%u,\"mdns\":%u,"
    "\"server_heap_delta\":%d,\"server_create_failures\":%lu}",
    (unsigned long)millis(), (unsigned long)testStartedMs,
    (unsigned long)(usbBytes >> 20), (unsigned long)(tcpBytes >> 20),
    (unsigned long)udpPackets, (unsigned long)tcpConnections, (unsigned long)ioErrors,
    ramReady, (unsigned long)ramPasses, (unsigned long)ramErrors,
    sdReady, (unsigned long)sdPasses, (unsigned long)sdErrors,
    (unsigned long)adcSamples, (unsigned long)adcErrors, (unsigned long)gpioEdges,
    (unsigned long)timerTicks, (unsigned long)Ethernet.receiveErrors(),
    (unsigned long)Ethernet.transmitBusy(), heap.uordblks, mdnsReady,
    serverHeapDelta, (unsigned long)serverCreationFailures);
  diagnostics.beginPacket(sender, port);
  diagnostics.write((const uint8_t *)status, strlen(status));
  diagnostics.endPacket();
}

void checkRAM() {
  if (!ramReady) return;
  volatile uint32_t *memory = psram.data();
  const uint32_t words = psram.size() / 4;
  uint32_t current = 0x5a39c6e1u + ramPasses * 0x9e3779b9u;
  uint32_t next = current + 0x9e3779b9u;
  for (unsigned count = 0; count < 64; count++) {
    if (ramFilled) {
      if (memory[ramIndex] != (ramIndex ^ current)) ramErrors++;
      memory[ramIndex] = ramIndex ^ next;
    } else {
      memory[ramIndex] = ramIndex ^ current;
    }
    ramIndex++;
    if (ramIndex == words) {
      ramIndex = 0;
      if (ramFilled) ramPasses++;
      ramFilled = true;
      break;
    }
  }
}

void checkSD() {
  static uint32_t lastRead = 0;
  if (!sdReady || millis() - lastRead < 20) return;
  lastRead = millis();
  uint8_t block[512];
  bool pass = cardFile.read(block, sizeof(block)) == sizeof(block);
  if (pass) {
    for (unsigned i = 0; i < sizeof(block); i++) {
      if (block[i] != (i * 37 + sdBlock * 13) % 256) pass = false;
    }
  }
  if (!pass) sdErrors++;
  sdBlock++;
  if (sdBlock == 64) {
    sdBlock = 0;
    sdPasses++;
    if (!cardFile.seekSet(0)) { sdErrors++; sdReady = false; }
  }
}
