#include <math.h>
#include <stdint.h>

void Distortion_Init(void);
void Audio_Process(volatile int16_t *pOut, volatile int16_t *pIn, uint16_t length);
