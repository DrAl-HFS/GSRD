
#include "gsrdUtil.h"


I32 scanEnvID (I32 v[], I32 max, const char *id)
{
   I32 n= 0;
   if (id && (max > 0))
   {
      char *pCh= getenv(id);
      if (pCh && *pCh)
      {
         size_t t;
         int nCh= 0;
         do
         {
            nCh= scanZD(&t, pCh);
            if (nCh > 0)
            {
               pCh+= nCh;
               v[n++]= (int)t;
            }
         } while ((nCh>0) && *pCh && (n < max));
      }
   }
   return(n);
} // scanEnvID

I32 charInSet (const char c, const char set[])
{
   I32 i= 0;
   while (set[i] && (set[i] != c)) { ++i; }
   return(c == set[i]);
} // charInSet
 
I32 skipSet (const char s[], const char set[])
{
   I32 i= 0;
   while (charInSet(s[i], set)) { ++i; }
   return(i);
} // skipSet

I32 skipPastSet (const char str[], const char set[])
{
   I32 i= 0;
   while (str[i] && !charInSet(str[i], set)) { ++i; }
   while (charInSet(str[i], set)) { ++i; }
   return(i);
} // skipPastSet

I32 scanNI32 (I32 v[], const I32 maxV, const char str[], I32 *pNS, const char skip[], const char end[])
{
   I32 s, nS=0, nV= 0;
   if (maxV > 0)
   {
      const char *pE= str;
      do
      {
         v[nV]= strtol(str+nS, &pE, 10);
         s= pE - (str+nS);
         if (s > 0)
         {
            ++nV;
            s+= skipSet(pE,skip);
            nS+= s;
         }
      } while ((nV < maxV) && (s > 0) && str[nS] && !charInSet(str[nS], end));
      if (pNS) { *pNS= nS; }
   }
   return(nV);
} // scanNI32

I32 scanNF32 (F32 v[], const I32 maxV, const char str[], I32 *pNS, const char skip[], const char end[])
{
   I32 s, nS=0, nV= 0;
   if (maxV > 0)
   {
      const char *pE=str;
      do
      {
         v[nV]= strtof(str+nS, &pE);
         s= pE - (str+nS);
         if (s > 0)
         {
            ++nV;
            s+= skipSet(pE,skip);
            nS+= s;
         }
      } while ((nV < maxV) && (s > 0) && str[nS] && !charInSet(str[nS], end));
      //report(VRB1,"scanNF32() %p %d\n", v, nV);
      if (pNS) { *pNS= nS; }
   }
   return(nV);
} // scanNF32

I32 scanTableF32 (F32 v[], I32 maxV, MinMaxI32 *pW, const char str[], I32 *pNS, const I32 maxS)
{
   MinMaxI32 w;
   I32 nV=0, nS=0, s, n, i=0;

   n= scanNF32(v, maxV-nV, str+nS, &s, ",", "\r\n");
   if (n > 0)
   {
      if (s > 0) { nS+= s; }
      w.min= w.max= nV= n;
      while ((nS < maxS) && (nV < maxV) && (s > 0) && (n > 0))
      {
         s= skipPastSet(str+nS,"\r\n");
         if (s > 0) { nS+= s; }
         //if (++i >= 255) { report(VRB1,"scanTableF32() %d [%d]=%s\n", i, maxS-nS, str+nS); }
         if (nS < maxS)
         {
            n= scanNF32(v+nV, maxV-nV, str+nS, &s, ",", "\r\n");
            if (s > 0) { nS+= s; }
            if (n > 0)
            {
               nV+= n;
               w.min= MIN(w.min, n);
               w.max= MAX(w.max, n);
            }
         }
      } 
      //report(VRB1,"scanTableF32() [%d] %G .. %G : %d*(%d,%d) %d\n", nV, v[0], v[nV-1], i, w.min, w.max, maxS-nS);
      if (pNS) { *pNS= nS; }
      if (pW) { *pW= w; }
   }
   return(nV);
} // scanTableF32

F32 linearityF32 (const F32 t[], const I32 n)
{
   F32 r= 0;
   if (n > 1)
   {
      const I32 m= n-1;
      const F32 de= (t[n-1] - t[0]) / m;
      F32 e, f, g, s2=0;

      e= t[0];
      for (I32 i= 1; i < m; ++i)
      {
         e+= de;
         f= e - t[i];
         s2+= f * f;
      }
      g= de * de * (m-1);
      g+= (0 >= g);
      r= 1 - sqrtf(s2 / g);
   }
   return(r);
} // linearityF32

size_t sumNZU (const size_t z[], const size_t n)
{
   size_t t= 0;
   for (size_t i= 0; i<n; i++) { t+= z[i]; }
   return(t);
} // sumNZU

size_t accumNZU (size_t a[], const size_t z[], const size_t n)
{
   size_t t= 0;
   for (size_t i= 0; i<n; i++) { a[i]= t+= z[i]; }
   return(t);
} // accumNZU

F64 scaleFNZU (F64 f[], const size_t z[], const size_t n, const F64 s)
{
   double t= 0;
   for (size_t i= 0; i<n; i++) { t+= f[i]= s * z[i]; }
   return(t);
} // scaleFNZU

#ifdef UTIL_TEST

int utilTest (void)
{
   return(0);
} // utilTest

#endif

#ifdef UTIL_MAIN

int main (int argc, char *argv[])
{
   return utilTest();
} // utilTest

#endif
