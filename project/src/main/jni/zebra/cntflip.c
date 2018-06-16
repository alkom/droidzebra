/*
   cntflip.c

   Last modified:     November 1, 2000
*/



#include <stdio.h>
#include <stdlib.h>
#include "cntflip.h"
#include "constant.h"
#include "error.h"
#include "macros.h"
#include "moves.h"
#include "texts.h"



INLINE static int
AnyDrctnlFlips( int *sq, int inc, int color, int oppcol ) {
  int *pt = sq + inc;

  if ( *pt == oppcol ) {
    pt += inc;
    if ( *pt == oppcol ) {
      pt += inc;
      if ( *pt == oppcol ) {
	pt += inc;
	if ( *pt == oppcol ) {
	  pt += inc;
	  if ( *pt == oppcol ) {
	    pt += inc;
	    if ( *pt == oppcol )
	      pt += inc;
	  }
	}
      }
    }
    if ( *pt == color )
      return TRUE;
  }

  return FALSE;
}


int
AnyFlips_compact( int *board, int sqnum, int color, int oppcol ) {
  int *sq;
  int *inc;

  sq = &board[sqnum];
  inc = first_flip_direction[sqnum];
  do {
    if ( AnyDrctnlFlips( sq, *inc, color, oppcol ) )
      return TRUE;
    inc++;
  } while ( *inc );

  return FALSE;
}
