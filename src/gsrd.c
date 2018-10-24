// gsrd.c - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-April 2018

#include "args.h"
#include "proc.h"
#include "image.h"

typedef struct
{
   ParamVal    pv;
   HostBuffTab hbt;
   ImgOrg      org;
   size_t      iter, baseIter;
   MemBuff     ws;
} Context;

/***/

static const Scalar gKL[3]= {-1020.0/1024, 170.0/1024, 85.0/1024};

static Context gCtx={0};

/***/

Context *initCtx (Context * const pC, const V2U16 * const pD, U16 nF, const ArgInfo * const pAI)
{
   U16 w, h;
   if (pD) { w= pD->x; h= pD->y; } else { w= 256; h= 256; }
   if (0 == nF) { nF= 4; }
   const size_t n= w * h;
   const size_t b2F= 2 * n * sizeof(Scalar);

   if (b2F > n)
   {
      InitSpatVarParam sv={0.100, 0.005, };
      sv.max= MAX(w, h);

      initParam(&(pC->pv), gKL, NULL, &(pAI->param), &sv);

      initOrg(&(pC->org), w, h, pAI->init.flags);

      pC->ws.bytes= w * h * 4 + (8<<10);
      pC->ws.bytes&= ~((4 << 10) - 1);
      pC->ws.p= malloc(pC->ws.bytes);

      if (pAI->files.lutPath)
      { // "init/viridis-table-byte-0256.csv"
         imageLoadLUT(&(pC->ws), pAI->files.lutPath);
      }

      if (initHBT(&(pC->hbt), b2F, nF, 1<<8))
      {
         pC->iter= 0;
         return(pC);
      }
   }
   return(NULL);
} // initCtx

void releaseCtx (Context * const pC)
{
   if (pC)
   {
      releaseParam(&(pC->pv));
      releaseHBT(&(pC->hbt));
      if (pC->ws.p) { free(pC->ws.p); }
      memset(pC, 0, sizeof(*pC));
   }
} // releaseCtx

size_t loadFrame 
(
   HostFB              * const pFB, 
   const DataFileInfo * const pDFI
)
{
   size_t r= 0;
   if (pFB && pFB->pAB && pDFI && (pDFI->flags & DFI_FLAG_VALIDATED))
   {
      r= loadBuff(pFB->pAB, pDFI->filePath, pDFI->bytes);
      if (r > 0) { pFB->iter= pDFI->iter; }
   }
   return(r);
} // loadFrame

size_t saveRGB (const HostFB * const pF, char path[], int m, int n, const ImgOrg * const pO)
{
   U8 *pB= gCtx.ws.p;
   size_t b= imageTransferRGB(pB, pF->pAB, pO, 0);

   n+= snprintf(path+n, m-n, "_%lux%lu_3U8.rgb", pO->def.x, pO->def.y);
   return saveBuff(pB, path, b);
} // saveRGB

size_t saveRaw (const HostFB * const pF, char path[], int m, int n, const ImgOrg * const pO)
{
   switch(pO->stride[0])
   {
      case 1 : n+= snprintf(path+n, m-n, "(%lu,%lu,2)", pO->def.x, pO->def.y); break;
      default : n+= snprintf(path+n, m-n, "_%lux%lu_%d", pO->def.x, pO->def.y, pO->stride[0]); break;
   }
   n+= snprintf(path+n, m-n, "%s", "F64.raw");
   return saveBuff(pF->pAB, path, sizeof(Scalar) * pO->n);
} // saveRaw

I32 genOutPath1 (char s[], const I32 m, const char path[], const char name[], const size_t i)
{
   I32 n= 0;

   if (path)
   {
      n+= snprintf(s+n, m-n, "%s", path);
      if (('/' != s[n-1]) && ((m-n) > 2)) { s[n++]= '/'; s[n]= 0; }
   }
   if (name) { n+= snprintf(s+n, m-n, "%s%07lu", name, i); }
   else { n+= snprintf(s+n, m-n, "gsrd%07lu", i); } 
   return(n);
} // genOutPath1

size_t saveFrame 
(
   const HostFB   * const pF, 
   const ImgOrg   * const pO, 
   const ArgInfo  * const pA,
   const U8       tMask
)
{
   size_t r= 0;
   if (pF && pF->pAB && pO && pA && (pA->files.flags & FLAG_FILE_OUT))
   {
      char path[256];
      const I32 m= sizeof(path)-1;
      I32 i= 0;

      do
      {
         const U8 t= pA->files.outType[i];
         if (t & tMask)
         {
            int n= genOutPath1(path, m, pA->files.outPath[i], pA->files.outName, pF->iter);
            switch(t & 0x3) 
            {
               case 2 : r= saveRaw(pF,path,m,n,pO); break;
               case 3 : r= saveRGB(pF,path,m,n,pO); break;
               default : printf("saveFrame() - t=%u\n", t);
            }
            printf("saveFrame() - %s %p %zu bytes\n", path, pF->pAB, r);
         }
      } while (++i < pA->files.nOutPath);
   }
   return(r);
} // saveFrame

void frameStat (BlockStat * const pS, StatG * const pSG, const Scalar * const pAB, const ImgOrg * const pO)
{
   const size_t n= pO->def.x * pO->def.y; // pO->n / 2
   const Scalar * const pA= pAB;
   const Scalar * const pB= pAB + pO->stride[3];
   BlockStat s;
   size_t i, k, hb=0, *pHB= &hb;
   Scalar map[2]={0,0};

   if (pSG && pSG->hB.pH && (pSG->hB.nH > 0)) { map[0]= pSG->hMap[0]; map[1]= pSG->hMap[1]; pHB= pSG->hB.pH; }
   initNFS(s.a+0, 2, NULL, 1); // KLUDGY
   initNFS(s.b+0, 2, NULL, 1);
   
   for (i=0; i < n; i++)
   {
      const Index j= i * pO->stride[0];
      const Scalar a= pA[j];
      const Scalar b= pB[j];

      statAdd(s.a + (a <= 0), a);
      statAdd(s.b + (b <= 0), b);
      k= b * map[0] + map[1];
      pHB[k]++;
   }
   if (pS) { *pS= s; }
   if (pSG)
   {
      statMerge(&(pSG->fsA), &(pSG->fsA), s.a+0); 
      statMerge(&(pSG->fsB), &(pSG->fsB), s.b+0); 
   }
} // frameStat

void summarise (HostFB * const pF, StatG * const pSG, const ImgOrg * const pO)
{
   const size_t n= pO->def.x * pO->def.y; // pO->n / 2
   FSFmt fmt;

   frameStat(&(pF->s), pSG, pF->pAB, pO);

   printf("summarise() - \n\t%zu  N min max sum mean var\n", pF->iter);
   fmt.pFtr= "\n"; fmt.pSep= " | <=0 ";
   fmt.limPer.min= 5000; fmt.limPer.max= n; fmt.sPer= 100.0 / n;
   fmt.pHdr= "\tA: "; 
   printNFS(pF->s.a, 2, &fmt);
   fmt.pHdr= "\tB: "; 
   printNFS(pF->s.b, 2, &fmt);
} // summarise

size_t findQHZ (double *pF, const size_t h[], const size_t nH, const size_t q)
{
   size_t i= 0, t= h[0];
   while ((i < nH) && (t < q)) { t+= h[++i]; } //printf("%zu ", t); }
   if (pF)
   {
      double f= 0.0;
      if ((i < nH) && (h[i] > 0))
      {
         //printf("findQHZ() %zu %zu %zu\n", q, t, h[i]);
         f= (double)(q - (t - h[i])) / h[i];
      }
      *pF= f + i;
   }
   return(i);
} // findQHZ

size_t findTailHZ (const size_t h[], const size_t nH, const size_t mT, const size_t nT)
{
   size_t i= 0, t= 0;
   do
   {
      while ((i < nH) && (h[i] > mT)) { ++i; }
      while ((i < nH) && (h[i] <= mT) && (t < nT)) { ++i; ++t; }
   } while ((i < nH) && (t < nT));
   if (t < nT) return(-1);
   return(i-nT);
} // findTailHZ

size_t findMaxHZ (const size_t h[], const size_t n)
{
   size_t i= 0, m= 0;
   while (++i < n) { if (h[i] > h[m]) { m= i; } }
   return(m);
} // findMaxHZ

void reportSG (MinMaxF64 *pT, const F64 q, const StatG * const pSG)
{
   FSFmt fmt;

   printf("reportSG() - \n\t  N min max sum mean var\n");
   fmt.pFtr= "\n"; fmt.pSep= " | <=0 ";
   fmt.limPer.min= -1; fmt.limPer.max= -1; fmt.sPer= 0.0;
   //if (pSG->fsB.n > 0) { fmt.sPer= 100.0 / pSG->fsB.n; } 
   fmt.pHdr= "\tA: "; 
   printNFS(&(pSG->fsA), 1, &fmt);
   fmt.pHdr= "\tB: "; 
   printNFS(&(pSG->fsB), 1, &fmt);

   if (pSG->hB.pH)
   {
      size_t i, m, t;
      Scalar f= 0, s= 0;

      t= sumNZU(pSG->hB.pH, pSG->hB.nH);
      if (t > 0)
      {
         s= 1.0 / t;
         // estimate median
         m= t / 2;
         i= findQHZ(&f, pSG->hB.pH, pSG->hB.nH, t * q);
         pT->min= f * pSG->rMap[0] + pSG->rMap[1];
         printf("Q%G [%zu] (%G) %G\n\n", q, i, f, pT->min);
         t= i + findTailHZ(pSG->hB.pH+i, pSG->hB.nH-i, 2, 3);
         f= t;
         pT->max= f * pSG->rMap[0] + pSG->rMap[1];
         printf("Tail [%zu]=%zu %G\n\n", t, pSG->hB.pH[t], pT->max);
#if 0
         {  // TODO: check for anomalies...
            size_t a= MAX(0, t-10);
            size_t b= MIN(pSG->hB.nH-1, t+10);
            for (i=a; i <= b; i++) { printf("[%zu]= %zu\n", i, pSG->hB.pH[i] ); }
            printf("\n");
         }
#endif
      }
   }
} // reportSG

void postProc (const ArgInfo *pAI)
{
   MinMaxF64 t;
   reportSG(&t, 0.5, &(gCtx.hbt.sg));
   if (FLAG_FILE_XFER & pAI->files.flags)
   {
      char path[256];
      const char *pOP=NULL;
      const I32 m= sizeof(path)-1;
      DataFileInfo dfi;
      HostFB *pFrame= gCtx.hbt.hfb+0;
      size_t i;

      memset(&dfi, 0, sizeof(dfi));
      dfi.filePath= path;
      {
         U8 j=0;
         while ((NULL == pOP) && (j < pAI->files.nOutPath))
         {
            if (2 == pAI->files.outType[j]) { pOP= pAI->files.outPath[j]; }
            j++;
         }
      }
      i= gCtx.baseIter;
      do
      {
         I32 n= genOutPath1(path, m, pOP, pAI->files.outName, i);
         n+= snprintf(path+n, m-n, "(%lu,%lu,2)F64.raw", gCtx.org.def.x, gCtx.org.def.y);

         if (scanDFI(&dfi, path) && loadFrame(pFrame, &dfi))
         {
            saveFrame(pFrame, &(gCtx.org), pAI, 0x4);
         }
         i+= pAI->proc.subIter;
      } while (i <= pAI->proc.maxIter);
   }
} // postProc

typedef struct { size_t s, e; } IdxSpan;
#define MAX_TAB_IS 32
size_t compare (HostFB * const pF1, HostFB * const pF2, const ImgOrg * const pO, const Scalar eps)
{
   const size_t n= pO->def.x * pO->def.y; // pO->n / 2
   const Scalar * const pA1= pF1->pAB;
   const Scalar * const pB1= pF1->pAB + pO->stride[3];
   const Scalar * const pA2= pF2->pAB;
   const Scalar * const pB2= pF2->pAB + pO->stride[3];
   FieldStat sa[3], sb[3];
   FSFmt fmt;
   size_t i;
   IdxSpan tabDiff[MAX_TAB_IS];
   U32 nTabDiff= 0;

   printf("----\ncompare() - (id:iter) %s:%zu\t\t%s:%zu\n", pF1->label, pF1->iter, pF2->label, pF2->iter);

   initNFS(sa, 3, NULL, 0);
   initNFS(sb, 3, NULL, 0);
   
   for (i=1; i < n; i++)
   {
      const Index j= i * pO->stride[0];

      const Scalar da= pA1[j] - pA2[j];
      const Scalar db= pB1[j] - pB2[j];
      const U8 iA= (da < -eps) | ((da > eps)<<1);
      const U8 iB= (db < -eps) | ((db > eps)<<1);
      statAdd(sa + iA, da);
      statAdd(sb + iB, db);
      if ((iA + iB) > 0)
      {
         U32 k=0;
         while (k < nTabDiff)
         {
            if (i == (tabDiff[k].s - 1)) { tabDiff[k].s--; break; }
            if (i == (tabDiff[k].e + 1)) { tabDiff[k].e++; break; }
            k++;
         }
         if ((k >= nTabDiff) && (nTabDiff < MAX_TAB_IS)) { tabDiff[nTabDiff].s= tabDiff[nTabDiff].e= i; nTabDiff++; }
      }
   }
   printf("\tDiff: N min max sum mean var\n");
   fmt.pFtr= "\n"; fmt.pSep= " | <>e ";
   fmt.limPer.min= 5000; fmt.limPer.max= n; fmt.sPer= 100.0 / n;
   fmt.pHdr= "\tdA: "; 
   printNFS(sa, 3, &fmt);
   fmt.pHdr= "\tdB: ";
   printNFS(sb, 3, &fmt);

   if (nTabDiff > 0)
   {
      printf("--------\n\ntabDiff[%u]:", nTabDiff);
      for (U32 k=0; k<nTabDiff; k++)
      {
         if (tabDiff[k].s == tabDiff[k].e) { printf("%zu, ", tabDiff[k].s); }
         else { printf("%zu..%zu, ", tabDiff[k].s, tabDiff[k].e); }
      }
      printf("\n\n");
   }
   return(sa[1].n + sa[2].n + sb[1].n + sb[2].n);
} // compare

const V2U16 *selectDef (const DataFileInfo * const pIF, const InitInfo * const pII)
{
   if (pIF && (pIF->nV > 1)) return &(pIF->def);
   if (pII && (pII->nD > 1)) return &(pII->def);
   return(NULL);
} // selectDef

int main ( int argc, char* argv[] )
{
   int n= 0, i= 0, nCmp=0, nErr= 0;
   ArgInfo ai={0,};

   if (argc > 1)
   {
      n= scanArgs(&ai, (const char **)(argv+1), argc-1);
      printf("proc: f=0x%zX, m=%zu, s=%zu\n", ai.proc.flags, ai.proc.maxIter, ai.proc.subIter);
   }
   procTest();

   const DataFileInfo * const pIF= &(ai.files.init);
   const InitInfo * const pII= &(ai.init);
   const ProcInfo * const pPI= &(ai.proc);
   if (procInitAcc(pPI->flags) && initCtx(&gCtx, selectDef(pIF, pII), 8, &ai))
   {
      SMVal tE0, tE1;
      HostFB *pFrame, *pF2=NULL;
      U8 afb=0, nIdx=0, fIdx[4], k=0;

      do
      {
         size_t iM= pPI->maxIter;
         tE0= tE1= 0;
         if (pPI->subIter > 0) { iM= pPI->subIter; }
         afb&= 0x3;
         pFrame= gCtx.hbt.hfb + afb;
         if (0 == loadFrame(pFrame, pIF))  //printf("nB=%zu\n",
         {
            initHFB(pFrame, &(gCtx.org), pII->patternID);
            saveFrame(pFrame, &(gCtx.org), &ai, 0x2);
         }
         gCtx.iter= gCtx.baseIter= pFrame->iter;
         //k= (gCtx.iter - pFrame->iter) & 0x1;
         // if verbose...
         summarise(pFrame, NULL, &(gCtx.org)); // ignore initial dist. &(gCtx.hbt.sg)
         printf("---- %s ----\n", procGetCurrAccTxt(pFrame->label, sizeof(pFrame->label)-1));
         memcpy(pFrame[1].label, pFrame[0].label, sizeof(pFrame->label));
         do
         {
            size_t iR= pPI->maxIter - gCtx.iter;
            if (iM > iR) { iM= iR; }

            deltaT();
            gCtx.iter+= procNI(pFrame[(k^0x1)].pAB, pFrame[k].pAB, &(gCtx.org), &(gCtx.pv), iM);
            tE0= deltaT();
            tE1+= tE0;
            
            k= (gCtx.iter - pFrame->iter) & 0x1;
            pFrame[k].iter= gCtx.iter;

            summarise(pFrame+k, &(gCtx.hbt.sg), &(gCtx.org));
            printf("\ttE= %G, %G\n", tE0, tE1);

            saveFrame(pFrame+k, &(gCtx.org), &ai, 0x2);
         } while (gCtx.iter < pPI->maxIter);
         fprintf(stderr,"%s\t%zu\t%zu\t%G\n", pFrame->label, pPI->maxIter, pPI->subIter, tE1);
         if (nIdx < 4) { fIdx[nIdx++]= afb+k; }
         afb+= 2;
      } while (procSetNextAcc(PROC_NOWRAP));
      if (nIdx > 1)
      {
         pF2= gCtx.hbt.hfb+fIdx[1];
         pFrame=  gCtx.hbt.hfb+fIdx[0];
         //if (pFrame->iter == pF1->iter) 
         { nCmp++; nErr= compare(pFrame, pF2, &(gCtx.org), 1.0/(1<<30)); }
      }
      if ((nIdx > 0) && (nIdx < 4))
      {
         fIdx[nIdx]= ( fIdx[nIdx-1] + 1 ) & 0x3;
         pF2= gCtx.hbt.hfb + fIdx[nIdx];
         if (loadFrame( pF2, &(ai.files.cmp) ))
         {
            snprintf(pF2->label, sizeof(pF2->label)-1, "CF");
            nCmp++;
            nErr= compare(pFrame, pF2, &(gCtx.org), 1.0/(1<<10));
         }
      }
      postProc(&ai);
   }
   releaseCtx(&gCtx);
 
   if (nCmp > 0)
   {
      if (0 != nErr) { printf( "Test FAILED\n"); }
      else {printf( "Test PASSED\n"); }
   } else { printf("Complete\n"); }
   return(0);
} // main
