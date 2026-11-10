#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <HardwareTimer.h>
#include <SPI.h>
#include <Network.h>
#include <Adafruit_SPIFlashBase.h>
#include <Wire.h>
#include <SensirionI2cStc42.h>
#include <MetroNeoPixel.h>
#include <MetroPSRAM.h>
#include <MetroSDIO.h>

const uint16_t pairs[][2] = {{0,1},{2,3},{4,5},{6,7},{8,9},{10,11},{12,13},
                            {A0,A1},{A2,A3},{A4,A5}};
SPIClass flashSPI(PIN_FLASH_MOSI, PIN_FLASH_MISO, PIN_FLASH_SCK);
Adafruit_FlashTransport_SPI flashTransport(PIN_FLASH_CS, &flashSPI);
Adafruit_SPIFlashBase externalFlash(&flashTransport);
TwoWire &stemma = Wire;
volatile uint32_t edges = 0;
volatile uint32_t ticks = 0;
bool ran = false;
uint32_t lastBlink = 0;
bool led = false;
bool watchInputs = false;
uint32_t buttonPresses = 0, buttonReleases = 0;
uint32_t cardRemovals = 0, cardInsertions = 0;

void setup() {
  SerialUSB.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_SD_DETECT, INPUT_PULLUP);
}

void loop() {
  Network.poll();
  monitorInputs();
  while (SerialUSB.available()) {
    char command = SerialUSB.read();
    if (command == 't') runTests();
    else if (command == 'm') testHeaderPWM();
    else if (command == 'a') testDAC();
    else if (command == 'r') testPSRAM();
    else if (command == 's') testSDIO();
    else if (command == 'w') testSDFile();
    else if (command == 'i') testI2C();
    else if (command == 'e') testEthernet();
    else if (command == 'f') testExternalFlash(false);
    else if (command == 'p') testExternalFlash(true);
    else if (command == 'b') {
      buttonPresses = buttonReleases = cardRemovals = cardInsertions = 0;
      watchInputs = true;
      inputStatus();
    }
    else if (command == 'v') inputStatus();
    else if (command == 'n') testNeoCapture();
    else SerialUSB.write(command);
  }
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    led = !led;
    digitalWrite(LED_BUILTIN, led);
    static unsigned color;
    metroNeoPixelWrite(color == 0 ? 12 : 0, color == 1 ? 12 : 0, color == 2 ? 12 : 0);
    color = (color + 1) % 3;
  }
}

void monitorInputs() {
  const uint16_t pins[] = {PIN_BUTTON, PIN_SD_DETECT};
  static int previous[] = {HIGH, LOW};
  static int stable[] = {HIGH, LOW};
  static uint32_t changedAt[2] = {};
  for (unsigned i = 0; i < 2; i++) {
    int level = digitalRead(pins[i]);
    if (level != previous[i]) {
      previous[i] = level;
      changedAt[i] = millis();
    }
    if (millis() - changedAt[i] < 20 || level == stable[i]) continue;
    stable[i] = level;
    if (!watchInputs) continue;
    if (i == 0) {
      if (level == LOW) buttonPresses++;
      else buttonReleases++;
      SerialUSB.print(level == LOW ? "USER pressed\n\r" : "USER released\n\r");
    } else {
      if (level == HIGH) cardRemovals++;
      else cardInsertions++;
      SerialUSB.print(level == HIGH ? "Card removed\n\r" : "Card inserted\n\r");
    }
  }
}

void inputStatus() {
  SerialUSB.print("USER level="); SerialUSB.print(digitalRead(PIN_BUTTON));
  SerialUSB.print(" presses="); SerialUSB.print(buttonPresses);
  SerialUSB.print(" releases="); SerialUSB.print(buttonReleases);
  SerialUSB.print("\n\rSD detect level="); SerialUSB.print(digitalRead(PIN_SD_DETECT));
  SerialUSB.print(" removals="); SerialUSB.print(cardRemovals);
  SerialUSB.print(" insertions="); SerialUSB.print(cardInsertions);
  SerialUSB.print("\n\rUptime ms="); SerialUSB.print(millis());
  SerialUSB.print("\n\rInput status finished.\n\r");
}

void testNeoCapture() {
  // D13/PB12 output is jumpered to D12/PA9, TIM1 channel 2.
  // PWM input mode resets the timer on rising edges. CH1 records HIGH width,
  // CH2 records the period. DMA1 channels 2/3 capture every edge with IRQs off.
  // WCH CH32V407 RM v1.2, Figure 11-1 and section 14.3.6.
  pinMode(D12, INPUT);
  digitalWrite(D13, LOW);
  pinMode(D13, OUTPUT);
  RCC_PB2PeriphClockCmd(RCC_PB2Periph_AFIO | RCC_PB2Periph_TIM1, ENABLE);
  RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
  GPIO_PinRemapConfig(GPIO_FullRemap_TIM1, DISABLE);
  TIM_DeInit(TIM1);
  TIM_TimeBaseInitTypeDef base = {};
  base.TIM_Period = 65535;
  base.TIM_Prescaler = 0;
  base.TIM_ClockDivision = TIM_CKD_DIV1;
  base.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit(TIM1, &base);
  TIM_ICInitTypeDef input = {};
  input.TIM_Channel = TIM_Channel_2;
  input.TIM_ICPolarity = TIM_ICPolarity_Rising;
  input.TIM_ICSelection = TIM_ICSelection_DirectTI;
  input.TIM_ICPrescaler = TIM_ICPSC_DIV1;
  TIM_PWMIConfig(TIM1, &input);
  TIM_SelectInputTrigger(TIM1, TIM_TS_TI2FP2);
  TIM_SelectSlaveMode(TIM1, TIM_SlaveMode_Reset);
  TIM_SelectMasterSlaveMode(TIM1, TIM_MasterSlaveMode_Enable);
  RCC_ClocksTypeDef clocks = {};
  RCC_GetClocksFreq(&clocks);
  uint32_t timerHz = clocks.PCLK2_Frequency;
  if (timerHz != clocks.HCLK_Frequency) timerHz *= 2;
  SerialUSB.print("Capture timer Hz="); SerialUSB.print(timerHz);
  SerialUSB.print(" SysTick CMP="); SerialUSB.print((uint32_t)SysTick->CMP);
  SerialUSB.print("\n\r");
  const uint8_t patterns[] = {0x00, 0xff, 0xa5};
  bool allPassed = true;
  for (uint8_t pattern : patterns) {
    volatile uint16_t high[24] = {}, period[24] = {};
    DMA_DeInit(DMA1_Channel2);
    DMA_DeInit(DMA1_Channel3);
    DMA_InitTypeDef dma = {};
    dma.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = 24;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_VeryHigh;
    dma.DMA_PeripheralBaseAddr = (uint32_t)&TIM1->CH1CVR;
    dma.DMA_MemoryBaseAddr = (uint32_t)high;
    DMA_Init(DMA1_Channel2, &dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&TIM1->CH2CVR;
    dma.DMA_MemoryBaseAddr = (uint32_t)period;
    DMA_Init(DMA1_Channel3, &dma);
    DMA_Cmd(DMA1_Channel2, ENABLE);
    DMA_Cmd(DMA1_Channel3, ENABLE);
    TIM_DMACmd(TIM1, TIM_DMA_CC1 | TIM_DMA_CC2, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
    metroNeoPixelWriteOnPin(D13, pattern, pattern, pattern);
    delayMicroseconds(5);
    TIM_Cmd(TIM1, DISABLE);
    TIM_DMACmd(TIM1, TIM_DMA_CC1 | TIM_DMA_CC2, DISABLE);
    DMA_Cmd(DMA1_Channel2, DISABLE);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    bool pass = DMA_GetCurrDataCounter(DMA1_Channel2) == 0 &&
                DMA_GetCurrDataCounter(DMA1_Channel3) == 0;
    SerialUSB.print("Pattern 0x"); SerialUSB.print(pattern, HEX);
    SerialUSB.print(" captured high/period=");
    SerialUSB.print(24 - DMA_GetCurrDataCounter(DMA1_Channel2));
    SerialUSB.print('/');
    SerialUSB.print(24 - DMA_GetCurrDataCounter(DMA1_Channel3));
    SerialUSB.print("\n\rHIGH ns:");
    for (unsigned i = 0; i < 24; i++) {
      int highNs = high[i] * (1000000000UL / timerHz);
      bool expectedOne = (pattern & (0x80 >> (i % 8))) != 0;
      pass &= (highNs >= 500) == expectedOne;
      pass &= abs(highNs - (expectedOne ? 750 : 300)) <= (expectedOne ? 150 : 100);
      SerialUSB.print(' '); SerialUSB.print(highNs);
    }
    SerialUSB.print("\n\rPeriod ns (first omitted):");
    for (unsigned i = 1; i < 24; i++) {
      int periodNs = period[i] * (1000000000UL / timerHz);
      pass &= abs(periodNs - 1250) <= 150;
      SerialUSB.print(' '); SerialUSB.print(periodNs);
    }
    SerialUSB.print("\n\r");
    result("NeoPixel 24-bit pattern, pulse widths and periods", pass);
    allPassed &= pass;
  }
  TIM_DeInit(TIM1);
  pinMode(D12, INPUT);
  digitalWrite(D13, LOW);
  result("NeoPixel all three captured frames", allPassed);
  SerialUSB.print("NeoPixel capture finished.\n\r");
}

void result(const char *name, bool pass) {
  SerialUSB.print(pass ? "PASS " : "FAIL ");
  SerialUSB.print(name);
  SerialUSB.print("\n\r");
  SerialUSB.flush();
}

void releaseHeaders() {
  for (unsigned i = 0; i < 10; i++) {
    pinMode(pairs[i][0], INPUT);
    pinMode(pairs[i][1], INPUT);
  }
}

void edge() { edges++; }
void timerTick() { ticks++; }

void runTests() {
  SerialUSB.print("\n\rAdafruit Metro CH32V4x7 hardware test\n\r");
  SerialUSB.flush();
  releaseHeaders();
  for (unsigned i = 0; i < 10; i++) {
    bool pass = true;
    for (unsigned direction = 0; direction < 2; direction++) {
      uint16_t out = pairs[i][direction];
      uint16_t in = pairs[i][1-direction];
      pinMode(in, INPUT);
      digitalWrite(out, LOW);
      pinMode(out, OUTPUT);
      for (unsigned value = 0; value < 2; value++) {
        digitalWrite(out, value ? HIGH : LOW);
        delayMicroseconds(20);
        pass &= digitalRead(in) == (value ? HIGH : LOW);
      }
      pinMode(out, INPUT);
    }
    SerialUSB.print("Pair "); SerialUSB.print(i);
    SerialUSB.print(" "); result("GPIO both directions", pass);
  }

  pinMode(D3, INPUT);
  pinMode(D2, OUTPUT);
  digitalWrite(D2, LOW);
  edges = 0;
  attachInterrupt(D3, GPIO_Mode_IN_FLOATING, edge, EXTI_Mode_Interrupt,
                  EXTI_Trigger_Rising);
  for (unsigned i = 0; i < 100; i++) {
    digitalWrite(D2, HIGH); delayMicroseconds(100);
    digitalWrite(D2, LOW); delayMicroseconds(100);
  }
  delay(1); // Let the final pending edge run before detaching the callback.
  SerialUSB.print("IRQ edges="); SerialUSB.print(edges); SerialUSB.print("\n\r");
  detachInterrupt(digitalPinToInterrupt(D3));
  result("GPIO interrupt 100 edges", edges == 100);
  releaseHeaders();

  Serial1.begin(115200);
  bool uartPass = true;
  while (Serial1.available()) Serial1.read();
  for (unsigned i = 0; i < 256; i++) {
    Serial1.write((uint8_t)i);
    uint32_t start = millis();
    while (!Serial1.available() && millis() - start < 20) {}
    if (Serial1.read() != (int)i) uartPass = false;
  }
  result("UART D1 to D0, all 256 bytes", uartPass);
  Serial1.end();
  releaseHeaders();

  analogReadResolution(12);
  for (unsigned i = 7; i < 10; i++) {
    bool pass = true;
    for (unsigned direction = 0; direction < 2; direction++) {
      uint16_t out = pairs[i][direction];
      uint16_t in = pairs[i][1-direction];
      pinMode(out, OUTPUT);
      digitalWrite(out, LOW); delay(2);
      int low = analogRead(in);
      digitalWrite(out, HIGH); delay(2);
      int high = analogRead(in);
      pass &= low < 120 && high > 3975;
      SerialUSB.print("ADC "); SerialUSB.print(in);
      SerialUSB.print(" low="); SerialUSB.print(low);
      SerialUSB.print(" high="); SerialUSB.print(high);
      SerialUSB.print("\n\r");
      pinMode(out, INPUT);
      pinMode(in, INPUT);
    }
    result("ADC paired rail readings", pass);
  }

  SPI.begin();
  for (unsigned mode = 0; mode < 4; mode++) {
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, mode));
    bool pass = true;
    for (unsigned i = 0; i < 256; i++) {
      if (SPI.transfer((uint8_t)i) != i) pass = false;
    }
    SPI.endTransaction();
    SerialUSB.print("SPI mode "); SerialUSB.print(mode);
    SerialUSB.print(" "); result("MOSI to MISO", pass);
  }
  SPI.end();

  HardwareTimer timer(TIM6);
  ticks = 0;
  timer.setOverflow(1000, HERTZ_FORMAT);
  timer.attachInterrupt(timerTick);
  timer.resume();
  delay(100);
  timer.pause();
  timer.detachInterrupt();
  SerialUSB.print("Timer ticks="); SerialUSB.print(ticks); SerialUSB.print("\n\r");
  result("TIM6 interrupt at 1 kHz", ticks >= 98 && ticks <= 102);
  pinMode(LED_BUILTIN, OUTPUT);
  SerialUSB.print("Core test run finished.\n\r");
}

void testHeaderPWM() {
  const uint16_t routes[][2] = {{D0,D1},{D1,D0},{D2,D3},{D3,D2},
      {D8,D9},{D9,D8},{D10,D11},{D11,D10},{D12,D13},
      {A1,A0},{A2,A3},{A3,A2},{MISO,MOSI},{MOSI,MISO}};
  const char *names[] = {"D0","D1","D2","D3","D8","D9","D10",
      "D11","D12","A1","A2","A3","MISO","MOSI"};
  Serial1.end();
  SPI.end();
  releaseHeaders();
  analogWriteResolution(8);
  bool allPassed = true;
  for (unsigned route = 0; route < 14; route++) {
    uint16_t out = routes[route][0];
    uint16_t in = routes[route][1];
    pinMode(in, INPUT);
    analogWriteFrequency(1000);
    bool pass = true;
    SerialUSB.print("PWM "); SerialUSB.print(names[route]);
    for (unsigned duty = 64; duty <= 192; duty += 64) {
      analogWrite(out, duty);
      delay(3);
      uint32_t high = 0, low = 0;
      // Poll the physical partner pin so measurement does not claim a timer
      // shared with the output under test.
      for (unsigned sample = 0; sample < 8; sample++) {
        unsigned long highPulse = pulseIn(in, HIGH, 10000);
        unsigned long lowPulse = pulseIn(in, LOW, 10000);
        pass &= highPulse != 0 && lowPulse != 0;
        high += highPulse;
        low += lowPulse;
      }
      high /= 8;
      low /= 8;
      int period = high + low;
      int expectedHigh = duty * 1000 / 256;
      pass &= abs(period - 1000) <= 20;
      pass &= abs((int)high - expectedHigh) <= 20;
      SerialUSB.print(" high/low us="); SerialUSB.print(high);
      SerialUSB.print('/'); SerialUSB.print(low);
    }
    SerialUSB.print("\n\r");
    result(names[route], pass);
    allPassed &= pass;
    analogWrite(out, 0);
    pinMode(out, INPUT);
    pinMode(in, INPUT);
  }
  pinMode(LED_BUILTIN, OUTPUT);
  result("All 14 header PWM outputs at 1 kHz, 25/50/75 percent", allPassed);
}

void testDAC() {
  // The fixture already connects DAC1 (A0/PA4) to ADC3 (A1/PA3).
  // A shared reference makes this a functional check, not absolute calibration.
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  analogWriteResolution(12);
  analogReadResolution(12);
  bool pass = true;
  for (unsigned level = 1024; level <= 3072; level += 1024) {
    analogWrite(A0, level);
    delay(10);
    analogRead(A1); // Discard the first conversion after changing the level.
    uint32_t sum = 0;
    for (unsigned sample = 0; sample < 32; sample++) sum += analogRead(A1);
    int measured = sum / 32;
    SerialUSB.print("DAC A0="); SerialUSB.print(level);
    SerialUSB.print(" ADC A1="); SerialUSB.print(measured);
    SerialUSB.print(" error counts="); SerialUSB.print(measured - (int)level);
    SerialUSB.print("\n\r");
    pass &= abs(measured - (int)level) <= 64;
  }
  result("DAC A0 to ADC A1 at quarter/half/three-quarter scale", pass);
  DAC_Cmd(DAC_Channel_1, DISABLE);
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  analogWriteResolution(8);
}

void testPSRAM() {
#ifdef CH32V467
  MetroPSRAM psram;
  bool pass = psram.begin();
  result("PSRAM initialization", pass);
  if (!pass) return;
  SerialUSB.print("PSRAM MR0="); SerialUSB.print(psram.readModeRegister(0), HEX);
  SerialUSB.print(" MR1="); SerialUSB.print(psram.readModeRegister(1), HEX);
  SerialUSB.print(" MR2="); SerialUSB.print(psram.readModeRegister(2), HEX);
  SerialUSB.print(" MR4="); SerialUSB.print(psram.readModeRegister(4), HEX);
  SerialUSB.print("\n\r"); SerialUSB.flush();
  volatile uint32_t *memory = psram.data();
  uint32_t words = psram.size() / 4;
  uint32_t errors = 0;
  for (uint32_t i = 0; i < words; i++) memory[i] = i ^ 0x5a39c6e1;
  for (uint32_t i = 0; i < words; i++) {
    if (memory[i] != (i ^ 0x5a39c6e1)) errors++;
    memory[i] = ~(i ^ 0x5a39c6e1);
  }
  for (uint32_t i = 0; i < words; i++) {
    if (memory[i] != ~(i ^ 0x5a39c6e1)) errors++;
  }
  SerialUSB.print("PSRAM word errors="); SerialUSB.print(errors);
  SerialUSB.print("\n\r");
  result("PSRAM 4 MiB address patterns and inverse", errors == 0);
  // Verify sub-word writes do not alter their neighbours on this silicon lot.
  volatile uint8_t *bytes = (volatile uint8_t *)memory;
  volatile uint16_t *halves = (volatile uint16_t *)memory;
  for (unsigned i = 0; i < 1024; i++) memory[i] = 0xa55a3cc3;
  bool bytePass = true;
  for (unsigned i = 0; i < 4096; i++) bytes[i] = (i * 29 + 17) % 256;
  for (unsigned i = 0; i < 4096; i++) {
    if (bytes[i] != (i * 29 + 17) % 256) bytePass = false;
  }
  result("PSRAM byte writes across 4 KiB", bytePass);
  bool halfPass = true;
  for (unsigned i = 0; i < 2048; i++) halves[i] = i * 137 + 31;
  for (unsigned i = 0; i < 2048; i++) {
    if (halves[i] != (uint16_t)(i * 137 + 31)) halfPass = false;
  }
  result("PSRAM halfword writes across 4 KiB", halfPass);
#else
  SerialUSB.print("SKIP PSRAM: CH32V407 has none.\n\r");
#endif
}


void testSDIO() {
  MetroSDIO card;
  bool pass = card.begin();
  result("SDIO four-bit initialization", pass);
  if (!pass) {
    SerialUSB.print("SDIO command="); SerialUSB.print(card.lastCommand());
    SerialUSB.print(" error=0x"); SerialUSB.print(card.error(), HEX);
    SerialUSB.print(" response=0x"); SerialUSB.print(card.response(), HEX);
    SerialUSB.print("\n\r"); return;
  }
  SerialUSB.print("SDIO sectors="); SerialUSB.print(card.sectorCount());
  SerialUSB.print(" clock Hz="); SerialUSB.print(card.clockHz());
  SerialUSB.print("\n\r");
  uint8_t block[512], again[512];
  pass = true;
  for (uint32_t i = 0; i < 16; i++) {
    if (!card.readSector(i, block) || !card.readSector(i, again) || memcmp(block, again, 512)) {
      pass = false; break;
    }
  }
  result("SDIO repeated sector reads and hardware CRC", pass);
  if (!pass) {
    SerialUSB.print("SDIO error=0x"); SerialUSB.print(card.error(), HEX);
    SerialUSB.print("\n\r"); card.end(); return;
  }
  FsVolume volume;
  pass = volume.begin(&card);
  result("SDIO filesystem mount", pass);
  if (pass) {
    SerialUSB.print("Filesystem type="); SerialUSB.print(volume.fatType());
    SerialUSB.print("\n\r");
    volume.end();
  }
  card.end();
}

void testSDFile() {
  MetroSDIO card;
  FsVolume volume;
  if (!card.begin() || !volume.begin(&card)) { result("SDIO file setup", false); return; }
  char name[13];
  FsFile file;
  bool opened = false;
  for (unsigned i = 0; i < 100; i++) {
    snprintf(name, sizeof(name), "METRO%02u.TST", i);
    if (!volume.exists(name)) {
      opened = file.open(name, O_RDWR | O_CREAT | O_EXCL);
      break;
    }
  }
  if (!opened) { result("SDIO new test file", false); volume.end(); card.end(); return; }
  uint8_t block[512];
  bool pass = true;
  for (unsigned sector = 0; sector < 64 && pass; sector++) {
    for (unsigned i = 0; i < 512; i++) block[i] = (i * 37 + sector * 13) % 256;
    pass = file.write(block, sizeof(block)) == sizeof(block);
  }
  pass &= file.sync();
  file.close();
  pass &= file.open(name, O_RDONLY);
  for (unsigned sector = 0; sector < 64 && pass; sector++) {
    pass = file.read(block, sizeof(block)) == sizeof(block);
    for (unsigned i = 0; i < 512; i++) {
      if (block[i] != (i * 37 + sector * 13) % 256) pass = false;
    }
  }
  pass &= file.fileSize() == 32768;
  file.close();
  result("SDIO new 32 KiB file write/reopen/read", pass);
  SerialUSB.print("Preserved test file: "); SerialUSB.print(name); SerialUSB.print("\n\r");
  volume.end(); card.end();
}


void testI2C() {
  stemma.begin();
  unsigned found = 0;
  for (uint8_t address = 8; address < 120; address++) {
    stemma.beginTransmission(address);
    if (!stemma.endTransmission()) {
      SerialUSB.print("I2C address 0x"); SerialUSB.print(address, HEX);
      SerialUSB.print("\n\r"); found++;
    }
  }
  result("STEMMA I2C responds (software controller)", found > 0);
  testSensor();
  stemma.end();
  SerialUSB.print("I2C test finished.\n\r");
}

void testSensor() {
  SensirionI2cStc42 sensor;
  sensor.begin(stemma, STC42A_I2C_ADDR_2B);
  int16_t error = sensor.stopMeasurement();
  uint32_t product = 0;
  int8_t serial[8] = {};
  error = sensor.readProductInfo(product, serial, sizeof(serial));
  result("STC42 identity and CRC", error == 0);
  if (error) { SerialUSB.print(error); SerialUSB.print("\n\r"); return; }
  SerialUSB.print("STC42 product=0x"); SerialUSB.print(product, HEX);
  SerialUSB.print(" serial=");
  for (uint8_t value : serial) { SerialUSB.print(value, HEX); SerialUSB.print(' '); }
  SerialUSB.print("\n\r");
  error = sensor.compensateHumidityWithSht41();
  if (!error) error = sensor.compensateTemperatureWithSht41();
  if (!error) error = sensor.startConcentrationMeasurement(1000);
  if (error) { result("STC42 start measurement", false); return; }
  for (unsigned i = 0; i < 3; i++) {
    delay(1100);
    float hydrogen, temperature, humidity;
    uint16_t pressure;
    STC42ReadErrorFlags flags = {};
    error = sensor.readMeasurements(hydrogen, temperature, humidity, pressure, flags);
    result("STC42 measurement and CRC", error == 0);
    if (!error) {
      SerialUSB.print("H2 vol%="); SerialUSB.print(hydrogen, 4);
      SerialUSB.print(" temperature C="); SerialUSB.print(temperature, 2);
      SerialUSB.print(" RH%="); SerialUSB.print(humidity, 2);
      SerialUSB.print(" pressure mbar="); SerialUSB.print(pressure);
      SerialUSB.print(" flags=0x"); SerialUSB.print(flags.value, HEX);
      SerialUSB.print("\n\r");
    }
  }
  sensor.stopMeasurement();
}


void testEthernet() {
  static bool started = false;
  if (!started) {
    // The PCB routes the internal PHY LEDs to PD14/PD15.
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_AFIO | RCC_PB2Periph_GPIOD, ENABLE);
    AFIO->PCFR1 |= 1u << 31;
    GPIO_InitTypeDef gpio = {};
    gpio.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_High;
    GPIO_Init(GPIOD, &gpio);
    started = Ethernet.begin();
    result("Ethernet MAC/PHY initialization", started);
  }
  if (!started) return;
  uint32_t start = millis();
  while (!Ethernet.hasIP() && millis() - start < 15000) { Network.poll(); delay(10); }
  result("Ethernet cable link", Ethernet.linkUp());
  if (!Ethernet.linkUp()) return;
  SerialUSB.print("Ethernet speed="); SerialUSB.print(Ethernet.linkSpeed());
  SerialUSB.print(Ethernet.fullDuplex() ? " Mbps full duplex" : " Mbps half duplex");
  SerialUSB.print(" MAC="); SerialUSB.print(Ethernet.macAddress());
  SerialUSB.print("\n\r");
  result("Ethernet DHCP", Ethernet.hasIP());
  if (!Ethernet.hasIP()) return;
  SerialUSB.print("Ethernet IP="); SerialUSB.print(Ethernet.localIP()); SerialUSB.print("\n\r");
  NetworkClient client;
  bool pass = client.connect("example.com", 80);
  if (pass) {
    client.print("GET / HTTP/1.0\r\nHost: example.com\r\nConnection: close\r\n\r\n");
    start = millis();
    while (!client.available() && millis() - start < 3000) delay(10);
    String status = client.readStringUntil('\n');
    SerialUSB.print("HTTP: "); SerialUSB.print(status); SerialUSB.print("\n\r");
    pass = status.startsWith("HTTP/1.1 200") || status.startsWith("HTTP/1.0 200");
    client.stop();
  }
  result("Ethernet DNS/TCP/HTTP response", pass);
  SerialUSB.print("Ethernet RX="); SerialUSB.print(Ethernet.framesReceived());
  SerialUSB.print(" TX="); SerialUSB.print(Ethernet.framesSent());
  SerialUSB.print(" RX errors="); SerialUSB.print(Ethernet.receiveErrors());
  SerialUSB.print("\n\r");
}

void testExternalFlash(bool writeTest) {
  // flashSPI selects silicon SPI3 through the dedicated flash pads.
  flashTransport.begin();
  flashTransport.setClockSpeed(1000000, 1000000);
  uint8_t id[3] = {};
  flashTransport.readCommand(SFLASH_CMD_READ_JEDEC_ID, id, sizeof(id));
  SerialUSB.print("External flash JEDEC=");
  for (uint8_t value : id) { SerialUSB.print(value, HEX); SerialUSB.print(' '); }
  SerialUSB.print(" SPI peripheral="); SerialUSB.print(3);
  SerialUSB.print("\n\r");
  // The fitted GD25Q32E shares the library's GD25Q32 JEDEC identity.
  if (id[0] != 0xc8 || id[1] != 0x40 || id[2] != 0x16) {
    result("External flash GD25Q32 identity", false);
    flashTransport.end(); return;
  }
  bool pass = externalFlash.begin();
  result("External flash Adafruit_SPIFlash initialization", pass);
  if (!pass) { flashTransport.end(); return; }
  flashTransport.setClockSpeed(8000000, 8000000);
  const uint32_t capacity = externalFlash.size();
  SerialUSB.print("External flash bytes="); SerialUSB.print(capacity);
  SerialUSB.print(" status=0x"); SerialUSB.print(externalFlash.readStatus(), HEX);
  SerialUSB.print("\n\r");
  if (capacity != 4UL * 1024 * 1024) {
    result("External flash 4 MiB capacity", false); externalFlash.end(); return;
  }
  uint8_t block[256];
  uint32_t nonblank = 0;
  for (uint32_t address = 0; address < capacity; address += sizeof(block)) {
    if (externalFlash.readBuffer(address, block, sizeof(block)) != sizeof(block)) {
      result("External flash full read", false); externalFlash.end(); return;
    }
    for (uint8_t value : block) if (value != 0xff) nonblank++;
  }
  result("External flash full 4 MiB read", true);
  SerialUSB.print("Nonblank bytes="); SerialUSB.print(nonblank); SerialUSB.print("\n\r");
  if (!writeTest) {
    SerialUSB.print("Flash read-only inspection finished.\n\r");
    externalFlash.end(); return;
  }
  // Never overwrite an existing flash image or filesystem. This command only
  // tests an entirely blank device, then returns its two test sectors to blank.
  if (nonblank) {
    SerialUSB.print("Flash write test refused: existing data present.\n\r");
    externalFlash.end(); return;
  }
  const uint32_t addresses[] = {0, capacity - 4096};
  for (unsigned region = 0; region < 2 && pass; region++) {
    for (unsigned offset = 0; offset < 4096 && pass; offset += sizeof(block)) {
      for (unsigned i = 0; i < sizeof(block); i++)
        block[i] = (i * 37 + offset / 256 * 13 + region * 71) % 256;
      pass = externalFlash.writeBuffer(addresses[region] + offset, block, sizeof(block)) == sizeof(block);
    }
  }
  for (unsigned region = 0; region < 2 && pass; region++) {
    for (unsigned offset = 0; offset < 4096 && pass; offset += sizeof(block)) {
      pass = externalFlash.readBuffer(addresses[region] + offset, block, sizeof(block)) == sizeof(block);
      for (unsigned i = 0; i < sizeof(block); i++)
        if (block[i] != (i * 37 + offset / 256 * 13 + region * 71) % 256) pass = false;
    }
  }
  result("External flash first/last 4 KiB program and readback", pass);
  bool erased = true;
  for (uint32_t address : addresses) {
    erased &= externalFlash.eraseSector(address / 4096);
    for (unsigned offset = 0; offset < 4096; offset += sizeof(block)) {
      erased &= externalFlash.readBuffer(address + offset, block, sizeof(block)) == sizeof(block);
      for (uint8_t value : block) if (value != 0xff) erased = false;
    }
  }
  result("External flash test sectors erased and verified blank", erased);
  externalFlash.end();
  SerialUSB.print("Flash write test finished.\n\r");
}
