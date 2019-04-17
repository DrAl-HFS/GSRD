// report.h - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb 2018 - April 2019

#ifndef REPORT_H
#define REPORT_H

#include "util.h"

#define OUT0 0
#define VRB1 1
#define VRB3 3
#define WRN4 4
#define WRN5 5
#define ERR6 6
#define ERR7 7

/***/

extern int report (U8 id, const char fmt[], ...);

extern int reportN (U8 id, const char *a[], int n, const char start[], const char sep[], const char *end);

#endif // REPORT_H
