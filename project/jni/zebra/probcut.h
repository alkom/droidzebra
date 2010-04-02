/*
   File:          probcut.h

   Created:       March 1, 1998

   Modified:      November 23, 2002
   
   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      The declaration of the Multi-Prob-Cut variables.
*/



#ifndef PROBCUT_H
#define PROBCUT_H



#include "epcstat.h"
#include "pcstat.h"



#define PERCENTILE                 1.0

#define MAX_CUT_DEPTH              22



typedef struct {
  int cut_tries;
  int cut_depth[2];
  int bias[2][61];
  int window[2][61];
} DepthInfo;



extern int use_end_cut[61];
extern int end_mpc_depth[61][4];
extern DepthInfo mpc_cut[MAX_CUT_DEPTH + 1];



void
init_probcut( void );



#endif  /* PROBCUT_H */
