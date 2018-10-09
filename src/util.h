
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

#define DFI_FLAG_VALIDATED (1<<0)

#define FLAG_INIT_ORG_INTERLEAVED   (1<<0)
#define FLAG_FILE_LUT  (1<<1)
#define FLAG_FILE_OUT  (1<<0)


typedef int Bool32;

typedef signed char  I8;
typedef signed short I16;
typedef signed long  I32;

typedef unsigned char  U8;
typedef unsigned short U16;
typedef unsigned long  U32;

typedef double SMVal; // Stat measure value
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
   size_t bytes;
   void  *p;
} MemBuff;

typedef struct { U16 x, y; } V2U16;
typedef struct { U32 x, y; } V2U32;
typedef struct { I32 min, max; } MinMaxI32;
typedef struct { U32 min, max; } MinMaxU32;
typedef struct { size_t min, max; } MinMaxZU;

typedef struct { U16 start, len; } ScanSeg;

typedef struct
{
   const char  *filePath;
   size_t      bytes, iter;
   ScanSeg     vSS;
   int         v[4], nV;
   V2U16       def;
   U8          elemBits;
   U8          flags;
   U8          pad[2];
} DataFileInfo;

typedef struct
{
   DataFileInfo  init, cmp;
   const char    *lutPath, *outName, *outPath[2];
   U8             flags, nOutPath, outType[2];
} FileInfo;

typedef struct
{
   V2U16 def;
   U8    nD, patternID;
   U8    flags;
   U8    pad[1];
} InitInfo;

typedef struct
{
   float v[3];
   U8    nV;
   char  id[3];
} ParamInfo;

typedef struct
{
   size_t   flags, maxIter, subIter;
} ProcInfo;

typedef struct
{
   FileInfo    files;
   InitInfo    init;
   ParamInfo   param;
   ProcInfo    proc;
} ArgInfo;

typedef struct { union { void *p; size_t w; }; size_t b; } Buffer; // char *p;

// Marsaglia MWC PRNG
typedef struct { uint z, w; } SeedMMWC;
typedef struct { float f[2]; SeedMMWC s; } RandF;

/***/

extern Bool32 validBuff (const Buffer *pB, size_t b);
extern size_t fileSize (const char * const path);
extern size_t loadBuff (void * const pB, const char * const path, const size_t bytes);
extern size_t saveBuff (const void * const pB, const char * const path, const size_t bytes);

extern SMVal deltaT (void);
extern U32 statGetRes1 (StatRes1 * const pR, const StatMom * const pS, const SMVal dof);

extern int scanEnvID (int v[], int max, const char *id);

extern int skipPastSet (const char str[], const char set[]);
extern int scanTableF32 (float v[], int maxV, MinMaxI32 *pW, const char str[], int *pNS, const int maxS);

// extern const char *sc (const char *s, const char c, const char * const e, const I8 o);
//extern int scanVI (int v[], const int vMax, ScanSeg * const pSS, const char s[]);
extern int scanArgs (ArgInfo * pAI, const char * const a[], int nA);

extern uint randMMWC (SeedMMWC *pS);
extern void initSeedMMWC (SeedMMWC *pS, U16 id);
extern double randN (RandF *pRF);
extern void initRF (RandF *pRF, float scale, float offset, U16 sid);
extern float randF (RandF *pRF);

#endif // UTIL_H
