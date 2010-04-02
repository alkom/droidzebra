/*
   File:          end.h

   Created:       June 25, 1997

   Modified:      November 24, 2005

   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      The interface to the endgame solver.
*/



#ifndef END_H
#define END_H



#include "search.h"



#define END_MOVE_LIST_HEAD        0
#define END_MOVE_LIST_TAIL        99



typedef struct  {
  int pred;
  int succ;
} MoveLink;


extern MoveLink end_move_list[100];
extern const unsigned int quadrant_mask[100];



int
end_game( int side_to_move,
	  int wld,
	  int force_echo,
	  int allow_book,
	  int komi,
	  EvaluationType *eval_info );

void
set_output_mode( int full );

void
setup_end( void );

int
get_earliest_wld_solve( void );

int
get_earliest_full_solve( void );



#endif  /* END_H */
