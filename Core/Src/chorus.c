#include "effects.h"
#include "audio.h"
#include <string.h>

// Chorus: sygnał opóźniony o 7 ms, opóźnienie "pływa" z LFO (+-5 ms przy depth = 1).
// Zmiana opóźnienia = lekka zmiana wysokości dźwięku, zmieszana z suchym daje efekt chorusa.
// source: [Dattorro1997b] Effect Design, Part 2: Delay Line Modulation and Chorus

#define CHORUS_LEN 1024         // 21 ms > 7 + 5 ms
#define DELAY_MS 7.0f           // środek opóźnienia (jak EHX Small Clone)
#define MIX 0.5f                // pół suchego, pół mokrego = chorus

static float line[CHORUS_LEN];  // linia opóźniająca (bufor kołowy)
static uint32_t pos;            // tu zapisujemy bieżącą próbkę
static float phase;             // faza LFO 0..1

// wygładzone parametry
static float depth_s, mix_s;

void Chorus_Init(void)
{
	memset(line, 0, sizeof(line));
	pos = 0;
	phase = 0.0f;

	depth_s = audio_params.chorus_depth;
	mix_s   = (audio_params.fx_chorus != 0) * MIX;
}

float Chorus_Process(float x)
{
	depth_s += SMOOTH * (audio_params.chorus_depth - depth_s);
	mix_s   += SMOOTH * ((audio_params.fx_chorus != 0) * MIX - mix_s);

	// LFO trójkątne -1..1 (faza rośnie od 0 do 1 i wraca do 0)
	phase += audio_params.chorus_rate_hz / FS;
	if (phase >= 1.0f) phase -= 1.0f;
	float lfo = 4.0f * fabsf(phase - 0.5f) - 1.0f;

	// odczyt d próbek wstecz, d nie jest całkowite -> interpolacja liniowa, source: [Dattorro1997b]
	float d = (DELAY_MS + depth_s * 5.0f * lfo) * (FS / 1000.0f);
	uint32_t di = (uint32_t)d;
	float frac = d - di;
	uint32_t i0 = (pos + CHORUS_LEN - di) % CHORUS_LEN;
	uint32_t i1 = (i0 + CHORUS_LEN - 1) % CHORUS_LEN;
	float wet = line[i0] + frac * (line[i1] - line[i0]);

	line[pos] = x;
	pos = (pos + 1) % CHORUS_LEN;

	return x + mix_s * (wet - x);
}
