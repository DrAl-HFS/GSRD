// data.h - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb 2018 - Jan 2019

#ifndef DATA_H
#define DATA_H

#include "args.h"
#include "randMar.h"
#include "report.h"

#define KR0  (Scalar)0.125
#define KRA0 (Scalar)0.0115
#define KDB0 (Scalar)0.0195
#define KLA0 (Scalar)0.25
#define KLB0 (Scalar)0.025

#define HFB_MAX   (4)

//typedef float      Scalar;
typedef double       Scalar;
typedef signed long  Stride;
typedef signed long  Index;

// DEPRECATE: Initialisation parameters
typedef struct
{  // reaction, replenishment, death
   Scalar kRR, kRA, kDB;
} RRDParam;

typedef struct
{
   Scalar rLA, rLB;
} InitLapRateScale;

typedef struct
{  // spatial variation of parameters
   Scalar varR, varD;
   U32   max; // spatial domain max axial size
} InitSpatVarParam;

// GSRD algorithm parameters

typedef struct
{  // Symmetric 9-point kernel coefficients
   Scalar a[3], b[3];
} KLAB;

typedef struct
{
   KLAB   kL;
   //RRDParam rrd;
   Scalar kRR, kRA, kDB;
} BaseParamVal;

typedef struct
{
   Scalar *pK;
   Stride iKRR, iKRA, iKDB;
} VarParamVal;

typedef struct
{
   BaseParamVal base;
   VarParamVal  var;
} ParamVal;

// Other stuff
typedef struct
{
   Stride h[6];//, v[6];
} BoundaryWrap;

typedef struct
{
   V2U32  def;
   Stride stride[4];
   size_t n;
   Stride nhStepWrap[2][8]; // neighbourhood 
   BoundaryWrap wrap;
} ImgOrg;

typedef struct
{
   Scalar   min, max;
   size_t   n;
   StatMom  s;
} FieldStat;

typedef struct
{
   FieldStat a[2], b[2];
} BlockStat;

typedef struct
{
   Scalar      *pAB;
   BlockStat   s;
   size_t      iter;
   char        label[12];
} HostFB;

typedef struct
{
   MemBuff     mb;
   FieldStat  fsA, fsB;
   HistZ       hB;
   Scalar      hMap[2], rMap[2];
} StatG;

typedef struct
{
   HostFB   hfb[HFB_MAX];
   StatG    sg;
   Scalar   *pC; // conservation tally field ?
} HostBuffTab;

typedef struct
{
   const char *pHdr, *pFtr, *pSep;
   MinMaxZU limPer; // Limit count to present as percentage
   Scalar   sPer;   // Percentage scale
} FSFmt;

typedef U8 MapSite;

typedef struct
{
   MapSite  *pM;
   size_t   nM;
} MapData;

typedef struct
{
   Scalar a, b;
} MapInitVal;

/***/

// void initWrap (BoundaryWrap *pW, const Stride stride[4]);
extern void initOrg (ImgOrg * const pO, const U16 w, const U16 h, const U8 flags);

extern MapSite *genMapReflective (MapData *pMD, const V2U32 *pDef);
extern MapSite *genMapPeriodic (MapData *pMD, const V2U32 *pDef);
extern void releaseMap (MapData *pMD);

extern size_t initParam
(
   ParamVal * const pP, 
   const Scalar            kL[3], // Symmetric 9-point kernel coefficients (subject to scaling)
   const InitLapRateScale * pRS, 
   const ParamInfo         * pPI, 
   const InitSpatVarParam * pSV
); // ParamArgs *

extern void releaseParam (ParamVal * const pP);
extern Bool32 initHBT (HostBuffTab * const pT, const size_t fb, const U32 mF, const U32 nH);
extern size_t initMapHFB (HostFB * const pB, const ImgOrg * const pO, const MapSite ms[], const MapInitVal miv[]);
extern void releaseHBT (HostBuffTab * const pT);



extern size_t initHFB (HostFB * const pB, const ImgOrg * const pO, const PatternInfo *pPI);

extern void initNFS (FieldStat * const pFS, const U32 nFS, const Scalar * const pS, const U32 mS);
extern void statAdd (FieldStat * const pFS, Scalar s);
extern void statMerge (FieldStat * const pFS, FieldStat * const pFS1, FieldStat * const pFS2);
extern void printNFS (const FieldStat fs[], const U32 nFS, const FSFmt * pFmt);

#endif // DATA_H
