#ifndef FFT_H
#define FFT_H

#include <stdint.h>

#define FFT_N       512     // 10.7 ms przy 48 kHz, rozdzielczość 93.75 Hz
#define FFT_BANDS   16

void FFT_Push(float x);                     // przerwanie audio: kolejna próbka wyjścia
void FFT_Bands(uint8_t bands[FFT_BANDS]);   // pętla główna: FFT ostatnich FFT_N próbek -> 16 pasm 0..99 (dBFS + 99)

#endif
