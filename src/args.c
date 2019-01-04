
#include "args.h"


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

I32 scanZD (size_t * const pZ, const char s[])
{
   const char *pE=NULL;
   I32 n=0;
   long long int z= strtoll(s, (char**)&pE, 10);
   if (pE && (pE > s))
   {
      if (pZ) { *pZ= z; }
      n= pE - s;
   }
   return(n);
} // scanZD

I32 scanRevZD (size_t * const pZ, const char s[], const I32 e)
{
   I32 i= e;
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

U8 findNSubStrCaseless (U8 r[], const char *s, const char *t[], const U8 nT)
{
   U8 i, nR= 0;
   for (i=0; i<nT; i++)
   {  // NB - strcasestr() omits upper 32 address bits on Centos7 x86_64
      if (0 != strcasestr(s, t[i])) { r[nR++]= i; }
   }
   return(nR);
} // findNSubStrCaseless

void addOutPath (ArgInfo *pAI, const char *pCh, const U8 u)
{
   if (pCh && pCh[0])
   {
      U8 i= pAI->files.nOutPath++;
      if (i < 2)
      {
         const char *tok[]={"raw","rgb"};
         U8 id[2];
         pAI->files.outPath[i]= pCh;
         pAI->files.outType[i]= u & USAGE_MASK;
         if (1 == findNSubStrCaseless(id, pCh, tok, 2)) { pAI->files.outType[i]|= ID_MASK & (ID_FILE_RAW+id[0]); }
         printf("addOutPath() - %s type:%d\n", pAI->files.outPath[i], pAI->files.outType[i]);
      }
   }
} // addOutPath

I32 scanPattern (PatternInfo *pPI, const char s[])
{
   I32 m, n, i= 0;
   m= 0;
   do
   {
      i+= charInSet(s[i],",;: ");
      if (isdigit(s[i]))
      {
         size_t v=0;
         n= scanZD(&v, s+i);
         if ((n > 0) && (v < ((size_t)1 << 32)) && (m < 2)) { pPI->n[m++]= v; }
         i+= n;
      }
   } while (charInSet(s[i],",;: "));
   while (m < 2) { pPI->n[m++]= 0; } 
   //i+= charInSet(s[i],",;: ");
   if (isalpha(s[i]))
   {
      char c= toupper(s[i]);
      switch(c)
      {
         case 'C' :
         case 'P' :
         case 'R' :
         case 'S' :
            pPI->id[0]= c;
            n= 1;
            while (isalpha(s[i+n]))
            {
               if (n < (sizeof(pPI->id)-1)) { pPI->id[n]= toupper(s[i+n]); }
               ++n;
            }
            pPI->id[n]= 0;
            i+= n;
            break;
         default :
            printf("scanPattern() - %s???\n", s+i);
            break;
      }
   }
   m= 0;
   do
   {
      i+= charInSet(s[i],",;: ");
      if (isdigit(s[i]))
      {
         char *pE;
         float f= strtof(s+i, &pE);
         if (pE) { i= pE - s; }
         if (m < 2) { pPI->s[m++]= f; }
      }
   } while (charInSet(s[i],",;: "));
   while (m < 2) { pPI->s[m++]= -1; } 
   return(i);
} // scanPattern

int scanArgs (ArgInfo *pAI, const char * const a[], int nA)
{
   const char *pCh;
   ArgInfo tmpAI;
   int nV= 0, nHE= (nA <= 1);

   //if (nA > 0) { printf("scanArgs() - %s\n", a[0]); }
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

            case 'B' :
               v= toupper( pCh[n] );
               if ('P' == v) { pAI->init.flags&= ~FLAG_INIT_BOUND_REFLECT; }
               if ('R' == v) { pAI->init.flags|= FLAG_INIT_BOUND_REFLECT; }
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
               ++nV;
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
               ++nV;
               break;

            case 'M' :
               pAI->init.flags|= FLAG_INIT_MAP;
               ++nV;
               break;

            case 'N' :
               pAI->files.outName= pCh+n;
               ++nV;
               break;

            case 'X' :
               pAI->files.flags|= FLAG_FILE_XFER;
               addOutPath(pAI, pCh+n, USAGE_XFER);
               ++nV;
               break;

            case 'O' :
               pAI->files.flags|= FLAG_FILE_OUT;
               addOutPath(pAI, pCh+n, USAGE_INITIAL|USAGE_PERIODIC|USAGE_FINAL);
               ++nV;
               break;

            case 'P' :
               scanPattern(&(pAI->init.pattern), pCh+n);
               ++nV;
               break;

            case 'R' :
               pAI->init.flags|= FLAG_INIT_ORG_INTERLEAVED;
               ++nV;
               break;

            case 'V' :
               pAI->param.nV= scanNF32(pAI->param.v, 3, pCh+n, NULL, ",;:", "");
               pAI->param.id[0]= 'B';
               pAI->param.id[1]= 'A';
               pAI->param.id[2]= 'L';
               ++nV;
               break;

            default : printf("scanArgs() - unknown flag -%c\n", c);

            case 'H' :
            case '?' : nHE++;
         }
      }
   }
   if (nHE > 0) { usage(); }
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
   "-A:[H|G|A|N]     Acceleration: Host GPU All None",
   "-B:[R|P]         Boundary: Reflective Periodic",
   "-C:<path>        Comparison file path",
   "-D:w,h           Define dimensions of simulation domain",
   "-I:<max>,<sub>   Iterations max is total run duration, sub is periodic analysis/save interval",
   "-L:<path>        LUT (colour map) applied during rgb conversion of output",
   "-M ?struct_file? Enable Map processing of spatial structure; requires reflective boundary",
   "-N:<prefix>      Name prefix for output files (instead of gsrd..)",
   "-O:<path>        Output file path and inferred type (raw or rgb)",
   "-P:#[P|S|C][R]#  Initial Pattern # number of Point, Square or Circle with optional Randomised biomass of size #",
   "-R               Interleave (versus planar) scalar fields",
   "-V:<#,#,#>       Parameter values for Death, Replenishment and Biomass Diffusion",
   "-X:<path>        Post processing (conversion) file path",
};
   int i, m= sizeof(strtab)/sizeof(strtab[0]);
   printf("Usage:\n");
   for (i= 0; i < m; i++ ) { printf("\t%s\n", strtab[i]); }
   printf("\n");
} // usage
