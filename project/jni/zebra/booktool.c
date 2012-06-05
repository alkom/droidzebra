/*
   File:         booktool.c

   Created:      December 31, 1997

   Modified:     December 30, 2004
   
   Author:       Gunnar Andersson (gunnar@radagast.se)

   Contents:     The interface to the book manipulation module.
*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constant.h"
#include "hash.h"
#include "osfbook.h"



#define DEFAULT_CUTOFF          16
#define DEFAULT_MAX_DIFF        24



int
main( int argc, char *argv[] ) {
  char *import_file_name;
  char *input_file_name;
  char *output_file_name;
  char *statistics_file_name;
  char *opening_in_file;
  char *position_file;
  char *opening_file;
  char *merge_script_file, *merge_output_file;
  char *export_file;
  char *merge_book_file;
  enum { MIDGAME_STATISTICS, ENDGAME_STATISTICS } statistics_type;
  double probability;
  double bonus;
  double min_eval_span, max_eval_span;
  double min_negamax_span, max_negamax_span;
  int error;
  int arg_index;
  int max_game_count, max_diff, cutoff;
  int import_games, input_database, output_database;
  int input_binary, output_binary;
  int output_compressed;
  int calculate_minimax, evaluate_all;
  int display_line;
  int do_statistics;
  int max_depth;
  int low_threshold, high_threshold;
  int complete_statistics;
  int give_help;
  int process_openings;
  int uncompress_database;
  int endgame_correct, max_empty, full_solve;
  int dump_positions, first_stage, last_stage;
  int clear_flags;
  int clear_low, clear_high;

  init_osf( TRUE );

  cutoff = DEFAULT_CUTOFF;
  max_diff = DEFAULT_MAX_DIFF;
  max_game_count = 0;
  import_games = FALSE;
  import_file_name = NULL;
  input_database = FALSE;
  input_file_name = NULL;
  input_binary = FALSE;
  output_database = FALSE;
  output_file_name = NULL;
  output_binary = TRUE;
  output_compressed = FALSE;
  uncompress_database = FALSE;
  calculate_minimax = FALSE;
  low_threshold = 60;
  high_threshold = 60;
  bonus = 0.0;
  evaluate_all = FALSE;
  display_line = FALSE;
  do_statistics = FALSE;
  statistics_file_name = NULL;
  probability = 0.0;
  max_depth = 0;
  complete_statistics = FALSE;
  statistics_type = MIDGAME_STATISTICS;
  give_help = FALSE;
  endgame_correct = FALSE;
  max_empty = 0;
  full_solve = FALSE;
  process_openings = FALSE;
  opening_in_file = NULL;
  dump_positions = FALSE;
  position_file = NULL;
  opening_file = NULL;
  merge_script_file = NULL;
  merge_output_file = NULL;
  export_file = NULL;
  merge_book_file = NULL;
  first_stage = 1;
  last_stage = 0;
  clear_flags = 0;
  clear_low = 0;
  clear_high = 60;
  error = FALSE;
  for ( arg_index = 1; (arg_index < argc) && !error; arg_index++ ) {
    if ( !strcasecmp( argv[arg_index], "-i" ) ) {
      import_games = TRUE;
      import_file_name = argv[++arg_index];
      max_game_count = atoi( argv[++arg_index] );
      build_tree( import_file_name, max_game_count, max_diff, cutoff );
    }
    else if ( !strcasecmp( argv[arg_index], "-r" ) ||
	      !strcasecmp( argv[arg_index], "-rb" ) ) {
      if ( input_database ) {
	puts( "Only one database can be read." );
	exit( EXIT_FAILURE );
      }
      if ( import_games ) {
	puts( "Can't load database after having imported games." );
	exit( EXIT_FAILURE );
      }
      input_database = TRUE;
      input_binary = !strcasecmp( argv[arg_index], "-rb" );
      input_file_name = argv[++arg_index];
      if ( input_binary )
	read_binary_database( input_file_name );
      else
	read_text_database( input_file_name );
    }
    else if ( !strcasecmp( argv[arg_index], "-w" ) ||
	      !strcasecmp( argv[arg_index], "-wb" ) ||
	      !strcasecmp( argv[arg_index], "-wc" ) ) {
      output_database = TRUE;
      output_binary = !strcasecmp( argv[arg_index], "-wb" );
      output_compressed = !strcasecmp( argv[arg_index], "-wc" );
      output_file_name = argv[++arg_index];
    }
    else if ( !strcasecmp( argv[arg_index], "-uc" ) ) {
      char *compressed_name = argv[++arg_index];
      char *target_name = argv[++arg_index];

      unpack_compressed_database( compressed_name, target_name );
      uncompress_database = TRUE;
      exit( EXIT_SUCCESS );
    }
    else if ( !strcasecmp( argv[arg_index], "-m" ) )
      calculate_minimax = TRUE;
    else if ( !strcasecmp( argv[arg_index], "-ld" ) ) {
      low_threshold = atoi( argv[++arg_index] );
      high_threshold = atoi( argv[++arg_index] );
      bonus = atof( argv[++arg_index] );
      set_deviation_value( low_threshold, high_threshold, bonus );
    }
    else if ( !strcasecmp( argv[arg_index], "-e" ) )
      evaluate_all = TRUE;
    else if ( !strcasecmp( argv[arg_index], "-d" ) )
      display_line = TRUE;
    else if ( !strcasecmp( argv[arg_index], "-c" ) )
      cutoff = atoi( argv[++arg_index] );
    else if ( !strcasecmp( argv[arg_index], "-pm" ) ) {
      do_statistics = TRUE;
      statistics_type = MIDGAME_STATISTICS;
      max_depth = atoi( argv[++arg_index] );
      probability = atof( argv[++arg_index] );
      max_diff = atoi( argv[++arg_index] );
      statistics_file_name = argv[++arg_index];
    }
    else if ( !strcasecmp( argv[arg_index], "-pe" ) ) {
      do_statistics = TRUE;
      statistics_type = ENDGAME_STATISTICS;
      max_depth = atoi( argv[++arg_index] );
      probability = atof( argv[++arg_index] );
      max_diff = atoi( argv[++arg_index] );
      statistics_file_name = argv[++arg_index];
    }
    else if ( !strcasecmp( argv[arg_index], "-o" ) )
      max_diff = atoi( argv[++arg_index] );
    else if ( !strcasecmp( argv[arg_index], "-l" ) )
      set_search_depth( atoi( argv[++arg_index] ) );
    else if ( !strcasecmp( argv[arg_index], "-evalspan" ) ) {
      min_eval_span = atof( argv[++arg_index] );
      max_eval_span = atof( argv[++arg_index] );
      set_eval_span( min_eval_span, max_eval_span );
    }
    else if ( !strcasecmp( argv[arg_index], "-negspan" ) ) {
      min_negamax_span = atof( argv[++arg_index] );
      max_negamax_span = atof( argv[++arg_index] );
      set_negamax_span( min_negamax_span, max_negamax_span );
    }
    else if ( !strcasecmp( argv[arg_index], "-batch" ) )
      set_max_batch_size( atoi(argv[++arg_index] ) );
    else if ( !strcasecmp( argv[arg_index], "-stat" ) )
      complete_statistics = TRUE;
    else if ( !strcasecmp( argv[arg_index], "-help" ) )
      give_help = TRUE;
    else if ( !strcasecmp( argv[arg_index], "-end" ) ) {
      endgame_correct = TRUE;
      max_empty = atoi( argv[++arg_index] );
      full_solve = atoi( argv[++arg_index] );
    }
    else if ( !strcasecmp( argv[arg_index], "-script" ) )
      set_output_script_name( argv[++arg_index] );
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
    else if ( !strcasecmp( argv[arg_index], "-opgen" ) ) {
      process_openings = TRUE;
      opening_in_file = argv[++arg_index];
    }
    else if ( !strcasecmp( argv[arg_index], "-dump" ) ) {
      dump_positions = TRUE;
      position_file = argv[++arg_index];
      first_stage = atoi( argv[++arg_index] );
      last_stage = atoi( argv[++arg_index] );
    }
    else if ( !strcasecmp( argv[arg_index], "-clearmid" ) )
      clear_flags |= CLEAR_MIDGAME;
    else if ( !strcasecmp( argv[arg_index], "-clearwld" ) )
      clear_flags |= CLEAR_WLD;
    else if ( !strcasecmp( argv[arg_index], "-clearexact" ) )
      clear_flags |= CLEAR_EXACT;
    else if ( !strcasecmp( argv[arg_index], "-clearbounds" ) ) {
      clear_low = atoi( argv[++arg_index] );
      clear_high = atoi( argv[++arg_index] );
    }
    else if ( !strcasecmp( argv[arg_index], "-h" ) ) {
      int hash_bits = atoi( argv[++arg_index] );
      resize_hash( hash_bits );
      printf( "Hash table size changed to %d elements\n", 1 << hash_bits );
    }
    else if ( !strcasecmp( argv[arg_index], "-fb" ) )
      set_black_force( TRUE );
    else if ( !strcasecmp( argv[arg_index], "-fw" ) )
      set_white_force( TRUE );
    else if ( !strcasecmp( argv[arg_index], "-merge" ) ) {
      merge_script_file = argv[++arg_index];
      merge_output_file = argv[++arg_index];
    }
    else if ( !strcasecmp( argv[arg_index], "-export" ) )
      export_file = argv[++arg_index];
    else if ( !strcasecmp( argv[arg_index], "-mergebook" ) )
      merge_book_file = argv[++arg_index];
    else
      error = TRUE;
    if ( arg_index >= argc )
      error = TRUE;
  }
  if ( !import_games && !input_database && !process_openings &&
       !uncompress_database )
    error = TRUE;
  if ( error || give_help ) {
    puts( "Usage:" );
    puts( "  osf [-i <game file> <max #games>]" );
    puts( "      [-r <database> | -rb <database>]" );
    puts( "      [-w <database> | -wb <database> | -wc <database>]" );
    puts( "      [-uc <compressed file> <binary database>]" );
    puts( "      [-c <cutoff>] [-o <outcome>] [-d]" );
    puts( "      [-m] [-e] [-l <depth>]" );
    puts( "      [-ld <low> <high> <bonus>]" );
    puts( "      [{-pm | pe} <depth> <prob> <max diff> <file name>]" );
    puts( "      [-batch <size>] [-stat]" );
    puts( "      [-negspan <min> <max>] [-evalspan <min> <max>]" );
    puts( "      [-end <max empty> <full>]" );
    puts( "      [-script <script name>]" );
    puts( "      [-private] [-public]" );
    puts( "      [-keepdraw] [-draw2black] [-draw2white] [-draw2none]" );
    puts( "      [-opgen <opening list file>]" );
    puts( "      [-dump <position file> <stage lo> <stage hi>]" );
    puts( "      [-clearmid] [-clearwld] [-clearexact]" );
    puts( "      [-clearbounds <low> <high>]" );
    puts( "      [-h <hash bits>]" );
    puts( "      [-fb -fw]" );
    puts( "      [-merge <script file> <output file>]" );
    puts( "      [-mergebook <binary book file>]" );
    puts( "      [-export <file>]" );
    puts( "      [-help]" );
    puts( "" );
    if ( give_help ) {
      puts( "Flags:" );
      puts( "  -i        Imports the game list in <game file>. "
	    "At most <#games> are loaded." );
      puts( "  -r/rb     Reads a database as text (-r) or binary (-rb)." );
      printf( "  -c        Import games up to <cutoff> empties. "
              "(Default: %d)\n", DEFAULT_CUTOFF );
      puts( "            Only applies to subsequent '-i' commands." );
      printf( "  -o        Only import games with result < <outcom>. "
	      "(Default: %d)\n", DEFAULT_MAX_DIFF );
      puts( "            Only applies to subsequent '-i' commands." );
      puts( "  -d        Displays the optimal minimax book line." );
      puts( "  -w/wb/wc  Saves db as text (-w) / binary (-wb) "
	    "/ compressed (wc)." );
      puts( "  -uc       Uncompresses compressed db to binary db." );
      puts( "  -m        Calculate the minimax values of all nodes." );
      puts( "  -ld       Deviations before <high> disks played are given a" );
      puts( "            bonus of <bonus> per disk. "
	    "Before <low> disks played" );
      puts( "            no bonus is given." );
      puts( "            punishment of <punishment> disks per move." );
      puts( "  -e        Evaluates all the nodes in the tree." );
      puts( "  -l        Learn the games using searches to depth <depth>." );
      puts( "  -pm       Generate midgame Prob-Cut statistics." );
      puts( "  -pe       Generate endgame Prob-Cut statistics." );
      puts( "  -evalspan Select nodes with evals in <minspan>-<maxspan>" );
      puts( "  -negspan  Select nodes with negamax in <minspan>-<maxspan>" );
      puts( "  -batch    At most search <size> nodes." );
      puts( "  -stat     Give full statistics for the tree." );
      puts( "  -help     Displays this text." );
      puts( "  -end      Corrects all nodes with <= <empty> disks." );
      puts( "            <full>=0 ==> WLD, otherwise exact score." );
      puts( "  -script   With -end: Positions are written to <script file>." );
      puts( "  -private  Treats all draws as losses for both sides "
	    "(default)." );
      puts( "  -public   No tweaking of draw scores." );
      puts( "  -keepdraw Book draws are counted as draws." );
      puts( "  -draw2black Book draws scored as 32-31 for black." );
      puts( "  -draw2white Book draws scored as 32-31 for white." );
      puts( "  -draw2none  Book draws scored as 32-31 for the opponent." );
      puts( "  -opgen    Converts the openings in <list file> to "
	    "source code." );
      puts( "  -clear*   Removes midgame/wld/full status from nodes." );
      puts( "  -clearbounds   Only remove from <low> to <high> moves." );
      puts( "  -dump     Save scores for positions with <lo> to <hi> moves." );
      puts( "  -h        Changes hash table size to 2^<hash bits>." );
      puts( "  -fb/fw    Force black/white's to only recurse optimal moves." );
      puts( "  -merge    Adds the scores from <output> defined in <script> "
	    "to the book." );
      puts( "  -mergebook  Adds the positions in <book file> to the book.");
      puts( "  -export   Saves all lines in the book to <file>." );
      puts( "" );
      puts( "Gunnar Andersson, December 30, 2004" );
    }
    else
      puts( "Try 'osf -help' for a description of the switches." );
  }
  if ( error )
    exit( EXIT_FAILURE );

  if ( process_openings )
    convert_opening_list( opening_in_file );

  if ( import_games || input_database )
    book_statistics( complete_statistics );

  if ( clear_flags )
    clear_tree( clear_low, clear_high, clear_flags );

  if ( merge_book_file )
    merge_binary_database( merge_book_file );

  if ( evaluate_all )
    evaluate_tree();

  if ( endgame_correct )
    correct_tree( max_empty, full_solve );

  if ( (merge_script_file != NULL) && (merge_output_file != NULL) )
    merge_position_list( merge_script_file, merge_output_file );

  if ( calculate_minimax )
    minimax_tree();

  if ( dump_positions )
    restricted_minimax_tree( first_stage, last_stage, position_file );

  if ( export_file != NULL ) {
    puts( "exporting" );
    export_tree( export_file );
  }

  if ( display_line ) {
    display_doubly_optimal_line( BLACKSQ );
    display_doubly_optimal_line( WHITESQ );
  }

  if ( do_statistics ) {
    if ( statistics_type == MIDGAME_STATISTICS )
      generate_midgame_statistics( max_depth, probability, max_diff * 128,
				   statistics_file_name );
    else
      generate_endgame_statistics( max_depth, probability, max_diff,
				   statistics_file_name );
  }

  if ( output_database ) {
    if ( output_binary )
      write_binary_database( output_file_name );
    else if ( output_compressed )
      write_compressed_database( output_file_name );
    else
      write_text_database( output_file_name );
  }

  return EXIT_SUCCESS;
}
