/*
   File:           moves.h

   Created:        June 30, 1997

   Modified:       August 1, 2002

   Author:         Gunnar Andersson (gunnar@radagast.se)

   Contents:       The move generator's interface.
*/



#ifndef MOVES_H
#define MOVES_H



#include "constant.h"



#ifdef __cplusplus
extern "C" {
#endif



/* The number of disks played from the initial position.
   Must match the current status of the BOARD variable. */
extern int disks_played;

/* Holds the last move made on the board for each different
   game stage. */
extern int last_move[65];

/* The number of moves available after a certain number
   of disks played. */
extern int move_count[MAX_SEARCH_DEPTH];

/* The actual moves available after a certain number of
   disks played. */
extern int move_list[MAX_SEARCH_DEPTH][64];

/* Directional flip masks for all board positions. */
extern const int dir_mask[100];

/* Increments corresponding to the different flip directions */
extern const int move_offset[8];

/* The directions in which there exist valid moves. If there are N such
   directions for a square, the Nth element of the list is 0. */
extern int flip_direction[100][16];

/* Pointers to FLIPDIRECTION[][0]. */
extern int *first_flip_direction[100];


void
bitboard_move_generation( int side_to_move );

void
init_moves( void );

int
generate_specific( int curr_move, int side_to_move );

int
generate_move( int side_to_move );

void
generate_all( int side_to_move );

int
count_all( int side_to_move, int empty );

int
game_in_progress( void );

int
make_move( int side_to_move, int move, int update_hash );

int
make_move_no_hash( int side_to_move, int move );

void
unmake_move( int side_to_move, int move );

void
unmake_move_no_hash( int side_to_move, int move );

int
valid_move( int move, int side_to_move );

int
get_move( int side_to_move );



#ifdef __cplusplus
}
#endif



#endif  /* MOVES_H */
