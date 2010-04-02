/*
   File:          stable.h

   Created:       March 20, 1999

   Modified:      August 1, 2002
   
   Authors:       Gunnar Andersson (gunnar@radagast.se)

   Contents:      Interface to the code which conservatively estimates
                  the number of stable (unflippable) discs.
*/



#ifndef STABLE_H
#define STABLE_H



#include "bitboard.h"



#ifdef __cplusplus
extern "C" {
#endif



extern BitBoard last_black_stable, last_white_stable;



/*
  COUNT_EDGE_STABLE
  Returns the number of stable edge discs for COLOR.
*/

int
count_edge_stable( int color, BitBoard col_bits, BitBoard opp_bits );


/*
  COUNT_STABLE
  Returns the number of stable discs for COLOR.
  Note: COUNT_EDGE_STABLE must have been called immediately
        before this function is called *or you lose big*.
*/

int
count_stable( int color, BitBoard col_bits, BitBoard opp_bits );


/*
  GET_STABLE
  Determines what discs on BOARD are stable with SIDE_TO_MOVE to play next.
  The stability status of all squares (black, white and empty)
  is returned in the boolean vector IS_STABLE.
*/

void
get_stable( int *board,
	    int side_to_move,
	    int *is_stable );

void
init_stable( void );

void
finalize_stable( void );



#ifdef __cplusplus
}
#endif



#endif  /* STABLE_H */
