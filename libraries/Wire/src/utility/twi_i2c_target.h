// SPDX-FileCopyrightText: Copyright (c) 2026 Adafruit Industries
// SPDX-License-Identifier: BSD-3-Clause
#pragma once
#include "twi.h"

#if defined(WIRE_I3C) && defined(CH32V4x7) && OPT_I2C_SLAVE
void i2c_target_init(i2c_t *obj, uint32_t ownAddress);
void i2c_target_deinit(i2c_t *obj);
#endif
