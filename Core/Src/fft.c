#include "fft.h"
#include "audio.h"

// Bufor kołowy z ostatnimi próbkami wyjścia - przerwanie pisze zawsze (stała praca)
static float fft_buf[FFT_N];
static volatile uint32_t fft_pos;

static float re[FFT_N], im[FFT_N];

// Granice pasm w binach (bin = 93.75 Hz), mniej więcej co pół oktawy:
// 94 Hz, 188, 281, 375, 469, 563-656, 750-938, 1.0-1.4k, 1.5-2.1k, 2.2-2.9k,
// 3.0-4.1k, 4.2-5.9k, 6.0-8.4k, 8.5-11.9k, 12-17k, 17-24k
static const uint16_t band_edge[FFT_BANDS + 1] = {
	1, 2, 3, 4, 5, 6, 8, 11, 16, 23, 32, 45, 64, 91, 128, 181, 256
};

void FFT_Push(float x)
{
	fft_buf[fft_pos] = x;
	fft_pos = (fft_pos + 1) % FFT_N;
}

// FFT radix-2 (Cooley-Tukey) w miejscu, n = potęga dwójki
static void FFT(float *re, float *im, int n) // source: [Zielinski2005] FFT o podstawie 2
{
	// przestawienie próbek w kolejności odwróconych bitów
	for (int i = 1, j = 0; i < n; i++) {
		int bit = n >> 1;
		for (; j & bit; bit >>= 1)
			j ^= bit;
		j ^= bit;
		if (i < j) {
			float t = re[i]; re[i] = re[j]; re[j] = t;
			t = im[i]; im[i] = im[j]; im[j] = t;
		}
	}

	// motylki: etapy po 2, 4, 8 ... n punktów
	for (int len = 2; len <= n; len <<= 1) {
		float wr = cosf(-2.0f * PI / len);
		float wi = sinf(-2.0f * PI / len);
		for (int i = 0; i < n; i += len) {
			float cr = 1.0f, ci = 0.0f;     // współczynnik obrotu w^k
			for (int k = 0; k < len / 2; k++) {
				int a = i + k, b = i + k + len / 2;
				float tr = re[b] * cr - im[b] * ci;
				float ti = re[b] * ci + im[b] * cr;
				re[b] = re[a] - tr;
				im[b] = im[a] - ti;
				re[a] += tr;
				im[a] += ti;
				float t = cr * wr - ci * wi;
				ci = cr * wi + ci * wr;
				cr = t;
			}
		}
	}
}

void FFT_Bands(uint8_t bands[FFT_BANDS])
{
	// kopia od najstarszej próbki (przerwanie w tym czasie nadpisze najwyżej 1-2 próbki
	// na samym początku, a tam okno i tak jest ~0)
	uint32_t pos = fft_pos;
	for (int i = 0; i < FFT_N; i++) {
		float hann = 0.5f - 0.5f * cosf(2.0f * PI * i / FFT_N);    // okno Hanna [Zielinski2005]
		re[i] = fft_buf[(pos + i) % FFT_N] * hann;
		im[i] = 0.0f;
	}

	FFT(re, im, FFT_N);

	// w paśmie bierzemy najmocniejszy bin. Sinus o amplitudzie A daje |X| = A * N / 4
	// (okno Hanna), pełna skala = 32768
	for (int b = 0; b < FFT_BANDS; b++) {
		float max = 0.0f;
		for (int k = band_edge[b]; k < band_edge[b + 1]; k++) {
			float p = re[k] * re[k] + im[k] * im[k];
			if (p > max) max = p;
		}
		float dbfs = 10.0f * log10f(max + 1e-12f) - 20.0f * log10f(32768.0f * FFT_N / 4);
		bands[b] = (uint8_t)fminf(fmaxf(dbfs + 99.0f, 0.0f), 99.0f);
	}
}
