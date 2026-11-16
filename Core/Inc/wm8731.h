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

/* Line-in gain for both channels: 0x00 = -34.5 dB .. 0x17 = 0 dB .. 0x1F = +12 dB. */
HAL_StatusTypeDef WM8731_SetLineInVolume(I2C_HandleTypeDef *hi2c, uint8_t vol);

/* Highest safe line-in gain on this board (0 dB). Above it the input overloads
   and the codec ADC locks up (constant output) until reset. */
#define WM8731_LINVOL_MAX   0x17

#ifdef __cplusplus
}
#endif

#endif /* WM8731_H */
