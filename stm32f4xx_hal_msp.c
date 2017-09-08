/**
  ******************************************************************************
  * File Name          : stm32f4xx_hal_msp.c
  * Description        : This file provides code for the MSP Initialization 
  *                      and de-Initialization codes.
  ******************************************************************************
  *
  * COPYRIGHT(c) 2016 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

extern DMA_HandleTypeDef
	AudioDma;

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{
  /* USER CODE BEGIN MspInit 0 */

  /* USER CODE END MspInit 0 */

  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_0);

  /* System interrupt init*/
  /* MemoryManagement_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
  /* BusFault_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);
  /* UsageFault_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0);
  /* DebugMonitor_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DebugMonitor_IRQn, 0, 0);
  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

  /* USER CODE BEGIN MspInit 1 */

  /* USER CODE END MspInit 1 */
}

void HAL_DAC_MspInit(DAC_HandleTypeDef* hdac)
{

  GPIO_InitTypeDef GPIO_InitStruct;
  if(hdac->Instance==DAC)
  {
  /* USER CODE BEGIN DAC_MspInit 0 */

  /* USER CODE END DAC_MspInit 0 */
    /* Peripheral clock enable */
    __HAL_RCC_DAC_CLK_ENABLE();

    /**DAC GPIO Configuration
    PA4     ------> DAC_OUT1
    */
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Peripheral DMA init*/

    AudioDma.Instance = DMA1_Stream5;
    AudioDma.Init.Channel = DMA_CHANNEL_7;


    AudioDma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    AudioDma.Init.PeriphInc = DMA_PINC_DISABLE;
    AudioDma.Init.MemInc = DMA_MINC_ENABLE;
    AudioDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    AudioDma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    AudioDma.Init.Mode = DMA_NORMAL;
    AudioDma.Init.Priority = DMA_PRIORITY_LOW;
    AudioDma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&AudioDma );

    __HAL_LINKDMA( hdac, DMA_Handle1, AudioDma );

  }

}


void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{

  GPIO_InitTypeDef GPIO_InitStruct;
  if(hi2c->Instance==I2C1)
  {
  /* USER CODE BEGIN I2C1_MspInit 0 */

  /* USER CODE END I2C1_MspInit 0 */
  
    /**I2C1 GPIO Configuration    
    PB6     ------> I2C1_SCL
    PB9     ------> I2C1_SDA 
    */
    GPIO_InitStruct.Pin = Audio_SCL_Pin|Audio_SDA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Peripheral clock enable */
    __HAL_RCC_I2C1_CLK_ENABLE();
  /* USER CODE BEGIN I2C1_MspInit 1 */

  /* USER CODE END I2C1_MspInit 1 */
  }

}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{

  if(hi2c->Instance==I2C1)
  {
  /* USER CODE BEGIN I2C1_MspDeInit 0 */

  /* USER CODE END I2C1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C1_CLK_DISABLE();
  
    /**I2C1 GPIO Configuration    
    PB6     ------> I2C1_SCL
    PB9     ------> I2C1_SDA 
    */
    HAL_GPIO_DeInit(GPIOB, Audio_SCL_Pin|Audio_SDA_Pin);

  }
  /* USER CODE BEGIN I2C1_MspDeInit 1 */

  /* USER CODE END I2C1_MspDeInit 1 */

}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base)
{
	if(htim_base->Instance==TIM5)
		{
			/* USER CODE BEGIN TIM5_MspInit 0 */

			/* USER CODE END TIM5_MspInit 0 */
			/* Peripheral clock enable */
			__HAL_RCC_TIM5_CLK_ENABLE();
			/* USER CODE BEGIN TIM5_MspInit 1 */

			/* USER CODE END TIM5_MspInit 1 */
		}
		else if(htim_base->Instance==TIM6)
		{
			/* USER CODE BEGIN TIM6_MspInit 0 */

			/* USER CODE END TIM6_MspInit 0 */
			/* Peripheral clock enable */
			__HAL_RCC_TIM6_CLK_ENABLE();
			/* Peripheral interrupt init */
			//HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 0, 0);
			//HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
			/* USER CODE BEGIN TIM6_MspInit 1 */

			/* USER CODE END TIM6_MspInit 1 */
		}


}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* htim_base)
{
	if(htim_base->Instance==TIM5)
	{
		/* USER CODE BEGIN TIM5_MspDeInit 0 */

		/* USER CODE END TIM5_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_TIM5_CLK_DISABLE();
		/* USER CODE BEGIN TIM5_MspDeInit 1 */

		/* USER CODE END TIM5_MspDeInit 1 */
	}
	else if(htim_base->Instance==TIM6)
	{
		/* USER CODE BEGIN TIM6_MspDeInit 0 */

		/* USER CODE END TIM6_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_TIM6_CLK_DISABLE();

		/* Peripheral interrupt DeInit*/
		/* USER CODE BEGIN TIM6:TIM6_DAC_IRQn disable */
		/**
		 * Uncomment the line below to disable the "TIM6_DAC_IRQn" interrupt
		 * Be aware, disabling shared interrupt may affect other IPs
		 */
		/* HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn); */
		/* USER CODE END TIM6:TIM6_DAC_IRQn disable */

		/* USER CODE BEGIN TIM6_MspDeInit 1 */

		/* USER CODE END TIM6_MspDeInit 1 */
	}


}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
