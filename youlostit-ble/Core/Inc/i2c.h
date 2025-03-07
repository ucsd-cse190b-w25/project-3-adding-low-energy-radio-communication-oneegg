/*
 * i2c.h
 *
 *  Created on: Jan 31, 2025
 *      Author: miles
 */

#include <stdint.h>

#ifndef I2C_H_
#define I2C_H_

#define I2C_TIMINGR_SCLH_F 0x00000F00
#define I2C_TIMINGR_SCLL_13 0x00000013

#define I2C_TIMINGR_SCLH_3 0x00000300
#define I2C_TIMINGR_SCLL_6 0x00000006

#define I2C_TIMINGR_SCLDEL_4 0x00400000
#define I2C_TIMINGR_SDADEL_2 0x00020000

#define I2C_TIMINGR_SCLDEL_1 0x00100000
#define I2C_TIMINGR_SDADEL_1 0x00010000

void i2c_init();

uint8_t i2c_transaction(uint8_t address, uint8_t dir, uint8_t* data, uint8_t len);

#endif /* I2C_H_ */
