# Wireless Digital Guitar Effects Pedal — STM32 Firmware

Bachelor's thesis project (Politechnika Krakowska, WIEiK) — a wireless digital guitar effects pedal built on STM32F407VG. This is part of a dual MCU STM32 + ESP32 (Iot) and external I2S codec WM8731 system which involves passthrough audio digital signal processing (DSP) from electric guitar with wireless control and analysis (FFT, Graphs etc.).

## Architecture 

Planned final architecture (this part may change during development):

- **MCU:** STM32F407VGT6 (ARM Cortex-M4, 168 MHz, FPU)
- **Audio codec:** WM8731 (MIKROE-506) via I2S with DMA ping-pong buffering 
- **Anti-aliasing:** Sallen-Key active filters (TL072)
- **Wireless control:** ESP32 (FreeRTOS, WiFi/MQTT, OLED UI) connected via UART
- **Bypass:** True bypass with G5V-1 relay + footswitch
- **Power:** 9V input, dual LDO regulation with ferrite isolation

## Toolchain

- **IDE:** STM32CubeIDE 2.1.0
- **HAL:** STM32Cube HAL/LL drivers
- **Target:** bare-metal (no RTOS on STM32 side)

## Building

Open the project in STM32CubeIDE (`File → Open Projects from File System`) and build with RUN. 

## Repo Structure

```
Core/           — application code (main.c, interrupts, HAL config)
Drivers/        — CMSIS & HAL drivers
Middlewares/    — ST USB Host library
USB_HOST/       — USB Host application & config
*.ioc           — CubeMX project file
*.ld            — linker scripts
```

## References

- [STM32F407VG Datasheet](https://www.st.com/resource/en/datasheet/stm32f407vg.pdf)
- [STM32F407 Reference Manual (RM0090)](https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [WM8731 Codec Datasheet](https://www.cirrus.com/products/wm8731/)
- [MIKROE-506 Audio Codec Proto Board](https://www.mikroe.com/audio-codec-proto-board)
- [Audio EQ Cookbook (biquad coefficients)](https://www.w3.org/2011/audio/audio-eq-cookbook.html)

## License

University thesis project — not licensed for redistribution.
