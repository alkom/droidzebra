/*
   File:           eval.h

   Created:        July 1, 1997

   Modified:       September 15, 2001

   Author:         Gunnar Andersson (gunnar@radagast.se)

   Contents:       The interface to the evaluation function.
*/



#ifndef EVAL_H
#define EVAL_H



#include "search.h"



/* An evaluation indicating a won midgame position where no
   player has any moves available. */

#define MIDGAME_WIN           29000

/* An eval so high it must have originated from a midgame win
   disturbed by some randomness. */
#define ALMOST_MIDGAME_WIN    (MIDGAME_WIN - 4000)



void
toggle_experimental( int use );

int
experimental_eval( void );

void
init_eval( void );

int
static_evaluation( int side_to_move );

#define static_evaluation( side_to_move )        \
  ( INCREMENT_COUNTER( evaluations ) ,           \
    pattern_evaluation( side_to_move ) )

int
terminal_evaluation( int side_to_move );



#endif  /* EVAL_H */
