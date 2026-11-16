#include "audio.h"
#include "stm32f4xx.h"
#include <string.h>

volatile AudioParams audio_params = {
	.bypass             = 0,
	.fx_dist            = 1,
	.volume             = 1.0f,
	.eq_low_db          = 0.0f,
	.eq_mid_db          = 0.0f,
	.eq_high_db         = 0.0f,
	.clean_gain         = 20.0f,    // ~poziom DIST przy typowym graniu
	.drive              = 60.0f,
	.clip_level         = 1500.0f,
	.linvol             = 15,       // -12 dB
};

volatile uint32_t dbg_cycles_max = 0;   // max cykli CPU na jedno Audio_Process (podgląd w debuggerze)

#define MIX_HEADROOM 0.5f       // -6 dB zapasu za przesterem, EQ może podbić o +12 dB

// Wygładzone parametry
static float drive_s, clean_s, clip_s, vol_s, dist_s, bypass_s;

// EQ: 3 biquady (low shelf, peak, high shelf)
typedef struct {
	float b0, b1, b2, a1, a2;
} Biquad;

static Biquad eq[3];
static float eq_x1[3], eq_x2[3], eq_y1[3], eq_y2[3];

// półki z S = 1 (najbardziej stroma półka bez przegięcia)
static Biquad EQ_LowShelf(float f0, float gain_db) // source: [BristowJohnson2021] Audio EQ Cookbook
{
	float A = powf(10.0f, gain_db / 40.0f);
	float w = 2.0f * PI * f0 / FS;
	float c = cosf(w);
	float sa = 2.0f * sqrtf(A) * sinf(w) * 0.70710678f;   // 2*sqrt(A)*alpha
	float a0 = (A + 1) + (A - 1) * c + sa;
	Biquad q;
	q.b0 = A * ((A + 1) - (A - 1) * c + sa) / a0;
	q.b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0;
	q.b2 = A * ((A + 1) - (A - 1) * c - sa) / a0;
	q.a1 = -2 * ((A - 1) + (A + 1) * c) / a0;
	q.a2 = ((A + 1) + (A - 1) * c - sa) / a0;
	return q;
}

static Biquad EQ_HighShelf(float f0, float gain_db) // source: [BristowJohnson2021] Audio EQ Cookbook
{
	float A = powf(10.0f, gain_db / 40.0f);
	float w = 2.0f * PI * f0 / FS;
	float c = cosf(w);
	float sa = 2.0f * sqrtf(A) * sinf(w) * 0.70710678f;
	float a0 = (A + 1) - (A - 1) * c + sa;
	Biquad q;
	q.b0 = A * ((A + 1) + (A - 1) * c + sa) / a0;
	q.b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
	q.b2 = A * ((A + 1) + (A - 1) * c - sa) / a0;
	q.a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
	q.a2 = ((A + 1) - (A - 1) * c - sa) / a0;
	return q;
}

static Biquad EQ_Peak(float f0, float gain_db, float Q) // source: [BristowJohnson2021] Audio EQ Cookbook
{
	float A = powf(10.0f, gain_db / 40.0f);
	float w = 2.0f * PI * f0 / FS;
	float alpha = sinf(w) / (2.0f * Q);
	float a0 = 1 + alpha / A;
	Biquad q;
	q.b0 = (1 + alpha * A) / a0;
	q.b1 = -2 * cosf(w) / a0;
	q.b2 = (1 - alpha * A) / a0;
	q.a1 = q.b1;
	q.a2 = (1 - alpha / A) / a0;
	return q;
}

// Pętla główna: współczynniki EQ liczone tylko po zmianie gałek (cos, sin, pow są za wolne
// na każdą próbkę)
void Audio_Update(void)
{
	static float low = 100.0f, mid = 100.0f, high = 100.0f;    // 100 = jeszcze nie policzone
	float l = audio_params.eq_low_db, m = audio_params.eq_mid_db, h = audio_params.eq_high_db;
	if (l == low && m == mid && h == high) return;
	low = l;
	mid = m;
	high = h;

	Biquad nowe[3];
	nowe[0] = EQ_LowShelf(100.0f, low);
	nowe[1] = EQ_Peak(800.0f, mid, 0.8f);
	nowe[2] = EQ_HighShelf(3500.0f, high);

	// podmiana całego zestawu naraz, żeby przerwanie nie wzięło połowy starych współczynników
	__disable_irq();
	memcpy(eq, nowe, sizeof(eq));
	__enable_irq();
}

void Audio_Init(void)
{
	Audio_Update();

	drive_s  = audio_params.drive;
	clean_s  = audio_params.clean_gain;
	clip_s   = audio_params.clip_level;
	vol_s    = audio_params.volume;
	dist_s   = audio_params.fx_dist != 0;
	bypass_s = audio_params.bypass != 0;

	// Licznik cykli DWT do pomiaru obciążenia CPU (source: [RM0090])
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void Audio_NextEffect(void)
{
	audio_params.fx_dist = !audio_params.fx_dist;     // CLEAN <-> DIST
}

// Wszystko liczy się zawsze, przełączniki to tylko płynne miksy. Przerwanie trwa wtedy
// zawsze tyle samo i zakłócenia od CPU w ADC kodeka są tylko na 24 kHz (poza pasmem).
void Audio_Process(volatile int16_t *pOut, volatile int16_t *pIn, uint16_t length)
{
	uint32_t t0 = DWT->CYCCNT;

	// filtry 1. rzędu y += a * (x - y), source: [Zielinski2005]
	const float hpf_alpha = 0.05f;      // ~390 Hz przy fs = 48 kHz
	static float hpf_dc = 0.0f;

	// 2. Filtr dolnoprzepustowy (LPF / Tone) PO przesterze:
	// Wygładza ostre krawędzie (~5.4 kHz przy fs = 48 kHz)
	const float tone_alpha = 0.51f;
	static float tone = 0.0f;

	// poprzednie próbki dla oversamplingu
	static float tight_prev = 0.0f, os_prev = 0.0f;

	// usuwanie składowej stałej po przesterze (~30 Hz): obcięty sygnał gitary nie jest symetryczny
	const float dc_alpha = 0.004f;
	static float dc = 0.0f;

	for (uint16_t i = 0; i < length; i += 2)
	{
		float in = (float)pIn[i];       // gitara tylko na lewym wejściu

		drive_s  += SMOOTH * (audio_params.drive - drive_s);
		clean_s  += SMOOTH * (audio_params.clean_gain - clean_s);
		clip_s   += SMOOTH * (fmaxf(audio_params.clip_level, 100.0f) - clip_s);
		vol_s    += SMOOTH * (audio_params.volume - vol_s);
		dist_s   += SMOOTH * ((audio_params.fx_dist != 0) - dist_s);
		bypass_s += SMOOTH * ((audio_params.bypass != 0) - bypass_s);

		// --- KROK 1: PRE-HPF (odcinanie dudniącego basu przed przesterem) ---
		hpf_dc += hpf_alpha * (in - hpf_dc);
		float tight = (in - hpf_dc) * drive_s;

		// --- KROK 2: 4x OVERSAMPLING I HARD CLIP ---
		// Clip robi harmoniczne ponad 24 kHz, które odbijają się w pasmo (aliasing).
		// Dlatego clip 4 razy na próbkę w punktach na prostej między próbkami, potem średnia.
		// source: [Yeh2009] aliasing w przesterze i nadpróbkowanie
		// średnia z 2 próbek zeruje 24 kHz (zakłócenie od CPU), żeby nie wchodziło do clipa
		float t = 0.5f * (tight + tight_prev);
		tight_prev = tight;

		float sum = 0.0f;
		for (int j = 1; j <= 4; j++)
		{
			float u = os_prev + (t - os_prev) * (j / 4.0f);     // punkt na prostej
			sum += fminf(fmaxf(u, -clip_s), clip_s);            // hard clip (fminf/fmaxf bez skoków, zawsze ten sam czas)
		}
		os_prev = t;
		float x = sum / 4.0f;

		// --- KROK 3: WYRÓWNANIE GŁOŚNOŚCI ---
		float boost = x * (24000.0f / clip_s);

		// --- KROK 4: POST-LPF (wygładzanie góry) + USUNIĘCIE DC (ten sam filtr co w KROKU 1) ---
		tone += tone_alpha * (boost - tone);
		dc += dc_alpha * (tone - dc);
		float dist = tone - dc;

		// --- KROK 5: CLEAN / DIST (płynne przejście) + ZAPAS ---
		float clean = in * clean_s;
		float y = MIX_HEADROOM * (clean + dist_s * (dist - clean));

		// --- KROK 6: EQ (3 biquady, Direct Form 1) ---
		// source: [BristowJohnson2021]
		for (int b = 0; b < 3; b++)
		{
			float out = eq[b].b0 * y + eq[b].b1 * eq_x1[b] + eq[b].b2 * eq_x2[b]
			          - eq[b].a1 * eq_y1[b] - eq[b].a2 * eq_y2[b];
			eq_x2[b] = eq_x1[b];
			eq_x1[b] = y;
			eq_y2[b] = eq_y1[b];
			eq_y1[b] = out;
			y = out;
		}

		// --- KROK 7: EFEKTY ---
		// TODO
		float out = y;

		// --- KROK 8: GŁOŚNOŚĆ, BYPASS, OGRANICZNIK ---
		out *= vol_s;
		out += bypass_s * (in - out);
		out = fminf(fmaxf(out, -32000.0f), 32000.0f);      // twarde obcięcie tuż pod pełną skalą
		pOut[i]     = (int16_t)out;                         // mono: ten sam sygnał na L i R
		pOut[i + 1] = (int16_t)out;
	}

	uint32_t dt = DWT->CYCCNT - t0;
	if (dt > dbg_cycles_max) dbg_cycles_max = dt;
}
