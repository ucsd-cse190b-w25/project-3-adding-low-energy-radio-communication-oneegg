/*
 * lsm6dsl.c
 *
 *  Created on: Feb 3, 2025
 *      Author: miles
 */

#include "lsm6dsl.h"

void lsm6dsl_init()
{
	uint8_t command_length = 2;

	// CTRL6_C = 0001 0000 -> disables high-performance, allows lowest ODR
	// uint8_t enable_lp_mode[2] = {0x15, 0x10};

	// CTRL1_XL = {odr} 0000 -> sets the ODR (Hz) of the accelerometer
	uint8_t set_odr[2] = {0x10, 0x10};

	// INT1_CTRL = 0000 0001 -> enables data-ready interrupt
	uint8_t set_interrupt_data_rdy[2] = {0x0d, 0x01};

	// Write startup sequence to accelerometer over I2C
	// i2c_transaction(0x6a, 0, enable_lp_mode, 2);
	i2c_transaction(0x6a, 0, set_odr, 2);
	i2c_transaction(0x6a, 0, set_interrupt_data_rdy, 2);

}

void lsm6dsl_read_xyz(int16_t* x, int16_t* y, int16_t* z)
{
	uint8_t register_addr = 0x28;
	int16_t* coordinates[] = {x, y, z};

	// Get accelerometer data for each coordinate
	for (int i = 0; i < 3; i++) {
		// Lower-order bit
		i2c_transaction(0x6a, 0, &register_addr, 1);
		i2c_transaction(0x6a, 1, (uint8_t*)coordinates[i], 1);
		register_addr++;

		// Higher-order bit
		i2c_transaction(0x6a, 0, &register_addr, 1);
		i2c_transaction(0x6a, 1, ((uint8_t*)coordinates[i] + 1), 1);
		register_addr++;
	}
}

