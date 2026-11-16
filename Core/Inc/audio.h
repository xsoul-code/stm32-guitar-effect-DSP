#ifndef AUDIO_H
#define AUDIO_H

#include <math.h>
#include <stdint.h>

#define FS 48000.0f         // częstotliwość próbkowania kodeka
#define PI 3.14159265f
#define SMOOTH 0.001f       // wygładzanie parametrów, ~20 ms przy 48 kHz (bez skoków = bez trzasków)

// Wszystkie parametry pedału. Zmieniane z debuggera (Live Expressions: audio_params)
// i przez UART z ESP32 (esp32.c). Zakresy jak w tabeli w esp32.c.
// Tor: wejście L -> [CLEAN | DIST] -> EQ -> VOLUME -> limiter -> L/R
typedef struct {
	// przełączniki 0 / 1 (float, żeby wszystkie parametry były jednego typu)
	float bypass;               // 1 = wejście prosto na wyjście
	float fx_dist;              // 1 = przester, 0 = czysty tor (clean_gain)
	float volume;               // 0 .. 1
	float eq_low_db;            // -12 .. +12 dB, półka 100 Hz
	float eq_mid_db;            // -12 .. +12 dB, szczyt 800 Hz
	float eq_high_db;           // -12 .. +12 dB, półka 3.5 kHz
	// CLEAN
	float clean_gain;           // 0 .. 64
	// DIST
	float drive;                // 1 .. 200
	float clip_level;           // 100 .. 30000 (clip symetryczny +-clip_level)
	// KODEK / ESP32
	float linvol;               // wzmocnienie wejścia kodeka 0 .. 23 (15 = -12 dB, 23 = 0 dB)
	float fft;                  // 1 = widmo do ESP32 co 100 ms
} AudioParams;

extern volatile AudioParams audio_params;

void Audio_Init(void);
void Audio_Process(volatile int16_t *pOut, volatile int16_t *pIn, uint16_t length);
void Audio_Update(void);        // pętla główna: przelicza współczynniki EQ po zmianie gałek
void Audio_NextEffect(void);    // przycisk: CLEAN <-> DIST

#endif
