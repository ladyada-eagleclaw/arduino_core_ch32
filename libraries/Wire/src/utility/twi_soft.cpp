// SPDX-FileCopyrightText: Copyright (c) 2026 Adafruit Industries
// SPDX-License-Identifier: BSD-3-Clause
#include "Arduino.h"
#include "twi.h"

#if defined(WIRE_SOFTWARE)
// Controller only, with external pull-ups. setClock() limits the maximum
// frequency; GPIO overhead and interrupts can make the bus slower.
#if OPT_I2C_SLAVE
#error "WIRE_SOFTWARE supports controller mode only; set OPT_I2C_SLAVE=0"
#endif

static void halfPeriod(i2c_t *obj) { delayMicroseconds(obj->halfPeriodUs); }

static bool clockHigh(i2c_t *obj) {
  digitalWriteFast(obj->scl, HIGH);
  uint32_t start = micros();
  while (!digitalReadFast(obj->scl)) {
    if (micros() - start >= 25000) {
      obj->softwareTimeout = true;
      return false;
    }
  }
  return true;
}

static void releaseBus(i2c_t *obj) {
  digitalWriteFast(obj->sda, HIGH);
  digitalWriteFast(obj->scl, HIGH);
  obj->softwareHeld = false;
}

static bool startCondition(i2c_t *obj) {
  digitalWriteFast(obj->sda, HIGH);
  halfPeriod(obj);
  if (!clockHigh(obj))
    return false;
  halfPeriod(obj);
  // Do not transmit into a stuck bus or another controller's transaction.
  if (!digitalReadFast(obj->sda))
    return false;
  digitalWriteFast(obj->sda, LOW);
  halfPeriod(obj);
  digitalWriteFast(obj->scl, LOW);
  obj->softwareHeld = true;
  return true;
}

static bool stopCondition(i2c_t *obj) {
  digitalWriteFast(obj->sda, LOW);
  halfPeriod(obj);
  bool ready = clockHigh(obj);
  halfPeriod(obj);
  digitalWriteFast(obj->sda, HIGH);
  halfPeriod(obj);
  obj->softwareHeld = false;
  return ready;
}

static bool writeByte(i2c_t *obj, uint8_t value) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    digitalWriteFast(obj->sda, (value & mask) != 0);
    halfPeriod(obj);
    if (!clockHigh(obj))
      return false;
    halfPeriod(obj);
    digitalWriteFast(obj->scl, LOW);
  }
  digitalWriteFast(obj->sda, HIGH);
  halfPeriod(obj);
  if (!clockHigh(obj))
    return false;
  bool ack = !digitalReadFast(obj->sda);
  halfPeriod(obj);
  digitalWriteFast(obj->scl, LOW);
  return ack;
}

static bool readByte(i2c_t *obj, uint8_t *value, bool ack) {
  *value = 0;
  digitalWriteFast(obj->sda, HIGH);
  for (unsigned bit = 0; bit < 8; bit++) {
    halfPeriod(obj);
    if (!clockHigh(obj))
      return false;
    *value = (*value << 1) | digitalReadFast(obj->sda);
    halfPeriod(obj);
    digitalWriteFast(obj->scl, LOW);
  }
  digitalWriteFast(obj->sda, !ack);
  halfPeriod(obj);
  if (!clockHigh(obj))
    return false;
  halfPeriod(obj);
  digitalWriteFast(obj->scl, LOW);
  digitalWriteFast(obj->sda, HIGH);
  return true;
}

void i2c_setTiming(i2c_t *obj, uint32_t frequency) {
  if (!frequency)
    return;
  uint32_t half = 500000UL / frequency + (500000UL % frequency != 0);
  obj->halfPeriodUs = constrain(half, 1UL, 65535UL);
}

void i2c_custom_init(i2c_t *obj, uint32_t timing, uint32_t, uint32_t) {
  obj->softwareReady = false;
  obj->softwareHeld = false;
  obj->softwareTimeout = false;
  if (!obj->isMaster || obj->sda == NC || obj->scl == NC ||
      obj->sda == obj->scl)
    return;
  if (pinNametoDigitalPin(obj->sda) == PNUM_NOT_DEFINED ||
      pinNametoDigitalPin(obj->scl) == PNUM_NOT_DEFINED)
    return;
  pinMode(pinNametoDigitalPin(obj->sda), OUTPUT_OD);
  pinMode(pinNametoDigitalPin(obj->scl), OUTPUT_OD);
  releaseBus(obj);
  i2c_setTiming(obj, timing);
  obj->softwareReady = true;
}

void i2c_init(i2c_t *obj) { i2c_custom_init(obj, 100000, 0, 0); }

void i2c_deinit(i2c_t *obj) {
  if (!obj->softwareReady)
    return;
  if (obj->softwareHeld)
    stopCondition(obj);
  pinMode(pinNametoDigitalPin(obj->sda), INPUT);
  pinMode(pinNametoDigitalPin(obj->scl), INPUT);
  obj->softwareReady = false;
}

i2c_status_e i2c_master_write(i2c_t *obj, uint8_t address, uint8_t *data,
                              uint16_t size, uint8_t sendstop) {
  if (!obj->softwareReady)
    return I2C_ERROR;
  obj->softwareTimeout = false;
  i2c_status_e result = I2C_OK;
  if (!startCondition(obj))
    result = I2C_BUSY;
  else if (!writeByte(obj, address & 0xfe))
    result = I2C_NACK_ADDR;
  else {
    for (unsigned i = 0; i < size; i++) {
      if (!writeByte(obj, data[i])) {
        result = I2C_NACK_DATA;
        break;
      }
    }
  }
  if (obj->softwareHeld && (sendstop || result != I2C_OK))
    stopCondition(obj);
  if (obj->softwareTimeout) {
    releaseBus(obj);
    return I2C_TIMEOUT;
  }
  return result;
}

i2c_status_e i2c_software_read(i2c_t *obj, uint8_t address, uint8_t *data,
                               uint16_t size, bool sendstop) {
  if (!obj->softwareReady)
    return I2C_ERROR;
  if (!size)
    return I2C_OK;
  obj->softwareTimeout = false;
  i2c_status_e result = I2C_OK;
  if (!startCondition(obj))
    result = I2C_BUSY;
  else if (!writeByte(obj, address | 1))
    result = I2C_NACK_ADDR;
  else {
    for (unsigned i = 0; i < size; i++) {
      if (!readByte(obj, &data[i], i + 1 < size)) {
        result = I2C_TIMEOUT;
        break;
      }
    }
  }
  if (obj->softwareHeld && (sendstop || result != I2C_OK))
    stopCondition(obj);
  if (obj->softwareTimeout) {
    releaseBus(obj);
    return I2C_TIMEOUT;
  }
  return result;
}

i2c_status_e i2c_master_read(i2c_t *obj, uint8_t address, uint8_t *data,
                             uint16_t size) {
  return i2c_software_read(obj, address, data, size, true);
}
#endif
