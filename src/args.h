// args.h - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-October 2018

#ifndef ARGS_H
#define ARGS_H

#include "util.h"


#define PROC_FLAG_ACCHOST  (1<<0)
#define PROC_FLAG_ACCGPU   (1<<1)


#define DFI_FLAG_VALIDATED (1<<0)

#define FLAG_INIT_ORG_INTERLEAVED   (1<<0)
//#define FLAG_INIT_BOUND_PERIODIC  (0<<1)
#define FLAG_INIT_BOUND_REFLECT   (1<<1)
#define FLAG_INIT_MAP            (1<<2)

#define FLAG_FILE_LUT  (1<<2)
#define FLAG_FILE_XFER (1<<1)
#define FLAG_FILE_OUT  (1<<0)

#define ID_FILE_NONE   (0x00)
#define ID_FILE_RAW    (0x01)
#define ID_FILE_RGB    (0x02)
#define ID_MASK        (0x0F)
#define USAGE_NONE     (0x00)
#define USAGE_INITIAL  (0x10)
#define USAGE_PERIODIC (0x20)
#define USAGE_FINAL    (0x40)
#define USAGE_XFER     (0x80)
#define USAGE_MASK     (0xF0)


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
   char  id[4];
   U32   n[2];
   F32   s[2];
} PatternInfo;

typedef struct
{
   PatternInfo pattern;
   V2U16 def;
   U8    nD;
   U8    flags;
   U8    pad[2];
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


/***/

extern size_t scanDFI (DataFileInfo * pDFI, const char * const path);
// extern const char *sc (const char *s, const char c, const char * const e, const I8 o);
//extern int scanVI (int v[], const int vMax, ScanSeg * const pSS, const char s[]);
extern int scanArgs (ArgInfo * pAI, const char * const a[], int nA);

extern void usage (void);

#endif // ARGS_H
