//
// This file is part of the GNU ARM Eclipse distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

// ----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "diag/Trace.h"
#include "cmsis/cmsis_device.h"
#include "ctype.h"
#include <sys/stat.h>
#include "stm32f4xx.h"

// ----------------------------------------------------------------------------
//
// Standalone STM32F4 led blink sample (trace via DEBUG).
//
// In debug configurations, demonstrate how to print a greeting message
// on the trace device. In release configurations the message is
// simply discarded.
//
// Then demonstrates how to blink a led with 1 Hz, using a
// continuous loop and SysTick delays.
//
// Trace support is enabled by adding the TRACE macro definition.
// By default the trace messages are forwarded to the DEBUG output,
// but can be rerouted to any device or completely suppressed, by
// changing the definitions required in system/src/diag/trace_impl.c
// (currently OS_USE_TRACE_ITM, OS_USE_TRACE_SEMIHOSTING_DEBUG/_STDOUT).
//

// ----- Timing definitions -------------------------------------------------

// Keep the LED on for 2/3 of a second.
#define BLINK_ON_TICKS  (TIMER_FREQUENCY_HZ * 3 / 4)
#define BLINK_OFF_TICKS (TIMER_FREQUENCY_HZ - BLINK_ON_TICKS)

// ----- main() ---------------------------------------------------------------

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

void binary_iterator(int M, int N, int O, int P) {

	if (P == 0 && O == 0 && N == 0 && M == 0) {
		P = 1;
	}

	else if (P == 1 && O == 0 && N == 0 && M == 0) {
		O = 1;
		P = 0;
	} else if (P == 0 && O == 1 && N == 0 && M == 0) {
		P = 1;
	} else if (P == 1 && O == 1 && N == 0 && M == 0) {
		P = 0;
		O = 0;
		N = 1;
	} else if (P == 0 && O == 0 && N == 1 && M == 0) {
		P = 1;
	} else if (P == 1 && O == 0 && N == 1 && M == 0) {
		N = 1;
	} else if (P == 1 && O == 1 && N == 1 && M == 0) {

		P = 0;
		O = 0;
		N = 0;
		M = 1;
	}

	else if (P == 1 && O == 1 && N == 0 && M == 0) {
		N = 1;
		O = 0;
	} else if (P == 1 && O == 1 && N == 1 && M == 0) {
		M = 1;
		N = 0;
	}

	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, O);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, P);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, N);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, M);

}

int main(int argc, char* argv[]) {

	HAL_Init();
	__HAL_RCC_GPIOE_CLK_ENABLE()
	;
	__HAL_RCC_GPIOD_CLK_ENABLE()
	;
	__HAL_RCC_GPIOA_CLK_ENABLE()
	;

	GPIO_InitTypeDef GPIO_InitStructure;
	//GPIO_InitTypeDef GPIO_Init;
	// Send a greeting to the trace device (skipped on Release).
	//trace_puts("Hello ARM World!");

	// At this stage the system clock should have already been configured
	// at high speed.
	//trace_printf("System clock: %u Hz\n", SystemCoreClock);

	//timer_start();
	GPIO_InitStructure.Pin = GPIO_PIN_0;
	GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Alternate = 0;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14
			| GPIO_PIN_15;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Alternate = 0;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStructure);

	//Seven Segment Pins
	GPIO_InitStructure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11
			| GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14;
	GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Alternate = 0;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStructure);

	trace_printf("Sys Clock Frequency %u\r\n", HAL_RCC_GetSysClockFreq());
	trace_printf("HClk frequency %u\r\n", HAL_RCC_GetHCLKFreq());
	trace_printf("PClk 1 frequency %u\r\n", HAL_RCC_GetPCLK1Freq());
	trace_printf("PClk 2 frequency %u\r\n", HAL_RCC_GetPCLK2Freq());
	//uint32_t seconds = 0;
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, 0);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, 0);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, 0);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, 0);

	__HAL_RCC_TIM3_CLK_ENABLE()
	;	// enable clock for Timer 3

	TIM_HandleTypeDef s_TimerInstance; // define a handle to initialize timer

	s_TimerInstance.Instance = TIM3; // Point to Timer 3
	s_TimerInstance.Init.Prescaler = 83; // Timer clock frequency is 84 MHz.
	// This prescaler will give 10 kHz timing_tick_frequency
	s_TimerInstance.Init.CounterMode = TIM_COUNTERMODE_UP;
	s_TimerInstance.Init.Period = 49999; // To count until 50 milliseconds.
	// http://cerdemir.com/timers-in-stm32f4-discovery.
	//see the above link for Prescaler and period settings
	s_TimerInstance.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	s_TimerInstance.Init.RepetitionCounter = 0;
	HAL_TIM_Base_Init(&s_TimerInstance);// Initialize timer with above parameters

	HAL_TIM_Base_Start(&s_TimerInstance); // start timer

	int previousStateF = 0;
	int confirmationF = 0;
	int reset = 0, reset2 = 0;
	int timer = 0;

	// Infinite loop
	while (1) {
		//Check button and timer
		int timerValue = __HAL_TIM_GET_COUNTER(&s_TimerInstance);
		int pin1 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
		//Debounce code, check if the debounce time has been reached
		if (timer == timerValue && previousStateF == 1) {
			//If button is still pushed down, confirm the signal
			if (pin1 == 1 && reset2 == 0) {
				confirmationF += 1;
				reset2 = 1;
			}
			//If the button is released, reset everything
			else if (pin1 == 0) {
				previousStateF = 0;
				reset = 0;
				reset2 = 0;
			}
		}

		//Check if the button was pushed and ensure this statement
		//is not used multiple times so the timer value doesn't
		//change constantly
		if (pin1 == 1 && reset == 0) {
			previousStateF = 1;
			timer = timerValue;
			reset = 1;
		}
		//

		//Using the confirmation flag to turn on and off the LED
		if (confirmationF > 9) {
			HAL_GPIO_WritePin( GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
			//confirmationF = 0;
		} else {
			HAL_GPIO_WritePin( GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
		}
	}

}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
