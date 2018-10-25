// image.h - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-October 2018

#ifndef IMAGE_H
#define IMAGE_H

#include "data.h"

typedef struct
{
   Scalar a, b;
} ScalAB;

typedef struct
{
   MinMaxF64 dom;
} ImgMap;

/***/

extern int imageLoadLUT (const MemBuff *pB, const char path[]);

extern size_t imageTransferRGB (U8 *pRGB, const Scalar * const pAB, const ImgOrg * const pO, const ImgMap * const pM);

extern size_t sfFromMap (Scalar * const pAB, const ImgOrg * const pO, const U8 *pM, const ScalAB vSAB[], const int maxSAB);

#endif // IMAGE_H
