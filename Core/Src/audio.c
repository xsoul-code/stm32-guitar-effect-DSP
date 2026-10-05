#include "audio.h"

volatile float drive = 60.0f;
volatile float clip_level = 1500.0f;

void Distortion_Init(void) // TODO: Distortion proper initialization with parameters
{}

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

	const float hpf_alpha = 0.05f;
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
