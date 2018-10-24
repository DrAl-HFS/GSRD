// image.c - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-April 2018
// image.c - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-October 2018

#include "image.h"

typedef struct
{
   U8 r,g,b,x;
} RGBX;

typedef struct
{
   float *pT;
   RGBX  *pRGBX;
   RGBX  lh[2];
   U16   n;
   U8    flags;
   U8    pad[1];
} ImageLUT;


static ImageLUT gLUT={0,};

static const MinMaxF64 gDom={0,1};

/***/

static int validLUT (const float *pF, const int nF, const MinMaxI32 *pMM)
{
   if (pF && (nF > 0) && pMM && (pMM->min >= 3) && (pMM->max == pMM->min))
   {
      int r= nF / pMM->min;
      if (nF == (r * pMM->min))
      {
         return(r);
      }
   }
   return(0);
} // validLUT

void clean (signed char *pS, size_t n)
{
   while (pS[n] < ' ') { printf("[%zu] 0x%02X\n", n, 0xFF & pS[n]); --n; }
} // clean

int imageLoadLUT (const MemBuff *pB, const char path[])
{
   size_t b= fileSize(path);
   int pad= 4 - (b & 0x3);
   if ((b > 0) && validBuff(pB, b + pad)) 
   {
      char *pS;
      F32 *pF;
      MinMaxI32 mm;
      I32 i, j, m, n=-1, r=-1, s=-1;

      pS= (char*)(pB->w + pB->bytes - (b + pad));
      memset(pS+b, 0, pad);
      b= loadBuff(pS, path, b);
      if (b > 0)
      {
         //clean(pS,b+pad);
         m= (pB->bytes - (b + pad)) / sizeof(*pF);
         pF= pB->p;

         s= skipPastSet(pS, "\r\n"); // HACKY skip header line
         //printf("imageLoadLUT() %s %p %zu %s\n", path, pS, b, pS+s);
         m= MIN(256 * 4, m); // prevent mysterious crash
         n= scanTableF32(pF, m, &mm, pS+s, &s, b-s);
         r= validLUT(pF,n,&mm);
         if (r > 0)
         {
            ImageLUT *pL= &gLUT;
            pL->pT= (float*)(pB->w + pB->bytes) - r;
            pL->pRGBX= (RGBX*)gLUT.pT - r;
            pL->n= r;
            pL->flags= 0;
            printf("imageLoadLUT() %d,%d\n", mm.min, r);
            for (i= 0; i < r; i++)
            {
               j= i * mm.min;
               pL->pT[i]= pF[j+0];
               pL->pRGBX[i].r= pF[j+1];
               pL->pRGBX[i].g= pF[j+2];
               pL->pRGBX[i].b= pF[j+3];
               pL->pRGBX[i].x= 0xFF;
               //printf("%G %u %u %u\n", pL->pT[i], pL->pRGBX[i].r, pL->pRGBX[i].g, pL->pRGBX[i].b);
            }
            pL->lh[0].r= 0; pL->lh[0].g= 0; pL->lh[0].b= 0x08; pL->lh[0].x= 0xFF;
            pL->lh[1].r= 0xFF; pL->lh[0].g= 0xFF; pL->lh[0].b= 0xFF; pL->lh[0].x= 0xFF;
            return(r);
         }
      }
      printf("imageLoadLUT() %zu %d,%d\n", b, n, r);
   }
   return(0);
} // imageLoadLUT

size_t imageTransferRGB (U8 *pRGB, const Scalar * const pAB, const ImgOrg * const pO,  const ImgMap * const pM)
{
   Bool32 useLUT= FALSE;
   F32 map[2]={0,0};
   F32 a, b;
   size_t i, k=0;
   I32 j, m, x, y;

   if (gLUT.pRGBX && (gLUT.n > 1)) { map[0]= gLUT.n; useLUT= TRUE; } else { map[0]= 0xFF; }
   if (pM && (pM->dom.max > pM->dom.min))
   {
      map[0]/= (pM->dom.max - pM->dom.min);
      map[1]= - map[0] * pM->dom.min;
   }
   if (useLUT)
   {
      const ImageLUT *pL= &gLUT;
      m= pL->n - 1;
      for (y= 0; y < pO->def.y; y++)
      {
         for (x= 0; x< pO->def.x; x++)
         {
            const RGBX *pT;

            i= x * pO->stride[0] + y * pO->stride[1];
            //a= pAB[i]; 
            b= pAB[i + pO->stride[3]];

            j= b * map[0] + map[1];

            if (j >= 0)
            {
               if (j > m) { j= m; }
               pT= pL->pRGBX + j; 
            }
            else { pT= pL->lh+0; }
            
            pRGB[k++]= pT->r;
            pRGB[k++]= pT->g;
            pRGB[k++]= pT->b;
         }
      }
   }
   else
   {
      for (y= 0; y < pO->def.y; y++)
      {
         for (x= 0; x< pO->def.x; x++)
         {
            i= x * pO->stride[0] + y * pO->stride[1];
            a= pAB[i]; 
            b= pAB[i + pO->stride[3]];

            j= b * map[0] + map[1];
            if (j < 0) { j= 0; }
            pRGB[k++]= a * 0x40;
            //w= b;//1.0 / (1.0 + exp(-b)); // sigmoid
            pRGB[k++]= j;
            pRGB[k++]= 0xFF-j;
         }
      }
   }
   return(k);
} // imageTransferRGB

size_t sfFromMap
(
   Scalar * const pAB, 
   const ImgOrg * const pO, 
   const U8 *pM, 
   const ScalAB vSAB[], 
   const int maxSAB
)
{
   size_t i, k=0;
   int x, y;

   for (y= 0; y < pO->def.y; y++)
   {
      for (x= 0; x< pO->def.x; x++)
      {
         U8 m= pM[k++];
         m= MIN(m, maxSAB);
         i= x * pO->stride[0] + y * pO->stride[1];
         pAB[i]= vSAB[m].a;
         pAB[i + pO->stride[3]]= vSAB[m].b;
      }
   }
   return(k);
} // sfFromMap
