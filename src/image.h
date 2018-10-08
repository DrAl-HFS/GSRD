// image.h - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-April 2018

#ifndef IMAGE_H
#define IMAGE_H

#include "data.h"


/***/

extern int imageLoadLUT (Buffer *pB, const char path[]);

// void initWrap (BoundaryWrap *pW, const Stride stride[4]);
extern size_t imageTransferRGB (U8 *pRGB, const Scalar * const pAB, const ImgOrg * const pO, U32 mode);

#endif // IMAGE_H
