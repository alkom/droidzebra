/*
   File:          learn.h

   Created:       November 29, 1997
   
   Modified:      November 18, 2001

   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      The interface to the learning module.
*/



#ifndef LEARN_H
#define LEARN_H



#ifdef __cplusplus
extern "C" {
#endif



void
clear_stored_game( void );

void
store_move( int disks_played, int move );

void
set_learning_parameters( int depth, int cutoff );

int
game_learnable( int finished, int move_count );

void
init_learn( const char *file_name, int is_binary );

void
learn_game( int move_count, int private_game, int save_database );

void
full_learn_public_game( int length, int *moves, int cutoff,
			int deviation_depth, int exact, int wld );



#ifdef __cplusplus
}
#endif



#endif  /* LEARN_H */
