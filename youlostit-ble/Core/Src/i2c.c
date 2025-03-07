/*
 * i2c.c
 *
 *  Created on: Jan 31, 2025
 *      Author: miles
 */

#include <i2c.h>

/* Include memory map of our MCU */
#include <stm32l475xx.h>

void i2c_init()
{
	// Reset I2C bus
	I2C2->CR1 &= 0;

	// Enable I2C & GPIO peripheral clock
	RCC->APB1ENR1 |= RCC_APB1ENR1_I2C2EN;
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

	// Sets I2C clock source to use the system clock
	RCC->CCIPR |= RCC_CCIPR_I2C2SEL_0;
	RCC->CCIPR &= ~RCC_CCIPR_I2C2SEL_1;

	/* BAUD RATE configuration - following Table 235 in the reference manual */
	I2C2->TIMINGR &= ~(I2C_TIMINGR_PRESC | I2C_TIMINGR_SCLH | I2C_TIMINGR_SCLL);

	// System clock starts at 4 MHz, so no prescaling needed
	// Set SCL high/low period
	I2C2->TIMINGR |= I2C_TIMINGR_SCLH_F;
	I2C2->TIMINGR |= I2C_TIMINGR_SCLL_13;

	// Set SCLDEL and SDADEL to 1
	I2C2->TIMINGR |= I2C_TIMINGR_SCLDEL_4;
	I2C2->TIMINGR |= I2C_TIMINGR_SDADEL_2;


	/* GPIO configuration - similar to in leds.c */
	GPIOB->MODER &= ~GPIO_MODER_MODE10;
	GPIOB->MODER &= ~GPIO_MODER_MODE11;

	GPIOB->MODER |= GPIO_MODER_MODE10_1;
	GPIOB->MODER |= GPIO_MODER_MODE11_1;

	GPIOB->OTYPER |= GPIO_OTYPER_OT10;
	GPIOB->OTYPER |= GPIO_OTYPER_OT11;

	GPIOB->PUPDR |= GPIO_PUPDR_PUPD10;
	GPIOB->PUPDR |= GPIO_PUPDR_PUPD11;

	GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEED10;
	GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEED11;

	// Clear alternate function register
	GPIOB->AFR[1] &= 0;

	// Map GPIO pins PB10/PB11 to use the alternate functions for I2C
	GPIOB->AFR[1] |= GPIO_AFRH_AFSEL10_2;
	GPIOB->AFR[1] |= GPIO_AFRH_AFSEL11_2;

	// Enable I2C
	I2C2->CR1 |= I2C_CR1_PE;
}

uint8_t i2c_transaction(uint8_t address, uint8_t dir, uint8_t* data, uint8_t len)
{
	/* Set data transfer direction - 0 write, 1 read */
	if (dir == 0) {
		I2C2->CR2 &= ~I2C_CR2_RD_WRN;
	} else {
		I2C2->CR2 |= I2C_CR2_RD_WRN;
	}

	// Set address, turn on autoend, set number of bytes, and restart start generation
	I2C2->CR2 |= (address << 1) | I2C_CR2_AUTOEND | (uint32_t) (len << 16) | I2C_CR2_START;

	if ((I2C2->ISR & I2C_ISR_NACKF) != 0) {
		// Dang we got NACKed
		printf("We got NACKed\n");
		return 0;
	}

	if (dir == 0) {
		/* Writing to secondary device */

		for (int i = 0; i < len; i++) {
			// Write a byte of data to TXDR
			I2C2->TXDR = data[i];

			// do nothing until TXIS is 1
			while ((I2C2->ISR & I2C_ISR_TXE) == 0) {}
		}
	}
	else {
		/* Reading from secondary device */
		for (int i = 0; i < len; i++) {
			// do nothing until RXNE is 1
			while ((I2C2->ISR & I2C_ISR_RXNE) == 0) {}

			// Read a byte of data
			data[i] = I2C2->RXDR;
		}
	}

	I2C2->CR2 = 0;

	return 1;
}

