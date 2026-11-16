#include "esp32.h"
#include "audio.h"
#include "wm8731.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BAUD        115200
#define RX_BUF_LEN  256         // bufor kołowy na odebrane bajty
#define LINE_LEN    64          // maks. długość linii razem z '\n'

static volatile uint8_t rx_buf[RX_BUF_LEN];
static volatile uint32_t rx_head;   // pisze przerwanie
static uint32_t rx_tail;            // czyta pętla główna

// Tabela parametrów: kolejność = kolejność odpowiedzi na "GET ALL".
// SET poza zakresem jest obcinany do min / max (to nie błąd).
typedef struct {
	const char *name;
	volatile float *ptr;
	float min, max;
} Param;

static const Param params[] = {
	{ "bypass",         &audio_params.bypass,         0, 1 },
	{ "fx_dist",        &audio_params.fx_dist,        0, 1 },
	{ "volume",         &audio_params.volume,         0, 1 },
	{ "eq_low_db",      &audio_params.eq_low_db,      -12, 12 },
	{ "eq_mid_db",      &audio_params.eq_mid_db,      -12, 12 },
	{ "eq_high_db",     &audio_params.eq_high_db,     -12, 12 },
	{ "clean_gain",     &audio_params.clean_gain,     0, 64 },
	{ "drive",          &audio_params.drive,          1, 200 },
	{ "clip_level",     &audio_params.clip_level,     100, 30000 },
	{ "linvol",         &audio_params.linvol,         0, WM8731_LINVOL_MAX },
};
#define N_PARAMS (sizeof(params) / sizeof(params[0]))

// Przerwanie USART3: każdy odebrany bajt do bufora kołowego (priorytet niższy niż audio)
// source: [RM0090] rozdz. USART (rejestry SR, DR, BRR, CR1)
void USART3_IRQHandler(void)
{
	uint32_t sr = USART3->SR;
	uint8_t c = USART3->DR;         // odczyt DR kasuje też flagę przepełnienia (ORE)

	if (sr & USART_SR_RXNE) {
		rx_buf[rx_head] = c;
		rx_head = (rx_head + 1) % RX_BUF_LEN;
	}
}

// Wysyłanie na czekaniu - linia 64 B to ~5.5 ms, w pętli głównej nie przeszkadza
static void Send(const char *s)
{
	while (*s) {
		while (!(USART3->SR & USART_SR_TXE)) {}
		USART3->DR = *s++;
	}
}

static const Param *FindParam(const char *name)
{
	for (uint32_t i = 0; i < N_PARAMS; i++)
		if (strcmp(params[i].name, name) == 0)
			return &params[i];
	return NULL;
}

static void SendVal(const Param *p)
{
	char buf[LINE_LEN + 1];      // + miejsce na '\0'

	// printf z newlib-nano nie ma %f -> wartość w tysięcznych i "%d.%03d"
	int m = (int)roundf(*p->ptr * 1000.0f);
	snprintf(buf, sizeof(buf), "VAL %s %s%d.%03d\n", p->name, m < 0 ? "-" : "", abs(m) / 1000, abs(m) % 1000);
	Send(buf);
}

static void SendErr(const char *code, const char *name)
{
	char buf[LINE_LEN + 1];      // + miejsce na '\0'
	snprintf(buf, sizeof(buf), "ERR %s %.45s\n", code, name);  // nazwa ucięta, żeby linia zmieściła się w 64 B
	Send(buf);
}

static void Command(const char *line)
{
	char cmd[LINE_LEN] = "", name[LINE_LEN] = "", val[LINE_LEN] = "";
	int n = sscanf(line, "%63s %63s %63s", cmd, name, val);

	if (strcmp(cmd, "PING") == 0) {
		Send("PONG\n");
		return;
	}

	if (strcmp(cmd, "GET") == 0 && n >= 2) {
		if (strcmp(name, "ALL") == 0) {
			for (uint32_t i = 0; i < N_PARAMS; i++)
				SendVal(&params[i]);
			Send("END\n");
			return;
		}
		const Param *p = FindParam(name);
		if (p) SendVal(p);
		else   SendErr("UNKNOWN_PARAM", name);
		return;
	}

	if (strcmp(cmd, "SET") == 0 && n >= 2) {
		const Param *p = FindParam(name);
		if (!p) {
			SendErr("UNKNOWN_PARAM", name);
			return;
		}

		// strtof zwraca w end miejsce, gdzie skończyła się liczba - musi to być koniec napisu
		char *end;
		float v = strtof(val, &end);
		if (n < 3 || end == val || *end != '\0') {
			SendErr("BAD_VALUE", name);
			return;
		}

		*p->ptr = fminf(fmaxf(v, p->min), p->max);
		SendVal(p);     // odpowiedź = wartość naprawdę ustawiona
		return;
	}

	Send("ERR UNKNOWN_CMD\n");
}

void ESP32_Init(void)
{
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_USART3_CLK_ENABLE();

	// PD8 = TX, PD9 = RX (AF7), wolne zbocza - mniej zakłóceń w audio
	GPIO_InitTypeDef g = {0};
	g.Pin = GPIO_PIN_8 | GPIO_PIN_9;
	g.Mode = GPIO_MODE_AF_PP;
	g.Pull = GPIO_PULLUP;
	g.Speed = GPIO_SPEED_FREQ_LOW;
	g.Alternate = GPIO_AF7_USART3;
	HAL_GPIO_Init(GPIOD, &g);

	// 115200 8N1, przerwanie od odebranego bajtu
	USART3->BRR = HAL_RCC_GetPCLK1Freq() / BAUD;
	USART3->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;

	HAL_NVIC_SetPriority(USART3_IRQn, 2, 0);    // audio (DMA) ma 0
	HAL_NVIC_EnableIRQ(USART3_IRQn);

	// Od resetu do tej chwili (~220 ms, start kodeka) PD8 nie jest jeszcze wyjściem i linia pływa -
	// ESP32 zbiera śmieci bez '\n'. Pierwsze '\n' kończy tę linię, więc HELLO dochodzi czyste.
	Send("\nHELLO 4\n");
}

void ESP32_Poll(void)
{
	static char line[LINE_LEN];
	static uint32_t len = 0;

	while (rx_tail != rx_head) {
		char c = rx_buf[rx_tail];
		rx_tail = (rx_tail + 1) % RX_BUF_LEN;

		if (c == '\r') continue;
		if (c == '\n') {
			line[len] = '\0';
			if (len > 0) Command(line);
			len = 0;
		}
		else if (len < LINE_LEN - 1) line[len++] = c;    // za długa linia jest ucinana
	}
}

void ESP32_SendParam(const char *name)
{
	const Param *p = FindParam(name);
	if (p) SendVal(p);
}

