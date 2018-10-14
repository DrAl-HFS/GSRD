// data.c - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-April 2018

#include "data.h"

static const InitLapRateScale gILRS[]={{0.5, 0.1}, {KLA0,KLB0}};
// Knotwork emergence from about D=0.066, becoming granular from about 0.074
static const RRDParam          gRRDP[]={{0.5, 0.023, 0.072},{1.0, 0.023, 0.072},{KR0,KRA0,KDB0}};
//static const ParamInfo         gPI[]={{0.023, 0.072,}, 2, {0,}};
static const InitSpatVarParam gISVP[]={0.100, 0.005};

/***/
//deprecate
static void initWrap (BoundaryWrap *pW, const Stride stride[4])
{
   pW->h[0]= stride[1]; pW->h[1]= stride[2] - stride[1];	// 0..3 LO, 2..5 HI
   pW->h[2]= -stride[0]; pW->h[3]= stride[0];
   pW->h[4]= stride[1] - stride[2]; pW->h[5]= -stride[1];
} // initWrap

void initOrg (ImgOrg * const pO, U16 w, U16 h, U8 flags)
{
   if (pO)
   {
      pO->def.x= w;
      pO->def.y= h;
      if (0 == (flags & 1))
      {  // planar
         pO->stride[0]= 1;     // next element, same phase
         pO->stride[3]= w * h; // same element, next phase
      }
      else
      {  // interleaved
         pO->stride[0]= 2; // same phase
         pO->stride[3]= 1; // next phase
      }
      pO->stride[1]= w * pO->stride[0]; // line
      pO->stride[2]= h * pO->stride[1]; // plane / buffer
      pO->n= 2 * w * h; // complete buffer
// neighbours
      pO->nhStepWrap[0][0]= -pO->stride[0];
      pO->nhStepWrap[0][1]= pO->stride[0];
      pO->nhStepWrap[0][2]= -pO->stride[1];
      pO->nhStepWrap[0][3]= pO->stride[1];

      pO->nhStepWrap[1][0]= pO->nhStepWrap[0][0] + pO->stride[1];
      pO->nhStepWrap[1][1]= pO->nhStepWrap[0][1] + -pO->stride[1];
      pO->nhStepWrap[1][2]= pO->nhStepWrap[0][2] + pO->stride[2];
      pO->nhStepWrap[1][3]= pO->nhStepWrap[0][3] + -pO->stride[2];

      pO->hw[0]= pO->nhStepWrap[0][3];
      pO->hw[1]= pO->nhStepWrap[1][2];
      pO->hw[2]= pO->nhStepWrap[0][0];
      pO->hw[3]= pO->nhStepWrap[0][1];
      pO->hw[4]= pO->nhStepWrap[1][3];
      pO->hw[5]= pO->nhStepWrap[0][2];

      initWrap(&(pO->wrap), pO->stride);
      int e= 0;
      for (int i=0; i<6; i++)
      {
         if (pO->wrap.h[i] != pO->hw[i]) { e++; printf("[%d] %d ?%d?\n", i, pO->wrap.h[i], pO->hw[i]); } 
      }
      printf("initOrg() - %d errors\n", e);
   }
} // initOrg

//size_t paramBytes (U16 w, U16 h) { return(MAX(w,h) * 3 * sizeof(Scalar)); }
Scalar validateKL (Scalar kR[3], const Scalar kL[3])
{
   Scalar w= 1, t= kL[1] + kL[2];
   
   if (t > 1) { w= 1 / t; }
   //do
   {
      kR[0]= -4 * w * t;
      kR[1]= w * kL[1];
      kR[2]= w * kL[2];
   } //while (kR[0] != t)
   return(kR[0] + 4 * (kR[1] + kR[2]));
} // validateKL

double reactSafeLim (const Scalar kV[3], const InitLapRateScale * const pRS)
{
   //return(1 - 4 * (kV[1] + kV[2]) * MAX(pRS->rLA, pRS->rLB) );
   return(1 + kV[0] * MAX(pRS->rLA, pRS->rLB) );
} // reactSafeLim

size_t initParam
(
   ParamVal * const pP, 
   const Scalar            kL[3], 
   const InitLapRateScale * pRS, 
   const ParamInfo         * pPI, 
   const InitSpatVarParam * pSV
)
{
   U32 i, n= 0;
   Scalar kV[3];
   RRDParam rrd= gRRDP[0];
   InitLapRateScale rs= gILRS[0];

   if (NULL != pRS) { rs= *pRS; }
   if (NULL == pSV) { pSV= gISVP; }

   if (pPI)
   {
      for ( i= 0; i < pPI->nV; ++i )
      {
         if ((pPI->v[i] > 0.0) && (pPI->v[i] < 1.0)) switch(pPI->id[i])
         { 
            case 'D' :
            case 'B' : rrd.kDB= pPI->v[i]; break;
            case 'R' :
            case 'A' : rrd.kRA= pPI->v[i]; break;
            case 'L' : rs.rLB= pPI->v[i]; break;
         }
      }
   }
 
   // Set diffusion weights
   printf("initParam() - kL:e=%.f\n", validateKL(kV, kL));
   for ( i= 0; i<3; ++i ) { pP->base.kL.a[i]= kV[i] * rs.rLA; pP->base.kL.b[i]= kV[i] * rs.rLB; }
   printf("Laplacian Parameters Committed: %G %G\n", rs.rLA, rs.rLB);
   // reaction rates
   rrd.kRR= reactSafeLim(kV, &rs);

   //if (pRRD->kRR > rrd.kRR) { printf("WARNING: initParam() - reaction rate %G exceeds safe (diffusion defined) limit %G\n", pRRD->kRR, rrd.kRR); }
   pP->base.kRR= rrd.kRR;
   pP->base.kRA= rrd.kRA;
   pP->base.kDB= rrd.kDB;
   printf("Rate Parameters Committed: %G %G %G\n", rrd.kRR, rrd.kRA, rrd.kDB);

   pP->var.pK= NULL;
   if (pSV)
   {
      pP->var.pK= malloc( 3 * pSV->max * sizeof(*(pP->var.pK)) );
   }
   if (pP->var.pK && pSV)
   {
      Scalar *pKRR, *pKRA, *pKDB;
      Scalar kRR=rrd.kRR, kRA=rrd.kRA, kDB=rrd.kDB;
      Scalar dKRR=0, dKRA=0, dKDB=0;
      
      pP->var.iKRR= 0; pP->var.iKRA= pP->var.iKRR + n; pP->var.iKDB= pP->var.iKRA+n;
      pKRR= pP->var.pK + pP->var.iKRR;
      pKRA= pP->var.pK + pP->var.iKRA;
      pKDB= pP->var.pK + pP->var.iKDB;

      dKRR= (kRR * pSV->varR) / n;
      dKDB= (kDB * pSV->varD) / n;
      for (i= 0; i<n; ++i)
      {
         pKRR[i]= kRR; kRR+= dKRR;
         pKRA[i]= kRA; kRA+= dKRA;
         pKDB[i]= kDB; kDB+= dKDB;
      }
   }
   else
   {
      pP->var.iKRR= pP->var.iKRA= pP->var.iKDB= 0;
   }
   return(n);
} // initParam

void releaseParam (ParamVal * const pP)
{
   if (pP && pP->var.pK) { free(pP->var.pK); pP->var.pK= NULL; }
} // releaseParam

Bool32 initHBT (HostBuffTab * const pT, const size_t fb, const U32 mF)
{
   U32 nF= 0, i= 0;
   Bool32 a;
   
   memset(pT, 0, sizeof(*pT));
   if (mF & 1)
   { 
	   pT->pC= malloc(fb / 2);
	   nF+= (NULL != pT->pC);
   }
   do
   {
      pT->hfb[i].pAB= malloc(fb);
      a= (NULL != pT->hfb[i].pAB);
      if (a)
      {
         BlockStat *pS= &(pT->hfb[i].s);
         memset(pS, -1, sizeof(*pS));
         pT->hfb[i].iter= -1;
         nF+= 2;
      }
   } while (a && (nF < mF) && (++i < HFB_MAX));
   return(mF == nF);
} // initHBT

void releaseHBT (HostBuffTab * const pT)
{
   for ( int i= 0; i< HFB_MAX; ++i )
   { 
      if (pT->hfb[i].pAB) { free(pT->hfb[i].pAB); pT->hfb[i].pAB= NULL; }
   }
   if (pT->pC) { free(pT->pC); pT->pC= NULL; }
} // releaseHBT


size_t initHFB (HostFB * const pB, const ImgOrg * const pO, const U8 patternID)
{
   size_t i, j, nB= 0;
   int x, y, ix, iy, mx, my;
   RandF rf={{0,0},};

   memset(pB->pAB, 0, pO->n * sizeof(*(pB->pAB)));
   for (y= 0; y < pO->def.y; y++)
   { 
      for (x= 0; x < pO->def.x; x++)
      {
         i= x * pO->stride[0] + y * pO->stride[1];
         pB->pAB[i]= 0.5 + 0.5 * (1 & (x ^ y));
      }
   }

   if (patternID & 0x4) { initRF(&rf, 0.25, -0.125, 54321); }
   switch ( patternID & 0x3 )
   {
      case 1 :
      case 2 :
      {
         int k= MIN(pO->def.x, pO->def.y) / 32;
         mx= pO->def.x / 2; my= pO->def.y / 2;
         for (y= -k; y < k; y++)
         { 
            for (x= -k; x < k; x++)
            {
               if ((1 == patternID) || ((x*x + y*y) <= (k*k)))
               {
                  i= (mx+x) * pO->stride[0] + (my+y) * pO->stride[1];
                  j= i + pO->stride[3];
                  pB->pAB[i]= 0.5; pB->pAB[j]= 0.25 + randF(&rf); 
                  nB++;
               }
            }
         }
      }
      break;
      default:
      {
         i= (pO->stride[1] + pO->stride[2] ) / 2; // midpoint
         j= i + pO->stride[3];
         pB->pAB[i]= 0.5; pB->pAB[j]= 0.25; 
         nB++;
      }
      break;
   }
   pB->iter= 0;
   // stat ?
   return(nB);
} // initHFB

#ifndef M_INF
#define M_INF  1E308
#endif

void initNFS (FieldStat fs[], const U32 nFS, const Scalar * const pS, const U32 mS)
{
   U32 i= 0;
   if (pS)
   {
      while (i < mS)
      {
         Scalar s= pS[i];
         fs[i].min= fs[i].max= s;
         fs[i].n= 1;
         fs[i].s.m[0]= 1;
         fs[i].s.m[1]= s;
         fs[i].s.m[2]= s * s;
         ++i;
      }
   }
   else while (i < nFS)
   {
      fs[i].min= M_INF;
      fs[i].max= -M_INF;
      fs[i].n= 0;
      fs[i].s.m[0]= 0;
      fs[i].s.m[1]= 0;
      fs[i].s.m[2]= 0;
      ++i;
   }
} // initNFS

void statAdd (FieldStat * const pFS, Scalar s)
{
   if (s < pFS->min) { pFS->min= s; }
   if (s > pFS->max) { pFS->max= s; }
   pFS->n++;
   pFS->s.m[0]+= 1;
   pFS->s.m[1]+= s;
   pFS->s.m[2]+= s * s;
} // statAdd

void printNFS (const FieldStat fs[], const U32 nFS, const FSFmt * pFmt)
{
   //if (sizeof(SMVal) > sizeof(double)) { fmtStr= "%LG"; }
   FSFmt fmt = { "", " ", "\n", {0, 0}, 0, };
   int nS= nFS;

   if (pFmt)
   {
      if (pFmt->pHdr) { fmt.pHdr= pFmt->pHdr; }
      if (pFmt->pSep) { fmt.pSep= pFmt->pSep; }
      if (pFmt->pFtr) { fmt.pFtr= pFmt->pFtr; }
      fmt.limPer= pFmt->limPer;
      if (pFmt->sPer > 0) { fmt.sPer= pFmt->sPer; }
   }
   printf("%s", fmt.pHdr);
   for (U32 i= 0; i < nFS; ++i )
   {
      if ((fs[i].n >= fmt.limPer.min) && (fs[i].n <= fmt.limPer.max) && (fmt.sPer > 0))
      { printf("%.1f%%", fs[i].n * fmt.sPer); }
      else { printf("%zu", fs[i].n); }

      if (fs[i].s.m[0] > 0)
      {
         printf(" %G %G", fs[i].min, fs[i].max);
         if (fs[i].n > 1)
         {
            StatRes1 r;
            if (statGetRes1(&r, &(fs[i].s), 0)) { printf(" %G %G %G", fs[i].s.m[1], r.m, r.v); }
         }
      }
      if (--nS > 0) { printf("%s", fmt.pSep); }
   }
   printf("%s", fmt.pFtr);
} // printNFS
