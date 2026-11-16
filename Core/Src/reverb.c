#include "effects.h"
#include "audio.h"
#include <string.h>

// Pogłos Schroedera (układ jak we Freeverbie): 8 równoległych filtrów grzebieniowych
// z LPF w pętli (gęste odbicia, wysokie gasną szybciej) -> 4 szeregowe allpassy (rozmycie ech).
// source: [Schroeder1962], [Smith2010] Freeverb
// Wyłączenie fx_reverb zamyka tylko wejście - ogon wybrzmiewa.
// Bufory w CCM RAM (64 KB tylko dla CPU, bez DMA) - w zwykłym RAM jest już linia echa.
// Startup nie zeruje CCM, więc robi to Reverb_Init.

#define CCM __attribute__((section(".ccmram")))

// długości z Freeverba (próbki), wzajemnie "niewspółmierne", żeby echa się nie nakładały [Smith2010]
static const uint16_t comb_len[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const uint16_t ap_len[4]   = { 556, 441, 341, 225 };

#define AP_G    0.5f            // wzmocnienie allpassów
#define DAMP    0.405f          // LPF w pętli grzebienia, ~4 kHz (wysokie gasną szybciej)
#define IN_GAIN 0.16f           // poziom wejścia do grzebieni: przy decay 0.5 i mix 1 ogon ~ tak głośny jak suchy

static float comb_buf[8][1617] CCM;     // każdy grzebień używa pierwszych comb_len[c] próbek
static float ap_buf[4][556] CCM;
static uint16_t comb_pos[8], ap_pos[4];
static float comb_lp[8];                // stan LPF w pętli każdego grzebienia

// wygładzone parametry
static float send_s, mix_s;

void Reverb_Init(void)
{
	memset(comb_buf, 0, sizeof(comb_buf));
	memset(ap_buf, 0, sizeof(ap_buf));
	memset(comb_pos, 0, sizeof(comb_pos));
	memset(ap_pos, 0, sizeof(ap_pos));
	memset(comb_lp, 0, sizeof(comb_lp));

	send_s = audio_params.fx_reverb != 0;
	mix_s  = audio_params.reverb_mix;
}

float Reverb_Process(float x)
{
	send_s += SMOOTH * ((audio_params.fx_reverb != 0) - send_s);
	mix_s  += SMOOTH * (audio_params.reverb_mix - mix_s);

	// decay 0..1 -> sprzężenie grzebieni 0.84..0.99 (ogon ~1 s .. ~15 s)
	float g = 0.84f + 0.15f * audio_params.reverb_decay;

	float in = send_s * x * IN_GAIN;

	// --- 8 GRZEBIENI RÓWNOLEGLE (y[n] = x[n] + g * LPF(y[n-M])), source: [Schroeder1962], [Smith2010] ---
	float sum = 0.0f;
	for (int c = 0; c < 8; c++) {
		float y = comb_buf[c][comb_pos[c]];
		comb_lp[c] += DAMP * (y - comb_lp[c]);
		comb_buf[c][comb_pos[c]] = in + g * comb_lp[c];
		if (++comb_pos[c] >= comb_len[c]) comb_pos[c] = 0;
		sum += y;
	}

	// --- 4 ALLPASSY SZEREGOWO (v = x + g*v[n-M], y = v[n-M] - g*v), source: [Schroeder1962] ---
	float y = sum;
	for (int a = 0; a < 4; a++) {
		float dly = ap_buf[a][ap_pos[a]];
		float v = y + AP_G * dly;
		y = dly - AP_G * v;
		ap_buf[a][ap_pos[a]] = v;
		if (++ap_pos[a] >= ap_len[a]) ap_pos[a] = 0;
	}

	return mix_s * y;
}
