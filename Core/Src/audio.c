#include "audio.h"

/* Struktura 2-biegunowego filtru IIR (Biquad Direct Form 1) */
typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float x1, x2;
    float y1, y2;
} BiquadFilter;

static BiquadFilter lpf_l = {0};
static BiquadFilter lpf_r = {0};

volatile float drive = 20.0f;
volatile float clip_level = 500.0f;

/* Inicjalizacja filtru dolnoprzepustowego Butterwortha 2. rzędu (odcięcie 4.5 kHz przy fs = 48 kHz) */
void Distortion_Init(void)
{
    // Współczynniki przeliczone dla fs = 48 kHz, fc = 4500 Hz, Q = 0.707 (Butterworth)
    const float b0 = 0.06050f;
    const float b1 = 0.12100f;
    const float b2 = 0.06050f;
    const float a1 = -1.19393f;
    const float a2 = 0.43592f;

    lpf_l.b0 = b0; lpf_l.b1 = b1; lpf_l.b2 = b2;
    lpf_l.a1 = a1; lpf_l.a2 = a2;

    lpf_r.b0 = b0; lpf_r.b1 = b1; lpf_r.b2 = b2;
    lpf_r.a1 = a1; lpf_r.a2 = a2;
}

/* Szybkie przetwarzanie jednej próbki przez filtr IIR */
static inline float Biquad_Process(BiquadFilter *f, float in)
{
    float out = (f->b0 * in) + (f->b1 * f->x1) + (f->b2 * f->x2) - (f->a1 * f->y1) - (f->a2 * f->y2);

    // Przesunięcie historii
    f->x2 = f->x1;
    f->x1 = in;
    f->y2 = f->y1;
    f->y1 = out;

    return out;
}
/* Nie używana implementacja
static inline void FX_Dist(float x_l, float x_r, float tight_l, float tight_r, float drive, float clip_level)
{
	x_l = tight_l * drive;
	x_r = tight_r * drive;

	if (x_l > clip_level)        x_l = clip_level;
	else if (x_l < -clip_level * 1.2f) x_l = -clip_level * 1.2f;

	if (x_r > clip_level)        x_r = clip_level;
	else if (x_r < -clip_level * 1.2f) x_r = -clip_level * 1.2f;
}
*/
void Audio_Process(volatile int16_t *pOut, volatile int16_t *pIn, uint16_t length)
{
	// PARAMETRY BRZMIENIA:
	           // Siła przesteru (20.0 - 60.0)
	    // Poziom obcięcia fali

	// 1. Filtr górnoprzepustowy (HPF) PRZED przesterem:
	// Odcina sub-bas i dudnienie (~300 Hz przy fs = 48 kHz, tak samo jak 0.02 przy 96 kHz)
	// Zwiększ hpf_alpha, żeby jeszcze bardziej odchudzić bas
	const float hpf_alpha = 0.039f;
	static float hpf_dc_l = 0.0f;
	static float hpf_dc_r = 0.0f;

	// 2. Filtr dolnoprzepustowy (LPF / Tone) PO przesterze:
	// Wygładza ostre krawędzie i likwiduje cyfrowy aliasing (~5.4 kHz przy fs = 48 kHz)
	const float tone_alpha = 0.51f;
	static float tone_l = 0.0f;
	static float tone_r = 0.0f;

	for (uint16_t i = 0; i < length; i += 2)
	{
		float in_l = (float)pIn[i];
		float in_r = (float)pIn[i + 1];

		// --- KROK 1: PRE-HPF (odcinanie dudniącego basu przed przesterem) ---
		hpf_dc_l += hpf_alpha * (in_l - hpf_dc_l);
		float tight_l = in_l - hpf_dc_l;

		hpf_dc_r += hpf_alpha * (in_r - hpf_dc_r);
		float tight_r = in_r - hpf_dc_r;

		// --- KROK 2: WZMOCNIENIE I HARD CLIP ---

		//float x_l = 0, x_r = 0;
		float x_l = tight_l * drive;
		float x_r = tight_r * drive;

		if (x_l > clip_level)        x_l = clip_level;
		else if (x_l < -clip_level * 1.2f) x_l = -clip_level * 1.2f;

		if (x_r > clip_level)        x_r = clip_level;
		else if (x_r < -clip_level * 1.2f) x_r = -clip_level * 1.2f;

		// --- KROK 3: WYRÓWNANIE GŁOŚNOŚCI ---
		float boost_l = x_l * (26000.0f / clip_level);
		float boost_r = x_r * (26000.0f / clip_level);

		// --- KROK 4: POST-LPF (wygładzanie góry i aliasingu) ---

		tone_l += tone_alpha * (boost_l - tone_l);
		tone_r += tone_alpha * (boost_r - tone_r);

		// --- KROK 5: LIMITER WYJŚCIOWY ---
		if (tone_l > 32000.0f)  tone_l = 32000.0f;
		else if (tone_l < -32000.0f) tone_l = -32000.0f;

		if (tone_r > 32000.0f)  tone_r = 32000.0f;
		else if (tone_r < -32000.0f) tone_r = -32000.0f;

		pOut[i]     = (int16_t)tone_l;
		pOut[i + 1] = (int16_t)tone_r;
	}
}
