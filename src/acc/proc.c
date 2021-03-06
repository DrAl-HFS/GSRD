// proc.c - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb 2018 - April 2019

#include "proc.h"

#ifndef NACC
#include <openacc.h>
#endif
#ifndef NOMP
#include <omp.h>
#endif

#if 0 //def __PGI   // HACKY
#define INLINE inline
#warning __PGI : INLINE -> inline
#endif

#ifndef INLINE
#define INLINE
#endif

#define ACC_DEV_MAX 4

typedef struct
{
   U8 c, n, x[2];
} AccDev;

typedef struct
{
   AccDev d[ACC_DEV_MAX];
   U8 nDev, iCurr;
   U8 iHost;
   U8 pad[1];
} AccDevTable;

typedef struct
{
   U32 o, n; // offset & count
} SDC; // Sub-Domain-Copy

typedef struct
{
   MinMaxU32 mm;  // Y spans
   SDC in, out, upd;
} DomSub;

typedef struct
{
   int n, t;
} MapDev;

typedef struct
{
   DomSub  d[2];
   MapDev  dev;
} DSMapNode;

// typedef struct { V2U32 min, max; } DomSub2D;

static AccDevTable gDev={0,};


/*** Dummies for build compatibility ***/
#ifdef NACC
#define acc_device_host 0
#define acc_device_nvidia 1
#define acc_device_not_host 2
int acc_get_num_devices (int t) { return(0); }
int acc_get_device_num (int t) { return(-1); }
void acc_set_device_num (int n, int t) { ; }
void acc_wait_all (void) { ; }
#endif
#ifdef NOMP
int omp_get_max_threads (void) { return(0); }
int omp_get_num_threads (void) { return(0); }
int omp_get_thread_num (void) { return(0); }
#endif

/*** Misc util ***/

static void addDevType (AccDevTable *pT, const U8 c, const U8 nC, const U8 x0, const U8 x1)
{
   int n= nC;
   U8 nA= 0;
   while (--n >= 0)
   {
      if (pT->nDev < ACC_DEV_MAX)
      {
         pT->d[pT->nDev].c= c;
         pT->d[pT->nDev].n= n;
         pT->d[pT->nDev].x[0]= x0;
         pT->d[pT->nDev].x[1]= x1;
         ++(pT->nDev);
      }
   }
} // addDevType

/*** Application (model) routines ***/

// Diffusion operators:

// Compute a symmetric 9-point discrete Laplacian operator on a 2D scalar field using
// four stride offsets to deal with boundary conditions.
// pS gives the address of the scalar value at the centre of the operator kernel
// Strides s[0..3] -> -X, +X -Y, +Y
// Weights k[0..2] centre, horizontal&vertical, diagonal
INLINE Scalar laplace2D4S9P (const Scalar * const pS, const Stride s[4], const Scalar k[3])
{
   return( pS[0] * k[0] +
           (pS[ s[0] ] + pS[ s[1] ] + pS[ s[2] ] + pS[ s[3] ]) * k[1] + 
           (pS[ s[0]+s[2] ] + pS[ s[0]+s[3] ] + pS[ s[1]+s[2] ] + pS[ s[1]+s[3] ]) * k[2] ); 
} // laplace2D4S9P

// As preceding but eight stride offsets to deal with arbitrary spatial structure.
// pS gives the address of the scalar value at the centre of the operator kernel
// Strides s[0..3] -> -X, +X -Y, +Y, s[4..7] -> ??? -X-Y, +X-Y, -X+Y, +X+Y ???
// Weights k[0..2] centre, horizontal&vertical, diagonal
INLINE Scalar laplace2D8S9P (const Scalar * const pS, const Stride s[8], const Scalar k[3])
{
   return( pS[0] * k[0] +
           (pS[ s[0] ] + pS[ s[1] ] + pS[ s[2] ] + pS[ s[3] ]) * k[1] + 
           (pS[ s[4] ] + pS[ s[5] ] + pS[ s[6] ] + pS[ s[7] ]) * k[2] ); 
} // laplace2D8S9P

/***/
// RENAME proc2D4S9P ???
// Compute a single point Grey-Scott update based on the preceding Laplacian operator applied to
// each of the reagent scalar fields A & B. Double-buffering of scalar fields is used to prevent
// temporal feed-back error while maintaining relatively straightforward parallelisability.
// pR and pS respectively give the result and source scalar field pairs
// i is the index of the element to be updated
// j is the stride between reagents within a scalar field pair
// wrap[] is used as described in the Laplacian operator
// pP gives the coefficients describing diffusion and reaction rates (unparameterised in this instance)
INLINE void proc1 (Scalar * const pR, const Scalar * const pS, const Index i, const Stride j, const Stride wrap[4], const BaseParamVal * const pP)
{
   const Scalar * const pA= pS+i, a= *pA;
   const Scalar * const pB= pS+i+j, b= *pB;
   const Scalar rab2= a * b * b; // pP->kRR * 

   pR[i]= a + laplace2D4S9P(pA, wrap, pP->kL.a) - rab2 + pP->kRA * (1 - a);
   pR[i+j]= b + laplace2D4S9P(pB, wrap, pP->kL.b) + rab2 - pP->kDB * b;
} // proc1

// Wrapper function for proc1() that determines wrap values for the local spatial indices.
// This automates boundary processing.
INLINE void proc1XY (Scalar * const pR, const Scalar * const pS, const Index x, const Index y, const ImgOrg *pO, const BaseParamVal * const pP)
{
   Stride wrap[4];

   wrap[0]= pO->nhStepWrap[ (x <= 0) ][0];
   wrap[1]= pO->nhStepWrap[ (x >= (Index)(pO->def.x-1)) ][1];
   wrap[2]= pO->nhStepWrap[ (y <= 0) ][2];
   wrap[3]= pO->nhStepWrap[ (y >= (Index)(pO->def.y-1)) ][3];

   proc1(pR, pS, x * pO->stride[0] + y * pO->stride[1], pO->stride[3], wrap, pP);
} // proc1XY

// Single GS iteration over field/image using brute force boundary check for every site
void procAXY (Scalar * restrict pR, const Scalar * restrict pS, const ImgOrg * pO, const BaseParamVal * pP)
{
   #pragma acc data present( pR[:pO->n], pS[:pO->n], pO[:1], pP[:1] )
   {
      #pragma acc parallel loop
      for (U32 y= 0; y < pO->def.y; ++y )
      {
         #pragma acc loop vector
         for (U32 x= 0; x < pO->def.x; ++x )
         {
            proc1XY(pR, pS, x, y, pO, pP);
         }
      }
   }
} // procAXY

/*** Modified versions for complex geometry specified by map ***/

INLINE void proc2D8S9P (Scalar * const pR, const Scalar * const pS, const Index i, const Stride j, const Stride wrap[8], const BaseParamVal * const pP)
{
   const Scalar * const pA= pS+i, a= *pA;
   const Scalar * const pB= pS+i+j, b= *pB;
   const Scalar rab2= a * b * b; //pP->kRR * 

   pR[i]= a + laplace2D8S9P(pA, wrap, pP->kL.a) - rab2 + pP->kRA * (1 - a);
   pR[i+j]= b + laplace2D8S9P(pB, wrap, pP->kL.b) + rab2 - pP->kDB * b;
} // proc2D8S9P

// Wrapper function for proc2D8S9P() that determines wrap values for the local spatial indices using map entry
// Current implementation restricted to reflective boundary processing.
INLINE void proc1M8S (Scalar * const pR, const Scalar * const pS, const Index x, const Index y, const ImgOrg *pO, const BaseParamVal * const pP, const MapSite m)
{
   Stride wrap[8];
#if 1
   for (int i=0; i<8; i++) { wrap[i]= ((1<<i) & m) ? pO->nhStepWrap[0][i] : 0; }
#else
   //for (int i=0; i<8; i++) { wrap[i]= pO->nhStepWrap[ (0 == (m & (1<<i))) ][i]; }
   wrap[0]= pO->nhStepWrap[ (0 == (m & 0x01)) ][0];
   wrap[1]= pO->nhStepWrap[ (0 == (m & 0x02)) ][1];
   wrap[2]= pO->nhStepWrap[ (0 == (m & 0x04)) ][2];
   wrap[3]= pO->nhStepWrap[ (0 == (m & 0x08)) ][3];
   wrap[4]= pO->nhStepWrap[ (0 == (m & 0x10)) ][4];
   wrap[5]= pO->nhStepWrap[ (0 == (m & 0x20)) ][5];
   wrap[6]= pO->nhStepWrap[ (0 == (m & 0x40)) ][6];
   wrap[7]= pO->nhStepWrap[ (0 == (m & 0x80)) ][7];
#endif
   proc2D8S9P(pR, pS, x * pO->stride[0] + y * pO->stride[1], pO->stride[3], wrap, pP);
} // proc1M8S

// Single GS iteration over field/image with non-uniform structure using neighbour map for reflective boundary check at every site
void procMXY (Scalar * restrict pR, const Scalar * restrict pS, const ImgOrg * pO, const BaseParamVal * pP, const MapData * pMD)
{
   const MapSite * const pM= pMD->pM;
   #pragma acc data present( pR[:pO->n], pS[:pO->n], pM[:pMD->nM], pO[:1], pP[:1] )
   {
      #pragma acc parallel loop
      for (U32 y= 0; y < pO->def.y; ++y )
      {
         #pragma acc loop vector
         for (U32 x= 0; x < pO->def.x; ++x )
         {
            const MapSite m= pM[ x + y * pO->def.x ];

            if (0 != m) { proc1M8S(pR, pS, x, y, pO, pP, m); }
         }
      }
   }
} // procMXY

// Multiple GS iterations of even number - minimises buffer copying
U32 proc2IM
(
   Scalar * restrict pTR, // temp "result" (unused if acc. device has sufficient private memory)
   Scalar * restrict pSR, // source & result buffer
   const ImgOrg   * pO, 
   const ParamVal * pP, 
   const U32      nI,
   const MapData  * pMD
)
{
   #pragma acc data present_or_create( pTR[:pO->n] ) copy( pSR[:pO->n] ) \
                    present_or_copyin( pMD->pM[:pMD->nM], pO[:1], pP[:1] )
   {
      for (U32 i= 0; i < nI; ++i )
      {
         procMXY(pTR,pSR,pO,&(pP->base),pMD);
         procMXY(pSR,pTR,pO,&(pP->base),pMD);
      }
   }
   return(2*nI);
} // proc2IM

/**/

// Single GS iteration over field/image, avoiding most boundary checking
void procA (Scalar * restrict pR, const Scalar * restrict pS, const ImgOrg * pO, const BaseParamVal * pP)
{
   #pragma acc data present( pR[:pO->n], pS[:pO->n], pO[:1], pP[:1] )
   {
      #pragma acc parallel loop
      for (U32 y= 1; y < (pO->def.y-1); ++y )
      {
         #pragma acc loop vector
         for (U32 x= 1; x < (pO->def.x-1); ++x )
         {
            proc1(pR, pS, y * pO->stride[1] + x * pO->stride[0], pO->stride[3], pO->nhStepWrap[0], pP);
         }
      }

      // Boundaries
      #pragma acc parallel
      {  // Horizontal top & bottom avoiding corners
         #pragma acc loop vector
         for (U32 x= 1; x < (pO->def.x-1); ++x )
         {
            proc1(pR, pS, x * pO->stride[0], pO->stride[3], pO->wrap.h+0, pP);
         }
         #pragma acc loop vector
         for (U32 x= 1; x < (pO->def.x-1); ++x )
         {
            const Stride offsY= pO->stride[2] - pO->stride[1];
            proc1(pR, pS, x * pO->stride[0] + offsY, pO->stride[3], pO->wrap.h+2, pP);
         }
         // left & right including corners (requires boundary check)
         #pragma acc loop vector
         for (U32 y= 0; y < pO->def.y; ++y )
         {
            proc1XY(pR, pS, 0, y, pO, pP);
         }
         #pragma acc loop vector
         for (U32 y= 0; y < pO->def.y; ++y )
         {
            proc1XY(pR, pS, pO->def.x-1, y, pO, pP);
         }
      } // ... acc parallel
   } // ... acc data ..
} // procA

// Multiple GS iterations of even number - minimises buffer copying
U32 proc2IA
(
   Scalar * restrict pTR, // temp "result" (unused if acc. device has sufficient private memory)
   Scalar * restrict pSR, // source & result buffer
   const ImgOrg   * pO, 
   const ParamVal * pP, 
   const U32 nI
)
{
   #pragma acc data present_or_create( pTR[:pO->n] ) copy( pSR[:pO->n] ) \
                    present_or_copyin( pO[:1], pP[:1] )
   {
      for (U32 i= 0; i < nI; ++i )
      {
         procA(pTR,pSR,pO,&(pP->base));
         procA(pSR,pTR,pO,&(pP->base));
      }
   }
   return(2*nI);
} // proc2IA

// Multiple GS iterations of odd number - minimises buffer copying
U32 proc2I1A (Scalar * restrict pR, Scalar * restrict pS, const ImgOrg * pO, const ParamVal * pP, const U32 nI)
{
   #pragma acc data present_or_create( pR[:pO->n] ) copyin( pS[:pO->n] ) copyout( pR[:pO->n] ) \
                    present_or_copyin( pO[:1], pP[:1] )
   {
      procA(pR,pS,pO,&(pP->base));
      for (U32 i= 0; i < nI; ++i )
      {
         procA(pS,pR,pO,&(pP->base));
         procA(pR,pS,pO,&(pP->base));
      }
   }
   return(2*nI+1);
} // proc2I1A



/*** multi-device testing ***/
/* DEPRECATE
INLINE void procAXYDS
(
   Scalar * restrict pR,
   const Scalar * restrict pS,
   const ImgOrg * pO,
   const BaseParamVal * pP,
   const DomSub d[2]
)
{
//present( pR[d[0].in.o:d[0].in.n], pS[d[0].in.o:d[0].in.n], pR[d[1].in.o:d[1].in.n], pS[d[1].in.o:d[1].in.n] ) 
   #pragma acc data present( pR[:pO->n], pS[:pO->n], pO[:1], pP[:1], d[:2] )
   {
      #pragma acc parallel loop
      for (U32 i= 0; i < 2; ++i )
      {
         //pragma acc parallel loop
         for (U32 y= d[i].mm.min; y <= d[i].mm.max; ++y )
         {
            #pragma acc loop vector
            for (U32 x= 0; x < pO->def.x; ++x )
            {
               proc1XY(pR, pS, x, y, pO, pP);
            }
         }
      }
   }
} // procAXYDS
*/
void procMD
(
   Scalar * restrict pR,
   Scalar * restrict pS,
   const ImgOrg       * pO,
   const BaseParamVal * pP,
   const DSMapNode   * pDSMN
)
{
   const DomSub * pD= pDSMN->d;
/*
   const size_t i0a= pD[0].in.o, i0n= pD[0].in.n;
   const size_t i0b= i0a + pO->stride[2];
   const size_t i1a= pD[1].in.o, i1n= pD[1].in.n;
   const size_t i1b= i1a + pO->stride[2];

   const size_t o0a= pD[0].upd.o, o0n= pD[0].upd.n;
   const size_t o0b= i0a + pO->stride[2];
   const size_t o1a= pD[1].upd.o, o1n= pD[1].upd.n;
   const size_t o1b= i1a + pO->stride[2];

   pragma acc data present_or_create( pTR[ i0a:i0n ], pTR[ i1a:i1n ], pTR[ i0b:i0n ], pTR[ i1b:i1n ] ) \
               copyin( pSR[ i0a:i0n ], pSR[ i1a:i1n ], pSR[ i0b:i0n ], pSR[ i1b:i1n ] ) \
               copyout( pTR[ o0a:o0n ], pTR[ o1a:o1n ], pTR[ o0b:o0n ], pTR[ o1b:o1n ] ) \
               copyin( pO[:1], pP[:1], pD[:2] )
*/
   //acc_set_device_num ( pDSMN->dev.n, pDSMN->dev.c );
   const int n= pDSMN->dev.n, t= pDSMN->dev.t;
   #pragma acc set device_num(n) device_type(t)
   #pragma acc data copy( pR[:pO->n] , pS[:pO->n] ) copyin( pO[:1], pP[:1], pD[:2] )
   #pragma acc parallel async( pDSMN->dev.n )
   {
      //pragma acc parallel loop
      for (U32 i= 0; i < 2; ++i )
      {
         //pragma acc parallel loop
         for (U32 y= pD[i].mm.min; y <= pD[i].mm.max; ++y )
         {
            #pragma acc loop vector
            for (U32 x= 0; x < pO->def.x; ++x )
            {
               proc1XY(pR, pS, x, y, pO, pP);
            }
         }
      }
   }
} // procMD

U32 hackMD
(
   Scalar * restrict pTR,
   Scalar * restrict pSR,
   const ImgOrg   * pO,
   const ParamVal * pP,
   const U32 nI
)
{
   DSMapNode aD[3];
   DomSub *pD;
   U32 nD= 0, mD= MIN(2, gDev.nDev);
   if (mD > 1)
   {
      U32 o= 0, n= pO->def.y;
      if (gDev.iHost < gDev.nDev)
      {
         U32 se= 7;
         aD[nD].dev.n= gDev.d[gDev.iHost].n;
         aD[nD].dev.t= gDev.d[gDev.iHost].c;
         pD= aD[nD].d;

         pD->mm.min= 0;
         pD->mm.max= se - 1;
         pD->in.o= 0;
         pD->in.n= 2 + pD->mm.max - pD->mm.min;
         pD->upd= pD->out= pD->in;
         pD++;
         pD->mm.min= pO->def.y - se;
         pD->mm.max= pO->def.y - 1;
         pD->in.o= pD->mm.min - 1;
         pD->in.n= 2 + pD->mm.max - pD->mm.min;
         pD->upd= pD->out= pD->in;
         nD++;
         o+= se;
         n-= 2 * se;
      }
      if (nD < mD)
      {
         U32 iNH= 0;
         iNH+= (0 == gDev.iHost);

         aD[nD].dev.n= gDev.d[iNH].n;
         aD[nD].dev.t= gDev.d[iNH].c;
         pD= aD[nD].d;

         pD[0].mm.min= o;
         pD[0].mm.max= pD[0].mm.min + (n / 2) - 1;
         pD[0].in.o= pD[0].mm.min - 1;
         pD[0].in.n= 2 + pD[0].mm.max - pD[0].mm.min;
         pD[0].upd= pD[0].out= pD[0].in;

         pD[1].mm.min= pD[0].mm.max + 1;
         pD[1].mm.max= pD[1].mm.min + n - (n / 2) - 1;
         pD[1].in.o= pD[1].mm.min;
         pD[1].in.n= 2 + pD[1].mm.max - pD[1].mm.min;
         pD[1].upd= pD[1].out= pD[1].in;
        nD++;
      }

      for ( U32 j=0; j<nD; j++)
      {
         for ( U32 i= 0; i<2; ++i )
         {
            aD[j].d[i].in.o*=  pO->stride[1];
            aD[j].d[i].in.n*=  pO->stride[1];
            aD[j].d[i].out.o*= pO->stride[1];
            aD[j].d[i].out.n*= pO->stride[1];
            aD[j].d[i].upd.o*= pO->stride[1];
            aD[j].d[i].upd.n*= pO->stride[1];
         }

         report(VRB1,"[%u] %u:%u\n", j, aD[j].dev.t, aD[j].dev.n); pD= aD[j].d; // dump
         report(VRB1,"   mm \t%u,%u; %u,%u\n", pD[0].mm.min, pD[0].mm.max, pD[1].mm.min, pD[1].mm.max);
         report(VRB1,"   in \t%u,%u; %u,%u\n", pD[0].in.o, pD[0].in.n, pD[1].in.o, pD[1].in.n);
         report(VRB1,"   out\t%u,%u; %u,%u\n", pD[0].out.o, pD[0].out.n, pD[1].out.o, pD[1].out.n);
         report(VRB1,"   upd\t%u,%u; %u,%u\n", pD[0].upd.o, pD[0].out.n, pD[1].out.o, pD[1].out.n);
      }
      if (nD > 1)
      {
         for (U32 i= 0; i < nI; ++i )
         {
            //pragma omp parallel for
            for (U32 j= 0; j < nD; ++j)
            {
               procMD(pTR, pSR, pO, &(pP->base), aD+j);
            }
            //pragma omp parallel for
            for (U32 j= 0; j < nD; ++j)
            {
               procMD(pSR, pTR, pO, &(pP->base), aD+i);
            }
         }
         return(2 * nI);
      }
   }
   return(0);
} // hackMD

/*** External Interface ***/

/* /opt/pgi/linux.../2017/iclude/openacc.h

typedef enum{
        acc_device_none = 0,
        acc_device_default = 1,
        acc_device_host = 2,
        acc_device_not_host = 3,
        acc_device_nvidia = 4,
        acc_device_radeon = 5,
        acc_device_xeonphi = 6,
        acc_device_pgi_opencl = 7,
        acc_device_nvidia_opencl = 8,
        acc_device_opencl = 9
    }acc_device_t;

void acc_set_default_async(int async);
int acc_get_default_async(void);
extern int acc_get_num_devices( acc_device_t devtype );
extern acc_device_t acc_get_device(void);
extern void acc_set_device_num( int devnum, acc_device_t devtype );
extern int acc_get_device_num( acc_device_t devtype );
extern void acc_init( acc_device_t devtype );
extern void acc_shutdown( acc_device_t devtype );
extern void acc_set_deviceid( int devid );
extern int acc_get_deviceid( int devnum, acc_device_t devtype );
extern int acc_async_test( __PGI_LLONG async );
extern int acc_async_test_all(void);
extern void acc_async_wait( __PGI_LLONG async );
extern void acc_async_wait_all(void);
extern void acc_wait( __PGI_LLONG async );
extern void acc_wait_async( __PGI_LLONG arg, __PGI_LLONG async );
extern void acc_wait_all(void);
extern void acc_wait_all_async( __PGI_LLONG async );
extern int acc_on_device( acc_device_t devtype );
extern void acc_free(void*); */


Bool32 procInitAcc (const size_t f) // arg param ?
{
   int initOK= (0 == (f & (PROC_FLAG_ACCGPU|PROC_FLAG_ACCMCORE)));
   int nNV= acc_get_num_devices( acc_device_nvidia );
   int nH= acc_get_num_devices( acc_device_host );
   int nNH= acc_get_num_devices( acc_device_not_host );
   int id;

   report(VRB1,"procInitAcc() - nH=%d nNV=%d, (other=%d)\n", nH, nNV, nNH - nNV);
   gDev.nDev= 0;
   gDev.iHost= -1;
   if ((nH > 0) && (f & PROC_FLAG_ACCMCORE))
   {
      I32 v[2]=0;
      scanEnvID(v+0, 1, "ACC_NUM_CORES");
      scanEnvID(v+1, 1, "OMP_NUM_THREADS");
      id= acc_get_device_num(acc_device_host);
//acc_get_device_processors(id);???
      report(VRB1,"\tMC: C%d T%d id=%d\n", v[0], v[1], id);
      addDevType(&gDev, acc_device_host, nH, v[0], v[1]);
      gDev.iHost= 0;
   }
   if ((nNV > 0) && (f & PROC_FLAG_ACCGPU))
   {
      id= acc_get_device_num(acc_device_nvidia);
      report(VRB1,"\tNV:id=%d\n", id);
      addDevType(&gDev, acc_device_nvidia, nNV, 0, 0);
   }
   initOK+= gDev.nDev;
   if (gDev.nDev > 0)
   {
      gDev.iCurr= 0;
      acc_set_device_num( gDev.d[0].n, gDev.d[0].c );
   }
   report(VRB1,"procInitAcc(0x%x) - %d\n", f, initOK);
   return(initOK > 0);
} // procInitAcc

const char *procGetCurrAccTxt (char t[], int m)
{
   const AccDev * const pA= gDev.d + gDev.iCurr;
   const char *s="C";
   int n=0;

   switch (pA->c)
   {
      case acc_device_nvidia : s= "NV"; break;
      case acc_device_host :   s= "MC"; break;
      default : s= "?"; break;
   }

   n= snprintf(t, m, "%s%u", s, pA->n);
   if ((pA->x[0] > 0) || (pA->x[1] > 0))
   {
      n+= snprintf(t+n, m-n, "C%uT%u", pA->x[0], pA->x[1]);
   }
   return(t);
} // procGetCurrAccTxt

Bool32 procSetNextAcc (Bool32 wrap)
{
#ifndef NACC
   if (gDev.nDev > 0)
   {
      U8 iN= gDev.iCurr + 1;
      if (wrap) { iN= iN % gDev.nDev; }
      if (iN < gDev.nDev)
      {
         acc_set_device_num( gDev.d[iN].n, gDev.d[iN].c );
         gDev.iCurr= iN;
         return(TRUE);
      }
   }
#endif // NACC
   return(FALSE);
} // procSetNextAcc

// Perform arbitray number of GS iterations, selecting odd or even
// buffer handling (minimisation of copying) as appropriate
U32 procNI
(
   Scalar * restrict pR,
   Scalar * restrict pS,
   const ImgOrg   * pO,
   const ParamVal * pP,
   const U32      nI,
   const MapData  * pMD
)
{
   if (pMD && pMD->pM && (pMD->nM > 0))
   {
      if (0 == (nI & 1)) { return proc2IM(pR, pS, pO, pP, nI>>1, pMD); }
   }
   else
   {
      if (nI & 1) return proc2I1A(pR, pS, pO, pP, nI>>1);
      else
      {
         /*U32 n= hackMD(pR, pS, pO, pP, nI>>1);
         if (0 == n) { n= proc2IA(pR, pS, pO, pP, nI>>1); }
         return(n);*/
         return proc2IA(pR, pS, pO, pP, nI>>1);
      }
   }
   return(0);
} // procNI

void procTest (void)
{
#ifndef NOMP
   int i, n= omp_get_max_threads();
   report(VRB1,"procTest(%d)-\n", n);
   //n= MIN(2,n);
   #pragma omp parallel num_threads(n)
   {
      n= omp_get_num_threads();
      i= omp_get_thread_num();
      report(VRB1,"Hello %d / %d\n", i, n);
   }
   report(VRB1,"-procTest()\n");
#endif // NOMP
} // procTest

