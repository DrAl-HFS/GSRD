// util.h - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-October 2018

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

// General compiler tweaks
#pragma clang diagnostic ignored "-Wmissing-field-initializers"


#define PROC_FLAG_ACCHOST  (1<<0)
#define PROC_FLAG_ACCGPU   (1<<1)

#ifndef SWAP
#define SWAP(Type,a,b) { Type tmp= (a); (a)= (b); (b)= tmp; }
#endif
#ifndef MIN
#define MIN(a,b) (a)<(b)?(a):(b)
#endif
#ifndef MAX
#define MAX(a,b) (a)>(b)?(a):(b)
#endif
#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

typedef int Bool32;

typedef signed char  I8;
typedef signed short I16;
typedef signed long  I32;

typedef unsigned char  U8;
typedef unsigned short U16;
typedef unsigned long  U32;

typedef float F32;
typedef double F64;

typedef double SMVal; // Stat measure value - use widest available IEEE type
typedef struct
{
   SMVal    m[3];
} StatMom;

typedef struct
{
   SMVal m,v;
} StatRes1;

typedef struct
{
   size_t *pH, nH;
} HistZ;

typedef struct
{
   size_t bytes;
   union { void *p; size_t w; };
} MemBuff;
//typedef struct { union { void *p; size_t w; }; size_t b; } Buffer; // char *p;

typedef struct { U16 x, y; } V2U16;
typedef struct { U32 x, y; } V2U32;
typedef struct { I32 min, max; } MinMaxI32;
typedef struct { U32 min, max; } MinMaxU32;
typedef struct { size_t min, max; } MinMaxZU;
typedef struct { F64 min, max; } MinMaxF64;

typedef struct { U16 start, len; } ScanSeg;


/***/

extern Bool32 validBuff (const MemBuff *pB, size_t b);

extern const char *stripPath (const char *path);

extern size_t fileSize (const char * const path);
extern size_t loadBuff (void * const pB, const char * const path, const size_t bytes);
extern size_t saveBuff (const void * const pB, const char * const path, const size_t bytes);

extern SMVal deltaT (void);
extern U32 statGetRes1 (StatRes1 * const pR, const StatMom * const pS, const SMVal dof);

extern I32 scanEnvID (I32 v[], I32 max, const char *id);

extern I32 charInSet (const char c, const char set[]);
extern I32 skipSet (const char s[], const char set[]);
extern I32 skipPastSet (const char str[], const char set[]);

extern I32 scanNI32 (I32 v[], const I32 maxV, const char str[], I32 *pNS, const char skip[], const char end[]);
extern I32 scanNF32 (F32 v[], const I32 maxV, const char str[], I32 *pNS, const char skip[], const char end[]);
extern I32 scanTableF32 (F32 v[], I32 maxV, MinMaxI32 *pW, const char str[], I32 *pNS, const I32 maxS);

extern size_t sumNZU (const size_t z[], const size_t n);
extern size_t accumNZU (size_t a[], const size_t z[], const size_t n);
extern double scaleFNZU (F64 f[], const size_t z[], const size_t n, const F64 s);

#endif // UTIL_H
