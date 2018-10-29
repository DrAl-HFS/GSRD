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
   F32   *pT;
   RGBX  *pRGBX;
   U16   n;       // Number loaded
   I8    nL, nH;  // low & hi guard zones outside the loaded table
} ImageLUT;


static ImageLUT gLUT={0,};

static const F32 gEps= 1.0 / (1<<20);

/***/

//RGBX scaleRGBF (F32 r, F32 g, F32 b, F32 s)
RGBX packRGB8 (U8 r, U8 g, U8 b)
{
   RGBX v={r,g,b,0xFF};
   return(v);
} // packRGB8

static I32 validLUT (const F32 *pF, const I32 nF, const MinMaxI32 *pMM)
{
   I32 m=3;
   if (pMM && (pMM->min >= 3) && (pMM->max == pMM->min)) { m= pMM->min; }
   if (pF && (nF >= m))
   {
      int r= nF / m;
      if ((m > 4) || (r > (1<<12))) { printf("WARNING: validLUT() %d*%d\n", m, r); }
      if (nF == (r * pMM->min))
      {
         printf("validLUT() %d*%d=%d OK\n", m, r, nF);
         return(r);
      }
   }
   return(0);
} // validLUT

static void clean (signed char *pS, size_t n)
{
   while (pS[n] < ' ') { printf("[%zu] 0x%02X\n", n, 0xFF & pS[n]); --n; }
} // clean

I32 findLH (F32 v, const ImageLUT *pL)
{
   I32 l= -1, m= pL->n, i= 0, j= 0;
   while ((i < pL->nL) && (v < pL->pT[l-i])) { ++i; }
   if (i) { return(l-i); }
   while ((j < pL->nH) && (v > pL->pT[m+j])) { ++j; } 
   if (j) { return(m + j); }

   return(0);
} // findLH

void addGuards (ImageLUT *pL)
{
   
   if (0 != pL->nL)
   {
      F32 t= 0 - gEps * (pL->nL - 2);
      I32 s= -(pL->nL);
      for (I32 i= s; i < 0; i++)
      {
         pL->pT[i]= t; 
         t+= gEps;
         pL->pRGBX[i]= packRGB8(0x00,0x00,0x00);
      }
      if (pL->nL >= 3) { pL->pRGBX[-3]= packRGB8(0xFF,0x00,0x00); }
      pL->pRGBX[-1]= packRGB8(0x00,0x00,0x20);
   }
   if (0 != pL->nH)
   {
      F32 t= 1+gEps;
      I32 m= pL->n + pL->nH;
      for (I32 i= pL->n; i < m; i++)
      {
         pL->pT[i]= t; 
         t+= gEps;
         pL->pRGBX[i]= packRGB8(0xFF,0xFF,0xFF);
      }
   }
} // addGuards

int imageLoadLUT (const MemBuff *pB, const char path[])
{
   size_t b= fileSize(path);
   int pad= 4 - (b & 0x3);
   if ((b > 0) && validBuff(pB, b + pad)) 
   {
      char *pS;
      F32 *pF;
      MinMaxI32 mm;
      I32 m, n=-1, r=-1, s=-1;

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
         m= MIN(256 * 4, m); // prevent overrun crash
         n= scanTableF32(pF, m, &mm, pS+s, &s, b-s);
         r= validLUT(pF,n,&mm);
         if (r > 0)
         {
            ImageLUT *pL= &gLUT;

            pL->n= r;
            pL->nL= 3;
            pL->nH= 1;
            s= pL->n + pL->nL + pL->nH;
            if (validBuff(pB, s * (sizeof(F32) + sizeof(RGBX))))
            {
               pL->pT= (float*)(pB->w + pB->bytes) - s;
               pL->pRGBX= (RGBX*)gLUT.pT - s;
               addGuards(pL);
               for (I32 i= 0; i < r; i++)
               {
                  I32 j= i * mm.min;
                  pL->pT[i]= pF[j+0];
                  pL->pRGBX[i]= packRGB8(pF[j+1], pF[j+2], pF[j+3]);
                  //printf("%G %u %u %u\n", pL->pT[i], pL->pRGBX[i].r, pL->pRGBX[i].g, pL->pRGBX[i].b);
               }
               {
                  F32 l= linearityF32(pL->pT, pL->n);
                  if (l < 1) { printf("\tWARNING: linearityF32() = %G\n", l); }
               }
            }
            //pL->lh[0].r= 0; pL->lh[0].g= 0; pL->lh[0].b= 0x08; pL->lh[0].x= 0xFF;
            //pL->lh[1].r= 0xFF; pL->lh[0].g= 0xFF; pL->lh[0].b= 0xFF; pL->lh[0].x= 0xFF;
            return(r);
         }
      }
      printf("imageLoadLUT() %zu %d,%d\n", b, n, r);
   }
   return(0);
} // imageLoadLUT

size_t imageTransferRGB (U8 *pRGB, const Scalar * const pAB, const ImgOrg * const pO,  const ImgMap * const pM)
{
   const ImageLUT *pL= &gLUT;
   F32 dom[2]={0,1}, map[2]={1,0};
   F32 a, b, s;
   size_t i, k=0;
   I32 j, m, x, y;

   if (pM && (pM->dom.max > pM->dom.min))
   {
      dom[0]= pM->dom.min;
      dom[1]= pM->dom.max;
   }

   if (pL->pRGBX && (pL->n > 1))
   {
      s= pL->n - gEps;
      if (pL->pT)
      {
         const F32 de[2]= { dom[0] - gEps, dom[1] + gEps }; 
         if (0 != pL->nL)
         {
            pL->pT[-1]= MAX(pL->pT[-1], de[0]); 
         }
         if (0 != pL->nH)
         {
            const F32 d1= dom[1] + gEps; 
            pL->pT[pL->n]= MIN(pL->pT[pL->n], de[1]); 
         }
      }
   } else { s= 0xFF; pL= NULL; }

   map[0]= s / (dom[1] - dom[0]);
   map[1]= - map[0] * dom[0];

   if (pL)
   {
      //size_t out[3]= {0,0,0};
      printf("imageTransferRGB() %G,%G -> %G,%G\n", dom[0], dom[1], dom[0] * map[0] + map[1], dom[1] * map[0] + map[1]);
      m= pL->n - 1;
      for (y= 0; y < pO->def.y; y++)
      {
         for (x= 0; x< pO->def.x; x++)
         {
            const RGBX *pT;

            i= x * pO->stride[0] + y * pO->stride[1];
            //a= pAB[i]; 
            b= pAB[i + pO->stride[3]];
            
            if ((b >= dom[0]) && (b <= dom[1])) { j= b * map[0] + map[1]; }
            else { j= findLH(b, pL); } //if (j < 0) { out[j+pL->nL]++; } }
            
            pT= pL->pRGBX + j;
            pRGB[k++]= pT->r;// + pAB[i] * 0x80;
            pRGB[k++]= pT->g;
            pRGB[k++]= pT->b;
         }
      }
      //printf("out[]=%zu %zu %zu\n", out[0], out[1], out[2]);
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
