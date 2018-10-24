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


/***/

extern int imageLoadLUT (const MemBuff *pB, const char path[]);

// void initWrap (BoundaryWrap *pW, const Stride stride[4]);
extern size_t imageTransferRGB (U8 *pRGB, const Scalar * const pAB, const ImgOrg * const pO, U32 mode);

extern size_t sfFromMap (Scalar * const pAB, const ImgOrg * const pO, const U8 *pM, const ScalAB vSAB[], const int maxSAB);

#endif // IMAGE_H
