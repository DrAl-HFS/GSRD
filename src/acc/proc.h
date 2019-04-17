// proc.h - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb 2018 - April 2019

#ifndef PROC_H
#define PROC_H

#include "data.h"
#include "report.h"

#define PROC_NOWRAP FALSE

/***/

// Initialise state for host/gpu acc devices determined by flags arguement
// Flags defined in "args.h"
extern Bool32 procInitAcc (const size_t f);
// Get a text label for current acc device
extern const char *procGetCurrAccTxt (char t[], int m);
// Advance through sequence of acc devices
extern Bool32 procSetNextAcc (Bool32 wrap);


// Process nI iterations of the model, starting in buffer pS and ending in pR for an even number of iterations (or pS for odd)
extern U32 procNI (Scalar * restrict pR, Scalar * restrict pS, const ImgOrg * pO, const ParamVal * pP, const U32 nI, const MapData *pMD);

// Hacky test stuff
extern void procTest (void);

#endif // PROC_H
