/*
   File:          counter.c

   Created:       March 29, 1999

   Modified:      June 27, 1999
   
   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      The counter code. The current implementation is
                  capable of representing values up to 2^32 * 10^8,
		  i.e., 429496729600000000, assuming 32-bit integers.
*/



#include <math.h>
#include "counter.h"
#include "macros.h"



#define  DECIMAL_BASIS            100000000



/*
  RESET_COUNTER
*/

INLINE void
reset_counter( CounterType *counter ) {
  counter->lo = 0;
  counter->hi = 0;
}


/*
  ADJUST_COUNTER
  Makes sure that the LO part of the counter only contains 8 decimal digits.
*/

INLINE void
adjust_counter( CounterType *counter ) {
  while ( counter->lo >= DECIMAL_BASIS ) {
    counter->lo -= DECIMAL_BASIS;
    counter->hi++;
  }
}


/*
  COUNTER_VALUE
  Converts a counter to a single floating-point value.
*/

INLINE double
counter_value( CounterType *counter ) {
  adjust_counter( counter );
  return ((double) DECIMAL_BASIS) * counter->hi + counter->lo;
}


/*
  ADD_COUNTER
  Adds the value of the counter TERM to the counter SUM.
*/

INLINE void
add_counter( CounterType *sum, CounterType *term ) {
  sum->lo += term->lo;
  sum->hi += term->hi;
  adjust_counter( sum );
}
