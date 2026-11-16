#ifndef ESP32_H
#define ESP32_H

#include <stdint.h>

// Sterowanie z ESP32 przez UART (USART3: PD8 = TX -> ESP32 GPIO16, PD9 = RX <- ESP32 GPIO17).
// Protokół tekstowy: esp32-guitar-effect/docs/UART_PROTOCOL.md
// Wszystko wołane z pętli głównej, przerwanie UART tylko wrzuca bajty do bufora.

void ESP32_Init(void);                      // USART3 115200 8N1 + "HELLO"
void ESP32_Poll(void);                      // odebrane linie -> komendy (PING, SET, GET)
void ESP32_SendParam(const char *name);     // "VAL <name> <wartość>" po zmianie lokalnej (przycisk)
void ESP32_SendFFT(const uint8_t *bands);   // "FFT v1 ... v16" (gdy fft = 1, co 100 ms)

#endif
