#include <math.h>
#include <stdint.h>

#define GAIN 1
#define THRESHOLD 8000

void Distortion_Init(void);
void Audio_Process(volatile int16_t *pOut, volatile int16_t *pIn, uint16_t length);
