
#ifndef PROC_H
#define PROC_H

#ifdef OPENACC
#include <openacc.h>
//#include <assert.h>
//#include <accelmath.h>
#endif

#include "data.h"


/***/

//extern void bindData (const ParamVal * const pP, const ImgOrg * const pO, const U32 iS);

extern U32 proc (Scalar * restrict pR, const Scalar * restrict pS, const ImgOrg * pO, const ParamVal * pP, const U32 iterMax);
   
#endif // PROC_H
