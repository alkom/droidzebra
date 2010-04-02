/*
   File:          midgame.h

   Created:       July 1, 1998

   Modified:      August 1, 2002

   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      The midgame search driver.
*/



#ifndef MIDGAME_H
#define MIDGAME_H



#include "constant.h"
#include "search.h"



#ifdef __cplusplus
extern "C" {
#endif



#define INFINITE_DEPTH         60

/* The minimum depth to perform Multi-ProbCut */
#define MIN_MPC_DEPTH          9



void
setup_midgame( void );

void
toggle_midgame_hash_usage( int allow_read, int allow_write );

void
clear_midgame_abort( void );

int
is_midgame_abort( void );

void
set_midgame_abort( void );

void
toggle_midgame_abort_check( int toggle );

void
calculate_perturbation( void );

void
set_perturbation( int amplitude );

void
toggle_perturbation_usage( int toggle );

int
tree_search( int level, int max_depth, int side_to_move, int alpha,
	     int beta, int allow_hash, int allow_mpc, int void_legal );

int
middle_game( int side_to_move, int max_depth,
	     int update_evals, EvaluationType *eval_info );



#ifdef __cplusplus
}
#endif



#endif  /* MIDGAME_H  */
