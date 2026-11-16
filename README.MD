# Wireless Digital Guitar Effects Pedal — STM32 Firmware

Bachelor's thesis project (Politechnika Krakowska, WIEiK) — a wireless digital guitar effects pedal built on STM32F407VG. This is part of a dual MCU STM32 + ESP32 (Iot) and external I2S codec WM8731 system which involves passthrough audio digital signal processing (DSP) from electric guitar with wireless control and analysis (FFT, Graphs etc.).
This repository is work in progress. Final version may differ from initial plan.

## Architecture

- **MCU:** STM32F407VGT6 (ARM Cortex-M4, 168 MHz, FPU)
- **Audio codec:** WM8731 (MIKROE-506), 48 kHz, I2S full-duplex with circular DMA, control over I2C
- **Anti-aliasing:** Sallen-Key active filters (TL072)
- **Wireless control:** ESP32 connected via UART (separate repository `esp32-guitar-effect`):
  WiFi web panel, OLED status screen, 6 potentiometers (EQ low / mid / high, volume, drive, macro knob)
- **Bypass:** digital (`bypass` parameter from the web panel)
- **Power:** 9V input, Pololu buck converter

## Toolchain

- **IDE:** STM32CubeIDE 2.1.0
- **HAL:** STM32Cube HAL/LL drivers
- **Target:** bare-metal (no RTOS on STM32 side)

## Building

Open the project in STM32CubeIDE (`File → Open Projects from File System`) and build with RUN. 

## Signal Chain

Mono (guitar on the left input), 48 kHz, processed every 2 frames (I2S DMA buffer of 8 halfwords):

```
input -> [CLEAN | DIST (HPF, 4x oversampled hard clip, tone LPF)] -> EQ (3 biquads)
      -> CHORUS -> ECHO -> REVERB -> volume -> bypass -> limiter -> L/R
```

All effects are computed all the time and `fx_*` only fades them in/out, so the CPU load is the same in
every audio interrupt (load changes leak into the codec ADC as noise). Echo and reverb tails ring out
after switching them off.

## Control

All parameters live in `audio_params` (`Core/Inc/audio.h`, all `float`) — editable from the debugger
(Live Expressions) and over UART from the ESP32 (USART3: PD8 TX, PD9 RX, 115200 8N1). Text protocol v4
(`PING`, `SET`, `GET`, `GET ALL`, `FFT` spectrum stream) is described in the ESP32 repository:
`docs/UART_PROTOCOL.md`.

| Parameter | Range | Default |
|---|---|---|
| `bypass`, `fx_dist`, `fx_chorus`, `fx_echo`, `fx_reverb`, `fft` | 0 / 1 | 0, 1, 0, 0, 0, 0 |
| `volume` | 0 .. 1 | 1 |
| `eq_low_db`, `eq_mid_db`, `eq_high_db` | -12 .. 12 dB | 0 |
| `clean_gain` | 0 .. 64 | 20 |
| `drive` | 1 .. 200 | 60 |
| `clip_level` | 100 .. 30000 | 1500 |
| `chorus_rate_hz` | 0.05 .. 5 | 0.9 |
| `chorus_depth` | 0 .. 1 | 0.5 |
| `echo_time_ms` | 20 .. 1000 | 380 |
| `echo_feedback` | 0 .. 0.95 | 0.35 |
| `echo_mix` | 0 .. 1 | 0.35 |
| `reverb_mix` | 0 .. 1 | 0.3 |
| `reverb_decay` | 0 .. 1 | 0.5 |
| `linvol` | 0 .. 23 (codec input gain, 15 = -12 dB) | 15 |
| `out_db` | -40 .. 0 dB (codec headphone out, analog; ~-20 for a guitar amp input) | -20 |

Button B1 cycles CLEAN → DIST → CHORUS → ECHO → REVERB, LEDs show the active effects.

## Repo Structure

```
Core/Src/main.c     — init, audio DMA interrupt, main loop (UART, button, LEDs, codec gain, FFT)
Core/Src/audio.c    — audio_params, signal chain (clean/dist, oversampling, EQ, volume, bypass)
Core/Src/chorus.c   — chorus (LFO modulated delay line)
Core/Src/echo.c     — tape-style echo (1 s delay line)
Core/Src/reverb.c   — Schroeder / Freeverb reverb (8 combs + 4 allpasses, buffers in CCM RAM)
Core/Src/fft.c      — 512-point FFT of the output, 16 bands for the ESP32
Core/Src/esp32.c    — USART3 and the UART protocol
Core/Src/wm8731.c   — WM8731 codec over I2C
Drivers/            — CMSIS & HAL drivers
*.ioc               — CubeMX project file
*.ld                — linker scripts
```

## References

Sources in code comments (`// source: [...]`) use the keys from the thesis bibliography:
[BristowJohnson2021] EQ biquads, [Zielinski2005] one-pole filters / FFT / Hann window,
[Yeh2009] aliasing and oversampling of the clip, [Dattorro1997b] chorus, [Wilmering2020] echo,
[Schroeder1962] and [Smith2010] reverb, [RM0090] DMA / USART registers, [WM8731] codec.


- [STM32F407VG Datasheet](https://www.st.com/resource/en/datasheet/stm32f407vg.pdf)
- [STM32F407 Reference Manual (RM0090)](https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [WM8731 Codec Datasheet](https://www.cirrus.com/products/wm8731/)
- [MIKROE-506 Audio Codec Proto Board](https://www.mikroe.com/audio-codec-proto-board)
- [Audio EQ Cookbook (biquad coefficients)](https://www.w3.org/2011/audio/audio-eq-cookbook.html)
- [Freeverb (J. O. Smith, Physical Audio Signal Processing)](https://ccrma.stanford.edu/~jos/pasp/Freeverb.html)

## License

University thesis project — not licensed for redistribution.
