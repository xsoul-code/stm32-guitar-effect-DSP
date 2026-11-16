#include "effects.h"
#include "audio.h"
#include <string.h>

// Echo: linia opóźniająca 1 s, powtórzenia przez sprzężenie. W pętli filtr dolnoprzepustowy,
// więc każde kolejne powtórzenie jest ciemniejsze. Wyłączenie fx_echo zamyka tylko wejście -
// powtórzenia wybrzmiewają.
// source: [Wilmering2020] (echo taśmowe / delay)

#define ECHO_LEN 48000          // 1 s przy 48 kHz, int16 = 96 KB w SRAM

static int16_t echo_buf[ECHO_LEN];
static uint32_t pos;            // tu zapisujemy bieżącą próbkę
static float lp;                // stan filtra w pętli

// wygładzone parametry
static float fb_s, mix_s, send_s;

void Echo_Init(void)
{
	memset(echo_buf, 0, sizeof(echo_buf));
	pos = 0;
	lp = 0.0f;

	fb_s   = audio_params.echo_feedback;
	mix_s  = audio_params.echo_mix;
	send_s = audio_params.fx_echo != 0;
}

float Echo_Process(float x)
{
	send_s += SMOOTH * ((audio_params.fx_echo != 0) - send_s);
	fb_s   += SMOOTH * (audio_params.echo_feedback - fb_s);
	mix_s  += SMOOTH * (audio_params.echo_mix - mix_s);

	// odczyt próbki sprzed d próbek (960 .. 48000, czyli 20 ms .. 1 s)
	uint32_t d = (uint32_t)(audio_params.echo_time_ms * (FS / 1000.0f));
	if (d > ECHO_LEN) d = ECHO_LEN;
	float y = echo_buf[(pos + ECHO_LEN - d) % ECHO_LEN];

	// pętla: wejście + sprzężenie -> LPF (~2.7 kHz) -> zapis, LPF jak w audio.c [Zielinski2005]
	lp += 0.3f * (send_s * x + fb_s * y - lp);
	echo_buf[pos] = (int16_t)fminf(fmaxf(lp, -32767.0f), 32767.0f);
	pos = (pos + 1) % ECHO_LEN;

	return mix_s * y;
}
