/*
   File:              moves.c

   Created:           June 30, 1997

   Modified:          April 24, 2001
   
   Author:            Gunnar Andersson (gunnar@radagast.se)

   Contents:          The move generator.
*/



#include <stdio.h>
#include <stdlib.h>
#include "cntflip.h"
#include "constant.h"
#include "doflip.h"
#include "globals.h"
#include "hash.h"
#include "macros.h"
#include "moves.h"
#include "patterns.h"
#include "search.h"
#include "texts.h"
#include "unflip.h"



/* Global variables */

int disks_played;
int move_count[MAX_SEARCH_DEPTH];
int move_list[MAX_SEARCH_DEPTH][64];
int *first_flip_direction[100];
int flip_direction[100][16];   /* 100 * 9 used */
int **first_flipped_disc[100];
int *flipped_disc[100][8];
const int dir_mask[100] = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,  81,  81,  87,  87,  87,  87,  22,  22,   0,
  0,  81,  81,  87,  87,  87,  87,  22,  22,   0,
  0, 121, 121, 255, 255, 255, 255, 182, 182,   0,
  0, 121, 121, 255, 255, 255, 255, 182, 182,   0,
  0, 121, 121, 255, 255, 255, 255, 182, 182,   0,
  0, 121, 121, 255, 255, 255, 255, 182, 182,   0,
  0,  41,  41, 171, 171, 171, 171, 162, 162,   0,
  0,  41,  41, 171, 171, 171, 171, 162, 162,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};
const int move_offset[8] = { 1, -1, 9, -9, 10, -10, 11, -11 };


/* Local variables */

static int flip_count[65];
static int sweep_status[MAX_SEARCH_DEPTH];



/*
  INIT_MOVES
  Initialize the move generation subsystem.
*/

void
init_moves( void ) {
  int i, j, k;
  int pos;
  int feasible;

  for ( i = 1; i <= 8; i++ )
    for ( j = 1; j <= 8; j++ ) {
      pos = 10 * i + j;
      for ( k = 0; k <= 8; k++ )
	flip_direction[pos][k] = 0;
      feasible = 0;
      for ( k = 0; k < 8; k++ )
	if ( dir_mask[pos] & (1 << k) ) {
	  flip_direction[pos][feasible] = move_offset[k];
	  feasible++;
	}
      first_flip_direction[pos] = &flip_direction[pos][0];
    }
}


/*
   RESET_GENERATION
   Prepare for move generation at a given level in the tree.
*/

INLINE static void
reset_generation( int side_to_move ) {
  sweep_status[disks_played] = 0;
}


/*
   GENERATE_SPECIFIC
*/

INLINE int
generate_specific( int curr_move, int side_to_move ) {
  return AnyFlips_compact( board, curr_move, side_to_move,
			   OPP( side_to_move ) );
}


/*
   GENERATE_MOVE
   side_to_move = the side to generate moves for

   Generate the next move in the ordering. This way not all moves possible
   in a position are generated, only those who need be considered.
*/

INLINE int
generate_move(int side_to_move) {
  int move;
  int move_index = 0;

  move_index = sweep_status[disks_played];
  while ( move_index < MOVE_ORDER_SIZE ) {
    move = sorted_move_order[disks_played][move_index];
    if ( (board[move] == EMPTY) &&
	 generate_specific( move, side_to_move ) ) {
      sweep_status[disks_played] = move_index + 1;
      return move;
    }
    else
      move_index++;
  }

  sweep_status[disks_played] = move_index;
  return ILLEGAL;
}



/*
   GENERATE_ALL
   Generates a list containing all the moves possible in a position.
*/

INLINE void
generate_all( int side_to_move ) {
  int count, curr_move;

  reset_generation( side_to_move );
  count = 0;
  curr_move = generate_move( side_to_move );
  while ( curr_move != ILLEGAL ) {
    move_list[disks_played][count] = curr_move;
    count++;
    curr_move = generate_move( side_to_move );
  }
  move_list[disks_played][count] = ILLEGAL;
  move_count[disks_played] = count;
}



/*
  COUNT_ALL
  Counts the number of moves for one player.
*/

INLINE int
count_all( int side_to_move, int empty ) {
  int move;
  int move_index;
  int mobility;
  int found_empty;

  mobility = 0;
  found_empty = 0;
  for ( move_index = 0; move_index < MOVE_ORDER_SIZE; move_index++ ) {
    move = sorted_move_order[disks_played][move_index];
    if ( board[move] == EMPTY ) {
      if ( generate_specific( move, side_to_move ) )
	mobility++;
      found_empty++;
      if ( found_empty == empty )
	return mobility;
    }
  }

  return mobility;
}


/*
   GAME_IN_PROGRESS
   Determines if any of the players has a valid move.
*/

int
game_in_progress( void ) {
  int black_count, white_count;

  generate_all( BLACKSQ );
  black_count = move_count[disks_played];
  generate_all( WHITESQ );
  white_count = move_count[disks_played];

  return (black_count > 0) || (white_count > 0);
}


/*
   MAKE_MOVE
   side_to_move = the side that is making the move
   move = the position giving the move

   Makes the necessary changes on the board and updates the
   counters.
*/

INLINE int
make_move( int side_to_move, int move, int update_hash ) {
  int flipped;
  unsigned int diff1, diff2;

  if ( update_hash ) {
    flipped = DoFlips_hash( move, side_to_move );
    if ( flipped == 0 )
      return 0;
    diff1 = hash_update1 ^ hash_put_value1[side_to_move][move];
    diff2 = hash_update2 ^ hash_put_value2[side_to_move][move];
    hash_stored1[disks_played] = hash1;
    hash_stored2[disks_played] = hash2;
    hash1 ^= diff1;
    hash2 ^= diff2;
  }
  else {
    flipped = DoFlips_no_hash( move, side_to_move );
    if ( flipped == 0 )
      return 0;
    hash_stored1[disks_played] = hash1;
    hash_stored2[disks_played] = hash2;
  }

  flip_count[disks_played] = flipped;

  board[move] = side_to_move;

  if ( side_to_move == BLACKSQ ) {
    piece_count[BLACKSQ][disks_played + 1] =
      piece_count[BLACKSQ][disks_played] + flipped + 1;
    piece_count[WHITESQ][disks_played + 1] =
      piece_count[WHITESQ][disks_played] - flipped;
  }
  else {  /* side_to_move == WHITESQ */
    piece_count[WHITESQ][disks_played + 1] =
      piece_count[WHITESQ][disks_played] + flipped + 1;
    piece_count[BLACKSQ][disks_played + 1] =
      piece_count[BLACKSQ][disks_played] - flipped;
  }

  disks_played++;

  return flipped;
}


/*
   MAKE_MOVE_NO_HASH
   side_to_move = the side that is making the move
   move = the position giving the move

   Makes the necessary changes on the board. Note that the hash table
   is not updated - the move has to be unmade using UNMAKE_MOVE_NO_HASH().
*/


INLINE int
make_move_no_hash( int side_to_move, int move ) {
  int flipped;

  flipped = DoFlips_no_hash( move, side_to_move );
  if ( flipped == 0 )
    return 0;

  flip_count[disks_played] = flipped;

  board[move] = side_to_move;

#if 1
  if ( side_to_move == BLACKSQ ) {
    piece_count[BLACKSQ][disks_played + 1] =
      piece_count[BLACKSQ][disks_played] + flipped + 1;
    piece_count[WHITESQ][disks_played + 1] =
      piece_count[WHITESQ][disks_played] - flipped;
  }
  else {  /* side_to_move == WHITESQ */
    piece_count[WHITESQ][disks_played + 1] =
      piece_count[WHITESQ][disks_played] + flipped + 1;
    piece_count[BLACKSQ][disks_played + 1] =
      piece_count[BLACKSQ][disks_played] - flipped;
  }
#else
  piece_count[side_to_move][disks_played + 1] =
    piece_count[side_to_move][disks_played] + flipped + 1;
  piece_count[OPP( side_to_move )][disks_played + 1] =
    piece_count[OPP( side_to_move )][disks_played] - flipped;
#endif
  disks_played++;

  return flipped;
}


/*
  UNMAKE_MOVE
  Takes back a move.
*/

INLINE void
unmake_move( int side_to_move, int move ) {
  board[move] = EMPTY;

  disks_played--;

  hash1 = hash_stored1[disks_played];
  hash2 = hash_stored2[disks_played];

  UndoFlips_inlined( flip_count[disks_played], OPP( side_to_move ) );
}


/*
  UNMAKE_MOVE_NO_HASH
  Takes back a move. Only to be called when the move was made without
  updating hash table, preferrable through MAKE_MOVE_NO_HASH().
*/

INLINE void
unmake_move_no_hash( int side_to_move, int move ) {
  board[move] = EMPTY;

  disks_played--;

  UndoFlips_inlined( flip_count[disks_played], OPP( side_to_move ) );
}


/*
   VALID_MOVE
   Determines if a move is legal.
*/

int
valid_move( int move, int side_to_move ) {
  int i, pos, count;

  if ( (move < 11) || (move > 88) || (board[move] != EMPTY) )
    return FALSE;

  for ( i = 0; i < 8; i++ )
    if ( dir_mask[move] & (1 << i) ) {
      for ( pos = move + move_offset[i], count = 0;
	    board[pos] == OPP( side_to_move ); pos += move_offset[i], count++ )
	;
      if ( board[pos] == side_to_move ) {
	if ( count >= 1 )
	  return TRUE;
      }
    }

  return FALSE;
}



/*
   GET_MOVE
   Prompts the user to enter a move and checks if the move is legal.
*/

int
get_move( int side_to_move ) {
  char buffer[255];
  int ready = 0;
  int curr_move;

  while ( !ready ) {
    if ( side_to_move == BLACKSQ )
      printf( "%s: ", BLACK_PROMPT );
    else
      printf( "%s: ", WHITE_PROMPT );
    scanf( "%s", buffer );
    curr_move = atoi( buffer );
    ready = valid_move( curr_move, side_to_move );
    if ( !ready ) {
      curr_move = (buffer[0] - 'a' + 1) + 10 * (buffer[1] - '0');
      ready = valid_move( curr_move, side_to_move );
    }
  }

  return curr_move;
}
