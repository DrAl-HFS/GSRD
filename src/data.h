// data.h - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-April 2018

#ifndef DATA_H
#define DATA_H

#include "util.h"

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
   Stride nhStepWrap[2][4], hw[6]; // neighbourhood 
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
   char        label[8];
} HostFB;

typedef struct
{
   HostFB   hfb[HFB_MAX];
   Scalar   *pC; // conservation tally field
} HostBuffTab;

typedef struct
{
   const char *pHdr, *pFtr, *pSep;
   MinMaxZU limPer; // Limit count to present as percentage
   Scalar   sPer;   // Percentage scale
} FSFmt;

/***/

// void initWrap (BoundaryWrap *pW, const Stride stride[4]);
extern void initOrg (ImgOrg * const pO, U16 w, U16 h, U8 flags);
extern size_t initParam
(
   ParamVal * const pP, 
   const Scalar            kL[3], // Symmetric 9-point kernel coefficients (subject to scaling)
   const InitLapRateScale * pRS, 
   const ParamInfo         * pPI, 
   const InitSpatVarParam * pSV
); // ParamArgs *

extern void releaseParam (ParamVal * const pP);
extern Bool32 initHBT (HostBuffTab * const pT, const size_t fb, const U32 mF);
extern void releaseHBT (HostBuffTab * const pT);



extern size_t initHFB (HostFB * const pB, const ImgOrg * const pO, const U8 patternID);

extern void initNFS (FieldStat fs[], const U32 nFS, const Scalar * const pS, const U32 mS);
extern void statAdd (FieldStat * const pFS, Scalar s);
extern void printNFS (const FieldStat fs[], const U32 nFS, const FSFmt * pFmt);

#endif // DATA_H
