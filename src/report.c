// report.c - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors  Feb 2018 - April 2019


#include "report.h"
#include <stdarg.h>

typedef struct
{
   U8 filterMask;
   U8 pad[7];
} ReportCtx;

static ReportCtx gRC={0xFF,0};


/***/

static const char * getPrefix (U8 id)
{
   if (id >= 4)
   {
      if (id <= 5) { return("WARNING: "); }
      if (id <= 7) { return("ERROR: "); }
   }
   return(NULL);
} // getPrefix

int report (U8 id, const char fmt[], ...)
{
   int r=0;
   va_list args;
   // NB: stdout & stderr are supposed to be macros (Cxx spec.) and so might
   // create safety/portability issues if not handled in the obvious way...
   if (0 == id)
   {
      va_start(args,fmt);
      r= vfprintf(stdout, fmt, args);
      va_end(args);
   }
   else if (id < 8)
   {
      if (gRC.filterMask & (1<<id))
      {
         const char *p= getPrefix(id);
         if (p) { fprintf(stderr, "%s", p); }
         va_start(args,fmt);
         r= vfprintf(stderr, fmt, args);
         va_end(args);
      }
   }
   //else user log/result files?
   return(r);
} // report

int reportN (U8 id, const char *a[], int n, const char start[], const char sep[], const char *end)
{
   int i,r=0;
   if ((0 == id) && (n > 0))
   {
      r= fprintf(stdout, "%s%s", start, a[0]);
      for (i=1; i<n; i++) { r+= fprintf(stdout, "%s%s", sep, a[i]); }
      if (end) { r+= fprintf(stdout, "%s", end); }
   }
   return(r);
} // reportN


