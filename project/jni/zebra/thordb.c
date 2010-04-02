/*
   File:          thordb.c

   Created:       March 30, 1999
   
   Modified:      October 6, 2002

   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      An interface to the Thor database designed to
                  perform fast lookup operations.
*/



#include "porting.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitboard.h"
#include "constant.h"
#include "error.h"
#include "macros.h"
#include "moves.h"
#include "myrandom.h"
#include "patterns.h"
#include "safemem.h"
#include "texts.h"
#include "thordb.h"



#include "thorop.c"



/* This constant denotes "No stored patterns are available". */
#define  NOT_AVAILABLE               -1

/* The index in the player database which corresponds to an unknown player */
#define  UNKNOWN_PLAYER              0

/* The possible statuses for an opening when matched with a position */
#define  OPENING_INCONCLUSIVE        0
#define  OPENING_MATCH               1
#define  OPENING_MISMATCH            2

/* The default and maximum number of criteria to use when sorting
   the set of matching games. */
#define  DEFAULT_SORT_CRITERIA       5

#define  MAX_SORT_CRITERIA           10



/* Type definitions */


typedef char int_8;

typedef short int_16;

typedef int int_32;

typedef struct {
  int creation_century;
  int creation_year;
  int creation_month;
  int creation_day;
  int game_count;
  int item_count;
  int origin_year;
  int reserved;
} PrologType;

typedef struct {
  int lex_order;
  int selected;
  const char *name;
} TournamentType;

typedef struct {
  PrologType prolog;
  char *name_buffer;
  int count;
  TournamentType *tournament_list;
} TournamentDatabaseType;

typedef struct {
  int lex_order;
  int is_program;
  int selected;
  const char *name;
} PlayerType;

typedef struct {
  PrologType prolog;
  char *name_buffer;
  int count;
  PlayerType *player_list;
} PlayerDatabaseType;

typedef char MoveListType[64];

typedef struct ThorOpeningNode_ {
  unsigned int hash1, hash2;
  int current_match;
  int frequency;
  int matching_symmetry;
  char child_move;
  char sibling_move;
  struct ThorOpeningNode_ *child_node;
  struct ThorOpeningNode_ *sibling_node;
  struct ThorOpeningNode_ *parent_node;
} ThorOpeningNode;

typedef struct {
  short tournament_no;
  short black_no;
  short white_no;
  short actual_black_score;
  short perfect_black_score;
  char moves[60];
  short move_count;
  char black_disc_count[61];
  ThorOpeningNode *opening;
  struct DatabaseType_ *database;
  unsigned int shape_hi, shape_lo;
  short shape_state_hi, shape_state_lo;
  unsigned int corner_descriptor;
  int sort_order;
  short matching_symmetry;
  short passes_filter;
} GameType;

typedef struct DatabaseType_ {
  PrologType prolog;
  GameType *games;
  int count;
  struct DatabaseType_ *next;
} DatabaseType;

typedef struct {
  double average_black_score;
  double next_move_score[100];
  int match_count;
  int black_wins;
  int draws;
  int white_wins;
  int median_black_score;
  int allocation;
  int next_move_frequency[100];
  GameType **match_list;
} SearchResultType;

typedef struct {
  int game_categories;
  int first_year;
  int last_year;
  PlayerFilterType player_filter;
} FilterType;



/* Local variables */

static int thor_game_count, thor_database_count;
static int thor_side_to_move;
static int thor_sort_criteria_count;
static int thor_games_sorted;
static int thor_games_filtered;
static int thor_row_pattern[8], thor_col_pattern[8];
static int thor_board[100];
static int b1_b1_map[100], g1_b1_map[100], g8_b1_map[100], b8_b1_map[100];
static int a2_b1_map[100], a7_b1_map[100], h7_b1_map[100], h2_b1_map[100];
static unsigned int primary_hash[8][6561], secondary_hash[8][6561];
static int *symmetry_map[8], *inv_symmetry_map[8];
static unsigned int move_mask_hi[100], move_mask_lo[100];
static unsigned int unmove_mask_hi[100], unmove_mask_lo[100];
static DatabaseType *database_head;
static PlayerDatabaseType players;
static SearchResultType thor_search;
static TournamentDatabaseType tournaments;
static ThorOpeningNode *root_node;
static int default_sort_order[DEFAULT_SORT_CRITERIA] = {
  SORT_BY_BLACK_NAME,
  SORT_BY_WHITE_NAME,
  SORT_BY_YEAR_REVERSED,
  SORT_BY_BLACK_SCORE,
  SORT_BY_TOURNAMENT
};
static int thor_sort_order[MAX_SORT_CRITERIA];
static FilterType filter;



/*
  CLEAR_THOR_BOARD
*/

static void
clear_thor_board( void ) {
  int pos;

  for ( pos = 11; pos <= 88; pos++ )
    thor_board[pos] = EMPTY;
  thor_board[45] = thor_board[54] = BLACKSQ;
  thor_board[44] = thor_board[55] = WHITESQ;
}


/*
  PREPARE_THOR_BOARD
  Mark the positions outside the board as OUTSIDE.
*/

static void
prepare_thor_board( void ) {
  int i, j;
  int pos;

  for ( i = 0; i < 10; i++ )
    for ( j = 0, pos = 10 * i; j < 10; j++, pos++ )
      if ( (i == 0) || (i == 9) || (j == 0) || (j == 9) )
	thor_board[pos] = OUTSIDE;
}


/*
  DIRECTIONAL_FLIP_COUNT
  Count the number of discs flipped in the direction given by INC
  when SQ is played by COLOR and flip those discs.
*/

INLINE static int
directional_flip_count( int sq, int inc, int color, int oppcol ) {
  int count = 1;
  int pt = sq + inc;

  if ( thor_board[pt] == oppcol) {
    pt += inc;
    if ( thor_board[pt] == oppcol ) {
      count++;
      pt += inc;
      if ( thor_board[pt] == oppcol ) {
	count++;
	pt += inc;
	if ( thor_board[pt] == oppcol ) {
	  count++;
	  pt += inc;
	  if ( thor_board[pt] == oppcol ) {
	    count++;
	    pt += inc;
	    if ( thor_board[pt] == oppcol ) {
	      count++;
	      pt += inc;
	    }
	  }
	}
      }
    }
    if ( thor_board[pt] == color ) {
      int g = count;
      do {
	pt -= inc;
	thor_board[pt] = color;
      } while( --g );
      return count;
    }
  }
  return 0;
}


/*
  DIRECTIONAL_FLIP_ANY
  Returns 1 if SQ is feasible for COLOR in the direction given by INC
  and flip the discs which are flipped if SQ is played.
*/

INLINE static int
directional_flip_any( int sq, int inc, int color, int oppcol ) {
  int pt = sq + inc;

  if ( thor_board[pt] == oppcol) {
    pt += inc;
    if ( thor_board[pt] == oppcol ) {
      pt += inc;
      if ( thor_board[pt] == oppcol ) {
	pt += inc;
	if ( thor_board[pt] == oppcol ) {
	  pt += inc;
	  if ( thor_board[pt] == oppcol ) {
	    pt += inc;
	    if ( thor_board[pt] == oppcol ) {
	      pt += inc;
	    }
	  }
	}
      }
    }
    if ( thor_board[pt] == color ) {
      pt -= inc;
      do {
	thor_board[pt] = color;
	pt -= inc;
      } while ( pt != sq );
      return TRUE;
    }
  }
  return FALSE;
}


/*
  COUNT_FLIPS
  Returns the number of discs flipped if SQNUM is played by COLOR
  and flips those discs (if there are any).
*/

INLINE static int
count_flips( int sqnum, int color, int oppcol ) {
  int count;
  int mask;

  count = 0;
  mask = dir_mask[sqnum];
  if ( mask & 128 )
    count += directional_flip_count( sqnum, -11, color, oppcol );
  if ( mask & 64 )
    count += directional_flip_count( sqnum, 11, color, oppcol );
  if ( mask & 32 )
    count += directional_flip_count( sqnum, -10, color, oppcol );
  if ( mask & 16 )
    count += directional_flip_count( sqnum, 10, color, oppcol );
  if ( mask & 8 )
    count += directional_flip_count( sqnum, -9, color, oppcol );
  if ( mask & 4 )
    count += directional_flip_count( sqnum, 9, color, oppcol );
  if ( mask & 2 )
    count += directional_flip_count( sqnum, -1, color, oppcol );
  if ( mask & 1 )
    count += directional_flip_count( sqnum, 1, color, oppcol );

  return count;
}


/*
  ANY_FLIPS
  Returns 1 if SQNUM flips any discs for COLOR, otherwise 0, and
  flips those discs.
*/

INLINE static int
any_flips( int sqnum, int color, int oppcol ) {
  int count;
  int mask;

  count = FALSE;
  mask = dir_mask[sqnum];
  if ( mask & 128 )
    count |= directional_flip_any( sqnum, -11, color, oppcol );
  if ( mask & 64 )
    count |= directional_flip_any( sqnum, 11, color, oppcol );
  if ( mask & 32 )
    count |= directional_flip_any( sqnum, -10, color, oppcol );
  if ( mask & 16 )
    count |= directional_flip_any( sqnum, 10, color, oppcol );
  if ( mask & 8 )
    count |= directional_flip_any( sqnum, -9, color, oppcol );
  if ( mask & 4 )
    count |= directional_flip_any( sqnum, 9, color, oppcol );
  if ( mask & 2 )
    count |= directional_flip_any( sqnum, -1, color, oppcol );
  if ( mask & 1 )
    count |= directional_flip_any( sqnum, 1, color, oppcol );

  return count;
}


/*
  COMPUTE_THOR_PATTERNS
  Computes the row and column patterns.

*/

INLINE static void
compute_thor_patterns( int *in_board ) {
  int i, j;
  int pos;

  for ( i = 0; i < 8; i++ ) {
    thor_row_pattern[i] = 0;
    thor_col_pattern[i] = 0;
  }

  for ( i = 0; i < 8; i++ )
    for ( j = 0, pos = 10 * i + 11; j < 8; j++, pos++ ) {
      thor_row_pattern[i] += pow3[j] * in_board[pos];
      thor_col_pattern[j] += pow3[i] * in_board[pos];
    }
}


/*
  GET_CORNER_MASK
  Returns an 32-bit mask for the corner configuration. The rotation
  which minimizes the numerical value is chosen.
  The mask is to be interpreted as follows: There are two bits
  for each corner; 00 means empty, 01 means black and 10 means white.
  The bit blocks are given in the order h8h1a8a1 (MSB to LSB).
  Furthermore, this 8-bit value is put in the leftmost byte if
  all four corners have been played, in the rightmost byte if only
  one corner has been played (obvious generalization for one or two
  corners).
*/

static unsigned int
get_corner_mask( int disc_a1, int disc_a8, int disc_h1, int disc_h8 ) {
  int i;
  int count;
  int mask_a1, mask_a8, mask_h1, mask_h8;
  unsigned out_mask;
  unsigned int config[8];

  mask_a1 = 0;
  if ( disc_a1 == BLACKSQ )
    mask_a1 = 1;
  else if ( disc_a1 == WHITESQ )
    mask_a1 = 2;

  mask_a8 = 0;
  if ( disc_a8 == BLACKSQ )
    mask_a8 = 1;
  else if ( disc_a8 == WHITESQ )
    mask_a8 = 2;

  mask_h1 = 0;
  if ( disc_h1 == BLACKSQ )
    mask_h1 = 1;
  else if ( disc_h1 == WHITESQ )
    mask_h1 = 2;

  mask_h8 = 0;
  if ( disc_h8 == BLACKSQ )
    mask_h8 = 1;
  else if ( disc_h8 == WHITESQ )
    mask_h8 = 2;

  count = 0;
  if ( disc_a1 != EMPTY )
    count++;
  if ( disc_a8 != EMPTY )
    count++;
  if ( disc_h1 != EMPTY )
    count++;
  if ( disc_h8 != EMPTY )
    count++;

  if ( count == 0 )
    return 0;

  config[0] = mask_a1 + 4 * mask_a8 + 16 * mask_h1 + 64 * mask_h8;
  config[1] = mask_a1 + 4 * mask_h1 + 16 * mask_a8 + 64 * mask_h8;
  config[2] = mask_a8 + 4 * mask_a1 + 16 * mask_h8 + 64 * mask_h1;
  config[3] = mask_a8 + 4 * mask_h8 + 16 * mask_a1 + 64 * mask_h1;
  config[4] = mask_h1 + 4 * mask_h8 + 16 * mask_a1 + 64 * mask_a8;
  config[5] = mask_h1 + 4 * mask_a1 + 16 * mask_h8 + 64 * mask_a8;
  config[6] = mask_h8 + 4 * mask_h1 + 16 * mask_a8 + 64 * mask_a1;
  config[7] = mask_h8 + 4 * mask_a8 + 16 * mask_h1 + 64 * mask_a1;

  out_mask = config[0];
  for ( i = 1; i < 8; i++ )
    out_mask = MIN( out_mask, config[i] );

  return out_mask << (8 * (count - 1));
}


/*
  PLAY_THROUGH_GAME
  Play the MAX_MOVES first moves of GAME and update THOR_BOARD
  and THOR_SIDE_TO_MOVE to represent the position after those moves.
*/

static int
play_through_game( GameType *game, int max_moves ) {
  int i;
  int move;
  int flipped;

  clear_thor_board();
  thor_side_to_move = BLACKSQ;

  for ( i = 0; i < max_moves; i++ ) {
    move = abs( game->moves[i] );
    flipped = any_flips( move, thor_side_to_move, OPP( thor_side_to_move ) );
    if ( flipped ) {
      thor_board[move] = thor_side_to_move;
      thor_side_to_move = OPP( thor_side_to_move );
    }
    else {
      thor_side_to_move = OPP( thor_side_to_move );
      flipped = any_flips( move, thor_side_to_move, OPP( thor_side_to_move ) );
      if ( flipped ) {
	thor_board[move] = thor_side_to_move;
	thor_side_to_move = OPP( thor_side_to_move );
      }
      else
	return FALSE;
    }
  }

  return TRUE;
}


/*
  PREPARE_GAME
  Performs off-line analysis of GAME to speed up subsequent requests.
  The main result is that the number of black discs on the board after
  each of the moves is stored.
*/

static void
prepare_game( GameType *game ) {
  int i;
  int move;
  int done;
  int flipped;
  int opening_match;
  int moves_played;
  int disc_count[3];
  unsigned int corner_descriptor;
  ThorOpeningNode *opening, *child;

  /* Play through the game and count the number of black discs
     at each stage. */

  clear_thor_board();
  disc_count[BLACKSQ] = disc_count[WHITESQ] = 2;
  thor_side_to_move = BLACKSQ;

  corner_descriptor = 0;

  moves_played = 0;
  done = FALSE;
  do {
    /* Store the number of black discs. */

    game->black_disc_count[moves_played] = disc_count[BLACKSQ];

    /* Make the move, update the board and disc count,
       and change the sign for white moves */

    move = game->moves[moves_played];
    flipped = count_flips( move, thor_side_to_move, OPP( thor_side_to_move ) );
    if ( flipped ) {
      thor_board[move] = thor_side_to_move;
      disc_count[thor_side_to_move] += flipped + 1;
      disc_count[OPP( thor_side_to_move )] -= flipped;
      if ( thor_side_to_move == WHITESQ )
	game->moves[moves_played] = -game->moves[moves_played];
      thor_side_to_move = OPP( thor_side_to_move );
      moves_played++;
    }
    else {
      thor_side_to_move = OPP( thor_side_to_move );
      flipped = count_flips( move, thor_side_to_move,
			     OPP( thor_side_to_move ) );
      if ( flipped ) {
	thor_board[move] = thor_side_to_move;
	disc_count[thor_side_to_move] += flipped + 1;
	disc_count[OPP( thor_side_to_move )] -= flipped;
	if ( thor_side_to_move == WHITESQ )
	  game->moves[moves_played] = -game->moves[moves_played];
	thor_side_to_move = OPP( thor_side_to_move );
	moves_played++;
      }
      else
	done = TRUE;
    }

    /* Update the corner descriptor if necessary */

    if ( (move == 11) || (move == 18) || (move == 81) || (move == 88) )
      corner_descriptor |=
	get_corner_mask( thor_board[11], thor_board[81],
			 thor_board[18], thor_board[88] );

  } while ( !done && (moves_played < 60) );

  game->black_disc_count[moves_played] = disc_count[BLACKSQ];
  game->move_count = moves_played;
  for ( i = moves_played + 1; i <= 60; i++ )
    game->black_disc_count[i] = -1;

  /* Find the longest opening which coincides with the game */

  opening = root_node;
  i = 0;
  opening_match = TRUE;
  while ( opening_match ) {
    move = opening->child_move;
    child = opening->child_node;
    while ( (child != NULL) && (move != abs( game->moves[i] )) ) {
      move = child->sibling_move;
      child = child->sibling_node;
    }
    if ( child == NULL )
      opening_match = FALSE;
    else {
      opening = child;
      i++;
    }
  }
  game->opening = opening;

  /* Initialize the shape state */

  game->shape_lo = 3 << 27;
  game->shape_hi = 3 << 3;
  game->shape_state_hi = 0;
  game->shape_state_lo = 0;

  /* Store the corner descriptor */

  game->corner_descriptor = corner_descriptor;
}


/*
  GET_INT_8
  Reads an 8-bit signed integer from STREAM. Returns TRUE upon
  success, FALSE otherwise.
*/

INLINE static int
get_int_8( FILE *stream, int_8 *value ) {
  int actually_read;

  actually_read = fread( value, sizeof( int_8 ), 1, stream );

  return (actually_read == 1);
}


/*
  GET_INT_16
  Reads a 16-bit signed integer from STREAM. Returns TRUE upon
  success, FALSE otherwise.
*/

INLINE static int
get_int_16( FILE *stream, int_16 *value ) {
  int actually_read;

  actually_read = fread( value, sizeof( int_16 ), 1, stream );

  return (actually_read == 1);
}


/*
  GET_INT_32
  Reads a 32-bit signed integer from STREAM. Returns TRUE upon
  success, FALSE otherwise.
*/

INLINE static int
get_int_32( FILE *stream, int_32 *value ) {
  int actually_read;

  actually_read = fread( value, sizeof( int_32 ), 1, stream );

  return (actually_read == 1);
}


/*
  TOURNAMENT_NAME
  Returns the name of the INDEXth tournament if available.
*/

INLINE static const char *
tournament_name( int index ) {
  if ( (index < 0) || (index >= tournaments.count) )
    return "<" NA_ERROR ">";
  else
    return tournaments.name_buffer + TOURNAMENT_NAME_LENGTH * index;
}


/*
  TOURNAMENT_LEX_ORDER
  Returns the index into the lexicographical order of the
  INDEXth tournament if available, otherwise the last
  index + 1.
*/

INLINE static int
tournament_lex_order( int index ) {
  if ( (index < 0) || (index >= tournaments.count) )
    return tournaments.count;
  else
    return tournaments.tournament_list[index].lex_order;
}


/*
  GET_PLAYER_NAME
  Returns the name of the INDEXth player if available.
*/

const char *
get_player_name( int index ) {
  if ( (index < 0) || (index >= players.count) )
    return "< " NA_ERROR " >";
  else
    return players.name_buffer + PLAYER_NAME_LENGTH * index;
}



/*
  GET_PLAYER_COUNT
  Returns the number of players in the database.
*/

int get_player_count( void ) {
  return players.count;
}



/*
  PLAYER_LEX_ORDER
  Returns the index into the lexicographical order of the
  INDEXth player if available, otherwise the last index + 1.
*/

INLINE static int
player_lex_order( int index ) {
  if ( (index < 0) || (index >= players.count) )
    return players.count;
  else
    return players.player_list[index].lex_order;
}


/*
  READ_PROLOG
  Reads the prolog from STREAM into PROLOG. As the prolog is common
  for all the three database types (game, player, tournament) also
  values which aren't used are read.
  Returns TRUE upon success, otherwise FALSE.
*/

static int
read_prolog( FILE *stream, PrologType *prolog ) {
  int success;
  int_8 byte_val;
  int_16 word_val;
  int_32 longint_val;

  success = get_int_8( stream, &byte_val );
  prolog->creation_century = byte_val;

  success = success && get_int_8( stream, &byte_val );
  prolog->creation_year = byte_val;

  success = success && get_int_8( stream, &byte_val );
  prolog->creation_month = byte_val;

  success = success && get_int_8( stream, &byte_val );
  prolog->creation_day = byte_val;

  success = success && get_int_32( stream, &longint_val );
  prolog->game_count = longint_val;

  success = success && get_int_16( stream, &word_val );
  prolog->item_count = word_val;

  success = success && get_int_16( stream, &word_val );
  prolog->origin_year = word_val;

  success = success && get_int_32( stream, &longint_val );
  prolog->reserved = longint_val;

  return success;
}


/*
  THOR_COMPARE_TOURNAMENTS
  Lexicographically compares the names of two tournaments
  represented by pointers.
*/

#ifdef _WIN32_WCE
static int CE_CDECL
#else
static int
#endif
thor_compare_tournaments( const void *t1, const void *t2 ) {
  TournamentType *tournament1 = (TournamentType *) *((TournamentType **) t1);
  TournamentType *tournament2 = (TournamentType *) *((TournamentType **) t2);

  return strcmp( tournament1->name, tournament2->name );
}


/*
  SORT_TOURNAMENT_DATABASE
  Computes the lexicographic order of all tournaments in the database.
*/

static void
sort_tournament_database( void ) {
  TournamentType **tournament_buffer;
  int i;

  tournament_buffer =
    (TournamentType **)
    safe_malloc( tournaments.count * sizeof( TournamentType * ) );
  for ( i = 0; i < tournaments.count; i++ )
    tournament_buffer[i] = &tournaments.tournament_list[i];
  qsort( tournament_buffer, tournaments.count, sizeof( TournamentType * ),
	 thor_compare_tournaments );
  for ( i = 0; i < tournaments.count; i++ )
    tournament_buffer[i]->lex_order = i;
  free( tournament_buffer );
}


/*
  READ_TOURNAMENT_DATABASE
  Reads the tournament database from FILE_NAME.
  Returns TRUE if all went well, otherwise FALSE.
*/

int
read_tournament_database( const char *file_name ) {
  FILE *stream;
  int i;
  int success;
  int actually_read;
  int buffer_size;

  stream = fopen( file_name, "rb" );
  if ( stream == NULL )
    return FALSE;
  if ( !read_prolog( stream, &tournaments.prolog ) ) {
    fclose( stream );
    return FALSE;
  }
  tournaments.count = tournaments.prolog.item_count;
  buffer_size = TOURNAMENT_NAME_LENGTH * tournaments.prolog.item_count;
  tournaments.name_buffer =
    (char *) safe_realloc( tournaments.name_buffer,  buffer_size );
  actually_read = fread( tournaments.name_buffer, 1, buffer_size, stream );
  success = (actually_read == buffer_size);

  fclose( stream );

  if ( success ) {
    tournaments.tournament_list = (TournamentType *)
      safe_realloc( tournaments.tournament_list,
		    tournaments.count * sizeof( TournamentType ) );
    for ( i = 0; i < tournaments.count; i++ ) {
      tournaments.tournament_list[i].name = tournament_name( i );
      tournaments.tournament_list[i].selected = TRUE;
    }
    sort_tournament_database();
    thor_games_sorted = FALSE;
    thor_games_filtered = FALSE;
  }

  return success;
}


/*
  GET_TOURNAMENT_NAME
  Returns the name of the INDEXth tournament if available.
*/

const char *
get_tournament_name( int index ) {
  if ( (index < 0) || (index >= tournaments.count) )
    return "< " NA_ERROR " >";
  else
    return tournaments.name_buffer + TOURNAMENT_NAME_LENGTH * index;
}



/*
  GET_TOURNAMENT_COUNT
  Returns the number of players in the database.
*/

int get_tournament_count( void ) {
  return tournaments.count;
}



/*
  THOR_COMPARE_PLAYERS
  Lexicographically compares the names of two players
  represented by pointers.
*/

#ifdef _WIN32_WCE
static int CE_CDECL
#else
static int
#endif
thor_compare_players( const void *p1, const void *p2 ) {
  char ch;
  char buffer1[PLAYER_NAME_LENGTH];
  char buffer2[PLAYER_NAME_LENGTH];
  int i;
  PlayerType *player1 = (PlayerType *) *((PlayerType **) p1);
  PlayerType *player2 = (PlayerType *) *((PlayerType **) p2);

  i = 0;
  do {
    ch = player1->name[i];
    buffer1[i] = tolower( ch );
    i++;
  } while ( ch != 0 );
  if ( buffer1[0] == '?' )  /* Put unknown players LAST */
     buffer1[0] = '~';

  i = 0;
  do {
    ch = player2->name[i];
    buffer2[i] = tolower( ch );
    i++;
  } while ( ch != 0 );
  if ( buffer2[0] == '?' )  /* Put unknown players LAST */
     buffer2[0] = '~';

  return strcmp( buffer1, buffer2 );
}


/*
  SORT_PLAYER_DATABASE
  Computes the lexicographic order of all players in the database.
*/

static void
sort_player_database( void ) {
  PlayerType **player_buffer;
  int i;

  player_buffer =
    (PlayerType **) safe_malloc( players.count * sizeof( PlayerType * ) );
  for ( i = 0; i < players.count; i++ )
    player_buffer[i] = &players.player_list[i];
  qsort( player_buffer, players.count, sizeof( PlayerType * ),
	 thor_compare_players );
  for ( i = 0; i < players.count; i++ )
    player_buffer[i]->lex_order = i;
  free( player_buffer );
}


/*
  READ_PLAYER_DATABASE
  Reads the player database from FILE_NAME.
  Returns TRUE if all went well, otherwise FALSE.
*/

int
read_player_database( const char *file_name ) {
  FILE *stream;
  int i;
  int success;
  int actually_read;
  int buffer_size;

  stream = fopen( file_name, "rb" );
  if ( stream == NULL )
    return FALSE;
  if ( !read_prolog( stream, &players.prolog ) ) {
    fclose( stream );
    return FALSE;
  }
  players.count = players.prolog.item_count;
  buffer_size = PLAYER_NAME_LENGTH * players.count;
  players.name_buffer =
    (char *) safe_realloc( players.name_buffer, buffer_size );
  actually_read = fread( players.name_buffer, 1, buffer_size, stream );
  success = (actually_read == buffer_size);

  fclose( stream );

  if ( success ) {
    players.player_list = (PlayerType *)
      safe_realloc( players.player_list,
		    players.count * sizeof( PlayerType ) );
    for ( i = 0; i < players.count; i++ ) {
      players.player_list[i].name = get_player_name( i );
      /* By convention, names of computer programs always contain
	 parenthesis within which the name of the creator of the
	 program is given. E.g. "Zebra (andersson)", "Sethos()". */
      if ( strchr( players.player_list[i].name, '(' ) != NULL )
	players.player_list[i].is_program = TRUE;
      else
	players.player_list[i].is_program = FALSE;
      players.player_list[i].selected = TRUE;
    }
    sort_player_database();
    thor_games_sorted = FALSE;
    thor_games_filtered = FALSE;
  }

  return success;
}


/*
  READ_GAME
  Reads a game from STREAM in GAME and prepares the game
  for database questions. Returns TRUE upon success,
  otherwise FALSE.
*/

static int
read_game( FILE *stream, GameType *game ) {
  int success;
  int actually_read;
  int_8 byte_val;
  int_16 word_val;

  success = get_int_16( stream, &word_val );
  game->tournament_no = word_val;

  success = success && get_int_16( stream, &word_val );
  game->black_no = word_val;

  success = success && get_int_16( stream, &word_val );
  game->white_no = word_val;

  success = success && get_int_8( stream, &byte_val );
  game->actual_black_score = byte_val;

  success = success && get_int_8( stream, &byte_val );
  game->perfect_black_score = byte_val;

  actually_read = fread( &game->moves, 1, 60, stream );
  prepare_game( game );

  return success && (actually_read == 60);
}



/*
  READ_GAME_DATABASE
  Reads a game database from FILE_NAME.
*/

int
read_game_database( const char *file_name ) {
  FILE *stream;
  int i;
  int success;
  DatabaseType *old_database_head;

  stream = fopen( file_name, "rb" );
  if ( stream == NULL )
    return FALSE;

  old_database_head = database_head;
  database_head = (DatabaseType *) safe_malloc( sizeof( DatabaseType ) );
  database_head->next = old_database_head;

  if ( !read_prolog( stream, &database_head->prolog ) ) {
    fclose( stream );
    return FALSE;
  }

  success = TRUE;
  database_head->count = database_head->prolog.game_count;
  database_head->games =
    (GameType *) safe_malloc( database_head->count * sizeof( GameType ) );
  for ( i = 0; i < database_head->count; i++ ) {
    success = success && read_game( stream, &database_head->games[i] );
    database_head->games[i].database = database_head;
  }

  thor_database_count++;
  thor_game_count += database_head->count;
  thor_games_sorted = FALSE;
  thor_games_filtered = FALSE;

  fclose( stream );

  return success;
}


/*
  GAME_DATABASE_ALREADY_LOADED
  Checks if the game database in FILE_NAME already exists in memory
  (the only thing actually checked is the prolog but this suffices
  according to the specification of the database format).
*/

int
game_database_already_loaded( const char *file_name ) {
  FILE *stream;
  DatabaseType *current_db;
  PrologType new_prolog;

  stream = fopen( file_name, "rb" );
  if ( stream == NULL )
    return FALSE;

  if ( !read_prolog( stream, &new_prolog ) ) {
    fclose( stream );
    return FALSE;
  }

  fclose( stream );

  current_db = database_head;
  while ( current_db != NULL ) {
    if ( (current_db->prolog.creation_century == new_prolog.creation_century)
	 && (current_db->prolog.creation_year == new_prolog.creation_year)
	 && (current_db->prolog.creation_month == new_prolog.creation_month)
	 && (current_db->prolog.creation_day == new_prolog.creation_day)
	 && (current_db->prolog.game_count == new_prolog.game_count)
	 && (current_db->prolog.item_count == new_prolog.item_count)
	 &&(current_db->prolog.origin_year == current_db->prolog.origin_year) )
      return TRUE;
    current_db = current_db->next;
  }

  return FALSE;
}



/*
  GET_DATABASE_COUNT
  Returns the number of game databases currently loaded.
*/

int
get_database_count( void ) {
  return thor_database_count;
}



/*
  GET_DATABASE_INFO
  Fills the vector INFO with the origin years and number of games of
  all game databases loaded.
  Enough memory must have been allocated prior to this function being
  called, that this is the case can be checked by calling GET_DATABASE_COUNT
  above.
*/

void
get_database_info( DatabaseInfoType *info ) {
  int i;
  int change;
  DatabaseInfoType temp;
  DatabaseType *current_db;

  current_db = database_head;
  for ( i = 0; i < thor_database_count; i++ ) {
    info[i].year = current_db->prolog.origin_year;
    info[i].count = current_db->count;
    current_db = current_db->next;
  }

  /* Sort the list */

  do {
    change = FALSE;
    for ( i = 0; i < thor_database_count - 1; i++ )
      if ( info[i].year > info[i + 1].year ) {
	change = TRUE;
	temp = info[i];
	info[i] = info[i + 1];
	info[i + 1] = temp;
      }
  } while ( change );
}



/*
  PRINT_GAME
  Outputs the information about the game GAME to STREAM.
  The flag DISPLAY_MOVES specifies if the moves of the
  game are to be output or not.
*/

static void
print_game( FILE *stream, GameType *game, int display_moves ) {
  int i;

  fprintf( stream, "%s  %d\n", tournament_name( game->tournament_no ),
	   game->database->prolog.origin_year );
  fprintf( stream, "%s %s %s\n", get_player_name( game->black_no ),
	   VERSUS_TEXT, get_player_name( game->white_no ) );
  fprintf( stream, "%d - %d   ", game->actual_black_score,
	  64 - game->actual_black_score );
  fprintf( stream, "[ %d - %d %s ]\n", game->perfect_black_score,
	  64 - game->perfect_black_score, PERFECT_TEXT );
  if ( display_moves )
    for ( i = 0; i < 60; i++ ) {
      fprintf( stream, " %d", abs( game->moves[i] ) );
      if ( (i % 20) == 19 )
	fputs( "\n", stream );
    }
  fputs( "\n", stream );
}



/*
  COMPUTE_PARTIAL_HASH
  Computes the primary and secondary hash values for the
  unit element in the rotation group.
*/

static void
compute_partial_hash( unsigned int *hash_val1, unsigned int *hash_val2 ) {
  int i;

  *hash_val1 = 0;
  *hash_val2 = 0;

  for ( i = 0; i < 8; i++ ) {
    *hash_val1 ^= primary_hash[i][thor_row_pattern[i]];
    *hash_val2 ^= secondary_hash[i][thor_row_pattern[i]];
  }
}


/*
  COMPUTE_FULL_PRIMARY_HASH
  COMPUTE_FULL_SECONDARY_HASH
  Compute the primary and secondary hash codes respectively
  for all elements in the rotation group.
  Note: The order of the hash codes must coincide with the
        definitions in INIT_SYMMETRY_MAPS().
*/

static void
compute_full_primary_hash( unsigned int *hash_val ) {
  int i;

  for ( i = 0; i < 4; i++ )
    hash_val[i] = 0;
  for ( i = 0; i < 8; i++ ) {
    /* b1 -> b1 */
    hash_val[0] ^= primary_hash[i][thor_row_pattern[i]];
    /* b8 -> b1 */
    hash_val[1] ^= primary_hash[i][thor_row_pattern[7 - i]];
    /* a2 -> b1 */
    hash_val[2] ^= primary_hash[i][thor_col_pattern[i]];
    /* h2 -> b1 */
    hash_val[3] ^= primary_hash[i][thor_col_pattern[7 - i]];
  }
  /* g1 -> b1 */
  hash_val[4] = bit_reverse_32( hash_val[0] );
  /* g8 -> b1 */
  hash_val[5] = bit_reverse_32( hash_val[1] );
  /* a7 -> b1 */
  hash_val[6] = bit_reverse_32( hash_val[2] );
  /* h7 -> b1 */
  hash_val[7] = bit_reverse_32( hash_val[3] );
}

static void
compute_full_secondary_hash( unsigned int *hash_val ) {
  int i;

  for ( i = 0; i < 4; i++ )
    hash_val[i] = 0;
  for ( i = 0; i < 8; i++ ) {
    /* b1 -> b1 */
    hash_val[0] ^= secondary_hash[i][thor_row_pattern[i]];
    /* b8 -> b1 */
    hash_val[1] ^= secondary_hash[i][thor_row_pattern[7 - i]];
    /* a2 -> b1 */
    hash_val[2] ^= secondary_hash[i][thor_col_pattern[i]];
    /* h2 -> b1 */
    hash_val[3] ^= secondary_hash[i][thor_col_pattern[7 - i]];
  }
  /* g1 -> b1 */
  hash_val[4] = bit_reverse_32( hash_val[0] );
  /* g8 -> b1 */
  hash_val[5] = bit_reverse_32( hash_val[1] );
  /* a7 -> b1 */
  hash_val[6] = bit_reverse_32( hash_val[2] );
  /* h7 -> b1 */
  hash_val[7] = bit_reverse_32( hash_val[3] );
}


/*
  PRIMARY_HASH_LOOKUP
  Checks if any of the rotations of the current pattern set
  match the primary hash code TARGET_HASH.
*/

static int
primary_hash_lookup( unsigned int target_hash ) {
  int i;
  int hit_mask;
  unsigned int hash_val[8];

  compute_full_primary_hash( hash_val );

  hit_mask = 0;
  for ( i = 0; i < 8; i++ )
    if ( hash_val[i] == target_hash )
      hit_mask |= (1 << i);

  return hit_mask;
}


/*
  SECONDARY_HASH_LOOKUP
  Checks if any of the rotations of the current pattern set
  match the secondary hash code TARGET_HASH.
*/

static int
secondary_hash_lookup( unsigned int target_hash ) {
  int i;
  int hit_mask;
  unsigned int hash_val[8];

  compute_full_secondary_hash( hash_val );

  hit_mask = 0;
  for ( i = 0; i < 8; i++ )
    if ( hash_val[i] == target_hash )
      hit_mask |= (1 << i);

  return hit_mask;
}


/*
  THOR_COMPARE
  Compares two games from a list of pointers to games.
  Only to be called by QSORT. A full comparison is
  performed using the priority order from THOR_SORT_ORDER.
*/

#ifdef _WIN32_WCE
static int CE_CDECL
#else
static int
#endif
thor_compare( const void *g1, const void *g2 ) {
  int i;
  int result;
  GameType *game1 = (GameType *) *((GameType **) g1);
  GameType *game2 = (GameType *) *((GameType **) g2);

  for ( i = 0; i < thor_sort_criteria_count; i++ ) {
    switch ( thor_sort_order[i] ) {
    default:  /* Really can't happen */
    case SORT_BY_YEAR:
      result = game1->database->prolog.origin_year -
	game2->database->prolog.origin_year;
      break;
    case SORT_BY_YEAR_REVERSED:
      result = game2->database->prolog.origin_year -
	game1->database->prolog.origin_year;
      break;
    case SORT_BY_BLACK_NAME:
      result = player_lex_order( game1->black_no ) -
	player_lex_order( game2->black_no );
      break;
    case SORT_BY_WHITE_NAME:
      result = player_lex_order( game1->white_no ) -
	player_lex_order( game2->white_no );
      break;
    case SORT_BY_TOURNAMENT:
      result = tournament_lex_order( game1->tournament_no ) -
	tournament_lex_order( game2->tournament_no );
      break;
    case SORT_BY_BLACK_SCORE:
      result = game1->actual_black_score - game2->actual_black_score;
      break;
    case SORT_BY_WHITE_SCORE:
      result = game2->actual_black_score - game1->actual_black_score;
      break;
    }
    if ( result != 0 )
      return result;
  }

  /* If control reaches this point, the two games couldn't be
     distinguished by the current search criteria. */

  return 0;
}


/*
  FILTER_DATABASE
  Applies the current filter rules to the database DB.
*/

static void
filter_database( DatabaseType *db ) {
  int i;
  int category;
  int passes_filter;
  int year;
  GameType *game;

  for ( i = 0; i < db->count; i++ ) {
    game = &db->games[i];
    passes_filter = TRUE;

    /* Apply the tournament filter */

    if ( passes_filter &&
	 !tournaments.tournament_list[game->tournament_no].selected )
      passes_filter = FALSE;

    /* Apply the year filter */

    if ( passes_filter ) {
      year = game->database->prolog.origin_year;
      if ( (year < filter.first_year) || (year > filter.last_year) )
	passes_filter = FALSE;
    }

    /* Apply the player filter */

    if ( passes_filter ) {
      switch ( filter.player_filter ) {
      case EitherSelectedFilter:
	if ( !players.player_list[game->black_no].selected &&
	     !players.player_list[game->white_no].selected )
	  passes_filter = FALSE;
	break;

      case BothSelectedFilter:
	if ( !players.player_list[game->black_no].selected ||
	     !players.player_list[game->white_no].selected )
	  passes_filter = FALSE;
	break;

      case BlackSelectedFilter:
	if ( !players.player_list[game->black_no].selected )
	  passes_filter = FALSE;
	break;

      case WhiteSelectedFilter:
	if ( !players.player_list[game->white_no].selected )
	  passes_filter = FALSE;
	break;
      }
    }

    /* Apply the game type filter */

    if ( passes_filter ) {
      if ( players.player_list[game->black_no].is_program ) {
	if ( players.player_list[game->white_no].is_program )
	  category = GAME_PROGRAM_PROGRAM;
	else
	  category = GAME_HUMAN_PROGRAM;
      }
      else {
	if ( players.player_list[game->white_no].is_program )
	  category = GAME_HUMAN_PROGRAM;
	else
	  category = GAME_HUMAN_HUMAN;
      }
      passes_filter = (category & filter.game_categories);
    }
    game->passes_filter = passes_filter;
  }
}


/*
  FILTER_ALL_DATABASES
  Applies the current filter rules to all databases.
*/

static void
filter_all_databases( void ) {
  DatabaseType *current_db;

  current_db = database_head;
  while ( current_db != NULL ) {
    filter_database( current_db );
    current_db = current_db->next;
  }
}



/*
  SET_PLAYER_FILTER
  Specify what players to search for. The boolean vector SELECTED
  must contain at least PLAYERS.COUNT values - check with
  GET_PLAYER_COUNT() if necessary.
*/

void
set_player_filter( int *selected ) {
  int i;

  for ( i = 0; i < players.count; i++ )
    players.player_list[i].selected = selected[i];
  thor_games_filtered = FALSE;
}



/*
  SET_PLAYER_FILTER_TYPE
  Specifies whether it suffices for a game to contain one selected
  player or if both players have to be selected for it be displayed.
*/

void
set_player_filter_type( PlayerFilterType player_filter ) {
  filter.player_filter = player_filter;
}



/*
  SET_TOURNAMENT_FILTER
  Specify what tournaments to search for. The boolean vector SELECTED
  must contain at least TOURNAMENTS.COUNT values - check with
  GET_TOURNAMENT_COUNT() if necessary.
*/

void
set_tournament_filter( int *selected ) {
  int i;

  for ( i = 0; i < tournaments.count; i++ )
    tournaments.tournament_list[i].selected = selected[i];
  thor_games_filtered = FALSE;
}



/*
  SET_YEAR_FILTER
  Specify the interval of years to which the search will be confined.
*/

void
set_year_filter( int first_year, int last_year ) {
  filter.first_year = first_year;
  filter.last_year = last_year;
  thor_games_filtered = FALSE;
}



/*
  SPECIFY_GAME_CATEGORIES
  Specify the types of games in the database that are displayed
  if they match the position probed for. The input is the binary
  OR of the flags for the types enabled.
*/

void
specify_game_categories( int categories ) {
  if ( categories != filter.game_categories ) {
    filter.game_categories = categories;
    thor_games_filtered = FALSE;
  }
}



/*
  SORT_THOR_GAMES
  Sorts the COUNT first games in the list THOR_SEARCH.MATCH_LIST.
  The first THOR_SORT_CRITERIA_COUNT entries of THOR_SORT_ORDER are
  used (in order) to sort the matches.
*/

static void
sort_thor_games( int count ) {

  if ( count <= 1 )  /* No need to sort 0 or 1 games. */
    return;

  qsort( thor_search.match_list, count, sizeof( GameType * ), thor_compare );
}



/*
  SPECIFY_THOR_SORT_ORDER
  Specifies that in subsequent calls to SORT_THOR_MATCHES,
  the COUNT first elements of SORT_ORDER are to be used
  (in decreasing order of priority).
  Note: If there aren't (at least) COUNT elements at the location
        to which SORT_ORDER points, a crash is likely.
*/

void
specify_thor_sort_order( int count, int *sort_order ) {
  int i;

  /* Truncate the input vector if it is too long */

  count = MIN( count, MAX_SORT_CRITERIA );

  /* Check if the new order coincides with the old order */

  if ( count != thor_sort_criteria_count )
    thor_games_sorted = FALSE;
  else
    for ( i = 0; i < count; i++ )
      if ( sort_order[i] != thor_sort_order[i] )
	thor_games_sorted = FALSE;

  thor_sort_criteria_count = count;
  for ( i = 0; i < count; i++ )
    thor_sort_order[i] = sort_order[i];
}



/*
  RECURSIVE_OPENING_SCAN
  Performs a preorder traversal of the opening tree rooted
  at NODE and checks which opening nodes are compatible
  with the primary and secondary hash codes from the 8 different
  rotations.
*/

static void
recursive_opening_scan( ThorOpeningNode *node, int depth, int moves_played, 
			unsigned int *primary_hash,
			unsigned int *secondary_hash ) {
  int i;
  int match;
  int matching_symmetry;
  ThorOpeningNode *child;

  /* Determine the status of the current node */

  if ( depth < moves_played ) {
    node->matching_symmetry = 0;
    node->current_match = OPENING_INCONCLUSIVE;
  }
  else if ( depth == moves_played ) {  /* Check the hash codes */
    match = FALSE;
    matching_symmetry = 0;
    for ( i = 7; i >= 0; i-- )
      if ( (node->hash1 == primary_hash[i]) &&
	   (node->hash2 == secondary_hash[i]) ) {
	match = TRUE;
	matching_symmetry = i;
      }
    if ( match ) {
      node->matching_symmetry = matching_symmetry;
      node->current_match = OPENING_MATCH;
    }
    else
      node->current_match = OPENING_MISMATCH;
  }
  else {  /* depth > moves_played */
    node->current_match = node->parent_node->current_match;
    node->matching_symmetry = node->parent_node->matching_symmetry;
  }

  /* Recursively search the childen */

  child = node->child_node;
  while ( child != NULL ) {
    recursive_opening_scan( child, depth + 1, moves_played,
			    primary_hash, secondary_hash );
    child = child->sibling_node;
  }
}


/*
  OPENING_SCAN
  Fills the opening tree with information on how well
  the current pattern configuration matches the openings.
*/

static void
opening_scan( int moves_played ) {
  unsigned int primary_hash[8];
  unsigned int secondary_hash[8];

  compute_full_primary_hash( primary_hash );
  compute_full_secondary_hash( secondary_hash );

  recursive_opening_scan( root_node, 0, moves_played,
			  primary_hash, secondary_hash );
}



/*
  RECURSIVE_FREQUENCY_COUNT
  Recursively fills frequency table FREQ_COUNT which is to contain
  the number of times each move has been played according to the
  trimmed set of openings from the Thor database.
*/

static void
recursive_frequency_count( ThorOpeningNode *node, int *freq_count, int depth,
			   int moves_played, int *symmetries,
			   unsigned int *primary_hash,
			   unsigned int *secondary_hash ) {
  int i, j;
  int child_move;
  ThorOpeningNode *child;

  if ( depth == moves_played ) {
    for ( i = 0; i < 8; i++ ) {
      j = symmetries[i];
      if ( (node->hash1 == primary_hash[j]) &&
	   (node->hash2 == secondary_hash[j]) ) {
	child_move = node->child_move;
	child = node->child_node;
	while ( child != NULL ) {
	  freq_count[inv_symmetry_map[j][child_move]] += child->frequency;
	  child_move = child->sibling_move;
	  child = child->sibling_node;
	}
	break;
      }
    }
  }
  else if ( depth < moves_played ) {
    child = node->child_node;
    while ( child != NULL ) {
      recursive_frequency_count( child, freq_count, depth + 1, moves_played,
				 symmetries, primary_hash, secondary_hash );
      child = child->sibling_node;
    }
  }
}


/*
  CHOOSE_THOR_OPENING_MOVE
  Computes frequencies for all moves from the given position,
  display these and chooses one if from a distribution skewed
  towards common moves. (If no moves are found, PASS is returned.)
*/

int
choose_thor_opening_move( int *in_board, int side_to_move, int echo ) {
  int i, j;
  int temp_symm;
  int pos;
  int disc_count;
  int freq_sum, acc_freq_sum;
  int random_move, random_value;
  int match_count;
  int symmetries[8];
  int freq_count[100];
  unsigned int primary_hash[8];
  unsigned int secondary_hash[8];
  struct { int move; int frequency; } move_list[64], temp;

  disc_count = 0;
  for ( i = 1; i <= 8; i++ )
    for ( j = 1, pos = 10 * i + 1; j <= 8; j++, pos++ ) {
      freq_count[pos] = 0;
      if ( in_board[pos] != EMPTY )
	disc_count++;
    }

  /* Check that the parity of the board coincides with standard
     Othello - if this is not the case, the Thor opening lines are useless
     as they don't contain any passes. */

  if ( (side_to_move == BLACKSQ) && (disc_count % 2 == 1) )
    return PASS;
  if ( (side_to_move == WHITESQ) && (disc_count % 2 == 0) )
    return PASS;

  /* Create a random permutation of the symmetries to avoid the same
     symmetry always being chosen in e.g. the initial position */

  for ( i = 0; i < 8; i++ )
    symmetries[i] = i;
  for ( i = 0; i < 7; i++ ) {
    j  = i + abs( my_random() ) % (8 - i);
    temp_symm = symmetries[i];
    symmetries[i] = symmetries[j];
    symmetries[j] = temp_symm;
  }

  /* Calculate frequencies for all moves */

  compute_thor_patterns( in_board );

  compute_full_primary_hash( primary_hash );
  compute_full_secondary_hash( secondary_hash );

  recursive_frequency_count( root_node, freq_count, 0, disc_count - 4,
			     symmetries, primary_hash, secondary_hash );

  freq_sum = 0;
  for ( i = 1; i <= 8; i++ )
    for ( j = 1, pos = 10 * i + 1; j <= 8; j++, pos++ )
      freq_sum += freq_count[pos];

  if ( freq_sum > 0 ) {  /* Position found in Thor opening tree */

    /* Create a list of the moves chosen from the position and also
       randomly select one of them. Probability for each move is
       proportional to the frequency of that move being played here. */

    random_value = abs( my_random() ) % freq_sum;
    random_move = PASS;
    acc_freq_sum = 0;
    match_count = 0;
    for ( i = 1; i <= 8; i++ )
      for ( j = 1, pos = 10 * i + 1; j <= 8; j++, pos++ )
	if ( freq_count[pos] > 0 ) {
	  move_list[match_count].move = pos;
	  move_list[match_count].frequency = freq_count[pos];
	  match_count++;
	  if ( (acc_freq_sum < random_value) &&
	       (acc_freq_sum + freq_count[pos] >= random_value) )
	    random_move = pos;
	  acc_freq_sum += freq_count[pos];
	}

    /* Optionally display the database moves sorted on frequency */

    if ( echo ) {
      for ( i = 0; i < match_count; i++ )
	for ( j = 0; j < match_count - 1; j++ )
	  if ( move_list[j].frequency < move_list[j + 1].frequency ) {
	    temp = move_list[j];
	    move_list[j] = move_list[j + 1];
	    move_list[j + 1] = temp;
	  }
#ifdef TEXT_BASED
      printf( "%s:        ", THOR_TEXT );
      for ( i = 0; i < match_count; i++ ) {
	printf( "%c%c: %4.1f%%    ", TO_SQUARE( move_list[i].move ),
		(100.0 * move_list[i].frequency) / freq_sum );
	if ( i % 6 == 4 )
	  puts( "" );
      }
      if ( match_count % 6 != 5 )
	puts( "" );
#endif
    }

    return random_move;
  }

  return PASS;
}


/*
  POSITION_MATCH
  Returns TRUE if the position after MOVE_COUNT moves of GAME, with
  SIDE_TO_MOVE being the player to move, matches the hash codes
  IN_HASH1 and IN_HASH2, otherwise FALSE.
*/

static int
position_match( GameType *game, int move_count, int side_to_move,
		unsigned int *shape_lo, unsigned int *shape_hi,
		unsigned int corner_mask,
		unsigned int in_hash1, unsigned int in_hash2 ) {
  int i;
  int shape_match;
  int primary_hit_mask, secondary_hit_mask;

  /* Verify that the number of moves and the side-to-move status
     are correct */

  if ( move_count >= game->move_count ) {
    if ( move_count > game->move_count )  /* Too many moves! */
      return FALSE;
    /* No side-to-move status to check if the game is over */
  }
  else {
    if ( side_to_move == BLACKSQ ) {  /* Black to move */
      if ( game->moves[move_count] < 0 )  /* White to move in the game */
	return FALSE;
    }
    else {  /* White to move */
      if ( game->moves[move_count] > 0 )  /* Black to move in the game */
	return FALSE;
    }
  }

  /* Check if the opening information suffices to
     determine if the position matches or not. */

  if ( game->opening->current_match == OPENING_MATCH ) {
    game->matching_symmetry = game->opening->matching_symmetry;
    return TRUE;
  }
  else if ( game->opening->current_match == OPENING_MISMATCH )
    return FALSE;

  /* Check if the lower 32 bits of the shape state coincide */

  if ( game->shape_state_lo < move_count ) {
    for ( i = game->shape_state_lo; i < move_count; i++ )
      game->shape_lo |= move_mask_lo[abs( (int) game->moves[i] )];
    game->shape_state_lo = move_count;
  }
  else if ( game->shape_state_lo > move_count ) {
    for ( i = game->shape_state_lo - 1; i >= move_count; i-- )
      game->shape_lo &= ~move_mask_lo[abs( (int) game->moves[i] )];
    game->shape_state_lo = move_count;
  }

  shape_match = FALSE;
  for ( i = 0; i < 8; i++ )
    shape_match |= (game->shape_lo == shape_lo[i]);
  if ( !shape_match )
    return FALSE;

  /* Check if the upper 32 bits of the shape state coincide */

  if ( game->shape_state_hi < move_count ) {
    for ( i = game->shape_state_hi; i < move_count; i++ )
      game->shape_hi |= move_mask_hi[abs( (int) game->moves[i] )];
    game->shape_state_hi = move_count;
  }
  else if ( game->shape_state_hi > move_count ) {
    for ( i = game->shape_state_hi - 1; i >= move_count; i-- )
      game->shape_hi &= ~move_mask_hi[abs( (int) game->moves[i] )];
    game->shape_state_hi = move_count;
  }

  shape_match = FALSE;
  for ( i = 0; i < 8; i++ )
    shape_match |= (game->shape_hi == shape_hi[i]);
  if ( !shape_match )
    return FALSE;

  /* Check if the corner mask is compatible with that of the game */

  if ( corner_mask & ~game->corner_descriptor )
    return FALSE;

  /* Otherwise play through the moves of the game until the
     number of discs is correct and check if the hash
     functions match the given hash values for at least one
     rotation (common to the two hash functions). */

  if ( play_through_game( game, move_count ) ) {
    compute_thor_patterns( thor_board );
    primary_hit_mask = primary_hash_lookup( in_hash1 );
    if ( primary_hit_mask ) {
      secondary_hit_mask = secondary_hash_lookup( in_hash2 );
      if ( primary_hit_mask & secondary_hit_mask )
	for ( i = 0; i < 8; i++ )
	  if ( (primary_hit_mask & secondary_hit_mask) & (1 << i) ) {
	    game->matching_symmetry = i;
	    return TRUE;
	  }
    }
  }

  return FALSE;
}


/*
  DATABASE_SEARCH
  Determines what positions in the Thor database match the position
  given by IN_BOARD with SIDE_TO_MOVE being the player whose turn it is.
*/

void
database_search( int *in_board,
		 int side_to_move ) {
  int i, j;
  int index;
  int pos;
  int sum;
  int move_count;
  int symmetry, next_move;
  int disc_count[3];
  int frequency[65], cumulative[65];
  unsigned int target_hash1, target_hash2;
  unsigned int corner_mask;
  unsigned int shape_lo[8], shape_hi[8];
  DatabaseType *current_db;
  GameType *game;

  /* We need a player and a tournament database. */

  if ( (players.count == 0) || (tournaments.count == 0) ) {
    thor_search.match_count = 0;
    return;
  }

  /* Make sure there's memory allocated if all positions
     in all databases match the position */

  if ( thor_search.allocation == 0 ) {
    thor_search.match_list =
      (GameType **) safe_malloc( thor_game_count * sizeof( GameType * ) );
    thor_search.allocation = thor_game_count;
  }
  else if ( thor_search.allocation < thor_game_count) {
    free( thor_search.match_list );
    thor_search.match_list =
      (GameType **) safe_malloc( thor_game_count * sizeof( GameType * ) );
    thor_search.allocation = thor_game_count;
  }

  /* If necessary, filter all games in the database */

  if ( !thor_games_filtered ) {
    filter_all_databases();
    thor_games_filtered = TRUE;
  }

  /* If necessary, sort all games in the database */

  if ( !thor_games_sorted ) {
    current_db = database_head;
    i = 0;
    while ( current_db != NULL ) {
      for ( j = 0; j < current_db->count; j++ ) {
	thor_search.match_list[i] = &current_db->games[j];
	i++;
      }
      current_db = current_db->next;
    }

    sort_thor_games( thor_game_count );

    for ( j = 0; j < thor_game_count; j++ )
      thor_search.match_list[j]->sort_order = j;

    thor_games_sorted = TRUE;
  }

  /* Determine disc count, hash codes, patterns and opening 
     for the position */

  disc_count[BLACKSQ] = disc_count[WHITESQ] = 0;
  for ( i = 1; i <= 8; i++ )
    for ( j = 1, pos = 10 * i + 1; j <= 8; j++, pos++ )
      if ( in_board[pos] == BLACKSQ )
	disc_count[BLACKSQ]++;
      else if ( in_board[pos] == WHITESQ )
	disc_count[WHITESQ]++;

  move_count = disc_count[BLACKSQ] + disc_count[WHITESQ] - 4;
  compute_thor_patterns( in_board );
  compute_partial_hash( &target_hash1, &target_hash2 );
  opening_scan( move_count );

  /* Determine the shape masks */

  for ( i = 0; i < 8; i++ ) {
    shape_lo[i] = 0;
    shape_hi[i] = 0;
  }
  for ( i = 0; i < 8; i++ )
    for ( j = 0, pos = 10 * i + 11; j < 8; j++, pos++ )
      if ( in_board[pos] != EMPTY ) {
	index = 8 * i + j;
	if ( index < 32 )
	  shape_lo[0] |= (1 << index);
	else
	  shape_hi[0] |= (1 << (index - 32));
	index = 8 * i + (7 - j);
	if ( index < 32 )
	  shape_lo[1] |= (1 << index);
	else
	  shape_hi[1] |= (1 << (index - 32));
	index = 8 * j + i;
	if ( index < 32 )
	  shape_lo[2] |= (1 << index);
	else
	  shape_hi[2] |= (1 << (index - 32));
	index = 8 * j + (7 - i);
	if ( index < 32 )
	  shape_lo[3] |= (1 << index);
	else
	  shape_hi[3] |= (1 << (index - 32));
	index = 8 * (7 - i) + j;
	if ( index < 32 )
	  shape_lo[4] |= (1 << index);
	else
	  shape_hi[4] |= (1 << (index - 32));
	index = 8 * (7 - i) + (7 - j);
	if ( index < 32 )
	  shape_lo[5] |= (1 << index);
	else
	  shape_hi[5] |= (1 << (index - 32));
	index = 8 * (7 - j) + i;
	if ( index < 32 )
	  shape_lo[6] |= (1 << index);
	else
	  shape_hi[6] |= (1 << (index - 32));
	index = 8 * (7 - j) + (7 - i);
	if ( index < 32 )
	  shape_lo[7] |= (1 << index);
	else
	  shape_hi[7] |= (1 << (index - 32));
      }

  /* Get the corner mask */

  corner_mask = get_corner_mask( in_board[11], in_board[81],
				 in_board[18], in_board[88] );

  /* Query the database about all positions in all databases.
     Only games which pass the currently applied filter are scanned.
     Also compute the frequency table and the next move table.
     To speed up sorting the games, the match table is first filled
     with NULLs and when a matching game is found, a pointer to it is
     inserted at a position determined by the field SORT_ORDER
     in the game. As this index is unique, no over-write
     can occur. */

  thor_search.match_count = 0;
  for ( i = 0; i < thor_game_count; i++ )
    thor_search.match_list[i] = NULL;

  for ( i = 0; i <= 64; i++ )
    frequency[i] = 0;

  for ( i = 0; i < 100; i++ ) {
    thor_search.next_move_frequency[i] = 0;
    thor_search.next_move_score[i] = 0.0;
  }

  current_db = database_head;
  while ( current_db != NULL ) {
    for ( i = 0; i < current_db->count; i++ ) {
      game = &current_db->games[i];
      if ( game->passes_filter ) {
	if ( disc_count[BLACKSQ] == game->black_disc_count[move_count] )
	  if ( position_match( game, move_count, side_to_move, shape_lo,
			       shape_hi, corner_mask, target_hash1,
			       target_hash2 ) ) {
	    thor_search.match_list[game->sort_order] = game;
	    symmetry = game->matching_symmetry;
	    if ( move_count < game->move_count ) {
	      next_move =
		symmetry_map[symmetry][abs( (int) game->moves[move_count])];
	      thor_search.next_move_frequency[next_move]++;
	      if ( game->actual_black_score == 32 )
		thor_search.next_move_score[next_move] += 0.5;
	      else if ( game->actual_black_score > 32 ) {
		if ( side_to_move == BLACKSQ )
		  thor_search.next_move_score[next_move] += 1.0;
	      }
	      else {
		if ( side_to_move == WHITESQ )
		  thor_search.next_move_score[next_move] += 1.0;
	      }
	    }
	    frequency[game->actual_black_score]++;
	    thor_search.match_count++;
	  }
      }
    }
    current_db = current_db->next;
  }

  /* Remove the NULLs from the list of matching games if there are any.
     This gives a sorted list. */

  if ( (thor_search.match_count > 0) &&
       (thor_search.match_count < thor_game_count) ) {
    i = 0;
    j = 0;
    while ( i < thor_search.match_count ) {
      if ( thor_search.match_list[j] != NULL ) {
	thor_search.match_list[i] = thor_search.match_list[j];
	i++;
      }
      j++;
    }
  }

  /* Count the number of black wins, draws and white wins.
     Also determine the average score. */

  sum = 0;
  for ( i = 0, thor_search.white_wins = 0; i <= 31; i++ ) {
    thor_search.white_wins += frequency[i];
    sum += i * frequency[i];
  }
  thor_search.draws = frequency[32];
  sum += 32 * frequency[32];
  for ( i = 33, thor_search.black_wins = 0; i <= 64; i++ ) {
    thor_search.black_wins += frequency[i];
    sum += i * frequency[i];
  }
  if ( thor_search.match_count == 0 )  /* Average of 0 values is pointless */
    thor_search.average_black_score = 32.0;
  else
    thor_search.average_black_score = ((double) sum) / thor_search.match_count;

  /* Determine the median score */

  if ( thor_search.match_count == 0 )  /* ...and so is median of 0 values */
    thor_search.median_black_score = 32;
  else {
    cumulative[0] = frequency[0];
    for ( i = 1; i <= 64; i++ )
      cumulative[i] = cumulative[i - 1] + frequency[i];

    /* Median is average between first value for which cumulative
       frequency reaches 50% and first value for which it is
       strictly larger than 50%. This definition works regardless
       of the parity of the number of values.
       By definition of median, both loops terminate for indices <= 64. */

    i = 0;
    while ( 2 * cumulative[i] < thor_search.match_count )
      i++;
    j = i;
    while ( 2 * cumulative[j] < thor_search.match_count + 1 )
      j++;
    thor_search.median_black_score = (i + j) / 2;
  }
}


/*
  GET_THOR_GAME
  Returns all available information about the INDEXth game
  in the list of matching games generated by DATABASE_SEARCH.
*/

GameInfoType
get_thor_game( int index ) {
  GameInfoType info;
  GameType *game;

  if ( (index < 0) || (index >= thor_search.match_count) ) {
    /* Bad index, so fill with empty values */
    info.black_name = "";
    info.white_name = "";
    info.tournament = "";
    info.year = 0;
    info.black_actual_score = 32;
    info.black_corrected_score = 32;
  }
  else {
    /* Copy name fields etc */
    game = thor_search.match_list[index];
    info.black_name = get_player_name( game->black_no );
    info.white_name = get_player_name( game->white_no );
    info.tournament = tournament_name( game->tournament_no );
    info.year = game->database->prolog.origin_year;
    info.black_actual_score = game->actual_black_score;
    info.black_corrected_score = game->perfect_black_score;
  }

  return info;
}



/*
  GET_THOR_GAME_MOVES
  Returns the moves, and number of moves, in the INDEXth game
  in the list of matching games generated by DATABASE_SEARCH.
  The game will not necessarily have the same rotational symmetry
  as the position searched for with database_search(); this depends
  on what rotation that gave a match.
*/

void
get_thor_game_moves( int index,
		     int *move_count,
		     int *moves ) {
  int i;
  GameType *game;

  if ( (index < 0) || (index >= thor_search.match_count) ) {
    /* Bad index, so fill with empty values */
    *move_count = 0;
  }
  else {
    game = thor_search.match_list[index];
    *move_count = game->move_count;
    switch ( game->matching_symmetry ) {
    case 0:  /* Symmetries that preserve the initial position. */
    case 2:
    case 5:
    case 7:
      for ( i = 0; i < game->move_count; i++ )
	moves[i] =
	  symmetry_map[game->matching_symmetry][abs( (int) game->moves[i] )];
      break;

    default:  /* Symmetries that reverse the initial position. */
      for ( i = 0; i < game->move_count; i++ )
	moves[i] = abs( (int) game->moves[i] );
      break;
    }
  }
}



/*
  GET_THOR_GAME_MOVE_COUNT
  Returns the number of moves in the INDEXth game
  in the list of matching games generated by DATABASE_SEARCH.
*/

int
get_thor_game_move_count( int index ) {
  if ( (index < 0) || (index >= thor_search.match_count) )
    /* Bad index */
    return PASS;
  else
    return thor_search.match_list[index]->move_count;
}



/*
  GET_THOR_GAME_MOVE
  Returns the MOVE_NUMBERth move in the INDEXth game
  in the list of matching games generated by DATABASE_SEARCH.
*/

int
get_thor_game_move( int index,
		    int move_number ) {
  if ( (index < 0) || (index >= thor_search.match_count) )
    return PASS;
  else {
    GameType *game = thor_search.match_list[index];
    if ( (move_number < 0) || (move_number >= game->move_count) )
      return PASS;
    else
      return
	symmetry_map[game->matching_symmetry][abs( (int) game->moves[move_number] )];
  }
}



/*
  GET_TOTAL_GAME_COUNT
  GET_MATCH_COUNT
  GET_BLACK_WIN_COUNT
  GET_DRAW_COUNT
  GET_WHITE_WIN_COUNT
  GET_BLACK_MEDIAN_SCORE
  GET_AVERAGE_BLACK_SCORE
  GET_MOVE_FREQUENCY
  GET_MOVE_WIN_RATE
  Accessor functions which return statistics from the last
  query to DATABASE_SEARCH.
*/

INLINE int
get_total_game_count( void ) {
  return thor_game_count;
}

INLINE int
get_match_count( void ) {
  return thor_search.match_count;
}

INLINE int
get_black_win_count( void ) {
  return thor_search.black_wins;
}

INLINE int
get_draw_count( void ) {
  return thor_search.draws;
}

INLINE int
get_white_win_count( void ) {
  return thor_search.white_wins;
}

INLINE int
get_black_median_score( void ) {
  return thor_search.median_black_score;
}

INLINE double
get_black_average_score( void ) {
  return thor_search.average_black_score;
}

INLINE int
get_move_frequency( int move ) {
  return thor_search.next_move_frequency[move];
}

INLINE double
get_move_win_rate( int move ) {
  if ( thor_search.next_move_frequency[move] == 0 )
    return 0.0;
  else
    return thor_search.next_move_score[move] /
      thor_search.next_move_frequency[move];
}



/*
  PRINT_THOR_MATCHES
  Outputs the MAX_GAMES first games found by the latest
  database search to STREAM.
*/

void
print_thor_matches( FILE *stream, int max_games ) {
  int i;

  for ( i = 0; i < MIN( thor_search.match_count, max_games ); i++ ) {
    if ( i == 0 )
      fputs( "\n", stream );
    print_game( stream, thor_search.match_list[i], FALSE );
  }
}


/*
  INIT_THOR_HASH
  Computes hash codes for each of the 6561 configurations of the 8 different
  rows. A special feature of the codes is the relation

     hash[flip[pattern]] == reverse[hash[pattern]]

  which speeds up the computation of the hash functions.
*/

static void
init_thor_hash( void ) {
  int i, j;
  int row[10];
  int flip_row[6561];
  int buffer[6561];

  for ( i = 0; i < 8; i++ )
    row[i] = 0;
  for ( i = 0; i < 6561; i++ ) {
    flip_row[i] = 0;
    for ( j = 0; j < 8; j++ )
      flip_row[i] += row[j] * pow3[7 - j];
    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while ( (row[j - 1] == 0) && (j < 8) );
  }

  for ( i = 0; i < 8; i++ ) {
    for ( j = 0; j < 6561; j++ )
      buffer[j] = abs( my_random() );
    for ( j = 0; j < 6561; j++ )
      primary_hash[i][j] = (buffer[j] & 0xFFFF0000) |
	(bit_reverse_32( buffer[flip_row[j]] ) & 0x0000FFFF);
    for ( j = 0; j < 6561; j++ )
      buffer[j] = abs( my_random() );
    for ( j = 0; j < 6561; j++ )
      secondary_hash[i][j] = (buffer[j] & 0xFFFF0000) |
	(bit_reverse_32( buffer[flip_row[j]] ) & 0x0000FFFF);
  }
}



/*
  INIT_MOVE_MASKS
  Initializes the shape bit masks for each of the possible moves.
*/

static void
init_move_masks( void ) {
  int i, j;
  int pos;
  int index;

  for ( i = 0; i < 4; i++ )
    for ( j = 0, pos = 10 * i + 11; j < 8; j++, pos++ ) {
      index = 8 * i + j;
      move_mask_lo[pos] = (1 << index);
      move_mask_hi[pos] = 0;
      unmove_mask_lo[pos] = ~(1 << index);
      unmove_mask_hi[pos] = ~0;
    }
  for ( i = 0; i < 4; i++ )
    for ( j = 0, pos = 10 * i + 51; j < 8; j++, pos++ ) {
      index = 8 * i + j;
      move_mask_lo[pos] = 0;
      move_mask_hi[pos] = (1 << index);
      unmove_mask_lo[pos] = ~0;
      unmove_mask_hi[pos] = ~(1 << index);
    }
}


/*
  INIT_SYMMETRY_MAPS
  Initializes the mappings which the 8 elements in the board
  symmetry group induce (and their inverses).
  Note: The order of the mappings must coincide with the order
        in which they are calculated in COMPUTE_FULL_PRIMARY_HASH()
	and COMPUTE_FULL_SECONDARY_HASH().
*/

static void
init_symmetry_maps( void ) {
  int i, j, k, pos;

  for ( i = 1; i <= 8; i++ )
    for ( j = 1; j <= 8; j++ ) {
      pos = 10 * i + j;
      b1_b1_map[pos] = pos;
      g1_b1_map[pos] = 10 * i + (9 - j);
      g8_b1_map[pos] = 10 * (9 - i) + (9 - j);
      b8_b1_map[pos] = 10 * (9 - i) + j;
      a2_b1_map[pos] = 10 * j + i;
      a7_b1_map[pos] = 10 * j + (9 - i);
      h7_b1_map[pos] = 10 * (9 - j) + (9 - i);
      h2_b1_map[pos] = 10 * (9 - j) + i;
    }

  symmetry_map[0] = b1_b1_map;
  inv_symmetry_map[0] = b1_b1_map;

  symmetry_map[1] = b8_b1_map;
  inv_symmetry_map[1] = b8_b1_map;

  symmetry_map[2] = a2_b1_map;
  inv_symmetry_map[2] = a2_b1_map;

  symmetry_map[3] = h2_b1_map;
  inv_symmetry_map[3] = a7_b1_map;

  symmetry_map[4] = g1_b1_map;
  inv_symmetry_map[4] = g1_b1_map;

  symmetry_map[5] = g8_b1_map;
  inv_symmetry_map[5] = g8_b1_map; 

  symmetry_map[6] = a7_b1_map;
  inv_symmetry_map[6] = h2_b1_map;

  symmetry_map[7] = h7_b1_map;
  inv_symmetry_map[7] = h7_b1_map;

  for ( i = 0; i < 8; i++ )
    for ( j = 1; j <= 8; j++ )
      for ( k = 1; k <= 8; k++ ) {
	pos = 10 * j + k;
	if ( inv_symmetry_map[i][symmetry_map[i][pos]] != pos )
	  fatal_error( "Error in map %d: inv(map(%d))=%d\n",
		       i, pos, inv_symmetry_map[i][symmetry_map[i][pos]] );
      }
}


/*
  NEW_THOR_OPENING_NODE
  Creates and initializes a new node for use in the opening tree.
*/

static ThorOpeningNode *
new_thor_opening_node( ThorOpeningNode *parent ) {
  ThorOpeningNode *node;

  node = (ThorOpeningNode *) safe_malloc( sizeof( ThorOpeningNode ) );
  node->child_move = 0;
  node->sibling_move = 0;
  node->child_node = NULL;
  node->sibling_node = NULL;
  node->parent_node = parent;

  return node;
}


/*
  CALCULATE_OPENING_FREQUENCY
  Calculates and returns the number of lines in the Thor opening base
  that match the line defined by NODE.
*/

static int
calculate_opening_frequency( ThorOpeningNode *node ) {
  int sum;
  ThorOpeningNode *child;

  child = node->child_node;
  if ( child == NULL )
    return node->frequency;
  else {
    sum = 0;
    do {
      sum += calculate_opening_frequency( child );
      child = child->sibling_node;
    } while ( child != NULL );
    node->frequency = sum;
    return sum;
  }
}


/*
  BUILD_THOR_OPENING_TREE
  Builds the opening tree from the statically computed
  structure THOR_OPENING_LIST (see thorop.c).
*/

static void
build_thor_opening_tree( void ) {
  char thor_move_list[61];
  int i, j;
  int move;
  int branch_depth, end_depth;
  int flipped;
  unsigned int hash1, hash2;
  ThorOpeningNode *parent, *last_child, *new_child;
  ThorOpeningNode *node_list[61];

  /* Create the root node and compute its hash value */

  root_node = new_thor_opening_node( NULL );
  clear_thor_board();
  compute_thor_patterns( thor_board );
  compute_partial_hash( &hash1, &hash2 );
  root_node->hash1 = hash1;
  root_node->hash2 = hash2;
  node_list[0] = root_node;

  /* Add each of the openings to the tree */

  for ( i = 0; i < THOR_LINE_COUNT; i++ ) {
    branch_depth = thor_opening_list[i].first_unique;
    end_depth = branch_depth + strlen( thor_opening_list[i].move_str ) / 2;
    for ( j = 0; j < end_depth - branch_depth; j++ )
      thor_move_list[branch_depth + j] =
	10 * (thor_opening_list[i].move_str[2 * j + 1] - '0') +
	( thor_opening_list[i].move_str[2 * j] - 'a' + 1 );

    /* Play through the moves common with the previous line
       and the first deviation */

    clear_thor_board();
    thor_side_to_move = BLACKSQ;

    for ( j = 0; j <= branch_depth; j++ ) {
      move = thor_move_list[j];
      flipped = any_flips( move, thor_side_to_move, OPP( thor_side_to_move ) );
      if ( flipped ) {
	thor_board[move] = thor_side_to_move;
	thor_side_to_move = OPP( thor_side_to_move );
      }
      else {
	thor_side_to_move = OPP( thor_side_to_move );
	flipped = any_flips( move, thor_side_to_move,
			     OPP( thor_side_to_move ) );
	if ( flipped ) {
	  thor_board[move] = thor_side_to_move;
	  thor_side_to_move = OPP( thor_side_to_move );
	}
	else {
#ifdef TEXT_BASED
	  puts( "This COULD happen (1) in BUILD_THOR_OPENING_TREE" );
#endif
	}
      }
    }

    /* Create the branch from the previous node */

    parent = node_list[branch_depth];
    new_child = new_thor_opening_node( parent );
    compute_thor_patterns( thor_board );
    compute_partial_hash( &hash1, &hash2 );
    new_child->hash1 = hash1;
    new_child->hash2 = hash2;
    if ( parent->child_node == NULL ) {
      parent->child_node = new_child;
      parent->child_move = thor_move_list[branch_depth];
    }
    else {
      last_child = parent->child_node;
      while ( last_child->sibling_node != NULL )
	last_child = last_child->sibling_node;
      last_child->sibling_node = new_child;
      last_child->sibling_move = thor_move_list[branch_depth];
    }
    node_list[branch_depth + 1] = new_child;

    /* Play through the rest of the moves and create new nodes for each
       of the resulting positions */

    for ( j = branch_depth + 1; j < end_depth; j++ ) {
      move = thor_move_list[j];
      flipped = any_flips( move, thor_side_to_move, OPP( thor_side_to_move ) );
      if ( flipped ) {
	thor_board[move] = thor_side_to_move;
	thor_side_to_move = OPP( thor_side_to_move );
      }
      else {
	thor_side_to_move = OPP( thor_side_to_move );
	flipped = any_flips( move, thor_side_to_move,
			     OPP( thor_side_to_move ) );
	if ( flipped ) {
	  thor_board[move] = thor_side_to_move;
	  thor_side_to_move = OPP( thor_side_to_move );
	}
	else {
#ifdef TEXT_BASED
	  puts( "This COULD happen (2) in BUILD_THOR_OPENING_TREE" );
#endif
	}
      }
      parent = new_child;
      new_child = new_thor_opening_node( parent );
      compute_thor_patterns( thor_board );
      compute_partial_hash( &hash1, &hash2 );
      new_child->hash1 = hash1;
      new_child->hash2 = hash2;
      parent->child_node = new_child;
      parent->child_move = thor_move_list[j];
      node_list[j + 1] = new_child;
    }
    new_child->frequency = thor_opening_list[i].frequency;
  }

  /* Calculate opening frequencies also for interior nodes */

  (void) calculate_opening_frequency( root_node );
}


/*
  PRINT_THOR_OPENING_TREE
*/

static void
print_thor_opening_tree( ThorOpeningNode *node, int depth ) {
  char move;
  ThorOpeningNode *child;

  move = node->child_move;
  child = node->child_node;
  while ( child != NULL ) {
#ifdef TEXT_BASED
    printf( "%*s%c%c    %d\n", 2 * depth, "", TO_SQUARE( move ),
	    child->frequency );
#endif
    print_thor_opening_tree( child, depth + 1 );
    move = child->sibling_move;
    child = child->sibling_node;
  }
}



#if FULL_ANALYSIS
/*
  RECURSIVE_FREQUENCY_SEARCH
  Recursively finds and prints all variations which occur in
  at least REQUIRED lines with indiced >= LO and < HI.
*/

static char last_line[64];
static int line_count = 0;
static int node_count = 0;

static int
recursive_frequency_search( MoveListType *list, int required, int common,
			    int lo, int hi ) {
  char move;
  int i, j;
  int frequency;
  int first_unique;
  int child_found;

  child_found = FALSE;
  j = lo;
  while ( j < hi ) {
    i = j;
    move = list[j][common];
    j++;
    while ( (j < hi) && (list[j][common] == move) )
      j++;
    frequency = j - i;
    if ( frequency >= required ) {
      child_found = TRUE;
      recursive_frequency_search( list, required, common + 1, i, j );
    }
  }

  if ( !child_found ) {
    first_unique = 0;
    if ( line_count != 0 )
      for ( i = 0; (i < common) && (list[lo][i] == last_line[i]);
	    i++, first_unique++ )
	;
#ifdef TEXT_BASED
    printf( " { %2d, %6d, %*s\"", first_unique, hi - lo,
	    2 * first_unique, "" );
#endif
    for ( i = first_unique; i < common; i++ ) {
#ifdef TEXT_BASED
      printf( "%c%c", TO_SQUARE( abs( list[lo][i] ) ) );
#endif
      node_count++;
    }
#ifdef TEXT_BASED
    printf( "\" },\n" );
#endif
    line_count++;
    for ( i = 0; i < 64; i++ )
      last_line[i] = list[lo][i];
  }

  return child_found;
}


/*
  STRCMP_WRAPPER
  We don't want no "passing arg 4 from incompatible pointer type", do we?
*/

#ifdef _WIN32_WCE
static int CE_CDECL
#else
static int
#endif
strcmp_wrapper( const void *s1, const void *s2 ) {
  return strcmp( (char *) s1, (char *) s2 );
}


/*
  FREQUENCY_ANALYSIS
  Builds the frequency table to be used in thorop.c.
*/

void
frequency_analysis( int game_count ) {
  int i;
  int game_index;
  DatabaseType *current_db;
  MoveListType *list;

  list = (MoveListType *) safe_malloc( game_count * sizeof( MoveListType ) );

  current_db = database_head;
  game_index = 0;
  while ( current_db != NULL ) {
    for ( i = 0; i < current_db->count; i++ ) {
      memcpy( &list[game_index], &current_db->games[i].moves,
	      current_db->games[i].move_count );
      list[game_index][current_db->games[i].move_count] = 0;
      game_index++;
    }
    current_db = current_db->next;
  }

  qsort( list, game_count, sizeof( MoveListType ), strcmp_wrapper );

  (void) recursive_frequency_search( list, (int) floor( 0.0005 * game_count ),
				     1, 0, game_count - 1 );

  free( list );

#ifdef TEXT_BASED
  printf( "Frequency analysis found %d nodes in %d lines.\n",
	  node_count, line_count );
#endif
}
#endif


/*
  GET_THOR_GAME_SIZE
  Returns the amount of memory which each game in the database takes.
*/

int
get_thor_game_size( void ) {
  return sizeof( GameType );
}


/*
  INIT_THOR_DATABASE
  Performs the basic initializations of the Thor database interface.
  Before any operation on the database may be performed, this function
  must be called.
*/

void
init_thor_database( void ) {
  int i;

  thor_game_count = 0;
  thor_database_count = 0;

  thor_search.match_list = NULL;
  thor_search.allocation = 0;
  thor_search.match_count = 0;
  thor_search.black_wins = 0;
  thor_search.draws = 0;
  thor_search.white_wins = 0;
  thor_search.median_black_score = 0;
  thor_search.average_black_score = 0.0;

  thor_sort_criteria_count = DEFAULT_SORT_CRITERIA;
  for ( i = 0; i < DEFAULT_SORT_CRITERIA; i++ )
    thor_sort_order[i] = default_sort_order[i];

  database_head = NULL;

  players.name_buffer = NULL;
  players.player_list = NULL;
  players.count = 0;

  tournaments.name_buffer = NULL;
  tournaments.tournament_list = NULL;
  tournaments.count = 0;

  thor_games_sorted = FALSE;
  thor_games_filtered = FALSE;

  init_move_masks();

  init_symmetry_maps();

  init_thor_hash();
  prepare_thor_board();
  build_thor_opening_tree();

  filter.game_categories = ALL_GAME_TYPES;
  filter.player_filter = EitherSelectedFilter;
  filter.first_year = -(1 << 25);  /* "infinity" */
  filter.last_year = +(1 << 25);

#if 0
  printf( "    %d\n", root_node->frequency );
  print_thor_opening_tree( root_node, 0 );
#endif
}
