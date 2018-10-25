
#include "util.h"

#define GETTIME(a) gettimeofday(a,NULL)
#define USEC(t1,t2) (((t2).tv_sec-(t1).tv_sec)*1000000+((t2).tv_usec-(t1).tv_usec))

const char *stripPath (const char *path)
{
   if (path && *path)
   {
      const char *t= path, *l= path;
      do
      {
         path= t;
         while (('\\' == *t) || ('/' == *t) || (':' == *t)) { ++t; }
         if (t > path) { l= t; }
         t += (0 != *t);
      } while (*t);
      return(l);
   }
   return(NULL);
} // stripPath

// const char *extractPath (char s[], int m, const char *pfe) {} // extractPath

Bool32 validBuff (const MemBuff *pB, size_t b)
{
   return(pB && pB->p && (pB->bytes >= b));
} // validBuff

size_t fileSize (const char * const path)
{
   if (path && path[0])
   {
      struct stat sb={0};
      int r= stat(path, &sb);
      if (0 == r) { return(sb.st_size); }
   }
   return(0);
} // fileSize

size_t loadBuff (void * const pB, const char * const path, const size_t bytes)
{
   FILE *hF= fopen(path,"r");
   if (hF)
   {
      size_t r= fread(pB, 1, bytes, hF);
      fclose(hF);
      if (r == bytes) { return(r); }
   }
   return(0);
} // loadBuff

size_t saveBuff (const void * const pB, const char * const path, const size_t bytes)
{
   FILE *hF= fopen(path,"w");
   if (hF)
   {
      size_t r= fwrite(pB, 1, bytes, hF);
      fclose(hF);
      if (r == bytes) { return(r); }
   }
   return(0);
} // saveBuff

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

U32 statGetRes1 (StatRes1 * const pR, const StatMom * const pS, const SMVal dof)
{
   StatRes1 r={ 0, 0 };
   U32 o= 0;
   if (pS && (pS->m[0] > 0))
   {
      r.m= pS->m[1] / pS->m[0];
      ++o;
      if (pS->m[0] != dof)
      { 
         //SMVal ess= (pS->m[1] * pS->m[1]) / pS->m[0];
         r.v= ( pS->m[2] - (pS->m[1] * r.m) ) / (pS->m[0] - dof);
         ++o;
      }
   }
   if (pR) { *pR= r; }
   return(o);
} // statGetRes1

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
      //printf("scanNF32() %p %d\n", v, nV);
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
         //if (++i >= 255) { printf("scanTableF32() %d [%d]=%s\n", i, maxS-nS, str+nS); }
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
      //printf("scanTableF32() [%d] %G .. %G : %d*(%d,%d) %d\n", nV, v[0], v[nV-1], i, w.min, w.max, maxS-nS);
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
