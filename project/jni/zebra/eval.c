/*
   File:           eval.c

   Created:        July 1, 1997

   Modified:       September 22, 1999

   Author:         Gunnar Andersson (gunnar@radagast.se)

   Contents:       Control mechanisms for board evaluation.
                   No knowledge in this module though.
*/



#include <stdio.h>
#include "counter.h"
#include "eval.h"
#include "globals.h"
#include "moves.h"
#include "search.h"



/* Local variables */

static int use_experimental;



/*
   TOGGLE_EXPERIMENTAL
   Toggles usage of novelties in the evaluation function on/off.
*/   

INLINE void
toggle_experimental( int use ) {
  use_experimental = use;
}



/*
  EXPERIMENTAL_EVAL
  Returns 1 if the experimental eval (if there is such) is used,
  0 otherwise.
*/

INLINE int
experimental_eval( void ) {
  return use_experimental;
}



/*
  INIT_EVAL
  Reset the evaluation module.
*/

void
init_eval( void ) {
}



/*
  TERMINAL_EVALUATION
  Evaluates the position when no player has any legal moves.
*/

INLINE int
terminal_evaluation( int side_to_move ) {
  int disc_diff;
  int my_discs, opp_discs;

  INCREMENT_COUNTER( evaluations );

  my_discs = piece_count[side_to_move][disks_played];
  opp_discs = piece_count[OPP( side_to_move )][disks_played];

  if ( my_discs > opp_discs )
    disc_diff = 64 - 2 * opp_discs;
  else if ( opp_discs > my_discs )
    disc_diff = 2 * my_discs - 64;
  else
    disc_diff = 0;

  if ( disc_diff > 0 )
    return +MIDGAME_WIN + disc_diff;
  else if ( disc_diff == 0 )
    return 0;
  else
    return -MIDGAME_WIN + disc_diff;
}
