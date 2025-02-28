/*
 * leds.c
 *
 *  Created on: Oct 3, 2023
 *      Author: schulman
 */


/* Include memory map of our MCU */
#include <stm32l475xx.h>

void leds_init()
{
	/* Enable GPIO (GPIOA and GPIOB) */
	RCC->AHB2ENR |= (RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN);

	/* Configure PA5 (LED1) as an output by clearing all bits and setting the mode */
	GPIOA->MODER &= ~GPIO_MODER_MODE5;
	GPIOA->MODER |= GPIO_MODER_MODE5_0;

	/* Configure the GPIO output as push pull (transistor for high and low) */
	GPIOA->OTYPER &= ~GPIO_OTYPER_OT5;

	/* Disable the internal pull-up and pull-down resistors */
	GPIOA->PUPDR &= GPIO_PUPDR_PUPD5;

	/* Configure the GPIO to use low speed mode */
	GPIOA->OSPEEDR |= (0x3 << GPIO_OSPEEDR_OSPEED5_Pos);

	/* Repeat the same steps for PB14 (LED2) */
	GPIOB->MODER &= ~GPIO_MODER_MODE14;
	GPIOB->MODER |= GPIO_MODER_MODE14_0;
	GPIOB->OTYPER &= ~GPIO_OTYPER_OT14;
	GPIOB->PUPDR &= GPIO_PUPDR_PUPD14;
	GPIOB->OSPEEDR |= (0x3 << GPIO_OSPEEDR_OSPEED14_Pos);

	/* Turn off the LEDs */
	GPIOA->ODR &= ~GPIO_ODR_OD5;
	GPIOB->ODR &= ~GPIO_ODR_OD14;
}

void leds_set(uint8_t led)
{
	/* Lower-order bit -> LED1 */
	if (led & 1) {
		GPIOA->ODR |= GPIO_ODR_OD5;
	} else {
		GPIOA->ODR &= ~GPIO_ODR_OD5;
	}

	/* Higher-order bit -> LED2 */
	if (led & 2) {
		GPIOB->ODR |= GPIO_ODR_OD14;
	} else {
		GPIOB->ODR &= ~GPIO_ODR_OD14;
	}
}
