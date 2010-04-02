/*
   File:          counter.h

   Created:       March 29, 1999

   Modified:      December 25, 1999
   
   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      The interface to the counter code.
*/



#ifndef COUNTER_H
#define COUNTER_H



#include "macros.h"



#define INCREMENT_COUNTER( counter )              counter.lo++
#define INCREMENT_COUNTER_BY( counter, term )     counter.lo += (term)



typedef struct {
  unsigned int hi;
  unsigned int lo;
} CounterType;



void
reset_counter( CounterType *counter );

double
counter_value( CounterType *counter );

void
adjust_counter( CounterType *counter );

void
add_counter( CounterType *sum, CounterType *term );



#endif  /* COUNTER_H */
