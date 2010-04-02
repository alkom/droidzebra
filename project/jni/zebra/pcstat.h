/*
   pcstat.h

   Automatically created by CORRELAT on Tue Sep 07 19:41:49 1999

   Modified December 25, 1999
*/



#ifndef PCSTAT_H
#define PCSTAT_H



#define MAX_CORR_DEPTH       14


#define MAX_SHALLOW_DEPTH    8


typedef struct {
  float const_base;
  float const_slope;
  float sigma_base;
  float sigma_slope;
} Correlation;


extern Correlation mid_corr[61][MAX_SHALLOW_DEPTH + 1];



#endif  /* PCSTAT_H */
