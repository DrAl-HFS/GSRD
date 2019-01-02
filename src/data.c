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

static void setStep8Stride2 (Stride step[8], const Stride stride[2])
{
   step[0]= -stride[0];  // -X
   step[1]= stride[0];   // +X
   step[2]= -stride[1];  // -Y
   step[3]= stride[1];   // +Y
   step[4]= -stride[0] -stride[1]; // -X-Y
   step[5]= stride[0]  -stride[1]; // +X-Y
   step[6]= -stride[0] +stride[1]; // -X+Y
   step[7]= stride[0]  +stride[1]; // +X+Y
} // setStep8Stride2

static void setWrap8Stride3 (Stride wrap[8], const Stride step[8], const Stride stride[3])
{
   wrap[0]= step[0] + stride[1];
   wrap[1]= step[1] - stride[1];
   wrap[2]= step[2] + stride[2];
   wrap[3]= step[3] - stride[2];
   wrap[4]= step[4] + stride[1] + stride[2];
   wrap[5]= step[5] - stride[1] + stride[2];
   wrap[6]= step[6] + stride[1] - stride[2];
   wrap[7]= step[7] - stride[1] - stride[2];
} // setWrap8Stri6e3

void initOrg (ImgOrg * const pO, const U16 w, const U16 h, const U8 flags)
{
   if (pO)
   {
      pO->def.x= w;
      pO->def.y= h;
      if (0 == (flags & FLAG_INIT_ORG_INTERLEAVED))
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
      setStep8Stride2(pO->nhStepWrap[0], pO->stride);
/*
      pO->nhStepWrap[0][0]= -pO->stride[0];  // -X
      pO->nhStepWrap[0][1]= pO->stride[0];   // +X
      pO->nhStepWrap[0][2]= -pO->stride[1];  // -Y
      pO->nhStepWrap[0][3]= pO->stride[1];   // +Y
      pO->nhStepWrap[0][4]= -pO->stride[0]-pO->stride[1]; // -X-Y
      pO->nhStepWrap[0][5]= pO->stride[0]-pO->stride[1];  // +X-Y
      pO->nhStepWrap[0][6]= -pO->stride[0]+pO->stride[1]; // -X+Y
      pO->nhStepWrap[0][7]= pO->stride[0]+pO->stride[1];  // +X+Y
*/
      if (flags & FLAG_INIT_BOUND_REFLECT)
      {
         for (int i=0; i < 8; i++) { pO->nhStepWrap[1][i]= 0; }
      }
      else
      {
         setWrap8Stride3(pO->nhStepWrap[1], pO->nhStepWrap[0], pO->stride);
/*
         pO->nhStepWrap[1][0]= pO->nhStepWrap[0][0] + pO->stride[1];
         pO->nhStepWrap[1][1]= pO->nhStepWrap[0][1] + -pO->stride[1];
         pO->nhStepWrap[1][2]= pO->nhStepWrap[0][2] + pO->stride[2];
         pO->nhStepWrap[1][3]= pO->nhStepWrap[0][3] + -pO->stride[2];
*/
      }
      // Old junk...
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

void genMap (MapSite *pM, const U8 *pS, const V2U32 *pDef, const U8 k)
{
   Stride s[2]= {1,pDef->x};
   for (int y=1; y < pDef->y-1; y++)
   {
      for (int x=1; x < pDef->x-1; x++)
      {
         Index i= x * s[0] + y * s[1];
         MapSite m=0x00;
         if (k != pS[i])
         {
            m|= (k != pS[i - s[0]]) << 0;
            m|= (k != pS[i + s[0]]) << 1;
            m|= (k != pS[i - s[1]]) << 2;
            m|= (k != pS[i + s[1]]) << 3;
            m|= (k != pS[i - s[0] - s[1]]) << 4;
            m|= (k != pS[i + s[0] - s[1]]) << 5;
            m|= (k != pS[i - s[0] + s[1]]) << 6;
            m|= (k != pS[i + s[0] + s[1]]) << 7;
         }
         pM[i]= m;
      }
   }
/*
size_t z= 0;
for (size_t i= 0; i<n; i++) { z+= (0 == pM[i]); }
printf("genTestMap() - z=%zu\n", z);
*/
} // genMap

MapSite *genMapReflective (MapData *pMD, const V2U32 *pDef)
{
   const U8 solid= 0;
   size_t n= pDef->x * pDef->y;
   size_t m= n * sizeof(MapSite);
   size_t iy;
   MapSite *pM= malloc(m);
   if (pM)
   { // reflective boundary flag layout: 
     // 0x8A .. 0xCB .. 0x49
     // 0xAE .. 0xFF .. 0x5D
     // 0x26 .. 0x37 .. 0x15  
      memset(pM, 0, m);
      pM[0]= 0x8A;
      for (int x=1; x < pDef->x-1; x++) { pM[ x ]=0xCB; }
      pM[pDef->x-1]= 0x49;
      for (int y=1; y < pDef->y-1; y++)
      {
         iy= y * pDef->x;
         pM[ iy ]= 0xAE;
         pM[ pDef->x-1 + iy ]= 0x5D;
         for (int x=1; x < pDef->x-1; x++) { pM[ x + iy ]=0xFF; }
      }
      iy= (pDef->y - 1) * pDef->x;
      pM[ iy ]= 0x26;
      for (int x=1; x < pDef->x-1; x++) { pM[ x + iy ]= 0x37; }
      pM[ pDef->x-1 + iy ]= 0x15;

      if (solid)
      {
         U8 *pS= malloc(n);
         if (pS)
         {
            int lx= pDef->x / 4, ux= lx * 3;
            int ly= pDef->y / 4, uy= ly * 3;
            
            memset(pS, 0, n);
            for (int y=ly; y < uy; y++)
            {
               iy= y * pDef->x;
               for (int x=lx; x < ux; x++) { pS[ x + iy ]= solid; }
            }
            genMap(pM, pS, pDef, solid);
            free(pS);
         }
      }
      if (pMD) { pMD->pM= pM; pMD->nM= n; }
   }
   return(pM);
} // genMapReflective

MapSite *genMapPeriodic (MapData *pMD, const V2U32 *pDef)
{  // unsupported
   if (pMD) { pMD->pM= NULL; pMD->nM= 0; }
   return(NULL);
} // genMapPeriodic

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

Bool32 initHBT (HostBuffTab * const pT, const size_t fb, const U32 mF, const U32 nH)
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

   initNFS( &(pT->sg.fsA), 1, NULL, 0 );
   initNFS( &(pT->sg.fsB), 1, NULL, 0 );

   if (nH > 0)
   {
      pT->sg.hB.pH= NULL; pT->sg.hB.nH= 0;
      pT->sg.mb.bytes= nH * sizeof(*(pT->sg.hB.pH));
      pT->sg.mb.p= malloc( pT->sg.mb.bytes );
      if (pT->sg.mb.p)
      {
         memset(pT->sg.mb.p, 0, pT->sg.mb.bytes);
         pT->sg.hB.pH= pT->sg.mb.p;
         pT->sg.hB.nH= nH; 
         pT->sg.hMap[0]= (nH - 1) / 1.0;
         pT->sg.hMap[1]= 0.0;
         pT->sg.rMap[0]= 1.0 / pT->sg.hMap[0];
         pT->sg.rMap[1]= -(pT->sg.rMap[0] * pT->sg.hMap[1]);
      }
   }
   return(mF == nF);
} // initHBT

void releaseHBT (HostBuffTab * const pT)
{
   for ( int i= 0; i< HFB_MAX; ++i )
   { 
      if (pT->hfb[i].pAB) { free(pT->hfb[i].pAB); pT->hfb[i].pAB= NULL; }
   }
   if (pT->sg.mb.p) { free(pT->sg.mb.p); pT->sg.mb.p= NULL; }
   if (pT->pC) { free(pT->pC); pT->pC= NULL; }
} // releaseHBT

typedef Bool32 (*ShapeFunc)(F32,F32,F32);

static Bool32 sfr (F32 dx, F32 dy, F32 r)
{
   return((fabs(dx) <= r) && (fabs(dy) <= r));
} // sfr

static Bool32 sfc (F32 dx, F32 dy, F32 r)
{
   return((dx*dx + dy*dy) <= (r*r));
} // sfc

size_t genBlob (HostFB * const pB, const ImgOrg * const pO, const V2F32 *pC, const F32 r, RandF *pR, ShapeFunc f)
{
   size_t i, j, nB= 0;
   V2U32 m= { pC->x+r+1, pC->y+r+1 };
   for (I32 iy= pC->y-r; iy <= m.y; iy++)
   { 
      for (I32 ix= pC->x-r; ix <= m.x; ix++)
      {
         if (f(ix-pC->x, iy-pC->y, r))
         {
            i= ix * pO->stride[0] + iy * pO->stride[1];
            j= i + pO->stride[3];
            pB->pAB[i]= 0.5; pB->pAB[j]= 0.25 + randF(pR); 
            nB++;
         }
      }
   }
   return(nB);
} // genBlob

typedef struct
{
   V2U16 i, s;
} RectLat;

void genRectLat (RectLat *pL, const V2U32 *pDef, U8 border, U32 n)
{
   if (n > 1)
   {
      const V2U32 sub= {pDef->x - 2 * border, pDef->y - 2 * border};
      F32 dA= ((F32) sub.x * sub.y) / n;
      F32 dW= 0.5 * sqrt(dA);
      pL->s.x= pL->s.y= MAX(2,dW);
      pL->i.x= border + (pL->s.x + (sub.x % pL->s.x)) / 2;
      pL->i.y= border + (pL->s.y + (sub.y % pL->s.y)) / 2;
   }
   else
   {
      pL->i.x= pDef->x / 2;
      pL->i.y= pDef->y / 2;
      pL->s= pL->i;
   }
} // genRectLat

size_t initHFB (HostFB * const pB, const ImgOrg * const pO, const PatternInfo *pPI)
{
   size_t i, j, nB= 0;
   int x, y;
   RandF rf={{0,0},};
   ShapeFunc pSF= NULL;
   const I32 minD= MIN(pO->def.x, pO->def.y);
   RectLat rl;

   memset(pB->pAB, 0, pO->n * sizeof(*(pB->pAB)));
   for (y= 0; y < pO->def.y; y++)
   { 
      for (x= 0; x < pO->def.x; x++)
      {
         i= x * pO->stride[0] + y * pO->stride[1];
         pB->pAB[i]= 0.4 + 0.2 * (1 & (x ^ y));
      }
   }

   if (charInSet('R', pPI->id+1)) { initRF(&rf, 0.25, -0.125, 54321); }
   switch ( pPI->id[0] )
   {
      case 'C' :
         pSF= sfc;
         break;
      case 'S' :
      case 'R' :
         pSF= sfr;
         break;
      case 'P' :
         break;
      default:
         printf("initHFB() - shape %s???", pPI->id);
         break;
   }

   genRectLat(&rl, &(pO->def), 2, pPI->n);
   printf("rl: %d %d   %d %d\n", rl.i.x, rl.i.y, rl.s.x, rl.s.y);
   if (pSF)
   {
      F32 r= 1;
      
      if (0 == pPI->s) { r= minD / 32; }
      else if (pPI->s < 1.0) { r= pPI->s * minD; }
      for (y= rl.i.y; y < pO->def.y; y+= rl.s.y)
      { 
         for (x= rl.i.x; x < pO->def.x; x+= rl.s.x)
         {
            V2F32 c= { x, y };
            printf("c: %f %f\n", c.x, c.y);
            nB+= genBlob(pB, pO, &c, r, &rf, pSF);
         }
      }
      printf("genGlob() - %zu\n", nB);
   }
   else
   {
      for (y= rl.i.y; y < pO->def.y; y+= rl.s.y)
      { 
         for (x= rl.i.x; x < pO->def.x; x+= rl.s.x)
         {
            i= x * pO->stride[0] + y * pO->stride[1];
            j= i + pO->stride[3];
            pB->pAB[i]= 0.5; pB->pAB[j]= 0.25 + randF(&rf);
            nB++;
         }
      }
   }
   pB->iter= 0;
   // stat ?
   return(nB);
} // initHFB

size_t initMapHFB (HostFB * const pB, const ImgOrg * const pO, const MapSite ms[], const MapInitVal miv[])
{
   size_t i, j, s= 0;
   int x, y, m;

   for (y= 0; y < pO->def.y; y++)
   { 
      for (x= 0; x < pO->def.x; x++)
      {
         m= ms[x + y * pO->def.x];
         s+= m;
         i= x * pO->stride[0] + y * pO->stride[1];
         j= i + pO->stride[3];
         pB->pAB[i]= miv[m].a;
         pB->pAB[j]= miv[m].b;
      }
   }
   return(s);
} // initMapHFB


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

void statAdd (FieldStat * const pFS, Scalar s) // w
{
   if (s < pFS->min) { pFS->min= s; }
   if (s > pFS->max) { pFS->max= s; }
   pFS->n++;
   pFS->s.m[0]+= 1; // w
   pFS->s.m[1]+= s; // w * s
   pFS->s.m[2]+= s * s; // w * s * s
} // statAdd

void statMerge (FieldStat * const pFS, FieldStat * const pFS1, FieldStat * const pFS2)
{
   pFS->min= MIN(pFS1->min, pFS2->min);
   pFS->max= MAX(pFS1->max, pFS2->max);
   pFS->n= pFS1->n + pFS2->n;
   pFS->s.m[0]= pFS1->s.m[0] + pFS2->s.m[0];
   pFS->s.m[1]= pFS1->s.m[1] + pFS2->s.m[1];
   pFS->s.m[2]= pFS1->s.m[2] + pFS2->s.m[2];
} // statMerge

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
