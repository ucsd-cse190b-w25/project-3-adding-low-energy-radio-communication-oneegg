/*
 * timer.c
 *
 *  Created on: Oct 5, 2023
 *      Author: schulman
 */

#include "timer.h"
#include <stm32l475xx.h>


void timer_init(TIM_TypeDef* timer)
{
	/* Resets/enable clock source */
	RCC->APB1RSTR1 |= RCC_APB1RSTR1_TIM2RST;
	RCC->APB1RSTR1 &= ~RCC_APB1RSTR1_TIM2RST;
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

	/* Make sure timer is off */
	timer->CR1 &= ~TIM_CR1_CEN;

	/* Reset timer state */
	timer->ARR = 0;
	timer->CNT = 0;

	/* Change timer direction (up-counting) */
	timer->CR1 &= ~(TIM_CR1_DIR | TIM_CR1_CMS | TIM_CR1_CKD);

	/* Use internal clock */
	timer->SMCR &= ~TIM_SMCR_SMS;

	/* 4MHz / 4000 PSC = 1kHZ speed for timer */
	timer->PSC = 4000 - 1;
	timer->ARR = 1000 - 1;

	/* Enable timer interrupt */
	timer->DIER |= TIM_DIER_UIE;

	/* Set priority and enable the NVIC timer interrupt */
	NVIC_SetPriority(TIM2_IRQn, 1);
	NVIC_EnableIRQ(TIM2_IRQn);

	/* Send update event to update values */
	timer->EGR = TIM_EGR_UG;

	/* Enable timer */
	timer->CR1 |= TIM_CR1_CEN;
}

void timer_reset(TIM_TypeDef* timer)
{
	timer->CNT = 0;
}

void timer_set_ms(TIM_TypeDef* timer, uint16_t period_ms)
{
	/* New timer period; remember speed is still 1kHz */
	timer->ARR = period_ms - 1;

	/* Send update event to update values */
	timer->EGR = TIM_EGR_UG;
}
