// SPDX-FileCopyrightText: Copyright (c) 2026 Adafruit Industries
// SPDX-License-Identifier: BSD-3-Clause
#include "twi_i2c_target.h"
#include "Arduino.h"

#if defined(WIRE_I3C) && defined(CH32V4x7) && OPT_I2C_SLAVE

// I2C1 and I3C have separate owners. Wire1 uses remapped PB9/PB8; the
// default PB7/PB6 mapping would conflict with the Metro's Serial1 header.
static i2c_t *owner;
static bool receiving;
static bool transmitting;
static bool fillingReply;
static uint8_t reply[I2C_TXRX_BUFFER_SIZE];
static uint16_t replyLength;
static uint16_t replyIndex;

/** @brief Finish a bounded receive before STOP or a repeated START. */
static void finishReceive() {
  if (!receiving)
    return;
  receiving = false;
  owner->slaveRxNbData = owner->i2cTxRxBufferSize;
  owner->i2cTxRxBufferSize = 0;
  if (owner->slaveRxNbData && owner->i2c_onSlaveReceive)
    owner->i2c_onSlaveReceive(owner);
}

/** @brief Drain the hardware even when the application's receive buffer is
 * full. */
static void receiveBytes() {
  // The data register and shift register hold at most two pending bytes.
  // Bound ISR work even if another byte arrives while servicing this event.
  for (unsigned count = 0; count < 2 && (I2C1->STAR1 & I2C_STAR1_RXNE);
       ++count) {
    uint8_t data = I2C1->DATAR;
    if (receiving && owner->i2cTxRxBufferSize < I2C_TXRX_BUFFER_SIZE)
      owner->i2cTxRxBuffer[owner->i2cTxRxBufferSize++] = data;
  }
}

/** @brief Claim I2C1 for interrupt-driven, seven-bit target transfers. */
void i2c_target_init(i2c_t *obj, uint32_t ownAddress) {
  if (owner && owner != obj)
    return;
  if (obj->isMaster || obj->sda != PB_9 || obj->scl != PB_8 ||
      obj->NoStretchMode || ownAddress < (0x08 << 1) ||
      ownAddress > (0x77 << 1))
    return;
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
  I2C_DeInit(I2C1);
  GPIO_PinRemapConfig(GPIO_Remap_I2C1, ENABLE);
  GPIO_InitTypeDef pins = {};
  pins.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
  pins.GPIO_Mode = GPIO_Mode_AF_OD;
  pins.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_SetBits(GPIOB, pins.GPIO_Pin);
  GPIO_Init(GPIOB, &pins);

  I2C_InitTypeDef config = {};
  config.I2C_ClockSpeed = 100000;
  config.I2C_Mode = I2C_Mode_I2C;
  config.I2C_DutyCycle = I2C_DutyCycle_2;
  config.I2C_OwnAddress1 = ownAddress;
  config.I2C_Ack = I2C_Ack_Enable;
  config.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
  I2C_Init(I2C1, &config);
  I2C_Cmd(I2C1, DISABLE);
  I2C_GeneralCallCmd(I2C1, obj->generalCall ? ENABLE : DISABLE);
  obj->i2c = I2C1;
  obj->handle.Instance = I2C1;
  obj->i2cTxRxBufferSize = 0;
  obj->slaveRxNbData = 0;
  receiving = transmitting = fillingReply = false;
  replyLength = replyIndex = 0;
  owner = obj;
  NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
  NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
  I2C_ITConfig(I2C1, I2C_IT_EVT | I2C_IT_ERR, ENABLE);
  NVIC_EnableIRQ(I2C1_EV_IRQn);
  NVIC_EnableIRQ(I2C1_ER_IRQn);
  I2C_Cmd(I2C1, ENABLE);
  // Disabling PE during setup clears ACK on this peripheral. Restore it only
  // after enabling the target, so the first address can be acknowledged.
  I2C_AcknowledgeConfig(I2C1, ENABLE);
}

/** @brief Stop interrupts and release only this instance's actual hardware. */
void i2c_target_deinit(i2c_t *obj) {
  if (owner != obj)
    return;
  NVIC_DisableIRQ(I2C1_EV_IRQn);
  NVIC_DisableIRQ(I2C1_ER_IRQn);
  I2C_DeInit(I2C1);
  NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
  NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
  pinMode(pinNametoDigitalPin(PB_9), INPUT);
  pinMode(pinNametoDigitalPin(PB_8), INPUT);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, DISABLE);
  owner = nullptr;
  receiving = transmitting = fillingReply = false;
  obj->handle.Instance = nullptr;
  obj->i2c = nullptr;
}

/** @brief Append a bounded reply inside onRequest, without waiting for bus
 * traffic. */
i2c_status_e i2c_slave_write(i2c_t *obj, uint8_t *data, uint16_t size) {
  if (owner != obj || !fillingReply || (!data && size))
    return I2C_ERROR;
  if (size > I2C_TXRX_BUFFER_SIZE - replyLength)
    return I2C_DATA_TOO_LONG;
  if (size)
    memcpy(reply + replyLength, data, size);
  replyLength += size;
  return I2C_OK;
}

/** @brief Attach the Wire receive bridge before enabling interrupts. */
void i2c_attachSlaveRxEvent(i2c_t *obj, void (*function)(i2c_t *)) {
  obj->i2c_onSlaveReceive = function;
}

/** @brief Attach the Wire request bridge before enabling interrupts. */
void i2c_attachSlaveTxEvent(i2c_t *obj, void (*function)(i2c_t *)) {
  obj->i2c_onSlaveTransmit = function;
}

extern "C" void I2C1_EV_IRQHandler(void)
    __attribute__((interrupt("WCH-Interrupt-fast")));
extern "C" void I2C1_ER_IRQHandler(void)
    __attribute__((interrupt("WCH-Interrupt-fast")));

/** @brief Clear a previous NACK/error before accepting another address. */
static void clearErrors() {
  uint16_t errors =
      I2C1->STAR1 &
      (I2C_STAR1_AF | I2C_STAR1_BERR | I2C_STAR1_ARLO | I2C_STAR1_OVR |
       I2C_STAR1_PECERR | I2C_STAR1_TIMEOUT | I2C_STAR1_SMBALERT);
  if (!errors)
    return;
  // Error flags are cleared by writing zero (RM 18.12.6).
  I2C1->STAR1 = (uint16_t)~errors;
  transmitting = receiving = false;
  owner->i2cTxRxBufferSize = 0;
  I2C_ITConfig(I2C1, I2C_IT_BUF, DISABLE);
  I2C1->CTLR1 |= I2C_CTLR1_ACK;
}

/** @brief Service address, data and STOP events without polling a controller.
 */
void I2C1_EV_IRQHandler(void) {
  if (!owner)
    return;
  // Event and error IRQs can both be pending after a short read followed by
  // another START. Do not let a late error IRQ cancel the new transaction.
  clearErrors();
  uint16_t status = I2C1->STAR1;
  receiveBytes();
  if (status & I2C_STAR1_STOPF) {
    // Finish the previous transaction before a possibly pending new address.
    // RM 18.12.6: read STAR1, then write CTLR1 to clear STOPF.
    (void)I2C1->STAR1;
    I2C1->CTLR1 |= I2C_CTLR1_PE | I2C_CTLR1_ACK;
    finishReceive();
    transmitting = false;
    I2C_ITConfig(I2C1, I2C_IT_BUF, DISABLE);
  }
  if (status & I2C_STAR1_ADDR) {
    // Deliver a register-selection write before invoking its read callback.
    finishReceive();
    // RM 18.12.6: reading STAR1 then STAR2 clears ADDR and releases SCL.
    (void)I2C1->STAR1;
    uint16_t direction = I2C1->STAR2;
    transmitting = (direction & I2C_STAR2_TRA) != 0;
    receiving = !transmitting;
    owner->i2cTxRxBufferSize = 0;
    if (transmitting) {
      replyLength = replyIndex = 0;
      fillingReply = true;
      if (owner->i2c_onSlaveTransmit)
        owner->i2c_onSlaveTransmit(owner);
      fillingReply = false;
    }
    I2C_ITConfig(I2C1, I2C_IT_BUF, ENABLE);
  }
  if (transmitting && (I2C1->STAR1 & I2C_STAR1_TXE)) {
    // A controller may read more than the callback supplied. Pad with 0xff;
    // never repeat callbacks or leave the bus stretched indefinitely.
    uint8_t data = 0xff;
    if (replyIndex < replyLength)
      data = reply[replyIndex++];
    I2C1->DATAR = data;
  }
}

/** @brief End a read on controller NACK and recover from bus errors. */
void I2C1_ER_IRQHandler(void) {
  if (owner)
    clearErrors();
}
#endif
