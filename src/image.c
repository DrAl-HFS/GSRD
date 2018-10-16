// image.c - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-April 2018

#include "image.h"

/*
typedef struct
{
   V2U32  def;
   Stride stride[4];
   size_t n;
   Stride nhStepWrap[2][4]; // neighbourhood 
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
*/
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
      float *pF;
      MinMaxI32 mm;
      int i, j, m, n=-1, r=-1, s=-1;

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
            pL->lh[0].r= 0; pL->lh[0].g= 0; pL->lh[0].b= 0x8; pL->lh[0].x= 0xFF;
            pL->lh[1].r= 0xFF; pL->lh[0].g= 0xFF; pL->lh[0].b= 0xFF; pL->lh[0].x= 0xFF;
            return(r);
         }
      }
      printf("imageLoadLUT() %zu %d,%d\n", b, n, r);
   }
   return(0);
} // imageLoadLUT

size_t imageTransferRGB (U8 *pRGB, const Scalar * const pAB, const ImgOrg * const pO, U32 mode)
{
   float a, b, w;
   size_t i, k=0;
   int j, x, y;

   if (gLUT.pRGBX && gLUT.n > 1)
   {
      const ImageLUT *pL= &gLUT;
      for (y= 0; y < pO->def.y; y++)
      {
         for (x= 0; x< pO->def.x; x++)
         {
            const RGBX *pT;

            i= x * pO->stride[0] + y * pO->stride[1];
            a= pAB[i]; b= pAB[i + pO->stride[3]];

            if (a < 0) { a= 0; } else if (a > 1) { a= 1; }

            if (b <= 0) { pT= pL->lh+0; }
            else if (b >= 1) { pT= pL->lh+1; }
            else { j= pL->n * b; pT= pL->pRGBX + j; };
            
            pRGB[k++]= pT->r; // + (0xFF - pT->r) * a;
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
            a= pAB[i]; b= pAB[i + pO->stride[3]];
            pRGB[k++]= a * 128;
            w= b;//1.0 / (1.0 + exp(b)); // sigmoid
            pRGB[k++]= w * 255;
            pRGB[k++]= (1-w) * 255;
         }
      }
   }
   return(k);
} // imageTransferRGB
