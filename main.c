//
// This file is part of the GNU ARM Eclipse distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

// ----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "diag/Trace.h"
#include "fatfs.h"
#include "usb_host.h"
#include "stm32f4xx_hal.h"
#include "Timer.h"
#include "BlinkLed.h"
#include "PlayMP3.h"
#include "cortexm/ExceptionHandlers.h"
#include "generic.h"
#include "timeKeeping.h"
#include "DebugPort.h"
#include "AudioChip.h"

// Disable specific warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

//Define button i/o pins
#define button_hour			(GPIO_PIN_4)
#define button_minute		(GPIO_PIN_5)
#define button_set_time	(GPIO_PIN_8)
#define button_set_alarm	(GPIO_PIN_9)
#define button_snooze	(GPIO_PIN_11)
// ----------------------------------------------------------------------------
//
// Standalone STM32F4 Simple Alarm Clock Stub Code
//
// This code just plays an MP3 file off of a connected USB flash drive.
//
// Trace support is enabled by adding the TRACE macro definition.
// By default the trace messages are forwarded to the DEBUG output,
// but can be rerouted to any device or completely suppressed, by
// changing the definitions required in system/src/diag/trace_impl.c
// (currently OS_USE_TRACE_ITM, OS_USE_TRACE_SEMIHOSTING_DEBUG/_STDOUT).
//

void SetTime(void), SetAlarm(void), Alarm(void), Snooze(void),
		SystemClock_Config(void), MX_GPIO_Init(void), MX_I2C1_Init(void),
		MX_USB_HOST_Process(void);

//uint16_t
//	CheckButtons( void );

// STMCube Example declarations.
// static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);

static void
MSC_Application(void);

//static void
//Error_Handler(void);
//
// Global variables
//
//RTC_InitTypeDef
//	ClockInit;				// Structure used to initialize the real time clock
//
RTC_TimeTypeDef ClockTime; // Structure to hold/store the current time

RTC_DateTypeDef ClockDate; // Structure to hold the current date

RTC_AlarmTypeDef ClockAlarm; // Structure to hold/store the current alarm time

TIM_HandleTypeDef Timer6_44Khz,	// Structure for the audio play back timer subsystem
		DisplayTimer;			// Structure for the LED display timer subsystem

DAC_HandleTypeDef AudioDac;	// Structure for the audio digital to analog converter subsystem

DMA_HandleTypeDef AudioDma;	// Structure for the audio DMA direct memory access controller subsystem

RTC_HandleTypeDef RealTimeClock;// Structure for the real time clock subsystem

I2C_HandleTypeDef	// Structure for I2C subsystem. Used for external audio chip
I2c;

volatile int DisplayClockModeCount,	// Number of display ticks to show the current clock mode time format
		PlayMusic = FALSE,		// Flag indicating if music should be played
		DebounceCount = 0;		// Buttons debounce count

volatile uint16_t ButtonsPushed;// Bit field containing the bits of which buttons have been pushed

FATFS UsbDiskFatFs;			// File system object for USB disk logical drive

char UsbDiskPath[4];			// USB Host logical drive path

int BcdTime[4],				// Array to hold the hours and minutes in BCD format
		DisplayedDigit = 0,	// Current digit being displayed on the LED display

		// Current format for the displayed time ( IE 12 or 24 hour format )
		ClockHourFormat = CLOCK_HOUR_FORMAT_12, AlarmPmFlag = 0, TimePmFlag = 0;

//
// Functions required for long files names on fat32 partitions
//

WCHAR ff_convert(WCHAR wch, UINT dir) {
	if (wch < 0x80) {
//
// ASCII Char
//
		return wch;
	}

//
// unicode not supported
//
	return 0;
}

WCHAR ff_wtoupper(WCHAR wch) {
	if (wch < 0x80) {
//
// ASCII Char
//
		if (wch >= 'a' && wch <= 'z') {
			wch &= ~0x20;
		}

		return wch;
	}

//
// unicode not supported
//
	return 0;
}

//
// Dummy interrupt handler function
//
/******************************************************************************************************************************************************
 void TIM6_DAC_IRQHandler(void) {
 HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn);
 }
 ******************************************************************************************************************************************************/
/*
 * Function: ConfigureAudioDma
 *
 * Description:
 *
 * Initialize DMA, DAC and timer 6 controllers for a mono channel audio to be played on PA4
 *
 */

void ConfigureAudioDma(void) {

	TIM_MasterConfigTypeDef Timer6MasterConfigSync;

	GPIO_InitTypeDef GPIO_InitStructure;

	DAC_ChannelConfTypeDef DacConfig;

//
// If we have the timer 6 interrupt enabled then disable the timer from running when we halt the processor or hit a breakpoint.
// This also applies to printing using the semihosting method which also uses breakpoints to transfer data to the host computer
//

	__HAL_DBGMCU_UNFREEZE_TIM5();

//
// Enable the clocks for GPIOA, GPIOC and Timer 6
//
	__HAL_RCC_TIM6_CLK_ENABLE()
	;
	__HAL_RCC_GPIOA_CLK_ENABLE()
	;

//
// Configure PA4 as an analog output ( used for D/A output of the analog signal ), and PA0 as the onboard blue button input
//

	GPIO_InitStructure.Pin = GPIO_PIN_4;
	GPIO_InitStructure.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStructure.Alternate = 0;
	HAL_GPIO_Init( GPIOA, &GPIO_InitStructure);

//
// Configure PA4 as an analog output ( used for D/A output of the analog signal ), and PA0 as the onboard blue button input
//
	GPIO_InitTypeDef GPIO_Init5;
	GPIO_Init5.Pin = GPIO_PIN_0;
	GPIO_Init5.Speed = GPIO_SPEED_MEDIUM;
	GPIO_Init5.Mode = GPIO_MODE_INPUT;
	GPIO_Init5.Pull = GPIO_PULLDOWN;
	GPIO_Init5.Alternate = 0;
	HAL_GPIO_Init( GPIOA, &GPIO_Init5);

//
// Configure timer 6 for a clock frequency of 44Khz and a triggered output for the DAC
//
	Timer6_44Khz.Instance = TIM6;
	Timer6_44Khz.Init.Prescaler = 20; //this value may have to be changed
	Timer6_44Khz.Init.CounterMode = TIM_COUNTERMODE_UP;
	Timer6_44Khz.Init.Period = 90; // this value may have to be changed
	Timer6_44Khz.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	HAL_TIM_Base_Init(&Timer6_44Khz);

	Timer6MasterConfigSync.MasterOutputTrigger = TIM_TRGO_UPDATE;
	Timer6MasterConfigSync.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	HAL_TIMEx_MasterConfigSynchronization(&Timer6_44Khz,
			&Timer6MasterConfigSync);

//
// Set the priority of the interrupt and enable it
//
	NVIC_SetPriority(TIM6_DAC_IRQn, 0);
	NVIC_EnableIRQ(TIM6_DAC_IRQn);

//
// Clear any pending interrupts
//
	__HAL_TIM_CLEAR_FLAG(&Timer6_44Khz, TIM_SR_UIF);

//
// Enable the timer interrupt and the DAC Trigger
//

	__HAL_TIM_ENABLE_DMA(&Timer6_44Khz, TIM_DIER_UDE);

//
// Enable the clocks for the DAC
//
	__HAL_RCC_DAC_CLK_ENABLE()
	;

	AudioDac.Instance = DAC;
	if (HAL_OK != HAL_DAC_Init(&AudioDac)) {
		trace_printf("DAC initialization failure\n");
		return;
	}

//
// Enable the trigger from the DMA controller and the output buffer of the DAC
//
	DacConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
	DacConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;

	if (HAL_DAC_ConfigChannel(&AudioDac, &DacConfig, DAC_CHANNEL_1) != HAL_OK) {
		trace_printf("DAC configuration failure\n");
		return;
	}

//
// Enable the clock for the DMA controller
//
	__HAL_RCC_DMA1_CLK_ENABLE()
	;

//
// Initialize the stream and channel number and the memory transfer settings
//

	AudioDma.Instance = DMA1_Stream5;
	AudioDma.Init.Channel = DMA_CHANNEL_7;
	AudioDma.Init.Direction = DMA_MEMORY_TO_PERIPH;
	AudioDma.Init.PeriphInc = DMA_PINC_DISABLE;
	AudioDma.Init.MemInc = DMA_MINC_ENABLE;
	AudioDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
	AudioDma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
	AudioDma.Init.Mode = DMA_NORMAL;
	AudioDma.Init.Priority = DMA_PRIORITY_MEDIUM;
	AudioDma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
	HAL_DMA_Init(&AudioDma);

//
// Link the DMA channel the to the DAC controller
//
	__HAL_LINKDMA(&AudioDac, DMA_Handle1, AudioDma);

//
// Enable the interrupt for the specific stream
//
	HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

//
// Start the timer
//
	__HAL_TIM_ENABLE(&Timer6_44Khz);

	return;
}

void ConfigureDisplay(void) {

	//
	// Enable clocks for PWR_CLK for RTC, GPIOE, GPIOD, GPIOC and TIM5.
	//
	__HAL_RCC_PWR_CLK_ENABLE()
	;
	__HAL_RCC_GPIOE_CLK_ENABLE()
	;
	__HAL_RCC_GPIOC_CLK_ENABLE()
	;
	__HAL_RCC_GPIOA_CLK_ENABLE()
	;
	__HAL_RCC_GPIOD_CLK_ENABLE()
	;
	__HAL_RCC_TIM5_CLK_ENABLE()
	;
	//
	// Enable the LED multiplexing display and push button timer (TIM5) at a frequency of 500Hz
	//
	//
	// Configure GPIO for receiving blue onboard button input
	// Use pin 0 of port A
	//
	GPIO_InitTypeDef GPIO_Init;
	GPIO_Init.Pin = GPIO_PIN_0;
	GPIO_Init.Speed = GPIO_SPEED_MEDIUM;
	GPIO_Init.Mode = GPIO_MODE_INPUT;
	GPIO_Init.Pull = GPIO_PULLDOWN;
	GPIO_Init.Alternate = 0;
	HAL_GPIO_Init( GPIOA, &GPIO_Init);
	//
	// Configure GPIO for selecting each segment on a digit.
	// Use free I/O pins of port E (pin 6 onwards).
	//
	GPIO_InitTypeDef GPIO_Init2; //A handle to initialize GPIO port E
	GPIO_Init2.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11
			| GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	GPIO_Init2.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_Init2.Speed = GPIO_SPEED_HIGH;
	GPIO_Init2.Pull = GPIO_NOPULL;
	GPIO_Init2.Alternate = 0;
	HAL_GPIO_Init(GPIOE, &GPIO_Init2);
	//
	// Configure GPIO for selecting each digit on the LED display.
	// Use pin 7 to 11 of port D.
	//
	GPIO_InitTypeDef GPIO_Init3;
	GPIO_Init3.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10
			| GPIO_PIN_11;
	GPIO_Init3.Speed = GPIO_SPEED_MEDIUM;
	GPIO_Init3.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_Init3.Pull = GPIO_NOPULL;
	GPIO_Init3.Alternate = 0;
	HAL_GPIO_Init( GPIOD, &GPIO_Init3);
	//
	// Configure the input pins 4, 5, 8, 9 and 11 of port C for reading the push buttons.
	// Use internal pull ups to reduce component count
	//
	GPIO_InitTypeDef GPIO_Init4;
	GPIO_Init4.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_9
			| GPIO_PIN_11;
	GPIO_Init4.Speed = GPIO_SPEED_MEDIUM;
	GPIO_Init4.Mode = GPIO_MODE_INPUT;
	GPIO_Init4.Pull = GPIO_PULLUP;
	GPIO_Init4.Alternate = 0;
	HAL_GPIO_Init( GPIOC, &GPIO_Init4);

	GPIO_Init4.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 ;
	GPIO_Init4.Speed = GPIO_SPEED_MEDIUM;
	GPIO_Init4.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_Init4.Pull = GPIO_NOPULL;
	GPIO_Init4.Alternate = 0;
	HAL_GPIO_Init( GPIOD, &GPIO_Init4);
	// Enable the real time clock alarm A interrupt
	//

	//
	// Enable the timer interrupt
	//

	//
	// Enable the LED display and push button timer
	//
	//
}

//variable used in SetTime()
int settingtime = 0;

//variable used in SetAlarm()
int settingalarm = 0;

//Variables to store the alarm time
int alarmd1 = -1;
int alarmd2 = -1;
int alarmd3 = -1;
int alarmd4 = -1;

//Variables to store the current time
int digit1 = 0;
int digit2 = 0;
int digit3 = 0;
int digit4 = 0;

//Variables to store the value of the Colon, Alarm On/Off Indicator, and Decimal Point LEDs
int alarmset = -1;
int colontop = -1;
int colonbot = -1;
int decimalset = -1;

/*
 * Function: SetDigit()
 *
 * Description: This function accepts the integer value of the digit to be set and the integer value to set this digit to.
 * 				The function then sets the desired digit to the desired value.
 */
void SetDigit(int Digit, int Number) {

	switch (Digit) {

////First Digit
	case 1:

		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_8, GPIO_PIN_SET); //Digit 1 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_9, GPIO_PIN_RESET); //Digit 2 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_10, GPIO_PIN_RESET); //Digit 3 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_11, GPIO_PIN_RESET); //Digit 4 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_7, GPIO_PIN_RESET); //DP Select

		switch (Number) {
		case 0:
			digit1 = 0;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);

			break;

		case 1:
			digit1 = 1;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);
			break;

		case 2:
			digit1 = 2;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 3:
			digit1 = 3;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 4:
			digit1 = 4;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 5:
			digit1 = 5;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 6:
			digit1 = 6;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 7:
			digit1 = 7;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);
			break;

		case 8:
			digit1 = 8;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 9:
			digit1 = 9;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		} //switch(Number)

		break; //case 1

////Second Digit
	case 2:

		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_8, GPIO_PIN_RESET); //Digit 1 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_9, GPIO_PIN_SET); //Digit 2 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_10, GPIO_PIN_RESET); //Digit 3 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_11, GPIO_PIN_RESET); //Digit 4 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_7, GPIO_PIN_RESET); //DP Select

		switch (Number) {
		case 0:
			digit2 = 0;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);

			break;

		case 1:
			digit2 = 1;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);
			break;

		case 2:
			digit2 = 2;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 3:
			digit2 = 3;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 4:
			digit2 = 4;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 5:
			digit2 = 5;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 6:
			digit2 = 6;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 7:
			digit2 = 7;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);
			break;

		case 8:
			digit2 = 8;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 9:
			digit2 = 9;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		} //switch(Number)

		break; //case 2

////Third Digit
	case 3:

		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_8, GPIO_PIN_RESET); //Digit 1 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_9, GPIO_PIN_RESET); //Digit 2 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_10, GPIO_PIN_SET); //Digit 3 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_11, GPIO_PIN_RESET); //Digit 4 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_7, GPIO_PIN_RESET); //DP Select

		switch (Number) {
		case 0:
			digit3 = 0;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);

			break;

		case 1:
			digit3 = 1;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);
			break;

		case 2:
			digit3 = 2;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 3:
			digit3 = 3;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 4:
			digit3 = 4;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 5:
			digit3 = 5;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 6:
			digit3 = 6;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 7:
			digit3 = 7;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);
			break;

		case 8:
			digit3 = 8;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 9:
			digit3 = 9;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		} //switch(Number)

		break; //case 3

////Fourth Digit
	case 4:

		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_8, GPIO_PIN_RESET); //Digit 1 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_9, GPIO_PIN_RESET); //Digit 2 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_10, GPIO_PIN_RESET); //Digit 3 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_11, GPIO_PIN_SET); //Digit 4 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_7, GPIO_PIN_RESET); //DP Select

		switch (Number) {
		case 0:
			digit4 = 0;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);

			break;

		case 1:
			digit4 = 1;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);
			break;

		case 2:
			digit4 = 2;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 3:
			digit4 = 3;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 4:
			digit4 = 4;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 5:
			digit4 = 5;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 6:
			digit4 = 6;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 7:
			digit4 = 7;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);
			break;

		case 8:
			digit4 = 8;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		case 9:
			digit4 = 9;
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
			break;

		} //switch(Number)

		break; //case 4

////Decimal Point, Colon, and Alarm On/Off indicator
	case 5:
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_8, GPIO_PIN_RESET); //Digit 1 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_9, GPIO_PIN_RESET); //Digit 2 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_10, GPIO_PIN_RESET); //Digit 3 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_11, GPIO_PIN_RESET); //Digit 4 Select
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_7, GPIO_PIN_SET); //DP Select

		switch (Number) {

//// Alarm Set
		case 1:
			alarmset = 1;
			HAL_GPIO_WritePin( GPIOD, GPIO_PIN_8, GPIO_PIN_SET);
			break;

//// Alarm Reset
		case 10:
			alarmset = 0;
			HAL_GPIO_WritePin( GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);
			break;

//// Colon Top Set
		case 2:
			colontop = 1;
			HAL_GPIO_WritePin( GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
			break;

//// Colon Top Reset
		case 20:
			colontop = 0;
			HAL_GPIO_WritePin( GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
			break;

//// Colon Bottom Set
		case 3:
			colonbot = 1;
			HAL_GPIO_WritePin( GPIOD, GPIO_PIN_15, GPIO_PIN_SET);
			break;

//// Colon Bottom Set
		case 30:
			colonbot = 0;
			HAL_GPIO_WritePin( GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
			break;

//// Decimal Point Set
		case 4:
			decimalset = 1;
			HAL_GPIO_WritePin( GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
			break;

//// Decimal Point Reset
		case 40:
			decimalset = 0;
			HAL_GPIO_WritePin( GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);
			break;

		} //switch(Number)

		break; //case 5

	} //switch(Digit)

} //SetDigit

int main(int argc, char* argv[]) {

	// Reset of all peripherals, Initializes the Flash interface and the System timer.
	HAL_Init();

	// enable clock for Timer 3
	__HAL_RCC_TIM3_CLK_ENABLE()
	;

	TIM_HandleTypeDef s_TimerInstance; // define a handle to initialize timer
	s_TimerInstance.Instance = TIM3; // Point to Timer 3
	s_TimerInstance.Init.Prescaler = 8399; // Timer clock frequency is 84 MHz.
	// This prescaler will give 10 kHz timing_tick_frequency
	s_TimerInstance.Init.CounterMode = TIM_COUNTERMODE_UP;
	s_TimerInstance.Init.Period = 9999; // To count until 1 sec
	// http://cerdemir.com/timers-in-stm32f4-discovery.
	//see the above link for Prescaler and period settings
	s_TimerInstance.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	s_TimerInstance.Init.RepetitionCounter = 0;
	HAL_TIM_Base_Init(&s_TimerInstance); // Initialize timer with above parameters
	HAL_TIM_Base_Start(&s_TimerInstance); // start timer

	// Configure the system clock
	SystemClock_Config();

	// Initialize all configured peripherals
	MX_GPIO_Init();

	// Enable the serial debug port. This allows for text messages to be sent via the STlink virtual communications port to the host computer.
	DebugPortInit();

	// Display project name with version number
	trace_puts("*\n"
			"*\n"
			"* Alarm clock project for stm32f4discovery board V2.00\n"
			"*\n"
			"*\n");

	// Initialize the I2C port for the external CODEC
	MX_I2C1_Init();

	// Configure the CODEC for analog pass through mode.
	// This allows for audio to be played out of the stereo jack
	InitAudioChip();

	// Initialize the flash file and the USB host adapter subsystem
	MX_FATFS_Init();
	MX_USB_HOST_Init();

	// Initialize the DMA and DAC systems. This allows for audio to be played out of GPIOA pin 4
	ConfigureAudioDma();

	// Initialize the seven segment display pins
	ConfigureDisplay();

	// Send a greeting to the trace device (skipped on Release).
	trace_puts("Initialization Complete");

	// At this stage the system clock should have already been configured at high speed.
	trace_printf("System clock: %u Hz\n", SystemCoreClock);

	// Start the system timer
	timer_start();

	blink_led_init();

	while (1) {

		//testing button debounce circuit
		if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4) == 0) {
			HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_12);
		}

		//Check if one minute has passed
		if (__HAL_TIM_GET_FLAG(&s_TimerInstance, TIM_FLAG_UPDATE) != RESET) { //check if timer period flag is set
			__HAL_TIM_CLEAR_IT(&s_TimerInstance, TIM_IT_UPDATE); // clear timer period flag

			//increment clock every second...

			//if the digit4 is 9: set it to zero
			if (digit4 == 9) {
				SetDigit(1, digit1);
				SetDigit(2, digit2);
				SetDigit(3, digit3);
				SetDigit(4, 0);

				//if digit3 is 5 as well: set it to 0
				if (digit3 == 5) {
					SetDigit(1, digit1);
					SetDigit(2, digit2);
					SetDigit(4, digit4);
					SetDigit(3, 0);

					// if digit2 is 9 as well: set it to 0 and increment digit1
					if (digit2 == 9) {
						SetDigit(1, digit1);
						SetDigit(4, digit4);
						SetDigit(3, digit3);
						SetDigit(2, 0);

						digit1++;
						SetDigit(4, digit4);
						SetDigit(2, digit2);
						SetDigit(3, digit3);
						SetDigit(1, digit1);

					} else if (digit2 == 4) {

						//if digit2 is 4 and digit1 is 2: set them to 0
						if (digit1 == 2) {
							SetDigit(1, 0);
							SetDigit(2, 0);
							SetDigit(3, digit3);
							SetDigit(4, digit4);

							//if digit 2 is 4: increment it
						} else {
							digit2++;
							SetDigit(1, digit1);
							SetDigit(4, digit4);
							SetDigit(3, digit3);
							SetDigit(2, digit2);

						} //if digit1 == 2

						//if digit 2 is neither 4 nor 9: increment it
					} else {
						digit2++;
						SetDigit(1, digit1);
						SetDigit(3, digit3);
						SetDigit(4, digit4);
						SetDigit(2, digit2);

					} //if-else digit2 == 9

					// if digit3 is not 9: increment it
				} else {
					digit3++;
					SetDigit(1, digit1);
					SetDigit(2, digit2);
					SetDigit(4, digit4);
					SetDigit(3, digit3);

				} //if-else digit3 == 5

				//if digit4 is not 9: increment it
			} else {
				digit4++;
				SetDigit(1, digit1);
				SetDigit(2, digit2);
				SetDigit(3, digit3);
				SetDigit(4, digit4);

			} //if-else digit4 == 9

			//Display current time while not incrementing clock.
		} else {
			SetDigit(1, digit1);
			HAL_Delay(2);
			SetDigit(2, digit2);
			HAL_Delay(2);
			SetDigit(3, digit3);
			HAL_Delay(2);
			SetDigit(4, digit4);
			HAL_Delay(2);

		} //if-else HAL TIM

		//If set time button pushed: call SetTime()
		if (HAL_GPIO_ReadPin(GPIOC, button_set_time) == 0) {
			settingtime = 1;
			SetTime();
		} //if set time pushed

		//If set alarm button pushed: call SetAlarm()
		if (HAL_GPIO_ReadPin(GPIOC, button_set_alarm) == 0) {
			settingalarm = 1;
			SetAlarm();
		} //if set alarm pushed

		//If it is time for the alarm to go off: call Alarm()
		if (digit1 == alarmd1 && digit2 == alarmd2 && digit3 == alarmd3
				&& digit4 == alarmd4) {
			Alarm();
		} //if alarm

	} //while

} //main

/** System Clock Configuration
 */
void SystemClock_Config(void) {

	RCC_OscInitTypeDef RCC_OscInitStruct;
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

	__HAL_RCC_PWR_CLK_ENABLE()
	;

	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 168;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	HAL_RCC_OscConfig(&RCC_OscInitStruct);

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2S;
	PeriphClkInitStruct.PLLI2S.PLLI2SN = 192;
	PeriphClkInitStruct.PLLI2S.PLLI2SR = 2;
	HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

	HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);

	HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

	/* SysTick_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* I2C1 init function */
void MX_I2C1_Init(void) {

	I2c.Instance = I2C1;
	I2c.Init.ClockSpeed = 100000;
	I2c.Init.DutyCycle = I2C_DUTYCYCLE_2;
	I2c.Init.OwnAddress1 = 0;
	I2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	I2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	I2c.Init.OwnAddress2 = 0;
	I2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	I2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	HAL_I2C_Init(&I2c);

}

void MX_GPIO_Init(void) {

	GPIO_InitTypeDef GPIO_InitStruct;

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOE_CLK_ENABLE()
	;
	__HAL_RCC_GPIOC_CLK_ENABLE()
	;
	__HAL_RCC_GPIOH_CLK_ENABLE()
	;
	__HAL_RCC_GPIOA_CLK_ENABLE()
	;
	__HAL_RCC_GPIOB_CLK_ENABLE()
	;
	__HAL_RCC_GPIOD_CLK_ENABLE()
	;

	GPIO_InitStruct.Pin = GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOD,
	LD4_Pin | LD3_Pin | LD5_Pin | LD6_Pin | Audio_RST_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : CS_I2C_SPI_Pin */
	GPIO_InitStruct.Pin = CS_I2C_SPI_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

	HAL_GPIO_Init(CS_I2C_SPI_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_Init( GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
	GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : PDM_OUT_Pin */
	GPIO_InitStruct.Pin = PDM_OUT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : B1_Pin */
	GPIO_InitStruct.Pin = B1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : PA5 PA6 PA7 */
	GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : BOOT1_Pin */
	GPIO_InitStruct.Pin = BOOT1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(BOOT1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : CLK_IN_Pin */
	GPIO_InitStruct.Pin = CLK_IN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(CLK_IN_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : LD4_Pin LD3_Pin LD5_Pin LD6_Pin
	 Audio_RST_Pin */
	GPIO_InitStruct.Pin = LD4_Pin | LD3_Pin | LD5_Pin | LD6_Pin | Audio_RST_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
	GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : MEMS_INT2_Pin */
	GPIO_InitStruct.Pin = MEMS_INT2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);

}

/**
 * @brief  Main routine for Mass Storage Class
 * @param  None
 * @retval None
 */

static void MSC_Application(void) {
	FRESULT Result; // FatFs function common result code

	//
	// Mount the flash drive using a fat file format
	//

	Result = f_mount(&UsbDiskFatFs, (TCHAR const*) USBH_Path, 0);
	if (FR_OK == Result) {

		//
		// File system successfully mounted, play all the music files in the directory.
		//
		while (HAL_GPIO_ReadPin(GPIOC, button_set_alarm) == 1) {
			PlayDirectory("", 0);
		}
	} else {
		//
		// FatFs Initialization Error
		//
		//	Error_Handler();
	}

	//
	// Unlink the USB disk I/O driver
	//
	FATFS_UnLinkDriver(UsbDiskPath);
}

/*********************************************************************************************************************************************************
 * Function: TIM5_IRQHandler
 *
 * Description:
 *
 * Timer interrupt handler that is called at a rate of 500Hz. This function polls the time and
 * displays it on the 7 segment display. It also checks for button presses and handles any bounce conditions.
 *

 void TIM5_IRQHandler(void) {

 }
 *********************************************************************************************************************************************************/

/*
 * Function: RTC_Alarm_IRQHandler
 *
 * Description:
 *
 * When alarm occurs, clear all the interrupt bits and flags then start playing music.
 *
 */
void RTC_Alarm_IRQHandler(void) {

//
// Verify that this is a real time clock interrupt
//
	if ( __HAL_RTC_ALARM_GET_IT( &RealTimeClock, RTC_IT_ALRA ) != RESET) {

//
// Clear the alarm flag and the external interrupt flag
//
		__HAL_RTC_ALARM_CLEAR_FLAG(&RealTimeClock, RTC_FLAG_ALRAF);
		__HAL_RTC_EXTI_CLEAR_FLAG(RTC_EXTI_LINE_ALARM_EVENT);

//
// Restore the alarm to it's original time. This could have been a snooze alarm
//
		HAL_RTC_SetAlarm_IT(&RealTimeClock, &ClockAlarm, RTC_FORMAT_BCD);

		PlayMusic = TRUE;

	}

}

/*
 * Function: SetTime
 *
 * Description:
 *
 * Advance either the time hours or minutes field. Validate the new time and then update the clock
 *
 */
int d1 = 0;
int d2 = 0;
int d3 = 0;
int d4 = 0;

void SetTime(void) {

	int readflag = 0;
	//Variables to store value of what to set each digit to

	while (settingtime == 1) {

		//if minute button pushed and d4 is not 9: increment d4
		if (HAL_GPIO_ReadPin(GPIOC, button_minute) == 0 && d4 != 9
				&& readflag == 0) {
			readflag = 1;
			d4++;
			SetDigit(4, d4);
			int t = 0;
			while (t < 6400000) {
				t++;
			}
			//HAL_Delay(2);

			//if minute button pushed while d4 is 9 and d3 is not 5: set d4 to 0, and increment d3
		} else if (HAL_GPIO_ReadPin(GPIOC, button_minute) == 0 && d4 == 9
				&& d3 != 5 && readflag == 0) {
			readflag = 1;
			d4 = 0;
			d3++;
			SetDigit(3, d3);
			//HAL_Delay(2);
			SetDigit(4, d4);
			//HAL_Delay(2);
			int t = 0;
			while (t < 6400000) {
				t++;
			}

			//if minute button pushed while d4 is 9 and d3 is 5: set d3 and d4 to 0
		} else if (HAL_GPIO_ReadPin(GPIOC, button_minute) == 0 && d4 == 9
				&& d3 == 5 && readflag == 0) {
			readflag = 1;
			d4 = 0;
			d3 = 0;
			SetDigit(3, d3);
			//HAL_Delay(2);
			SetDigit(4, d4);
			//HAL_Delay(2);
			int t = 0;
			while (t < 6400000) {
				t++;
			}

			//if hour button is pushed while d2 is not 9: increment d2
		} else if (HAL_GPIO_ReadPin(GPIOC, button_hour) == 0 && d2 != 9
				&& readflag == 0) {
			readflag = 1;
			d2++;
			SetDigit(2, d2);
			//HAL_Delay(2);
			int t = 0;
			while (t < 6400000) {
				t++;
			}

			//if hour button is pushed while d2 is 9 and d1 is not 2: set d2 to 0, and increment d1
		} else if (HAL_GPIO_ReadPin(GPIOC, button_hour) == 0 && d2 == 9
				&& d1 != 2 && readflag == 0) {
			readflag = 1;
			d2 = 0;
			d1++;
			SetDigit(1, d1);
			//HAL_Delay(2);
			SetDigit(2, d2);
			//HAL_Delay(2);
			int t = 0;
			while (t < 6400000) {
				t++;
			}

			//if hour button is pushed while d2 is 4 and d1 is 2: set d1 and d2 to 0
		} else if (HAL_GPIO_ReadPin(GPIOC, button_hour) == 0 && d2 == 4
				&& d1 == 2 && readflag == 0) {
			readflag = 1;
			d2 = 0;
			d1 = 0;
			SetDigit(1, d1);
			//HAL_Delay(2);
			SetDigit(2, d2);
			//HAL_Delay(2);
			int t = 0;
			while (t < 6400000) {
				t++;
			}

		} else if (readflag != 0) {
			readflag = 0;

			//if no buttons are pushed: display the time that the clock is being set to
		} else {
			SetDigit(1, d1);
			HAL_Delay(2);
			SetDigit(2, d2);
			HAL_Delay(2);
			SetDigit(3, d3);
			HAL_Delay(2);
			SetDigit(4, d4);
			HAL_Delay(2);
		} //else

		//if set time button is pushed: initialize current time to set time and break from setting time loop
		if (HAL_GPIO_ReadPin(GPIOC, button_set_time) == 0) {

			SetDigit(1, d1);
			HAL_Delay(2);
			SetDigit(2, d2);
			HAL_Delay(2);
			SetDigit(3, d3);
			HAL_Delay(2);
			SetDigit(4, d4);
			HAL_Delay(2);

			settingtime = 0;

		} //if push set time button to confirm time set

	} //while setting time

} //SetTime()

/*
 * Function: SetAlarm
 *
 * Description: Advance either the alarm hours or minutes field. Validate the new alarm time and then set the alarm
 *
 */
int ad1 = 0;
int ad2 = 0;
int ad3 = 0;
int ad4 = 0;
void SetAlarm(void) {

	int prevd4 = digit4;
	int prevd3 = digit3;
	int prevd2 = digit2;
	int prevd1 = digit1;
	int readflag = 0;
	//Variables to store value of what to set each digit to

	while (settingalarm == 1) {

		//if minute button pushed and d4 is not 9: increment d4
		if (HAL_GPIO_ReadPin(GPIOC, button_minute) == 0 && ad4 != 9
				&& readflag == 0) {
			readflag = 1;
			ad4++;
			SetDigit(4, ad4);
			int t = 0;
			while (t < 6400000) {
				t++;
			}
			//HAL_Delay(2);

			//if minute button pushed while d4 is 9 and d3 is not 5: set d4 to 0, and increment d3
		} else if (HAL_GPIO_ReadPin(GPIOC, button_minute) == 0 && ad4 == 9
				&& ad3 != 5 && readflag == 0) {
			readflag = 1;
			ad4 = 0;
			ad3++;
			SetDigit(3, ad3);
			//HAL_Delay(2);
			SetDigit(4, ad4);
			int t = 0;
			while (t < 6400000) {
				t++;
			}
			//HAL_Delay(2);

			//if minute button pushed while d4 is 9 and d3 is 5: set d3 and d4 to 0
		} else if (HAL_GPIO_ReadPin(GPIOC, button_minute) == 0 && ad4 == 9
				&& ad3 == 5 && readflag == 0) {
			readflag = 1;
			ad4 = 0;
			ad3 = 0;
			SetDigit(3, ad3);
			//HAL_Delay(2);
			SetDigit(4, ad4);
			int t = 0;
			while (t < 6400000) {
				t++;
			}
			//HAL_Delay(2);

			//if hour button is pushed while d2 is not 9: increment d2
		} else if (HAL_GPIO_ReadPin(GPIOC, button_hour) == 0 && ad2 != 9
				&& readflag == 0) {
			readflag = 1;
			ad2++;
			SetDigit(2, ad2);
			int t = 0;
			while (t < 6400000) {
				t++;
			}
			//HAL_Delay(2);

			//if hour button is pushed while d2 is 9 and d1 is not 2: set d2 to 0, and increment d1
		} else if (HAL_GPIO_ReadPin(GPIOC, button_hour) == 0 && ad2 == 9
				&& ad1 != 2 && readflag == 0) {
			readflag = 1;
			ad2 = 0;
			ad1++;
			SetDigit(1, ad1);
			//HAL_Delay(2);
			SetDigit(2, ad2);
			int t = 0;
			while (t < 6400000) {
				t++;
			}
			//HAL_Delay(2);

			//if hour button is pushed while d2 is 4 and d1 is 2: set d1 and d2 to 0
		} else if (HAL_GPIO_ReadPin(GPIOC, button_hour) == 0 && ad2 == 4
				&& ad1 == 2 && readflag == 0) {
			readflag = 1;
			ad2 = 0;
			ad1 = 0;
			SetDigit(1, ad1);
			//HAL_Delay(2);
			SetDigit(2, ad2);
			//HAL_Delay(2);
			int t = 0;
			while (t < 6400000) {
				t++;
			}
		} else if (readflag != 0) {

			readflag = 0;

			//if no buttons are pushed: display the time that the clock is being set to
		} else {
			SetDigit(1, ad1);
			HAL_Delay(2);
			SetDigit(2, ad2);
			HAL_Delay(2);
			SetDigit(3, ad3);
			HAL_Delay(2);
			SetDigit(4, ad4);
			HAL_Delay(2);

		} //else

		//if set alarm button is pushed: initialize alarm time to set alarm time and break from setting alarm loop
		if (HAL_GPIO_ReadPin(GPIOC, button_set_alarm) == 0) {
			settingalarm = 0;

			alarmd1 = ad1;
			alarmd2 = ad2;
			alarmd3 = ad3;
			alarmd4 = ad4;

			SetDigit(1, prevd1);
			SetDigit(2, prevd2);
			SetDigit(3, prevd3);
			SetDigit(4, prevd4);

		} //push set time button to confirm alarm set

	} //while setting alarm

} //SetAlarm()

/*
 * Function: Alarm
 *
 * Description: Plays music from a USB for the alarm, disables alarm if set alarm button is pushed,
 * 				snoozes alarm if snooze button is pushed.
 *
 */
void Alarm(void) {

/*	while (1) {
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
		HAL_Delay(500);
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
		HAL_GPIO_WritePin( GPIOD, GPIO_PIN_15, GPIO_PIN_SET);
	}
*/
	// Wait until the drive is mounted before we can play some music
	do {
		MX_USB_HOST_Process();
	} while (Appli_state != APPLICATION_READY);

	trace_printf("\n");

	PlayMusic = TRUE;

	//	checks for the alarm interrupt and call the music playing module
	while ( TRUE == PlayMusic) {

		MSC_Application();

		//if set alarm button is pushed disable the alarm
		if (HAL_GPIO_ReadPin(GPIOC, button_set_alarm) == 0) {
			alarmd1 = -1;
			alarmd2 = -1;
			alarmd3 = -1;
			alarmd4 = -1;
			PlayMusic = FALSE;

			//Snooze the alarm and stop playing music if snooze button pressed

		} //if set alarm pressed

			if (HAL_GPIO_ReadPin(GPIOC, button_snooze) == 0) {
				Snooze();
				PlayMusic = FALSE;
			} //if snooze pressed
		// Wait for an interrupt to occur
		__asm__ volatile ( "wfi" );

	} //while

} //Alarm()

/*
 * Function: Snooze
 *
 * Description: Add 10 Minutes to the current time and validate. Update the alarm and enable.
 *
 */

void Snooze(void) {

	//if digit3 of alarm is 5, set it to 0
	if (alarmd3 == 5) {
		alarmd3 = 0;

		//if digit3 of alarm is not 0, increment it
	} else {
		alarmd3 = alarmd3 + 1;
	} //else

} //Snooze()

//static void Error_Handler(void)
//{
//while(1)
//	{
//	}
//}
#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
