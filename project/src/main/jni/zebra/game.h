/*
   File:          game.h

   Created:       September 20, 1997
   
   Modified:      December 31, 2002

   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      The interface to the game routines.
*/



#ifndef GAME_H
#define GAME_H



#include "search.h"



#ifdef __cplusplus
extern "C" {
#endif



typedef struct {
  EvaluationType eval;
  int side_to_move;
  int move;
  int pv_depth;
  int pv[60];
} EvaluatedMove;



void
toggle_status_log( int write_log );

void
global_setup( int use_random,
	      int hash_bits );

void
global_terminate( void );

void
game_init( const char *file_name,
	   int *side_to_move );

void
clear_endgame_performed( void );

void
set_komi( int in_komi );

void
toggle_human_openings( int toggle );

void
toggle_thor_match_openings( int toggle );

void
set_forced_opening( const char *opening_str );

void
ponder_move( int side_to_move,
	     int book,
	     int mid,
	     int exact,
	     int wld );

int
extended_compute_move( int side_to_move,
		       int book_only,
		       int book,
		       int mid,
		       int exact,
		       int wld );

void
perform_extended_solve( int side_to_move,
			int actual_move,
			int book,
			int exact_solve );

int
get_evaluated_count( void );

EvaluatedMove
get_evaluated( int index );

void
clear_evaluated( void );

int
compute_move( int side_to_move,
	      int update_all,
	      int my_time,
	      int my_incr,
	      int timed_depth,
	      int book,
	      int mid,
	      int exact,
	      int wld,
	      int search_forced,
	      EvaluationType *eval_info );

void
get_search_statistics( int *max_depth,
		       double *node_count );

int
get_pv( int *destin );


#ifdef __cplusplus
}
#endif



#endif  /* GAME_H */
