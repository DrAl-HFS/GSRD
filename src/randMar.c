
#include "randMar.h"


// Marsaglia MWC PRNG
uint randMMWC (SeedMMWC *pS)
{
    pS->z = 36969 * (pS->z & 0xFFFF) + (pS->z >> 16);
    pS->w = 18000 * (pS->w & 0xFFFF) + (pS->w >> 16);
    return (pS->z << 16) + pS->w;
} // randMMWC

double randN (RandF *pRF)
{
   size_t u= randMMWC(&(pRF->s));
   return((1+u) * 1.0 / (2 + ((size_t)1 << 32))); // NB: closed interval
} // randN

static uint seedFromID (U16 id)
{
   uint i,n1,n2,t= 0xC5A3;
   //report(VRB1,"getseed()\n");
   for (i=0; i<4; ++i)
   {
      n1= (id & 0x3) * 4;
      id >>= 2;
      n2= 1 + (id & 0x3);
      id >>= 2;
      t= t << (4 + n2);
      t|= ((0xAC53 >> n1) & 0xF) << n2;
      if (i & 1) { t|= (1 << n2)-1; }
      //report(VRB1," [%u] %u %u %u\n", i, n1, n2, t);
   }
   return(t);
} // seedFromID

void initSeedMMWC (SeedMMWC *pS, U16 id)
{
   pS->z= seedFromID(id);
   pS->w= seedFromID(id ^ 0xFFFF);
} // initSeedMMWC

void initRF (RandF *pRF, float scale, float offset, U16 sid)
{
   pRF->f[0]= scale / (((size_t)1 << 32) - 1); // NB: open interval
   pRF->f[1]= offset;
   initSeedMMWC(&(pRF->s),sid);
} // initRF

float randF (RandF *pRF)
{
   return(randMMWC(&(pRF->s)) * pRF->f[0] + pRF->f[1]);
} // randF

void scaleRF (RandF *pRF, float s) { pRF->f[0]*= s; pRF->f[1]*= s; }
