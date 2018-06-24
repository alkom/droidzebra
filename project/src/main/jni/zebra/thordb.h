/*
   File:          thordb.h

   Created:       April 1, 1999
   
   Modified:      August 24, 2002

   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      An interface to the Thor database designed to
                  perform fast lookup operations.
*/



#ifndef THORDB_H
#define THORDB_H



#include <stdio.h>



#ifdef __cplusplus
extern "C" {
#endif



/* The fixed field lengths for entries in the tournament and player
   lists respectively. These are fixed in the specifications for
   the Thor database format. */
#define  TOURNAMENT_NAME_LENGTH      26
#define  PLAYER_NAME_LENGTH          20

/* Bit flags for the three categories of games */
#define  GAME_HUMAN_HUMAN            1
#define  GAME_HUMAN_PROGRAM          2
#define  GAME_PROGRAM_PROGRAM        4
#define  ALL_GAME_TYPES              (GAME_HUMAN_HUMAN |      \
                                      GAME_HUMAN_PROGRAM |    \
                                      GAME_PROGRAM_PROGRAM)

/* Generate the frequency table? */
#define  FULL_ANALYSIS               0

/* The ways the matching games can be sorted */
#define  FIRST_SORT_ORDER            0
#define  SORT_BY_YEAR                0
#define  SORT_BY_YEAR_REVERSED       1
#define  SORT_BY_BLACK_NAME          2
#define  SORT_BY_WHITE_NAME          3
#define  SORT_BY_TOURNAMENT          4
#define  SORT_BY_BLACK_SCORE         5
#define  SORT_BY_WHITE_SCORE         6
#define  LAST_SORT_ORDER             6



typedef struct {
  const char *black_name;
  const char *white_name;
  const char *tournament;
  int year;
  int black_actual_score;
  int black_corrected_score;
} GameInfoType;

typedef struct {
  int year;
  int count;
} DatabaseInfoType;

typedef enum { EitherSelectedFilter, BothSelectedFilter,
	       BlackSelectedFilter, WhiteSelectedFilter } PlayerFilterType;



int
read_player_database( const char *file_name );

const char *
get_player_name( int index );

int get_player_count( void );

int
read_tournament_database( const char *file_name );

const char *
get_tournament_name( int index );

int get_tournament_count( void );

int
read_game_database( const char *file_name );

int
game_database_already_loaded( const char *file_name );

int
get_database_count( void );

void
get_database_info( DatabaseInfoType *info );

int
choose_thor_opening_move( int *in_board, int side_to_move, int echo );

void
set_player_filter( int *selected );

void
set_player_filter_type( PlayerFilterType player_filter );

void
set_tournament_filter( int *selected );

void
set_year_filter( int first_year, int last_year );

void
specify_game_categories( int categories );

void
database_search( int *in_board, int side_to_move );

void
init_thor_database( void );

#if FULL_ANALYSIS
void
frequency_analysis( int game_count );
#endif

int
get_thor_game_size( void );

void
specify_thor_sort_order( int count, int *sort_order );

void
print_thor_matches( FILE *stream, int max_games );

int
get_total_game_count( void );

int
get_match_count( void );

int
get_black_win_count( void );

int
get_draw_count( void );

int
get_white_win_count( void );

int
get_black_median_score( void );

double
get_black_average_score( void );

int
get_move_frequency( int move );

double
get_move_win_rate( int move );

GameInfoType
get_thor_game( int index );

void
get_thor_game_moves( int index, int *move_count, int *moves );

int
get_thor_game_move_count( int index );

int
get_thor_game_move( int index,
		    int move_number );



#ifdef __cplusplus
}
#endif



#endif  /* THORDB_H */
