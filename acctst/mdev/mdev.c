#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <openacc.h>

#ifdef __PGI
#define INLINE inline
#else
#define INLINE
#endif

#define GETTIME(a) gettimeofday(a,NULL)
#define USEC(t1,t2) (((t2).tv_sec-(t1).tv_sec)*1000000+((t2).tv_usec-(t1).tv_usec))
typedef struct timeval timestruct;

typedef float Scalar;
typedef double SMVal;

SMVal deltaT (void)
{
   static struct timeval t[2]={0,};
   static int i= 0;
   SMVal dt;
   GETTIME(t+i);
   dt= 1E-6 * USEC(t[i^1],t[i]);
   i^= 1;
   return(dt);
} // deltaT

typedef struct
{
   int nH, iH, nNV, iNV, tA, iA;
} DevCtx;

static DevCtx gDC={0,};

/*---*/

// -> "PGC-S-0155-Procedures called in a compute region must have acc routine information: vSetLA (mdev.c: 84)"
// pragma acc routine vector -> "PGC-S-0155-misplaced #pragma acc routine  (mdev.c: 56)"
void vSet (Scalar * restrict pR, const size_t n, const Scalar v)
{
   for (size_t i= 0; i < n; ++i ) { pR[i]= v; }
} // vSet

void vCopy (Scalar * restrict pR, const size_t n, const Scalar * const pS)
{
   for (size_t i= 0; i < n; ++i ) { pR[i]= pS[i]; }
} // vCopy

void vAdd (Scalar * restrict pR, const Scalar * const pV1, const Scalar * const pV2, const size_t n)
{
   for (size_t i= 0; i < n; ++i ) { pR[i]= pV1[i] + pV2[i]; }
} // vAdd

void vSetLin (Scalar * restrict pR, const size_t n, const Scalar lm[2])
{
   for (size_t i= 0; i < n; ++i ) { pR[i]= i * lm[1] + lm[0]; }
} // vSetLin

void vLinComb (Scalar * restrict pR, const Scalar * const pV1, const Scalar * const pV2, const size_t n, const Scalar k[2])
{
   //pragma acc data present( pR[:n], pV1[:n], pV2[:n], k[:2] )
   for (size_t i= 0; i < n; ++i ) { pR[i]= pV1[i] * k[0] + pV2[i] * k[1]; }
} // vLinComb

#pragma acc routine(vSetLin) vector
#pragma acc routine(vLinComb) vector

void proc (Scalar * restrict pV0, Scalar * restrict pV1, Scalar * restrict pV2, Scalar * restrict pV3, size_t n)
{
   SMVal dt[8]={-1,};

   printf("proc()\n");
   #pragma acc set device_num(0) device_type(acc_device_host)
   dt[0]= deltaT();

   const Scalar lm1[2]={0.5*n,-0.5}, lm2[2]={-0.5*n,0.5}, k[2]= {0.25, 0.25};
   #pragma acc data create( pV1[:n], pV2[:n] ) copyin( lm1[:2], lm2[:2] ) copyout( pV1[:n], pV2[:n] )
   {
      #pragma acc parallel
      {
         vSetLin(pV1, n, lm1);
         vSetLin(pV2, n, lm2);
      }
   }
   dt[0]= deltaT();
   printf("\tdt: %G\n", dt[0]);

   #pragma acc set device_num(0) device_type(acc_device_nvidia)
   #pragma acc enter data create( pV0[:n] ) copyin( pV1[:n], pV2[:n], k[:2] )

   #pragma acc parallel async
   vLinComb(pV0, pV1, pV2, n, k);

   dt[1]= deltaT();
   printf("\tdt: %G %G\n", dt[0], dt[1]);

   #pragma acc set device_num(0) device_type(acc_device_host)
   #pragma acc enter data create( pV3[:n] ) copyin( pV1[:n], pV2[:n], k[:2] )

   #pragma acc parallel async
   vLinComb(pV3, pV1, pV2, n, k);

   dt[2]= deltaT();
   printf("\tdt: %G %G %G\n", dt[0], dt[1], dt[2]);


   #pragma acc set device_num(0) device_type(acc_device_nvidia)
   #pragma acc wait
   dt[3]= deltaT();
   printf("\tdt: %G %G %G %G\n", dt[0], dt[1], dt[2], dt[3]);
   #pragma acc update self( pV0[:n] ) async  // data copyout( pV0[:n] )

   #pragma acc set device_num(0) device_type(acc_device_host)
   #pragma acc wait
   dt[4]= deltaT();
   printf("\tdt: %G %G %G %G %G\n", dt[0], dt[1], dt[2], dt[3], dt[4]);
   #pragma acc update self( pV3[:n] ) async  // data copyout( pV3[:n] )
   
   #pragma acc wait_all

   dt[5]= deltaT();
   printf("\tdt: %G %G %G %G %G %G\n", dt[0], dt[1], dt[2], dt[3], dt[4], dt[5]);
   {
      Scalar s[4]= {0,0,0,0};
      for (size_t i= 0; i < n; ++i ) { s[0]+= pV0[i]; s[1]+= pV1[i]; s[2]+= pV2[i]; s[3]+= pV3[i]; }
      printf("\ts: %G %G %G %G\n", s[0], s[1], s[2], s[3]);
   }

   #pragma acc set device_num(0) device_type(acc_device_nvidia)
   #pragma acc exit data delete( pV0[:n], pV1[:n], pV2[:n], k[:2] )

   #pragma acc set device_num(0) device_type(acc_device_host)
   #pragma acc exit data delete( pV3[:n], pV1[:n], pV2[:n], k[:2] )
} // proc

int init (Scalar *pV[], const int d, const size_t n, int flags)
{
   const size_t b= n * sizeof(*pV[0]);
   int i= 0;

   gDC.nH= acc_get_num_devices( acc_device_host );
   gDC.iH= acc_get_device_num( acc_device_host );
   gDC.nNV= acc_get_num_devices( acc_device_nvidia );
   gDC.iNV= acc_get_device_num( acc_device_nvidia );
   gDC.tA= acc_get_device();
   gDC.iA= acc_get_device_num(gDC.tA);

   printf("init() - acc devices H[%d]:%d NV[%d]:%d active=%d:%d\n", gDC.nH, gDC.iH, gDC.nNV, gDC.iNV, gDC.tA, gDC.iA);
   if (gDC.nH > 0) { acc_init( acc_device_host ); }
   if (gDC.nNV > 0) { acc_init( acc_device_nvidia ); }

   i= 0;
   do
   {  // (Scalar*)
      pV[i]= malloc(b);
      if (pV[i]) { memset(pV[i], 0xFF, b); }
   } while (pV[i] && (++i < d));

   if (flags & 0x1) { for ( i= 0; i<d; ++i ) { printf("pV[%d]:%p\n", i, pV[i]); } }   
   return(d == i);
} // init

void cleanup (Scalar *pV[], const int d)
{
   int i= 0;
   do
   {
      if (pV[i]) { free(pV[i]); pV[i]= NULL; }
   } while (pV[i] && (++i < d));

   if (gDC.nH > 0) { acc_shutdown( acc_device_host ); }
   if (gDC.nNV > 0) { acc_shutdown( acc_device_nvidia ); }
} // cleanup

int main( int argc, char* argv[] )
{
   float *pV[4];
   size_t n=0;

   if (argc > 1) { n= atoi(argv[1]); }
   if (0 == n) { n= 1<<20; }

   printf("*****\n");
   if (init(pV, 4, n, 0x1))
   {
      proc(pV[0], pV[1], pV[2], pV[3], n);
   }
   cleanup(pV, 4);
   printf("*****\n");
   return(0);
} // main
