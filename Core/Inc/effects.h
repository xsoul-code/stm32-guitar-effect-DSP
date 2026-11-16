#ifndef EFFECTS_H
#define EFFECTS_H

// Efekty w torze (audio.c, KROK 7). Parametry czytane z audio_params.
// Każdy efekt liczy się zawsze (stała praca CPU), fx_* tylko płynnie otwiera/zamyka miks.
//   X_Init    - zerowanie linii opóźniających (start)
//   X_Process - przerwanie audio, jedna próbka

void Chorus_Init(void);
float Chorus_Process(float x);                              // zwraca suchy + mokry (mix)

#endif
