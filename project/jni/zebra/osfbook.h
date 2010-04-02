/*
   File:            osfbook.h

   Created:         December 31, 1997

   Modified:        December 30, 2002
   
   Author:          Gunnar Andersson (gunnar@radagast.se)

   Contents:        The interface to the book module.
*/



#ifndef OSFBOOK_H
#define OSFBOOK_H



#include "search.h"



#ifdef __cplusplus
extern "C" {
#endif



/* Values denoting absent moves and scores */
#define NONE                      -1
#define NO_MOVE                   -1
#define POSITION_EXHAUSTED        -2
#define NO_SCORE                  9999
#define CONFIRMED_WIN             30000
#define UNWANTED_DRAW             (CONFIRMED_WIN - 1)
#define INFINITE_WIN              32000
#define INFINITE_SPREAD           (1000 * 128)

/* Flag bits and shifts*/
#define NULL_MOVE                 0
#define BLACK_TO_MOVE             1
#define WHITE_TO_MOVE             2
#define WLD_SOLVED                4
#define NOT_TRAVERSED             8
#define FULL_SOLVED               16
#define PRIVATE_NODE              32
#define DEPTH_SHIFT               10

/* This is not a true flag bit but used in the context of generating
   book moves; it must be different from all flag bits though. */
#define DEVIATION                 64

/* Flag bits used by clear_tree() */
#define CLEAR_MIDGAME             1
#define CLEAR_WLD                 2
#define CLEAR_EXACT               4



/* The four different ways to handle draws:
      NEUTRAL:       Draws are counted as +0 for both sides.
      BLACK_WINS:    A draw is treated as a minimal win for black.
      WHITE_WINS:    A draw is treated as a minimal win for white.
      OPPONENT_WINS: A draw is treated as a win for Zebra's opponent.
   Not all draws are actually avoided; see the enum below.
*/

typedef enum { NEUTRAL, BLACK_WINS, WHITE_WINS, OPPONENT_WINS } DrawMode;

/* The two different game modes:
      PRIVATE_GAME:  Assume the opponent to know about all private book nodes.
      PUBLIC_GAME:   Assume the opponent to only know about public nodes.
   This has implications in the context of draw avoidance; the opponent
   is assumed not to know about the private draw lines.
*/

typedef enum { PRIVATE_GAME, PUBLIC_GAME } GameMode;

typedef struct {
  int move;
  int score;
  int flags;
  int parent_flags;
} CandidateMove;



/*
   GET_HASH
   Return the hash values for the current board position.
   The position is rotated so that the 64-bit hash value
   is minimized among all rotations. This ensures detection
   of all transpositions.
   See also init_maps().
*/

void
get_hash( int *val0, int *val1, int *orientation );



void
add_new_game( int move_count, short *game_move_list, int min_empties,
	      int earliest_full_solve, int earliest_wld_solve,
	      int update_path, int private_game );

void
build_tree( const char *file_name, int max_game_count,
	    int max_diff, int min_empties );

void
read_text_database( const char *file_name );

void
read_binary_database( const char *file_name );

void
merge_binary_database( const char *file_name );

void
write_text_database( const char *file_name );

void
write_binary_database( const char *file_name );

void
write_compressed_database( const char *file_name );

void
unpack_compressed_database( const char *in_name, const char *out_name );

void
minimax_tree( void );

void
evaluate_tree( void );

int
validate_tree( void );

#ifdef INCLUDE_BOOKTOOL
void
restricted_minimax_tree( int low, int high, const char *pos_file_name );

void
generate_midgame_statistics( int max_depth, double probability,
			     int max_diff, const char *statistics_file_name );

void
generate_endgame_statistics( int max_depth, double probability,
			     int max_diff, const char *statistics_file_name );

void
clear_tree( int low, int high, int flags );

void
set_output_script_name( const char *script_name );

void
correct_tree( int max_empty, int full_solve );

void
set_black_force( int force );

void
set_white_force( int force );

void
merge_position_list( const char *script_file,
		     const char *output_file );

void
export_tree( const char *file_name );

#endif

void
fill_endgame_hash( int cutoff, int level );

void
book_statistics( int full_statistics );

void
display_doubly_optimal_line( int original_side_to_move );

void
set_slack( int slack );

void
set_search_depth( int depth );

void
set_eval_span( double min_span, double max_span );

void
set_negamax_span( double min_span, double max_span );

void
set_max_batch_size( int size );

void
set_deviation_value( int low_threshold, int high_threshold, double bonus );

void
set_game_mode( GameMode mode );

void
set_draw_mode( DrawMode mode );

void
reset_book_search( void );

int
check_forced_opening( int side_to_move, const char *opening );

void
fill_move_alternatives( int side_to_move, int flags );

int
get_candidate_count( void );

CandidateMove
get_candidate( int index );

void
print_move_alternatives( int side_to_move );

int
get_book_move( int side_to_move, int update_slack, EvaluationType *eval_info );

void
convert_opening_list( const char *base_file );

const char *
find_opening_name( void );

void
init_osf( int do_global_setup );

void
clear_osf( void );



#ifdef __cplusplus
}
#endif



#endif  /* OSFBOOK_H */
