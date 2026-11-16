/**
  ******************************************************************************
  * @file    wm8731.c
  * @brief   WM8731 audio codec control driver (I2C).
  *          source: [WM8731] datasheet (register map, control interface)
  ******************************************************************************
  */
#include "wm8731.h"

/* 7-bit I2C address 0x1A, shifted left for HAL 8-bit addressing. */
#define WM8731_I2C_ADDR (0x1A << 1)

/* Control register map (register address is 7 bits). */
#define WM8731_REG_LLINEIN      0x00
#define WM8731_REG_RLINEIN      0x01
#define WM8731_REG_LHPOUT       0x02
#define WM8731_REG_RHPOUT       0x03
#define WM8731_REG_ANALOG_PATH  0x04
#define WM8731_REG_DIGITAL_PATH 0x05
#define WM8731_REG_PWR_DOWN     0x06
#define WM8731_REG_DIGITAL_IF   0x07
#define WM8731_REG_SAMPLING     0x08
#define WM8731_REG_ACTIVE       0x09
#define WM8731_REG_RESET        0x0F

/* Write one control word: 7-bit register address plus 9-bit data,
   packed into two bytes per the WM8731 control interface. */
static HAL_StatusTypeDef WM8731_WriteReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint16_t data)
{
    uint8_t bytes[2];
    bytes[0] = (reg << 1) | ((data >> 8) & 0x01);
    bytes[1] = data & 0xFF;
    return HAL_I2C_Master_Transmit(hi2c, WM8731_I2C_ADDR, bytes, 2, 100);
}

HAL_StatusTypeDef WM8731_SyncInit(I2C_HandleTypeDef *hi2c)
{
	HAL_StatusTypeDef st;
	 st = WM8731_WriteReg(hi2c, WM8731_REG_ACTIVE, 0x001);
	     if (st != HAL_OK) return st;
	 HAL_Delay(100);
	 return HAL_OK;
}

HAL_StatusTypeDef WM8731_Init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef st;

    /* 1. Software reset. */
    st = WM8731_WriteReg(hi2c, WM8731_REG_RESET, 0x000);
    if (st != HAL_OK) return st;
    HAL_Delay(5); /* let internal biases settle */

    /* 2. Power up all blocks. */
    st = WM8731_WriteReg(hi2c, WM8731_REG_PWR_DOWN, 0x000);
    if (st != HAL_OK) return st;

    /* 3. Line-in L+R (LRINBOTH): unmute, volume 0x0F = -12 dB (0x17 = 0 dB). */
    st = WM8731_WriteReg(hi2c, WM8731_REG_LLINEIN, 0x10F);
    if (st != HAL_OK) return st;

    /* 4. Headphone out L+R: 0x79 = 0 dB (1 Vrms full scale, line level), zero-cross detect on. */
    st = WM8731_WriteReg(hi2c, WM8731_REG_LHPOUT, 0x1F9);
    if (st != HAL_OK) return st;
    st = WM8731_WriteReg(hi2c, WM8731_REG_RHPOUT, 0x1F9);
    if (st != HAL_OK) return st;

    /* 5. Analog path: line-in to ADC, DAC to output, mic muted,
          analog bypass disabled !0x012!. */
    st = WM8731_WriteReg(hi2c, WM8731_REG_ANALOG_PATH, 0x012);
    if (st != HAL_OK) return st;

    /* 6. Digital path: no de-emphasis, no soft mute. */
    st = WM8731_WriteReg(hi2c, WM8731_REG_DIGITAL_PATH, 0x000);
    if (st != HAL_OK) return st;

    /* 7. Digital interface: I2S format, 16-bit, codec as master. */
    st = WM8731_WriteReg(hi2c, WM8731_REG_DIGITAL_IF, 0x042);
    if (st != HAL_OK) return st;

    /* 8. Sampling control: normal mode, SR = 0000 -> 48 kHz with 12.288 MHz MCLK. */
    st = WM8731_WriteReg(hi2c, WM8731_REG_SAMPLING, 0x000);
    if (st != HAL_OK) return st;

    /* 9. Activate digital core. */
    //st = WM8731_WriteReg(hi2c, WM8731_REG_ACTIVE, 0x001);
    //if (st != HAL_OK) return st;

    /* 10. Re-confirm power up, then settle. */
    st = WM8731_WriteReg(hi2c, WM8731_REG_PWR_DOWN, 0x000);
    if (st != HAL_OK) return st;
    HAL_Delay(100);

    /* 11. Re-confirm digital path. */
    //st = WM8731_WriteReg(hi2c, WM8731_REG_DIGITAL_PATH, 0x000);
    //if (st != HAL_OK) return st;

    return HAL_OK;
}
