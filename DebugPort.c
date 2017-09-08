/*
 * DebugPort.c
 *
 *  Created on: Mar 20, 2017
 *      Author: brent
 */

//
//
// Line Tracking Robot V3.00
//

//Maze solving robot code.  This code prioritizes left hand turns to eventually solve any maze that does
//not have a loop.  It uses 7 IR sensors and controls 2 motors to follow black electrical tape.

#include <stdio.h>
#include <stdlib.h>
#include "diag/Trace.h"
#include "cmsis/cmsis_device.h"
#include "stm32f4xx_hal.h"
#include "ctype.h"
#include "generic.h"
#include "DebugPort.h"

#include <sys/stat.h>

#define DELAY_1MS		(1)
#define DELAY_20MS		(39)
#define DELAY_200MS		(399)
#define DELAY_300MS		(599)
#define DELAY_500MS		(999)
#define DELAY_800MS		(1599)
#define	DELAY_1S		(1999)
#define DELAY_2S		(3999)

//
// If the DIAGNOSTIC_PORT_ENABLED not defined then disable the feature
//

#ifndef DIAGNOSTIC_PORT_ENABLED
#define DIAGNOSTIC_PORT_ENABLED (FALSE)
#endif


struct tCommands
{
	char *CommandStr;
	unsigned int (*CommandFunc)( char *CommandStr );
};


struct tUartBuffer
{
	char Tx[64];
	volatile unsigned int TxHeadPtr;
	volatile unsigned int TxTailPtr;
};


USART_HandleTypeDef
	DebugPort;

volatile struct tUartBuffer
	DebugPortBuffer;

void DebugPortInit( void )
{
	GPIO_InitTypeDef
		GPIO_InitStructure;

//
// Enable the clock to the following devices GPIOA, GPIOB, GPIOC, Timer 2, A/D
//

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_USART2_CLK_ENABLE();


	DebugPortBuffer.TxHeadPtr = 0;
	DebugPortBuffer.TxTailPtr = 0;


#if DIAGNOSTIC_PORT_ENABLED == TRUE

	//
	// Configure PA1, PA2, PA3 and PA5 as alternate functions for the 4 PWM outputs
	//

	GPIO_InitStructure.Pin = GPIO_PIN_2;
	GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStructure.Alternate = GPIO_AF7_USART2;
	HAL_GPIO_Init( GPIOA, &GPIO_InitStructure);


	DebugPort.Instance = USART2;
	DebugPort.Init.BaudRate = 38400;
	DebugPort.Init.WordLength = USART_WORDLENGTH_8B;
	DebugPort.Init.StopBits = USART_STOPBITS_1;
	DebugPort.Init.Parity = USART_PARITY_NONE;
	DebugPort.Init.Mode = USART_MODE_TX;
	HAL_USART_Init( &DebugPort );

//	__HAL_USART_ENABLE_IT( &DebugPort, USART_IT_TXE );

	NVIC_SetPriority( USART2_IRQn, 0 );
	NVIC_EnableIRQ( USART2_IRQn );
#endif

}





int _close(int file) { return -1; }

int _fstat(int file, struct stat *st)
{
	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int file) { return 1; }

int _lseek(int file, int ptr, int dir) { return 0; }

int _open(const char *name, int flags, int mode) { return -1; }

int _read(int file, char *ptr, int len)
{
	return(0);
}


int _write(int file, const char *ptr, int len)
{
#if DIAGNOSTIC_PORT_ENABLED == TRUE
	return DebugWrite( ptr, len );
#endif

	return len;
}


int DebugWrite( const char *ptr, int len )
{
	int todo;
	unsigned int TxHeadTmp;

	for (todo = 0; todo < len; todo++)
	{
		TxHeadTmp = DebugPortBuffer.TxHeadPtr+1;
		if ( TxHeadTmp >= sizeof( DebugPortBuffer.Tx ))
		{
			TxHeadTmp = 0;
		}

		while ( TxHeadTmp == DebugPortBuffer.TxTailPtr );

		DebugPortBuffer.Tx[TxHeadTmp] = *ptr++;
		DebugPortBuffer.TxHeadPtr = TxHeadTmp;

		__HAL_USART_ENABLE_IT( &DebugPort, USART_IT_TXE );

	}

	return( len );
}


void USART2_IRQHandler( void )
{
	if (( __HAL_USART_GET_FLAG( &DebugPort, USART_FLAG_TC ) != RESET ) &&
				( __HAL_USART_GET_IT_SOURCE( &DebugPort, USART_IT_TXE ) != RESET ))
	{
		DebugPortBuffer.TxTailPtr++;
		if ( DebugPortBuffer.TxTailPtr >= sizeof( DebugPortBuffer.Tx ))
		{
			DebugPortBuffer.TxTailPtr = 0;
		}

		DebugPort.Instance->DR = DebugPortBuffer.Tx[DebugPortBuffer.TxTailPtr];

		if ( DebugPortBuffer.TxHeadPtr == DebugPortBuffer.TxTailPtr )
		{
			__HAL_USART_DISABLE_IT( &DebugPort, USART_IT_TXE );
		}
	}
}

#if DIAGNOSTIC_PORT_ENABLED == TRUE
ssize_t	trace_write (const char* buf __attribute__((unused)), size_t nbyte __attribute__((unused)))
{
	return DebugWrite( buf, nbyte );
}
#endif



