/*
 * lsm6dsl.c
 *
 *  Created on: Feb 3, 2025
 *      Author: miles
 */

#include "lsm6dsl.h"

void lsm6dsl_init()
{
	/* Startup sequence - configure accelerometer (app note section 4.1) */
	uint8_t data[] = {0x10, 0x60, 0x0d, 0x01};
	uint8_t lp_data[] = {0x15, 0x90, 0x10, 0xb0, 0x0d, 0x01};

	i2c_transaction(0x6a, 0, data, 2);
	i2c_transaction(0x6a, 0, (data + 2), 2);
	// i2c_transaction(0x6a, 0, (lp_data + 4), 2);

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

