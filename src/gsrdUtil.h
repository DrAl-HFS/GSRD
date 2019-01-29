// gsrdUtil.h - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb 2018 - Jan 2019

#ifndef GSRD_UTIL_H
#define GSRD_UTIL_H

#include "util.h"



typedef struct
{
   size_t *pH, nH;
} HistZ;

//typedef struct { union { void *p; size_t w; }; size_t b; } Buffer; // char *p;

typedef struct { F32 x, y; } V2F32;
typedef struct { U16 x, y; } V2U16;
typedef struct { U32 x, y; } V2U32;
typedef struct { I32 min, max; } MinMaxI32;
typedef struct { U32 min, max; } MinMaxU32;
typedef struct { size_t min, max; } MinMaxZU;
typedef struct { F64 min, max; } MinMaxF64;

typedef struct { U16 start, len; } ScanSeg;


/***/

extern U32 statGetRes1 (StatRes1 * const pR, const StatMom * const pS, const SMVal dof);

extern I32 scanEnvID (I32 v[], I32 max, const char *id);

extern I32 charInSet (const char c, const char set[]);
extern I32 skipSet (const char s[], const char set[]);
extern I32 skipPastSet (const char str[], const char set[]);

extern I32 scanNI32 (I32 v[], const I32 maxV, const char str[], I32 *pNS, const char skip[], const char end[]);
extern I32 scanNF32 (F32 v[], const I32 maxV, const char str[], I32 *pNS, const char skip[], const char end[]);
extern I32 scanTableF32 (F32 v[], I32 maxV, MinMaxI32 *pW, const char str[], I32 *pNS, const I32 maxS);

// Assess linear progression in a sequence of numbers, using first and last as reference
// Based on "Gaussian linear regression" style SSD measurement on expected even spacing of intervening values
// Returns measure from 1.0 (perfect linearity) to 0.0 or lower (when deviations exceed expected spacing)
extern F32 linearityF32 (const F32 t[], const I32 n);

extern size_t sumNZU (const size_t z[], const size_t n);
extern size_t accumNZU (size_t a[], const size_t z[], const size_t n);
extern double scaleFNZU (F64 f[], const size_t z[], const size_t n, const F64 s);

#endif // GSRD_UTIL_H
