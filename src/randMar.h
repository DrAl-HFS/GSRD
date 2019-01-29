// randMar.h - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-October 2018

#ifndef RAND_MAR_H
#define RAND_MAR_H

#include "util.h"


// Marsaglia MWC PRNG
typedef struct { uint z, w; } SeedMMWC;
typedef struct { float f[2]; SeedMMWC s; } RandF;

/***/

extern uint randMMWC (SeedMMWC *pS);
extern void initSeedMMWC (SeedMMWC *pS, U16 id);
extern double randN (RandF *pRF);
extern void initRF (RandF *pRF, float scale, float offset, U16 sid);
extern float randF (RandF *pRF);
extern void scaleRF (RandF *pRF, float s);

#endif // RAND_MAR_H
