/**
  ******************************************************************************
  * @file    wm8731.h
  * @brief   WM8731 audio codec control interface (I2C).
  *          The codec is the I2S master (generates BCLK/LRC/MCLK); this
  *          module only drives its control registers over I2C. Audio
  *          samples travel over I2S with DMA, not through this module.
  ******************************************************************************
  */
#ifndef WM8731_H
#define WM8731_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* Reset and configure the codec: line-in to ADC, DAC to headphone out,
   I2S 16-bit, codec as master. Returns HAL_OK on success. */
HAL_StatusTypeDef WM8731_Init(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef WM8731_SyncInit(I2C_HandleTypeDef *hi2c);

#ifdef __cplusplus
}
#endif

#endif /* WM8731_H */
