/*
   File:           zebra.c

   Created:        June 5, 1997
   
   Modified:       December 25, 2005

   Author:         Gunnar Andersson (gunnar@radagast.se)

   Contents:       The module which controls the operation of standalone Zebra.
*/



#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "constant.h"
#include "counter.h"
#include "display.h"
#include "doflip.h"
#include "end.h"
#include "error.h"
#include "eval.h"
#include "game.h"
#include "getcoeff.h"
#include "globals.h"
#include "hash.h"
#include "learn.h"
#include "macros.h"
#include "midgame.h"
#include "moves.h"
#include "myrandom.h"
#include "osfbook.h"
#include "patterns.h"
#include "search.h"
#include "thordb.h"
#include "timer.h"



#define DEFAULT_HASH_BITS         18
#define DEFAULT_RANDOM            TRUE
#define DEFAULT_USE_THOR          FALSE
#define DEFAULT_SLACK             0.25
#define DEFAULT_WLD_ONLY          FALSE
#define SEPARATE_TABLES           FALSE
#define MAX_TOURNAMENT_SIZE       8
#define INFINIT_TIME              10000000.0
#define DEFAULT_DISPLAY_LINE      FALSE

#ifdef SCRZEBRA
#define SCRIPT_ONLY               TRUE
#else
#define SCRIPT_ONLY               FALSE
#endif

#if SCRIPT_ONLY
#define DEFAULT_USE_BOOK          FALSE
#else
#define DEFAULT_USE_BOOK          TRUE
#endif

/* Get rid of some ugly warnings by disallowing usage of the
   macro version of tolower (not time-critical anyway). */
#ifdef tolower
#undef tolower
#endif



/* Local variables */

#if !SCRIPT_ONLY
static double slack = DEFAULT_SLACK;
static double dev_bonus = 0.0;
static int low_thresh = 0;
static int high_thresh = 0;
static int rand_move_freq = 0;
static int tournament = FALSE;
static int tournament_levels;
static int deviation_depth, cutoff_empty;
static int one_position_only = FALSE;
static int use_timer = FALSE;
static int only_analyze = FALSE;
static int thor_max_games;
static int tournament_skill[MAX_TOURNAMENT_SIZE][3];
static int wld_skill[3], exact_skill[3];
#endif

static char *log_file_name;
static double player_time[3], player_increment[3];
static int skill[3];
static int wait;
static int use_book = DEFAULT_USE_BOOK;
static int wld_only = DEFAULT_WLD_ONLY;
static int use_learning;
static int use_thor;



/* ------------------- Function prototypes ---------------------- */

/* Administrative routines */

int
main( int argc, char *argv[] );

#if !SCRIPT_ONLY
static void
play_tournament( const char *move_sequence );

static void
play_game( const char *file_name,
	   const char *move_string,
	   const char *move_file_name,
	   int repeat );

static void
analyze_game( const char *move_string );
#endif

static void
run_endgame_script( const char *in_file_name, const char *out_file_name,
		    int display_line );

/* File handling procedures */

#if !SCRIPT_ONLY
static void
dump_position( int side_to_move );

static void
dump_game_score( int side_to_move );
#endif


/* ---------------------- Functions ------------------------ */


/*
   MAIN
Interprets the command-line parameters and starts the game.
*/

int
main( int argc, char *argv[] ) {
  const char *game_file_name = NULL;
  const char *script_in_file;
  const char *script_out_file;
#if !SCRIPT_ONLY
  const char *move_sequence = NULL;
  const char *move_file_name = NULL;
#endif
  int arg_index;
  int help;
  int hash_bits;
  int use_random;
#if !SCRIPT_ONLY
  int repeat = 1;
#endif
  int run_script;
  int script_optimal_line = DEFAULT_DISPLAY_LINE;
  int komi;
  time_t timer;

#if SCRIPT_ONLY
  printf( "\nscrZebra (c) 1997-2005 Gunnar Andersson, compile "
	  "date %s at %s\n\n", __DATE__, __TIME__ );
#else
  printf( "\nZebra (c) 1997-2005 Gunnar Andersson, compile "
	  "date %s at %s\n\n", __DATE__, __TIME__ );
#endif

  use_random = DEFAULT_RANDOM;
  wait = DEFAULT_WAIT;
  echo = DEFAULT_ECHO;
  display_pv = DEFAULT_DISPLAY_PV;
  use_learning = FALSE;
  use_thor = DEFAULT_USE_THOR;
  skill[BLACKSQ] = skill[WHITESQ] = -1;
  hash_bits = DEFAULT_HASH_BITS;
  game_file_name = NULL;
  log_file_name = NULL;
  run_script = FALSE;
  script_in_file = script_out_file = FALSE;
  komi = 0;
  player_time[BLACKSQ] = player_time[WHITESQ] = INFINIT_TIME;
  player_increment[BLACKSQ] = player_increment[WHITESQ] = 0.0;
  for ( arg_index = 1, help = FALSE; (arg_index < argc) && !help;
	arg_index++ ) {
    if ( !strcasecmp( argv[arg_index], "-e" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      echo = atoi( argv[arg_index] );
    }
    else if ( !strcasecmp( argv[arg_index], "-h" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      hash_bits = atoi( argv[arg_index] );
    }
#if !SCRIPT_ONLY    
    else if ( !strcasecmp( argv[arg_index], "-l" ) ) {
      tournament = FALSE;
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      skill[BLACKSQ] = atoi( argv[arg_index] );
      if ( skill[BLACKSQ] > 0 ) {
	if ( arg_index + 2 >= argc ) {
	  help = TRUE;
	  continue;
	}
	arg_index++;
	exact_skill[BLACKSQ] = atoi( argv[arg_index] );
	arg_index++;
	wld_skill[BLACKSQ] = atoi( argv[arg_index] );
      }
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      skill[WHITESQ] = atoi( argv[arg_index] );
      if ( skill[WHITESQ] > 0 ) {
	if ( arg_index + 2 >= argc ) {
	  help = TRUE;
	  continue;
	}
	arg_index++;
	exact_skill[WHITESQ] = atoi( argv[arg_index] );
	arg_index++;
	wld_skill[WHITESQ] = atoi( argv[arg_index] );
      }
    }
    else if ( !strcasecmp( argv[arg_index], "-t" ) ) {
      int i, j;

      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      tournament = TRUE;
      tournament_levels = atoi( argv[arg_index] );
      if ( arg_index + 3 * tournament_levels >= argc ) {
	help = TRUE;
	continue;
      }
      for ( i = 0; i < tournament_levels; i++ )
	for ( j = 0; j < 3; j++ ) {
	  arg_index++;
	  tournament_skill[i][j] = atoi( argv[arg_index] );
	}
    }
    else if ( !strcasecmp( argv[arg_index], "-w" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      wait = atoi( argv[arg_index] );
    }
    else if ( !strcasecmp( argv[arg_index], "-p" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      display_pv = atoi( argv[arg_index] );
    }
    else if ( !strcasecmp( argv[arg_index], "?" ) )
      help = 1;
    else if ( !strcasecmp( argv[arg_index], "-g" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      game_file_name = argv[arg_index];
    }
    else if ( !strcasecmp( argv[arg_index], "-r" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      use_random = atoi( argv[arg_index] );
    }
#endif
    else if ( !strcasecmp( argv[arg_index], "-b" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      use_book = atoi( argv[arg_index] );
    }
#if !SCRIPT_ONLY
    else if ( !strcasecmp( argv[arg_index], "-time" ) ) {
      if ( arg_index + 4 >= argc ) {
	help = TRUE;
	continue;
      }
      arg_index++;
      player_time[BLACKSQ] = atoi( argv[arg_index] );
      arg_index++;
      player_increment[BLACKSQ] = atoi( argv[arg_index] );
      arg_index++;
      player_time[WHITESQ] = atoi( argv[arg_index] );
      arg_index++;
      player_increment[WHITESQ] = atoi( argv[arg_index] );
      use_timer = TRUE;
    }
    else if ( !strcasecmp(argv[arg_index], "-learn" ) ) {
      if ( arg_index + 2 >= argc ) {
	help = TRUE;
	continue;
      }
      arg_index++;
      deviation_depth = atoi( argv[arg_index] );
      arg_index++;
      cutoff_empty = atoi( argv[arg_index] );
      use_learning = TRUE;
    }
    else if ( !strcasecmp( argv[arg_index], "-slack" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      slack = atof( argv[arg_index] );
    }
    else if ( !strcasecmp( argv[arg_index], "-dev" ) ) {
      if ( arg_index + 3 >= argc ) {
	help = TRUE;
	continue;
      }
      arg_index++;
      low_thresh = atoi( argv[arg_index] );
      arg_index++;
      high_thresh = atoi( argv[arg_index] );
      arg_index++;
      dev_bonus = atof( argv[arg_index] );
    }
    else if ( !strcasecmp( argv[arg_index], "-log" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      log_file_name = argv[arg_index];
    }
    else if ( !strcasecmp( argv[arg_index], "-private" ) )
      set_game_mode( PRIVATE_GAME );
    else if ( !strcasecmp( argv[arg_index], "-public" ) )
      set_game_mode( PUBLIC_GAME );
    else if ( !strcasecmp( argv[arg_index], "-keepdraw" ) )
      set_draw_mode( NEUTRAL );
    else if ( !strcasecmp( argv[arg_index], "-draw2black" ) )
      set_draw_mode( BLACK_WINS );
    else if ( !strcasecmp( argv[arg_index], "-draw2white" ) )
      set_draw_mode( WHITE_WINS );
    else if ( !strcasecmp( argv[arg_index], "-draw2none" ) )
      set_draw_mode( OPPONENT_WINS );
    else if ( !strcasecmp( argv[arg_index], "-test" ) )
      one_position_only = TRUE;
    else if ( !strcasecmp( argv[arg_index], "-seq" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      move_sequence = argv[arg_index];
    }
    else if ( !strcasecmp( argv[arg_index], "-seqfile" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      move_file_name = argv[arg_index];
    }
    else if ( !strcasecmp( argv[arg_index], "-repeat" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      repeat = atoi( argv[arg_index] );
    }
    else if ( !strcasecmp( argv[arg_index], "-thor" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      use_thor = TRUE;
      thor_max_games = atoi( argv[arg_index] );
    }
    else if ( !strcasecmp( argv[arg_index], "-analyze" ) )
      only_analyze = TRUE;
    else if ( !strcasecmp( argv[arg_index], "-randmove" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      rand_move_freq = atoi( argv[arg_index] );
      if ( rand_move_freq < 0 ) {
	help = TRUE;
	continue;
      }
    }
#endif
#if SCRIPT_ONLY
    else if ( !strcasecmp( argv[arg_index], "-wld" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      wld_only = atoi( argv[arg_index] );
    }
    else if ( !strcasecmp( argv[arg_index], "-line" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      script_optimal_line = atoi( argv[arg_index] );
    }
    else if ( !strcasecmp( argv[arg_index], "-script" ) ) {
      if ( arg_index + 2 >= argc ) {
	help = TRUE;
	continue;
      }
      arg_index++;
      script_in_file = argv[arg_index];
      arg_index++;
      script_out_file = argv[arg_index];
      run_script = TRUE;
    }
    else if ( !strcasecmp( argv[arg_index], "-komi" ) ) {
      if ( ++arg_index == argc ) {
	help = TRUE;
	continue;
      }
      komi = atoi( argv[arg_index] );
    }
#endif
    else
      help = 1;
    if ( arg_index >= argc )
      help = 1;
  }

#if SCRIPT_ONLY
  if ( !run_script )
    help = TRUE;
  if ( komi != 0 ) {
    if ( !wld_only ) {
      puts( "Komi can only be applied to WLD solves." );
      exit( EXIT_FAILURE );
    }
    set_komi( komi );
  }
#endif

  if ( help ) {
#if SCRIPT_ONLY
    puts( "Usage:" );
    puts( "  scrzebra [-e ...] [-h ...] [-wld ...] [-line ...] [-b ...] "
	  "[-komi ...] -script ..." );
    puts( "" );
    puts( "  -e <echo?>" );
    printf( "    Toggles screen output on/off (default %d).\n\n",
	    DEFAULT_ECHO );
    puts( "  -h <bits in hash key>" );
    printf( "    Size of hash table is 2^{this value} (default %d).\n\n",
	    DEFAULT_HASH_BITS );
    puts( "  -script <script file> <output file>" );
    puts( "    Solves all positions in script file for exact score.\n" );
    puts( "  -wld <only solve WLD?>" );
    printf( "    Toggles WLD only solve on/off (default %d).\n\n",
	    DEFAULT_WLD_ONLY );
    puts( "  -line <output line?>" );
    printf( "    Toggles output of optimal line on/off (default %d).\n\n",
	    DEFAULT_DISPLAY_LINE );
    puts( "  -b <use book?>" );
    printf( "    Toggles usage of opening book on/off (default %d).\n",
	    DEFAULT_USE_BOOK );
    puts( "" );
    puts( "  -komi <komi>" );
    puts( "    Number of discs that white has to win with (only WLD)." );
    puts( "" );
#else    
    puts( "Usage:" );
    puts( "  zebra [-b -e -g -h -l -p -t -time -w -learn -slack -dev -log" );
    puts( "         -keepdraw -draw2black -draw2white -draw2none" );
    puts( "         -private -public -test -seq -thor -script -analyze ?" );
    puts( "         -repeat -seqfile]" );
    puts( "" );
    puts( "Flags:" );
    puts( "  ? " );
    puts( "    Displays this text." );
    puts( "" );
    puts( "  -b <use book?>" );
    printf( "    Toggles usage of opening book on/off (default %d).\n",
	    DEFAULT_USE_BOOK );
    puts( "" );
    puts( "  -e <echo?>" );
    printf( "    Toggles screen output on/off (default %d).\n",
	    DEFAULT_ECHO );
    puts( "" );
    puts( "  -g <game file>" );
    puts( "" );
    puts( "  -h <bits in hash key>" );
    printf( "    Size of hash table is 2^{this value} (default %d).\n",
	    DEFAULT_HASH_BITS );
    puts( "" );
    puts( "  -l <black depth> [<black exact depth> <black WLD depth>]" );
    puts( "     <white depth> [<white exact depth> <white WLD depth>]" );
    printf( "    Sets the search depth. If <black depth> or <white depth> " );
    printf( "are set to 0, a\n" );
    printf( "    human player is assumed. In this case the other " );
    printf( "parameters must be omitted.\n" );
    printf( "    <* exact depth> specify the number of moves before the " );
    printf( "(at move 60) when\n" );
    printf( "    the exact game-theoretical value is calculated. <* WLD " );
    printf( "depth> are used\n" );
    puts( "    analogously for the calculation of Win/Loss/Draw." );
    puts( "" );
    puts( "  -p <display principal variation?>" );
    printf( "    Toggles output of principal variation on/off (default %d).\n",
	    DEFAULT_DISPLAY_PV );
    puts( "" );
    puts( "  -r <use randomization?>" );
    printf( "    Toggles randomization on/off (default %d)\n",
	    DEFAULT_RANDOM );
    puts( "" );
    puts( "  -t <number of levels> <(first) depth> ... <(last) wld depth>" );
    puts( "" );
    puts( "  -time <black time> <black increment> "
	  "<white time> <white increment>" );
    puts( "    Tournament mode; the format for the players is as above." );
    puts( "" );
    puts( "  -w <wait?>" );
    printf( "    Toggles wait between moves on/off (default %d).\n",
	    DEFAULT_WAIT );
    puts( "" );
    puts( "  -learn <depth> <cutoff>" );
    puts( "    Learn the game with <depth> deviations up to <cutoff> empty." );
    puts( "" );
    puts( "  -slack <disks>" );
    printf( "    Zebra's opening randomness is <disks> disks (default %f).\n",
	   DEFAULT_SLACK );
    puts( "" );
    puts( "  -dev <low> <high> <bonus>" );
    puts( "    Give deviations before move <high> a <bonus> disk bonus but" );
    puts( "    don't give any extra bonus for deviations before move <low>." );
    puts( "" );
    puts( "  -log <file name>" );
    puts( "    Append all game results to the specified file." );
    puts( "" );
    puts( "  -private" );
    puts( "    Treats all draws as losses for both sides." );
    puts( "" );
    puts( "  -public" );
    puts( "    No tweaking of draw scores." );
    puts( "" );
    puts( "  -keepdraw" );
    puts( "    Book draws are counted as draws." );
    puts( "" );
    puts( "  -draw2black" );
    puts( "    Book draws scored as 32-31 for black." );
    puts( "" );
    puts( "  -draw2white" );
    puts( "    Book draws scored as 32-31 for white." );
    puts( "" );
    puts( "  -draw2none" );
    puts( "    Book draws scored as 32-31 for the opponent." );
    puts( "" );
    puts( "  -test" );
    puts( "    Only evaluate one position, then exit." );
    puts( "" );
    puts( "  -seq <move sequence>" );
    puts( "    Forces the game to start with a predefined move sequence;" );
    puts( "    e.g. f4d6c3." );
    puts( "" );
    puts( "  -seqfile <filename" );
    puts( "    Specifies a file from which move sequences are read." );
    puts( "" );
    puts( "  -thor <game count>" );
    puts( "    Look for each position in the Thor database; "
	  "list the first <game count>." );
    puts( "" );
    puts( "  -script <script file> <output file>" );
    puts( "    Solves all positions in script file for exact score." );
    puts( "" );
    puts( "  -wld <only solve WLD?>" );
    printf( "    Toggles WLD only solve on/off (default %d).\n\n",
	    DEFAULT_WLD_ONLY );
    puts( "  -analyze" );
    puts( "    Used in conjunction with -seq; all positions are analyzed." );
    puts( "  -repeat <#iterations>" );
    puts( "    Repeats the operation the specified number of times. " );
#endif
    puts( "" );
    exit( EXIT_FAILURE );
  }
  if ( hash_bits < 1 ) {
    printf( "Hash table key must contain at least 1 bit\n" );
    exit( EXIT_FAILURE );
  }

  global_setup( use_random, hash_bits );
  init_thor_database();

  if ( use_book )
    init_learn( "book.bin", TRUE );
  if ( use_random && !SCRIPT_ONLY ) {
    time( &timer );
    my_srandom( timer );
  }
  else
    my_srandom( 1 );

#if !SCRIPT_ONLY
  if ( !tournament && !run_script ) {
    while ( skill[BLACKSQ] < 0 ) {
      printf( "Black parameters: " );
      scanf( "%d", &skill[BLACKSQ] );
      if ( skill[BLACKSQ] > 0 )
	scanf( "%d %d", &exact_skill[BLACKSQ], &wld_skill[BLACKSQ] );
    }
    while ( skill[WHITESQ] < 0 ) {
      printf( "White parameters: " );
      scanf( "%d", &skill[WHITESQ] );
      if ( skill[WHITESQ] > 0 )
	scanf( "%d %d", &exact_skill[WHITESQ], &wld_skill[WHITESQ] );
    }
  }

  if ( one_position_only )
    toggle_smart_buffer_management( FALSE );
#endif

  if ( run_script )
    run_endgame_script( script_in_file, script_out_file,
			script_optimal_line );
#if !SCRIPT_ONLY
  else {
    if ( tournament )
      play_tournament( move_sequence );
    else {
      if ( only_analyze )
	analyze_game( move_sequence );
      else
	play_game( game_file_name, move_sequence, move_file_name, repeat );
    }
  }
#endif

  global_terminate();

  return EXIT_SUCCESS;
}


#if !SCRIPT_ONLY
/*
   PLAY_TOURNAMENT
   Administrates the tournament between different levels
   of the program.
*/   

static void
play_tournament( const char *move_sequence ) {
  int i, j;
  int result[MAX_TOURNAMENT_SIZE][MAX_TOURNAMENT_SIZE][3];
  double tourney_time;
  double score[MAX_TOURNAMENT_SIZE];
  double color_score[3];
  CounterType tourney_nodes;

  reset_counter( &tourney_nodes );

  tourney_time = 0.0;
  for ( i = 0; i < MAX_TOURNAMENT_SIZE; i++ )
    score[i] = 0.0;
  color_score[BLACKSQ] = color_score[WHITESQ] = 0.0;
  for ( i = 0; i < tournament_levels; i++ )
    for ( j = 0; j < tournament_levels; j++ ) {
      skill[BLACKSQ] = tournament_skill[i][0];
      exact_skill[BLACKSQ] = tournament_skill[i][1];
      wld_skill[BLACKSQ] = tournament_skill[i][2];
      skill[WHITESQ] = tournament_skill[j][0];
      exact_skill[WHITESQ] = tournament_skill[j][1];
      wld_skill[WHITESQ] = tournament_skill[j][2];
      play_game( NULL, move_sequence, NULL, 1 );
      add_counter( &tourney_nodes, &total_nodes );
      tourney_time += total_time;
      result[i][j][BLACKSQ] = disc_count(BLACKSQ);
      result[i][j][WHITESQ] = disc_count(WHITESQ);
      if ( disc_count( BLACKSQ ) > disc_count( WHITESQ ) ) {
	score[i] += 1.0;
	color_score[BLACKSQ] += 1.0;
      }
      else if ( disc_count( BLACKSQ ) == disc_count( WHITESQ ) ) {
	score[i] += 0.5;
	score[j] += 0.5;
	color_score[BLACKSQ] += 0.5;
	color_score[WHITESQ] += 0.5;
      }
      else {
        score[j] += 1.0;
	color_score[WHITESQ] += 1.0;
      }
    }
  adjust_counter( &tourney_nodes );
  printf( "\n\nTime:  %.1f s\nNodes: %.0f\n", tourney_time,
	  counter_value( &tourney_nodes ) );
  puts( "\nCompetitors:" );
  for ( i = 0; i < tournament_levels; i++ ) {
    printf( "  Player %2d: %d-%d-%d\n", i + 1, tournament_skill[i][0],
	    tournament_skill[i][1], tournament_skill[i][2] );
  }
  printf( "\n       " );
  for ( i = 0; i < tournament_levels; i++ )
    printf( " %2d    ", i + 1 );
  puts( "  Score");
  for ( i = 0; i < tournament_levels; i++ ) {
    printf( "  %2d   ", i + 1 );
    for ( j = 0; j < tournament_levels; j++ )
      printf( "%2d-%2d  ", result[i][j][BLACKSQ], result[i][j][WHITESQ] );
    printf( "  %4.1f\n", score[i] );
  }
  puts( "" );
  printf( "Black score: %.1f\n", color_score[BLACKSQ] );
  printf( "White score: %.1f\n", color_score[WHITESQ] );
  puts( "" );
}
#endif


#if !SCRIPT_ONLY
/*
   PLAY_GAME
   Administrates the game between two players, humans or computers.
*/   

static void
play_game( const char *file_name,
	   const char *move_string,
	   const char *move_file_name,
	   int repeat ) {

  EvaluationType eval_info;
  const char *black_name;
  const char *white_name;
  const char *opening_name;
  double node_val, eval_val;
  double move_start, move_stop;
  double database_start, database_stop, total_search_time = 0.0;
  int i;
  int side_to_move;
  int curr_move;
  int timed_search;
  int rand_color = BLACKSQ;
  int black_hash1, black_hash2, white_hash1, white_hash2;
  int provided_move_count;
  int col, row;
  int thor_position_count;
  int provided_move[61];
  char move_vec[121];
  char line_buffer[1000];
  time_t timer;
  FILE *log_file;
  FILE *move_file;

  if ( move_file_name != NULL )
    move_file = fopen( move_file_name, "r" );
  else
    move_file = NULL;

 START:

  /* Decode the predefined move sequence */

  if ( move_file != NULL ) {
    char *newline_pos;

    fgets( line_buffer, sizeof line_buffer, move_file );
    newline_pos = strchr( line_buffer, '\n' );
    if ( newline_pos != NULL )
      *newline_pos = 0;
    move_string = line_buffer;
  }

  if ( move_string == NULL )
    provided_move_count = 0;
  else {
    provided_move_count = strlen(move_string) / 2;
    if ( (provided_move_count > 60) || (strlen( move_string ) % 2 == 1) )
      fatal_error( "Invalid move string provided" );
    for ( i = 0; i < provided_move_count; i++ ) {
      col = tolower( move_string[2 * i] ) - 'a' + 1;
      row = move_string[2 * i + 1] - '0';
      if ( (col < 1) || (col > 8) || (row < 1) || (row > 8) )
	fatal_error( "Unexpected character in move string" );
      provided_move[i] = 10 * row + col;
    }
  }

  /* Set up the position and the search engine */

  game_init( file_name, &side_to_move );
  setup_hash( TRUE );
  clear_stored_game();

  if ( echo && use_book )
    printf( "Book randomness: %.2f disks\n", slack );
  set_slack( floor( slack * 128.0 ) );
  toggle_human_openings( FALSE );

  if ( use_learning )
    set_learning_parameters( deviation_depth, cutoff_empty );
  reset_book_search();
  set_deviation_value( low_thresh, high_thresh, dev_bonus );

  if ( use_thor ) {

#if 1
    /* No error checking done as it's only for testing purposes */

    database_start = get_real_timer();
    (void) read_player_database( "thor\\wthor.jou");
    (void) read_tournament_database( "thor\\wthor.trn" );
    (void) read_game_database( "thor\\wth_2001.wtb" );
    (void) read_game_database( "thor\\wth_2000.wtb" );
    (void) read_game_database( "thor\\wth_1999.wtb" );
    (void) read_game_database( "thor\\wth_1998.wtb" );
    (void) read_game_database( "thor\\wth_1997.wtb" );
    (void) read_game_database( "thor\\wth_1996.wtb" );
    (void) read_game_database( "thor\\wth_1995.wtb" );
    (void) read_game_database( "thor\\wth_1994.wtb" );
    (void) read_game_database( "thor\\wth_1993.wtb" );
    (void) read_game_database( "thor\\wth_1992.wtb" );
    (void) read_game_database( "thor\\wth_1991.wtb" );
    (void) read_game_database( "thor\\wth_1990.wtb" );
    (void) read_game_database( "thor\\wth_1989.wtb" );
    (void) read_game_database( "thor\\wth_1988.wtb" );
    (void) read_game_database( "thor\\wth_1987.wtb" );
    (void) read_game_database( "thor\\wth_1986.wtb" );
    (void) read_game_database( "thor\\wth_1985.wtb" );
    (void) read_game_database( "thor\\wth_1984.wtb" );
    (void) read_game_database( "thor\\wth_1983.wtb" ); 
    (void) read_game_database( "thor\\wth_1982.wtb" );
    (void) read_game_database( "thor\\wth_1981.wtb" );
    (void) read_game_database( "thor\\wth_1980.wtb" );
    database_stop = get_real_timer();
#if FULL_ANALYSIS
    frequency_analysis( get_total_game_count() );
#endif
    printf( "Loaded %d games in %.3f s.\n", get_total_game_count(),
	    database_stop - database_start );
    printf( "Each Thor game occupies %d bytes.\n", get_thor_game_size() );
#else
    {
      /* This code only used for the generation of screen saver analyses */

      GameInfoType thor_game;
      char db_name[100], output_name[100];
      int j;
      int year = 1999;
      int move_count;
      int moves[100];
      FILE *stream;

      database_start = get_real_timer();
      (void) read_player_database( "thor\\wthor.jou");
      (void) read_tournament_database( "thor\\wthor.trn" );
      sprintf( db_name, "thor\\wth_%d.wtb", year );
      (void) read_game_database( db_name );
      database_stop = get_real_timer();
      printf( "Loaded %d games in %.3f s.\n", get_total_game_count(),
	      database_stop - database_start );
    
      database_search( board, side_to_move );
      thor_position_count = get_match_count();
      printf( "%d games match the initial position\n", thor_position_count );

      sprintf( output_name, "wc%d.dbs", year );
      stream = fopen( output_name, "w" );
      if ( stream != NULL ) {
	for ( i = 0; i < thor_position_count; i++ ) {
	  thor_game = get_thor_game( i );
	  if ( strcmp( thor_game.tournament, "Championnat du Monde" ) == 0 ) {
	    fprintf( stream, "%d\n", year );
	    fprintf( stream, "%s\n", thor_game.black_name );
	    fprintf( stream, "%s\n", thor_game.white_name );
	    get_thor_game_moves( i, &move_count, moves );
	    for ( j = 0; j < move_count; j++ )
	      fprintf( stream, "%c%c", TO_SQUARE( moves[j] ) );
	    fputs( "\n", stream );
	  }
	}
	fclose( stream );
      }
    }
#endif
  }

  if ( skill[BLACKSQ] == 0 )
    black_name = "Player";
  else
    black_name = "Zebra";
  if (skill[WHITESQ] == 0)
    white_name = "Player";
  else
    white_name = "Zebra";
  set_names( black_name, white_name );
  set_move_list( black_moves, white_moves, score_sheet_row );
  set_evals( 0.0, 0.0 );

  for ( i = 0; i < 60; i++ ) {
    black_moves[i] = PASS;
    white_moves[i] = PASS;
  }

  move_vec[0] = 0;

  black_hash1 = my_random();
  black_hash2 = my_random();
  white_hash1 = my_random();
  white_hash2 = my_random();
  while ( game_in_progress() ) {
    remove_coeffs( disks_played );
    if ( SEPARATE_TABLES ) {  /* Computer players won't share hash tables */
      if ( side_to_move == BLACKSQ ) {
	hash1 ^= black_hash1;
	hash2 ^= black_hash2;
      }
      else {
	hash1 ^= white_hash1;
	hash2 ^= white_hash2;
      }
    }
    generate_all( side_to_move );

    if ( side_to_move == BLACKSQ )
      score_sheet_row++;

    if ( move_count[disks_played] != 0 ) {
      move_start = get_real_timer();
      clear_panic_abort();

      if ( echo ) {
	set_move_list( black_moves, white_moves, score_sheet_row );
	set_times( floor( player_time[BLACKSQ] ),
		   floor( player_time[WHITESQ] ) );
	opening_name = find_opening_name();
	if ( opening_name != NULL )
	  printf( "\nOpening: %s\n", opening_name );
	if ( use_thor ) {
	  database_start = get_real_timer();
	  database_search( board, side_to_move );
	  thor_position_count = get_match_count();
	  database_stop = get_real_timer();
	  total_search_time += database_stop - database_start;
	  printf( "%d matching games  (%.3f s search time, %.3f s total)\n",
		  thor_position_count, database_stop - database_start,
		  total_search_time );
	  if ( thor_position_count > 0 ) {
	    printf( "%d black wins, %d draws, %d white wins\n",
		    get_black_win_count(), get_draw_count(),
		    get_white_win_count() );
	    printf( "Median score %d-%d", get_black_median_score(),
		    64 - get_black_median_score() );
	    printf( ", average score %.2f-%.2f\n", get_black_average_score(),
		    64.0 - get_black_average_score() );
	  }
	  print_thor_matches( stdout, thor_max_games );
	}
	display_board( stdout, board, side_to_move, TRUE, use_timer, TRUE );
      }
      dump_position( side_to_move );
      dump_game_score( side_to_move );

      /* Check what the Thor opening statistics has to say */

      (void) choose_thor_opening_move( board, side_to_move, echo );

      if ( echo && wait )
	dumpch();
      if ( disks_played >= provided_move_count ) {
	if ( skill[side_to_move] == 0 ) {
	  if ( use_book && display_pv ) {
	    fill_move_alternatives( side_to_move, 0 );
	    if ( echo )
	      print_move_alternatives( side_to_move );
	  }
	  puts( "" );
	  curr_move = get_move( side_to_move );
	}
	else {
	  start_move( player_time[side_to_move],
		      player_increment[side_to_move],
		      disks_played + 4 );
	  determine_move_time( player_time[side_to_move],
			       player_increment[side_to_move],
			       disks_played + 4 );
	  timed_search = (skill[side_to_move] >= 60);
	  toggle_experimental( FALSE );

	  curr_move =
	    compute_move( side_to_move, TRUE, player_time[side_to_move],
			  player_increment[side_to_move], timed_search,
			  use_book, skill[side_to_move],
			  exact_skill[side_to_move], wld_skill[side_to_move],
			  FALSE, &eval_info );
	  if ( side_to_move == BLACKSQ )
	    set_evals( produce_compact_eval( eval_info ), 0.0 );
	  else
	    set_evals( 0.0, produce_compact_eval( eval_info ) );
	  if ( eval_info.is_book && (rand_move_freq > 0) &&
	       (side_to_move == rand_color) &&
	       ((my_random() % rand_move_freq) == 0) ) {
	    puts( "Engine override: Random move selected." );
	    rand_color = OPP( rand_color );
	    curr_move =
	      move_list[disks_played][my_random() % move_count[disks_played]];
	  }
	}
      }
      else {
	curr_move = provided_move[disks_played];
	if ( !valid_move( curr_move, side_to_move ) )
	  fatal_error( "Invalid move %c%c in move sequence",
		       TO_SQUARE( curr_move ) );
      }

      move_stop = get_real_timer();
      if ( player_time[side_to_move] != INFINIT_TIME )
	player_time[side_to_move] -= (move_stop - move_start);

      store_move( disks_played, curr_move );

      sprintf( move_vec + 2 * disks_played, "%c%c", TO_SQUARE( curr_move ) );
      (void) make_move( side_to_move, curr_move, TRUE );
      if ( side_to_move == BLACKSQ )
	black_moves[score_sheet_row] = curr_move;
      else {
	if ( white_moves[score_sheet_row] != PASS )
	  score_sheet_row++;
	white_moves[score_sheet_row] = curr_move;
      }
    }
    else {
      if ( side_to_move == BLACKSQ )
	black_moves[score_sheet_row] = PASS;
      else
	white_moves[score_sheet_row] = PASS;
      if ( skill[side_to_move] == 0 ) {
	puts( "You must pass - please press Enter" );
	dumpch();
      }
    }

    side_to_move = OPP( side_to_move );
    if ( one_position_only )
      break;
  }

  if ( !echo && !one_position_only ) {
    printf( "\n");
    printf( "Black level: %d\n", skill[BLACKSQ] );
    printf( "White level: %d\n", skill[WHITESQ] );
  }

  if ( side_to_move == BLACKSQ )
    score_sheet_row++;
  dump_game_score( side_to_move );
	
  if ( echo && !one_position_only ) {
    set_move_list( black_moves, white_moves, score_sheet_row );
    if ( use_thor ) {
      database_start = get_real_timer();
      database_search( board, side_to_move );
      thor_position_count = get_match_count();
      database_stop = get_real_timer();
      total_search_time += database_stop - database_start;
      printf( "%d matching games  (%.3f s search time, %.3f s total)\n",
	      thor_position_count, database_stop - database_start,
	      total_search_time );
      if ( thor_position_count > 0 ) {
	printf( "%d black wins,  %d draws,  %d white wins\n",
		get_black_win_count(), get_draw_count(),
		get_white_win_count() );
	printf( "Median score %d-%d\n", get_black_median_score(),
		64 - get_black_median_score() );
	printf( ", average score %.2f-%.2f\n", get_black_average_score(),
		64.0 - get_black_average_score() );
      }
      print_thor_matches( stdout, thor_max_games );
    }
    set_times( floor( player_time[BLACKSQ] ), floor( player_time[WHITESQ] ) );
    display_board( stdout, board, side_to_move, TRUE, use_timer, TRUE );
  }

  adjust_counter( &total_nodes );
  node_val = counter_value( &total_nodes );
  adjust_counter( &total_evaluations );
  eval_val = counter_value( &total_evaluations );
  printf( "\nBlack: %d   White: %d\n", disc_count( BLACKSQ ),
	  disc_count( WHITESQ ) );
  printf( "Nodes searched:        %-10.0f\n", node_val );

  printf( "Positions evaluated:   %-10.0f\n", eval_val );

  printf( "Total time: %.1f s\n", total_time );

  if ( (log_file_name != NULL) && !one_position_only )  {
    log_file = fopen( log_file_name, "a" );
    if ( log_file != NULL ) {
      timer = time( NULL );
      fprintf( log_file, "# %s#     %2d - %2d\n", ctime( &timer ),
	       disc_count( BLACKSQ ), disc_count( WHITESQ ) );
      fprintf( log_file, "%s\n", move_vec );
      fclose( log_file );
    }
  }

  repeat--;

  toggle_abort_check( FALSE );
  if ( use_learning && !one_position_only )
    learn_game( disks_played, (skill[BLACKSQ] != 0) && (skill[WHITESQ] != 0),
		repeat == 0 );
  toggle_abort_check( TRUE );

  if ( repeat > 0 )
    goto START;

  if ( move_file != NULL )
    fclose( move_file );
}



/*
   ANALYZE_GAME
   Analyzes all positions arising from a given move sequence.
*/   

static void
analyze_game( const char *move_string ) {
  EvaluationType best_info1, best_info2, played_info1, played_info2;
  const char *black_name, *white_name;
  const char *opening_name;
  double move_start, move_stop;
  int i;
  int side_to_move, opponent;
  int curr_move, resp_move;
  int timed_search;
  int black_hash1, black_hash2, white_hash1, white_hash2;
  int provided_move_count;
  int col, row;
  int empties;
  int provided_move[61];
  unsigned int best_trans1, best_trans2, played_trans1, played_trans2;
  FILE *output_stream;

  /* Decode the predefined move sequence */

  if ( move_string == NULL )
    provided_move_count = 0;
  else {
    provided_move_count = strlen(move_string) / 2;
    if ( (provided_move_count > 60) || (strlen( move_string ) % 2 == 1) )
      fatal_error( "Invalid move string provided" );
    for ( i = 0; i < provided_move_count; i++ ) {
      col = tolower( move_string[2 * i] ) - 'a' + 1;
      row = move_string[2 * i + 1] - '0';
      if ( (col < 1) || (col > 8) || (row < 1) || (row > 8) )
	fatal_error( "Unexpected character in move string" );
      provided_move[i] = 10 * row + col;
    }
  }

  /* Open the output log file */

  output_stream = fopen( "analysis.log", "w" );
  if ( output_stream == NULL )
    fatal_error( "Can't create log file analysis.log - aborting" );

  /* Set up the position and the search engine */

  if ( echo )
    puts( "Analyzing provided game..." );

  game_init( NULL, &side_to_move );
  setup_hash( TRUE );
  clear_stored_game();

  if ( echo && use_book )
    puts( "Disabling usage of opening book" );
  use_book = FALSE;

  reset_book_search();

  black_name = "Zebra";
  white_name = "Zebra";
  set_names( black_name, white_name );
  set_move_list( black_moves, white_moves, score_sheet_row );
  set_evals( 0.0, 0.0 );

  for ( i = 0; i < 60; i++ ) {
    black_moves[i] = PASS;
    white_moves[i] = PASS;
  }

  black_hash1 = my_random();
  black_hash2 = my_random();
  white_hash1 = my_random();
  white_hash2 = my_random();

  best_trans1 = my_random();
  best_trans2 = my_random();
  played_trans1 = my_random();
  played_trans2 = my_random();

  while ( game_in_progress() && (disks_played < provided_move_count) ) {
    remove_coeffs( disks_played );
    if ( SEPARATE_TABLES ) {  /* Computer players won't share hash tables */
      if ( side_to_move == BLACKSQ ) {
	hash1 ^= black_hash1;
	hash2 ^= black_hash2;
      }
      else {
	hash1 ^= white_hash1;
	hash2 ^= white_hash2;
      }
    }
    generate_all( side_to_move );

    if ( side_to_move == BLACKSQ )
      score_sheet_row++;

    if ( move_count[disks_played] != 0 ) {
      move_start = get_real_timer();
      clear_panic_abort();

      if ( echo ) {
	set_move_list( black_moves, white_moves, score_sheet_row );
	set_times( floor( player_time[BLACKSQ] ),
		   floor( player_time[WHITESQ] ) );
	opening_name = find_opening_name();
	if ( opening_name != NULL )
	  printf( "\nOpening: %s\n", opening_name );

	display_board( stdout, board, side_to_move, TRUE, use_timer, TRUE );
      }

      /* Check what the Thor opening statistics has to say */

      (void) choose_thor_opening_move( board, side_to_move, echo );

      if ( echo && wait )
	dumpch();

      start_move( player_time[side_to_move],
		  player_increment[side_to_move],
		  disks_played + 4 );
      determine_move_time( player_time[side_to_move],
			   player_increment[side_to_move],
			   disks_played + 4 );
      timed_search = (skill[side_to_move] >= 60);
      toggle_experimental( FALSE );

      empties = 60 - disks_played;

      /* Determine the score for the move actually played.
         A private hash transformation is used so that the parallel
	 search trees - "played" and "best" - don't clash. This way
	 all scores are comparable. */

#if 0
      set_hash_transformation( my_random(), my_random() );
#else
      set_hash_transformation( played_trans1, played_trans2 );
#endif

      curr_move = provided_move[disks_played];
      opponent = OPP( side_to_move );
      (void) make_move( side_to_move, curr_move, TRUE );
      if ( empties > wld_skill[side_to_move] ) {
	reset_counter( &nodes );
	resp_move = compute_move( opponent, FALSE, player_time[opponent],
				  player_increment[opponent], timed_search,
				  use_book, skill[opponent] - 2,
				  exact_skill[opponent] - 1,
				  wld_skill[opponent] - 1, TRUE,
				  &played_info1 );
      }
      reset_counter( &nodes );
      resp_move = compute_move( opponent, FALSE, player_time[opponent],
			   player_increment[opponent], timed_search,
			   use_book, skill[opponent] - 1,
			   exact_skill[opponent] - 1,
			   wld_skill[opponent] - 1, TRUE, &played_info2 );

      unmake_move( side_to_move, curr_move );

      /* Determine the 'best' move and its score. For midgame moves,
	 search twice to dampen oscillations. Unless we're in the endgame
	 region, a private hash transform is used - see above. */

      if ( empties > wld_skill[side_to_move] ) {
#if 0
	set_hash_transformation( my_random(), my_random() );
#else
	set_hash_transformation( best_trans1, best_trans2 );
#endif
	reset_counter( &nodes );
	curr_move =
	  compute_move( side_to_move, FALSE, player_time[side_to_move],
			player_increment[side_to_move], timed_search,
			use_book, skill[side_to_move] - 1,
			exact_skill[side_to_move], wld_skill[side_to_move],
			TRUE, &best_info1 );
      }
      reset_counter( &nodes );
      curr_move =
	compute_move( side_to_move, FALSE, player_time[side_to_move],
		      player_increment[side_to_move], timed_search,
		      use_book, skill[side_to_move],
		      exact_skill[side_to_move], wld_skill[side_to_move],
		      TRUE, &best_info2 );

      if ( side_to_move == BLACKSQ )
	set_evals( produce_compact_eval( best_info2 ), 0.0 );
      else
	set_evals( 0.0, produce_compact_eval( best_info2 ) );

      /* Output the two score-move pairs */

      fprintf( output_stream, "%c%c ", TO_SQUARE( curr_move ) );
      if ( empties <= exact_skill[side_to_move] )
	fprintf( output_stream, "%+6d", best_info2.score / 128 );
      else if ( empties <= wld_skill[side_to_move] ) {
	if ( best_info2.res == WON_POSITION )
	  fputs( "    +1", output_stream );
	else if ( best_info2.res == LOST_POSITION )
	  fputs( "    -1", output_stream );
	else
	  fputs( "     0", output_stream );
      }
      else {
	/* If the played move is the best, output the already calculated
	   score for the best move - that way we avoid a subtle problem:
	   Suppose (N-1)-ply move is X but N-ply move is Y, where Y is
	   the best move. Then averaging the corresponding scores won't
	   coincide with the N-ply averaged score for Y. */
	if ( (curr_move == provided_move[disks_played]) &&
	     (resp_move != PASS) )
	  fprintf( output_stream, "%6.2f",
		   -(played_info1.score + played_info2.score) / (2 * 128.0) );
	else
	  fprintf( output_stream, "%6.2f",
		   (best_info1.score + best_info2.score) / (2 * 128.0) );
      }

      curr_move = provided_move[disks_played];

      fprintf( output_stream, "       %c%c ", TO_SQUARE( curr_move ) );
      if ( resp_move == PASS )
	fprintf( output_stream, "     ?" );
      else if ( empties <= exact_skill[side_to_move] )
	fprintf( output_stream, "%+6d", -played_info2.score / 128 );
      else if ( empties <= wld_skill[side_to_move] ) {
	if ( played_info2.res == WON_POSITION )
	  fputs( "    -1", output_stream );
	else if ( played_info2.res == LOST_POSITION )
	  fputs( "    +1", output_stream );
	else
	  fputs( "     0", output_stream );
      }
      else
	fprintf( output_stream, "%6.2f",
		 -(played_info1.score + played_info2.score) / (2 * 128.0) );
      fputs( "\n", output_stream );

      if ( !valid_move( curr_move, side_to_move ) )
	fatal_error( "Invalid move %c%c in move sequence",
		       TO_SQUARE( curr_move ) );

      move_stop = get_real_timer();
      if ( player_time[side_to_move] != INFINIT_TIME )
	player_time[side_to_move] -= (move_stop - move_start);

      store_move( disks_played, curr_move );

      (void) make_move( side_to_move, curr_move, TRUE );
      if ( side_to_move == BLACKSQ )
	black_moves[score_sheet_row] = curr_move;
      else {
	if ( white_moves[score_sheet_row] != PASS )
	  score_sheet_row++;
	white_moves[score_sheet_row] = curr_move;
      }
    }
    else {
      if ( side_to_move == BLACKSQ )
	black_moves[score_sheet_row] = PASS;
      else
	white_moves[score_sheet_row] = PASS;
    }

    side_to_move = OPP( side_to_move );
  }

  if ( !echo ) {
    printf("\n");
    printf("Black level: %d\n", skill[BLACKSQ]);
    printf("White level: %d\n", skill[WHITESQ]);
  }

  if ( side_to_move == BLACKSQ )
    score_sheet_row++;
  dump_game_score( side_to_move );
	
  if ( echo && !one_position_only ) {
    set_move_list( black_moves, white_moves, score_sheet_row );
    set_times( floor( player_time[BLACKSQ] ), floor( player_time[WHITESQ] ) );
    display_board( stdout, board, side_to_move, TRUE, use_timer, TRUE );
  }

  fclose( output_stream );
}
#endif


/*
  RUN_ENDGAME_SCRIPT
*/

#define BUFFER_SIZE           256

static void
run_endgame_script( const char *in_file_name,
		    const char *out_file_name,
		    int display_line ) {
  CounterType script_nodes;
  EvaluationType eval_info;
  char *comment;
  char buffer[BUFFER_SIZE];
  char board_string[BUFFER_SIZE], stm_string[BUFFER_SIZE];
  double start_time, stop_time;
  double search_start, search_stop, max_search;
  int i, j;
  int row, col, pos;
  int book, mid, exact, wld;
  int my_time, my_incr;
  int side_to_move, move;
  int score;
  int timed_search;
  int scanned, token;
  int position_count;
  FILE *script_stream;
  FILE *output_stream;

  /* Open the files and get the number of positions */

  script_stream = fopen( in_file_name, "r" );
  if ( script_stream == NULL ) {
    printf( "\nCan't open script file '%s' - aborting\n\n", in_file_name );
    exit( EXIT_FAILURE );
  }

  output_stream = fopen( out_file_name, "w" );
  if ( output_stream == NULL ) {
    printf( "\nCan't create output file '%s' - aborting\n\n", out_file_name );
    exit( EXIT_FAILURE );
  }
  fclose( output_stream );

  /* Initialize display subsystem and search parameters */

  set_names( "", "" );
  set_move_list( black_moves, white_moves, score_sheet_row );
  set_evals( 0.0, 0.0 );

  for ( i = 0; i < 60; i++ ) {
    black_moves[i] = PASS;
    white_moves[i] = PASS;
  }

  my_time = 100000000;
  my_incr = 0;
  timed_search = FALSE;
  book = use_book;
  mid = 60;
  if ( wld_only )
    exact = 0;
  else
    exact = 60;
  wld = 60;

  toggle_status_log( FALSE );

  reset_counter( &script_nodes );
  position_count = 0;
  max_search = -0.0;
  start_time = get_real_timer();

  /* Scan through the script file */

  for ( i = 0; ; i++ ) {
    int pass_count = 0;

    /* Check if the line is a comment or an end marker */

    fgets( buffer, BUFFER_SIZE, script_stream );
    if ( feof( script_stream ) )
      break;
    if ( buffer[0] == '%' ) {  /* Comment */
      output_stream = fopen( out_file_name, "a" );
      if ( output_stream == NULL ) {
	printf( "\nCan't append to output file '%s' - aborting\n\n",
		out_file_name );
	exit( EXIT_FAILURE );
      }
      fputs( buffer, output_stream );
      fclose( output_stream );
      if ( strstr( buffer, "% End of the endgame script" ) == buffer )
	break;
      else 
	continue;
    }

    if ( feof( script_stream ) ) {
      printf( "\nEOF encountered when reading position #%d - aborting\n\n",
	      i + 1 );
      exit( EXIT_FAILURE );
    }

    /* Parse the script line containing board and side to move */
    
    game_init( NULL, &side_to_move );
    set_slack( 0.0 );
    toggle_human_openings( FALSE );
    reset_book_search();
    set_deviation_value( 0, 60, 0.0 );
    setup_hash( TRUE );
    position_count++;

    scanned = sscanf( buffer, "%s %s", board_string, stm_string );
    if ( scanned != 2 ) {
      printf( "\nError parsing line %d - aborting\n\n", i + 1 );
      exit( EXIT_FAILURE );
    }

    if ( strlen( stm_string ) != 1 ) {
      printf( "\nAmbiguous side to move on line %d - aborting\n\n", i + 1 );
      exit( EXIT_FAILURE );
    }
    switch ( stm_string[0] ) {
    case 'O':
    case '0':
      side_to_move = WHITESQ;
      break;
    case '*':
    case 'X':
      side_to_move = BLACKSQ;
      break;
    default:
      printf( "\nBad side-to-move indicator on line %d - aborting\n\n",
	      i + 1 );
    }

    if ( strlen( board_string ) != 64 ) {
      printf( "\nBoard on line %d doesn't contain 64 positions - aborting\n\n",
	      i + 1 );
      exit( EXIT_FAILURE );
    }

    token = 0;
    for ( row = 1; row <= 8; row++ )
      for ( col = 1; col <= 8; col++ ) {
	pos = 10 * row + col;
	switch ( board_string[token] ) {
	case '*':
	case 'X':
	case 'x':
	  board[pos] = BLACKSQ;
	  break;
	case 'O':
	case '0':
	case 'o':
	  board[pos] = WHITESQ;
	  break;
	case '-':
	case '.':
	  board[pos] = EMPTY;
	  break;
	default:
	  printf( "\nBad character '%c' in board on line %d - aborting\n\n",
		  board_string[token], i + 1 );
	  break;
	}
	token++;
      }
    disks_played = disc_count( BLACKSQ ) + disc_count( WHITESQ ) - 4;

    /* Search the position */

    if ( echo ) {
      set_move_list( black_moves, white_moves, score_sheet_row );
      display_board( stdout, board, side_to_move, TRUE, FALSE, TRUE );
    }

    search_start = get_real_timer();
    start_move( my_time, my_incr, disks_played + 4 );
    determine_move_time( my_time, my_incr, disks_played + 4 );

    pass_count = 0;
    move = compute_move( side_to_move, TRUE, my_time, my_incr, timed_search,
			 book, mid, exact, wld, TRUE, &eval_info );
    if ( move == PASS ) {
      pass_count++;
      side_to_move = OPP( side_to_move );
      move = compute_move( side_to_move, TRUE, my_time, my_incr, timed_search,
			   book, mid, exact, wld, TRUE, &eval_info );
      if ( move == PASS ) {  /* Both pass, game over. */
	int my_discs = disc_count( side_to_move );
	int opp_discs = disc_count( OPP( side_to_move ) );
	if ( my_discs > opp_discs )
	  my_discs = 64 - opp_discs;
	else if ( opp_discs > my_discs )
	  opp_discs = 64 - my_discs;
	else
	  my_discs = opp_discs = 32;
	eval_info.score = 128 * (my_discs - opp_discs);
	pass_count++;
      }
    }

    score = eval_info.score / 128;
    search_stop = get_real_timer();
    if ( search_stop - search_start > max_search )
      max_search = search_stop - search_start;
    add_counter( &script_nodes, &nodes );

    output_stream = fopen( out_file_name, "a" );
    if ( output_stream == NULL ) {
      printf( "\nCan't append to output file '%s' - aborting\n\n",
	      out_file_name );
      exit( EXIT_FAILURE );
    }
    if ( wld_only ) {
      if ( side_to_move == BLACKSQ ) {
	if ( score > 0 )
	  fputs( "Black win", output_stream );
	else if ( score == 0 )
	  fputs( "Draw", output_stream );
	else
	  fputs( "White win", output_stream );
      }
      else {
	if ( score > 0 )
	  fputs( "White win", output_stream );
	else if ( score == 0 )
	  fputs( "Draw", output_stream );
	else
	  fputs( "Black win", output_stream );
      }
    }
    else {
      if ( side_to_move == BLACKSQ )
	fprintf( output_stream, "%2d - %2d",
		 32 + (score / 2), 32 - (score / 2) );
      else
	fprintf( output_stream, "%2d - %2d",
		 32 - (score / 2), 32 + (score / 2) );
    }
    if ( display_line && (pass_count != 2) ) {
      fputs( "   ", output_stream );
      if ( pass_count == 1 )
	fputs( " --", output_stream );
      for ( j = 0; j < full_pv_depth; j++ ) {
	fputs( " ", output_stream );
	display_move( output_stream, full_pv[j] );
      }
    }
    comment = strstr( buffer, "%" );
    if ( comment != NULL )  /* Copy comment to output file */
      fprintf( output_stream, "      %s", comment );
    else
      fputs( "\n", output_stream );
    fclose( output_stream );

    if ( echo )
      puts( "\n\n\n" );
  }

  /* Clean up and terminate */

  fclose( script_stream );

  stop_time = get_real_timer();

  printf( "Total positions solved:   %d\n", position_count );
  printf( "Total time:               %.1f s\n", stop_time - start_time );
  printf( "Total nodes:              %.0f\n", counter_value( &script_nodes ) );
  puts( "" );
  printf( "Average time for solve:   %.1f s\n",
	  (stop_time - start_time) / position_count );
  printf( "Maximum time for solve:   %.1f s\n", max_search );
  puts( "" );
  printf( "Average speed:            %.0f nps\n",
	  counter_value( &script_nodes ) / (stop_time - start_time ) );
  puts( "" );
}


#if !SCRIPT_ONLY
/*
   DUMP_POSITION
   Saves the current board position to disk.
*/

static void
dump_position( int side_to_move ) {
  int i, j;
  FILE *stream;

  stream = fopen( "current.gam", "w" );
  if ( stream == NULL )
    fatal_error( "File creation error when writing CURRENT.GAM\n" );

  for ( i = 1; i <= 8; i++ )
    for ( j = 1; j <= 8; j++ )
      switch ( board[10 * i + j] ) {
      case BLACKSQ:
	fputc( 'X', stream );
	break;

      case WHITESQ:
	fputc( 'O', stream );
	break;

      case EMPTY:
	fputc( '-', stream );
	break;

      default:  /* This really can't happen but shouldn't cause a crash */
	fputc( '?', stream );
	break;
      }
  fputs( "\n", stream );
  if ( side_to_move == BLACKSQ )
    fputs( "Black", stream );
  else
    fputs( "White", stream );
  fputs( " to move\nThis file was automatically generated\n", stream );
         
  fclose( stream );
}


/*
  DUMP_GAME_SCORE
  Writes the current game score to disk.
*/

static void
dump_game_score( int side_to_move ) {
  FILE *stream;
  int i;

  stream = fopen( "current.mov", "w" );
  if ( stream == NULL )
    fatal_error( "File creation error when writing CURRENT.MOV\n" );

  for ( i = 0; i <= score_sheet_row; i++ ) {
    fprintf( stream, "   %2d.    ", i + 1 );
    if ( black_moves[i] == PASS )
      fputs( "- ", stream );
    else
      fprintf( stream, "%c%c", TO_SQUARE( black_moves[i] ) );
    fputs( "  ", stream );
    if ( (i < score_sheet_row) ||
	 ((i == score_sheet_row) && (side_to_move == BLACKSQ)) ) {
      if ( white_moves[i] == PASS )
	fputs( "- ", stream );
      else
	fprintf( stream, "%c%c", TO_SQUARE( white_moves[i] ) );
    }
    fputs( "\n", stream );
  }
  fclose( stream );
}
#endif
