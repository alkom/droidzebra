/*
   File:          search.h

   Created:       July 1, 1997

   Modified:      August 1, 2002

   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      The interface to common search routines and variables.
*/



#ifndef SEARCH_H
#define SEARCH_H



#include "constant.h"
#include "counter.h"
#include "globals.h"



#ifdef __cplusplus
extern "C" {
#endif



#define USE_RANDOMIZATION       TRUE
#define USE_HASH_TABLE          TRUE
#define CHECK_HASH_CODES        FALSE

#define MOVE_ORDER_SIZE         60

#define INFINITE_EVAL           12345678



typedef enum { MIDGAME_EVAL, EXACT_EVAL, WLD_EVAL, SELECTIVE_EVAL,
	       FORCED_EVAL, PASS_EVAL, UNDEFINED_EVAL, INTERRUPTED_EVAL,
               UNINITIALIZED_EVAL } EvalType;
typedef enum { WON_POSITION, DRAWN_POSITION,
	       LOST_POSITION, UNSOLVED_POSITION } EvalResult;

/* All information available about a move decision. */
typedef struct {
  EvalType type;
  EvalResult res;
  int score;              /* For BOOK, MIDGAME and EXACT */
  double confidence;      /* For SELECTIVE */
  int search_depth;       /* For MIDGAME */
  int is_book;
} EvaluationType;



/* The time spent searching during the game. */
extern double total_time;

/* The value of the root position from the last midgame or
   endgame search. Can contain strange values if an event
   occurred. */
extern int root_eval;

/* Event flag which forces the search to abort immediately when set. */
extern int force_return;

/* The number of positions evaluated during the current search. */
extern CounterType evaluations;

/* The number of positions evaluated during the entire game. */
extern CounterType total_evaluations;

/* Holds the number of nodes searched during the current search. */
extern CounterType nodes;

/* Holds the total number of nodes searched during the entire game. */
extern CounterType total_nodes;

/* The last available evaluations for all possible moves at all
   possible game stages. */
extern Board evals[61];

/* Move lists */
extern int sorted_move_order[64][64];  /* 61*60 used */

/* The principal variation including passes */
extern int full_pv_depth;
extern int full_pv[120];

/* JCW's move order */
extern int position_list[100];



void
inherit_move_lists( int stage );

void
reorder_move_list( int stage );

void
setup_search( void );

int
disc_count( int side_to_move );

void
sort_moves( int list_size );

int
select_move( int first, int list_size );

int
float_move( int move, int list_size );

void
store_pv( int *pv_buffer, int *depth_buffer );

void
restore_pv( int *pv_buffer, int depth_buffer );

void
clear_pv( void );

void
set_ponder_move( int move );

void
clear_ponder_move( void );

int
get_ponder_move( void );

EvaluationType
create_eval_info( EvalType in_type, EvalResult in_res, int in_score,
		  double in_conf, int in_depth, int in_book );

double
produce_compact_eval( EvaluationType eval_info );

void
complete_pv( int side_to_move );

void
hash_expand_pv( int side_to_move, int mode, int flags, int max_selectivity );

void
set_current_eval( EvaluationType eval );

EvaluationType
get_current_eval( void );

void
negate_current_eval( int negate );



#ifdef __cplusplus
}
#endif



#endif  /* SEARCH_H */
