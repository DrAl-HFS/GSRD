
#include "util.h"

#define GETTIME(a) gettimeofday(a,NULL)
#define USEC(t1,t2) (((t2).tv_sec-(t1).tv_sec)*1000000+((t2).tv_usec-(t1).tv_usec))

// Replacement for strchr() (not correctly supported by pgi-cc) with extras
const char *sc (const char *s, const char c, const char * const e, const I8 o)
{
   if (s)
   {
      if (e) { while ((s <= e) && (*s != 0) && (*s != c)) { ++s; } }
      else { while ((*s != 0) && (*s != c)) { ++s; } }
      if (*s == c) { return(s+o); }
   }
   return(NULL);
} // sc

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

Bool32 validBuff (const Buffer *pB, size_t b)
{
   return(pB && pB->p && (pB->b >= b));
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

int scanZD (size_t * const pZ, const char s[])
{
   const char *pE=NULL;
   int n=0;
   long long int z= strtoll(s, (char**)&pE, 10);
   if (pE && (pE > s))
   {
      if (pZ) { *pZ= z; }
      n= pE - s;
   }
   return(n);
} // scanZD

int scanRevZD (size_t * const pZ, const char s[], const int e)
{
   int i= e;
   while ((i >= 0) && isdigit(s[i])) { --i; }
   i+= !isdigit(s[i]);
   //printf("scanRevZD() [%d]=%s\n", i, s+i);
   if (i <= e) { return scanZD(pZ, s+i); }
   //else
   return(0);
} // scanRevZD

int scanVI (int v[], const int vMax, ScanSeg * const pSS, const char s[])
{
   ScanSeg ss={0,0};
   const char *pT1, *pT2;
   int nI=0;
   
   pT1= sc(s, '(', NULL, 0);
   pT2= sc(pT1, ')', NULL, 0);
   if (pT1 && pT2 && isdigit(pT1[1]) && isdigit(pT2[-1]))
   {  // include delimiters in segment...
      ss.start= pT1 - s;
      ss.len= pT2 + 1 - pT1;
      // but exclude from further scan
      ++pT1; --pT2;
      do
      {
         const char *pE=NULL;
         v[nI]= strtol(pT1, (char**)&pE, 10);
         if (pE && (pE > pT1)) { nI++; pT1= pE; }
         //v[nI++]= atoi(pT1);
         pT1= sc(pT1, ',', pT2, 1);
      } while (pT1 && (nI < vMax));
   }
   if (pSS) { *pSS= ss; }
   return(nI);
} // scanVI

int charInSet (const char c, const char set[])
{
   int i= 0;
   while (set[i] && (set[i] != c)) { ++i; }
   return(c == set[i]);
} // charInSet
 
int skipSet (const char s[], const char set[])
{
   int i= 0;
   while (charInSet(s[i], set)) { ++i; }
   return(i);
} // skipSet

int skipPastSet (const char str[], const char set[])
{
   int i= 0;
   while (str[i] && !charInSet(str[i], set)) { ++i; }
   while (charInSet(str[i], set)) { ++i; }
   return(i);
} // skipPastSet

int scanNI32 (int v[], const int maxV, const char str[], int *pNS, const char skip[], const char end[])
{
   int s, nS=0, nV= 0;
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

int scanNF32 (float v[], const int maxV, const char str[], int *pNS, const char skip[], const char end[])
{
   int s, nS=0, nV= 0;
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

int scanTableF32 (float v[], int maxV, MinMaxI32 *pW, const char str[], int *pNS, const int maxS)
{
   MinMaxI32 w;
   int nV=0, nS=0, s, n, i=0;

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

U8 scanChZ (const char *s, char c)
{
   if (s)
   {
      c= toupper(c);
      while (*s && (toupper(*s) != c)) { ++s; }
      if (*s)
      {
         size_t z;
         if ((scanZD(&z, s+1) > 0) && (z > 0) && (z <= 64)) { return((U8)z); }
      }
   }
   return(0);
} // scanChZ

size_t scanDFI (DataFileInfo * pDFI, const char * const path)
{
   size_t bytes= fileSize(path);
   if (bytes > 0)
   {
      const char *pCh, *name=  stripPath(path);
      pDFI->filePath=   path;

      pDFI->bytes= bytes;
      pDFI->nV=    scanVI(pDFI->v, 4, &(pDFI->vSS), name);
      if (pDFI->nV > 1) { pDFI->def.x= pDFI->v[0]; pDFI->def.y= pDFI->v[1]; }
      pCh= name + pDFI->vSS.start + pDFI->vSS.len;
      pDFI->elemBits= scanChZ(pCh, 'F');
      if (pDFI->elemBits > 0)
      {
         scanRevZD(&(pDFI->iter), name, pDFI->vSS.start-1);
      }
      if (pDFI->nV > 0)
      {
         size_t bits= pDFI->elemBits;
         printf("%s -> v[%d]=(", name, pDFI->nV);
         for (int i=0; i < pDFI->nV; i++)
         {
            bits*= pDFI->v[i];
            printf("%d,", pDFI->v[i]);
         }
         printf(")*%u ", pDFI->elemBits);
         if (pDFI->bytes == ((bits+7) >> 3)) { pDFI->flags|= DFI_FLAG_VALIDATED; printf("OK\n"); }
         if (0 == (pDFI->flags & DFI_FLAG_VALIDATED)) { printf("WARNING: scanDFI() %zuBytes in file, expected %zubits\n", pDFI->bytes, bits); }
      }
   }
   return(bytes);
} // scanDFI

int scanEnvID (int v[], int max, const char *id)
{
   int n= 0;
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

int contigCharSetN (const char s[], const int maxS, const char t[], const int maxT)
{
   int n= 0;
   while ((n < maxS) && s[n])
   {
      int i= 0;
      while ((i < maxT) && t[i] && (s[n] != t[i])) { ++i; }
      if (s[n] != t[i]) { return(n); }
      // else
      ++n;
   }
   return(n);
} // contigCharSetN

int scanArgs (ArgInfo *pAI, const char * const a[], int nA)
{
   const char *pCh;
   ArgInfo tmpAI;
   int nV= 0;

   if (NULL == pAI) { pAI= &tmpAI; }
   while (nA-- > 0)
   {
      pCh= a[nA];
      if ('-' != pCh[0]) { nV+= ( scanDFI(&(pAI->files.init), pCh) > 0 ); }
      else
      {
         char c, v=0;
         int n= contigCharSetN(pCh+0, 2, "-", 2);
         c= pCh[n++];
         n+= contigCharSetN(pCh+n, 2, ":=", 2);
         switch(toupper(c))
         {
            case 'A' :
               v= toupper( pCh[n] );
               if ('H' == v) { pAI->proc.flags|= PROC_FLAG_ACCHOST; }
               if ('G' == v) { pAI->proc.flags|= PROC_FLAG_ACCGPU; }
               if ('A' == v) { pAI->proc.flags|= PROC_FLAG_ACCHOST|PROC_FLAG_ACCGPU; }
               if ('N' == v) { pAI->proc.flags&= ~(PROC_FLAG_ACCHOST|PROC_FLAG_ACCGPU); }
               ++nV;
               break;

            case 'C' : nV+= ( scanDFI(&(pAI->files.cmp), pCh+n) > 0 );
               //printf("cmp:iter=%zu\n", pAI->files.cmp.iter);
               break;

            case 'D' :
               // int v[2]
               //pAI->init.nD= scanNI32(v, 2, pCh+n);
               {  long l;
                  const char *pE=NULL;
                  //nV= scanVI(v, 2, NULL, pCh+n);
                  l= strtol(pCh+n, &pE, 10);
                  if ((l > 0) && (l <= (1<<14)))
                  { 
                     pAI->init.def.x= pAI->init.def.y= l;
                     if (pE && *pE)
                     {
                        pE= sc(pE,',',NULL,1);
                        if (pE)
                        {
                           l= strtol(pE, &pE, 10);
                           if ((l > 0) && (l <= (1<<14))) { pAI->init.def.y= l; }
                        }
                     }
                     pAI->init.nD= 2;
                  }
                  //printf("DEF:%s -> %d, %u %u\n", pCh+n, l, pAI->init.def.x, pAI->init.def.y);
               }
               break;

            case 'I' :
               n+= scanZD(&(pAI->proc.maxIter), pCh+n);
               n+= contigCharSetN(pCh+n, 2, ",;:", 3);
               n+= scanZD(&(pAI->proc.subIter), pCh+n);
               ++nV;
               break;

            case 'L' :
               pAI->files.flags|= FLAG_FILE_LUT;
               pAI->files.lutPath= pCh+n;
               break;
           
            case 'O' :
               pAI->files.flags|= FLAG_FILE_OUT;
               if (pCh[n])
               {
                  U8 i= pAI->files.nOutPath++;
                  if (i < 2)
                  {
                     pAI->files.outPath[i]= pCh+n;
                     if (0 == strncmp("raw",pCh+n,3)) { pAI->files.outType[i]= 2; }
                     else if (0 == strncmp("rgb",pCh+n,3)) { pAI->files.outType[i]= 3; }
                     else { pAI->files.outType[i]= 0xFF; }
                  }
               }
               break;

            case 'P' :
               {
                  long l= strtol(pCh+n, NULL, 10);
                  if (l >= 0) { pAI->init.patternID= l; }
               }
               break;

            case 'R' :
               pAI->init.flags|= FLAG_INIT_ORG_INTERLEAVED;
               break;

            case 'V' :
               pAI->param.nV= scanNF32(pAI->param.v, 3, pCh+n, NULL, ",;:", "");
               pAI->param.id[0]= 'B';
               pAI->param.id[1]= 'A';
               pAI->param.id[2]= 'L';
               break;

            default : printf("scanArgs() - unknown flag -%c\n", c);
         }
      }
   }
   //if (0 == pAI->proc.flags) { pAI->proc.flags= PROC_FLAG_HOST|PROC_FLAG_GPU; }
   if (0 == pAI->proc.maxIter) { pAI->proc.maxIter= 5000; }
   if (0 == pAI->proc.subIter) { pAI->proc.subIter= 1000; }
   if (pAI->proc.subIter > pAI->proc.maxIter) SWAP(size_t, pAI->proc.subIter, pAI->proc.maxIter);
   return(nV);
} // scanArgs

void usage (void)
{
static char *strtab[]=
{
   "-A:<opt>         Acceleration - specify <opt> H G A N for Host GPU All None",
   "-C:<path>        Comparison file path",
   "-D:w,h           Define dimensions of simulation domain",
   "-I:<max>,<sub>   Iterations total run duration and save interval",
   "-L:<path>        LUT (colour map) for rgb output",
   "-O:<path>        Output file path and inferred type",
   "-P:<id>          Initial Pattern id",
   "-R               Interleave (versus planar) scalar fields",
   "-V:<>            Parameter values"
};
   int i, m= sizeof(strtab)/sizeof(strtab[0]);
   for (i= 0; i < m; i++ ) { printf("%s", strtab[i]); }
} // usage

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
   //printf("getseed()\n");
   for (i=0; i<4; ++i)
   {
      n1= (id & 0x3) * 4;
      id >>= 2;
      n2= 1 + (id & 0x3);
      id >>= 2;
      t= t << (4 + n2);
      t|= ((0xAC53 >> n1) & 0xF) << n2;
      if (i & 1) { t|= (1 << n2)-1; }
      //printf(" [%u] %u %u %u\n", i, n1, n2, t);
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
