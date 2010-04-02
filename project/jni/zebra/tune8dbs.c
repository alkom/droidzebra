/*
   File:          tune8dbs.c

   Created:       July 25, 1998

   Modified:      December 20, 2001
   
   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      The database is analyzed with statistical methods;
                  feature values are determined using a damped variant
                  of Fletcher-Reeves' method on the sum of the squared
                  errors.
*/


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* Define the inline directive when available */

#ifdef __GNUC__
  #define INLINE __inline__
#else
  #define INLINE
#endif


/* Common constants */
#define TRUE                    1
#define FALSE                   0

/* These values are only used in the initialization phase */
#define UPDATE_BOUND             0.50

/* What to use and what not to use */
#define USE_PARITY               1
#define USE_A_FILE               0
#define USE_A_FILE2X             1
#define USE_B_FILE               1
#define USE_C_FILE               1
#define USE_D_FILE               1
#define USE_DIAG8                1
#define USE_DIAG7                1
#define USE_DIAG6                1
#define USE_DIAG5                1
#define USE_DIAG4                1
#define USE_CORNER52             1
#define USE_CORNER33             1

/* Miscellaneous parameters */
#define NULL_MOVE                0
#define DEFAULT_MAX_POSITIONS    10000
#define DEFAULT_BUFFER_SIZE      1000

/* Side-to-move values (from constant.h) */

#define BLACKSQ                 0
#define EMPTY                   1
#define WHITESQ                 2

/* Macros */

#define MIN(a,b)                ((a < b) ? a : b)
#define SQR(a)                  ((a) * (a))


/* All info needed for each pattern during the optimization process */

typedef struct {
  double solution;
  double gradient, direction;
  int pattern;
  int frequency;
  int most_common;
} InfoItem;

/* A compact 128-bit position representation + flags used
   in the parameter tuning. */

typedef struct {
  short row_bit_vec[8];
  short side_to_move;
  short score;
  short stage;
} CompactPosition;
   

double objective;
double abs_error_sum;
double max_delta, average_delta;
double delta_sum;
double quad_coeff, lin_coeff, const_coeff;
double total_weight;
double weight[61];
int stage_count;
int analysis_stage;
int update_count;
int last_active;
int max_positions, position_count;
int max_diff;
int relevant_count, node_count, interval;
int buffer_size, node_buffer_pos, short_buffer_pos;
int node_allocations, short_allocations;
int stage[64];
int active[61];
int compact[1048576];
int mirror[6561];
int flip52[59049];
int mirror7[2187];
int mirror6[729];
int mirror5[243];
int mirror4[81];
int mirror3[27];
int mirror82x[59049];
int identity10[59049];
int flip33[19683];
int mirror33[19683];
int board[100];

int row_pattern[8];
int col_pattern[8];
int diag1_pattern[15];
int diag2_pattern[15];
int row_no[100];
int row_index[100];
int col_no[100];
int col_index[100];
int diag1_no[100];
int diag1_index[100];
int diag2_no[100];
int diag2_index[100];

short *short_buffer;
CompactPosition *position_list;
InfoItem constant;
InfoItem parity;
InfoItem afile[6561], bfile[6561], cfile[6561], dfile[6561];
InfoItem corner52[59049];
InfoItem diag8[6561], diag7[2187], diag6[729], diag5[243], diag4[81];
InfoItem corner33[19683];
InfoItem afile2x[59049];

int inverse4[81];
int inverse5[243];
int inverse6[729];
int inverse7[2187];
int inverse8[6561];
int inverse9[19683];
int inverse10[59049];



/*
  PACK_POSITION
  Pack the information from the line BUFFER into node #INDEX
  in POSITION_LIST. Returns 1 if the position was incorporated,
  otherwise 0.
*/

int pack_position( char *buffer, int index ) {
  int black_mask, white_mask;
  int i, j;
  int mask;
  int stage, score;
  unsigned char hi_mask, lo_mask;

  sscanf( buffer + 34, "%d %d", &stage, &score );
  if ( abs( score ) > max_diff )
    return 0;

  for ( i = 0; i < 8; i++ ) {
    sscanf( buffer + 4 * i, "%c%c", &hi_mask, &lo_mask );
    hi_mask -= ' ';
    lo_mask -= ' ';
    black_mask = (hi_mask << 4) + lo_mask;
    sscanf( buffer + 4 * i + 2, "%c%c", &hi_mask, &lo_mask );
    hi_mask -= ' ';
    lo_mask -= ' ';
    white_mask = (hi_mask << 4) + lo_mask;
    mask = 0;
    for ( j = 0; j < 8; j++ ) {
      mask *= 4;
      if ( black_mask & (1 << j) )
	mask += BLACKSQ;
      else if ( white_mask & (1 << j) )
	mask += WHITESQ;
      else
	mask += EMPTY;
    }
    position_list[index].row_bit_vec[i] = mask;
  }

  switch ( buffer[33] ) {
  case '*':
    position_list[index].side_to_move = BLACKSQ;
    position_list[index].score = score;
    break;
  case 'O':
    position_list[index].side_to_move = WHITESQ;
    position_list[index].score = -score;
    break;
  default:
    printf( "Invalid side to move indicator on line %d in input file\n",
	    index );
    break;
  }

  position_list[index].stage = stage;

  return 1;
}


/*
  UNPACK_POSITION
  Expand the 128-bit compressed position into a full board.
*/


void unpack_position( int index ) {
  int i, j, pos;
  int mask;

  /* Nota bene: Some care has to be taken not to mirror-reflect
     each row - the MSDB is the leftmost position on each row. */

  for ( i = 0; i < 8; i++ ) {
    mask = position_list[index].row_bit_vec[i];
    for ( j = 0, pos = 10 * (i + 1) + 8; j < 8; j++, pos-- ) {
      board[pos] = mask & 3;
      mask >>= 2;
    }
  }
}


/*
  DISPLAY_BOARD
  Provides a crude position dump.
*/

void display_board( int index ) {
  int i, j;

  puts("");
  for (i = 1; i <= 8; i++) {
    printf("      ");
    for (j = 1; j <= 8; j++)
      switch (board[10 * i + j]) {
      case EMPTY:
	printf(" ");
	break;
      case BLACKSQ:
	printf("*");
	break;
      case WHITESQ:
	printf("O");
	break;
      default:
	printf("?");
	break;
      }
    puts("");
  }
  puts("");
  printf("side_to_move=%d\n", position_list[index].side_to_move);
  printf("stage=%d\n", position_list[index].stage);
  printf("score=%d\n", position_list[index].score);
}


/*
   READ_POSITION_FILE
   Reads a game database and creates a game tree containing its games.
*/   

void read_position_file(char *file_name) {
  FILE *stream;
  char buffer[100];

  position_list = malloc( max_positions * sizeof( CompactPosition ) );
  if ( position_list == NULL ) {
    printf( "Couldn't allocate space for %d positions\n", max_positions );
    exit( EXIT_FAILURE );
  }

  stream = fopen( file_name, "r" );
  if ( stream == NULL ) {
    printf( "Could not open game file '%s'\n", file_name );
    exit( EXIT_FAILURE );
  }

  position_count = 0;
  do {
    fgets( buffer, 100, stream );
    if ( !feof( stream ) )
      if ( pack_position( buffer, position_count ) )
	position_count++;
  } while ( !feof( stream ) && (position_count < max_positions) );

  fclose( stream );

  printf( "%d positions loaded\n", position_count );

  /*
  for ( int i = 0; i < position_count; i++ ) {
    unpack_position( i );
    display_board( i );
  }
  */
}


/*
  COMPUTE_PATTERNS
  Computes the board patterns corresponding to rows, columns
  and diagonals.
*/

void compute_patterns( void ) {
  int i, j, pos;

  for ( i = 0; i < 8; i++ ) {
    row_pattern[i] = 0;
    col_pattern[i] = 0;
  }
  for ( i = 0; i < 15; i++ ) {
    diag1_pattern[i] = 0;
    diag2_pattern[i] = 0;
  }

  for ( i = 1; i <= 8; i++ )
    for ( j = 1; j <= 8; j++ ) {
      pos = 10 * i + j;
      row_pattern[row_no[pos]] += board[pos] << (row_index[pos] << 1);
      col_pattern[col_no[pos]] += board[pos] << (col_index[pos] << 1);
      diag1_pattern[diag1_no[pos]] += board[pos] << (diag1_index[pos] << 1);
      diag2_pattern[diag2_no[pos]] += board[pos] << (diag2_index[pos] << 1);
    }
}


/*
   SORT
   Sorts an integer vector using bubble-sort.
*/

INLINE
void sort( int *vec, int count ) {
  int i;
  int temp;
  int changed;

  do {
    changed = 0;
    for ( i = 0; i < count - 1; i++ )
      if ( vec[i] > vec[i + 1] ) {
	changed = 1;
	temp = vec[i];
	vec[i] = vec[i + 1];
	vec[i + 1] = temp;
      }
  } while ( changed );
}


/*
   DETERMINE_FEATURES
   Takes a position and determines the values of all
   active features
*/

void determine_features( int side_to_move, int stage, int *global_parity,
			 int *buffer_a, int *buffer_b,
			 int *buffer_c, int *buffer_d,
			 int *buffer_52, int *buffer_33,
			 int *buffer_8, int *buffer_7, int *buffer_6,
			 int *buffer_5, int *buffer_4 ) {
  int config52, config33;

  compute_patterns();

   /* Non-pattern measures */

  if ( USE_PARITY ) {
    if (stage % 2 == 1)
      *global_parity = 1;
    else
      *global_parity = 0;
  }

   /* The A file (with or without adjacent X-squares) */

  if ( USE_A_FILE ) {
    if ( side_to_move == BLACKSQ ) {
      buffer_a[0] = mirror[compact[row_pattern[0]]];
      buffer_a[1] = mirror[compact[row_pattern[7]]];
      buffer_a[2] = mirror[compact[col_pattern[0]]];
      buffer_a[3] = mirror[compact[col_pattern[7]]];
    }
    else {
      buffer_a[0] = mirror[inverse8[compact[row_pattern[0]]]];
      buffer_a[1] = mirror[inverse8[compact[row_pattern[7]]]];
      buffer_a[2] = mirror[inverse8[compact[col_pattern[0]]]];
      buffer_a[3] = mirror[inverse8[compact[col_pattern[7]]]];
    }
  }
  else if ( USE_A_FILE2X ) {
    if ( side_to_move == BLACKSQ ) {
      buffer_a[0] = mirror82x[compact[row_pattern[0]] + 6561 * board[22] + 19683 * board[27]];
      buffer_a[1] = mirror82x[compact[row_pattern[7]] + 6561 * board[72] + 19683 * board[77]];
      buffer_a[2] = mirror82x[compact[col_pattern[0]] + 6561 * board[22] + 19683 * board[72]];
      buffer_a[3] = mirror82x[compact[col_pattern[7]] + 6561 * board[27] + 19683 * board[77]];
    }
    else {
      buffer_a[0] = mirror82x[inverse10[compact[row_pattern[0]] + 6561 * board[22] + 19683 * board[27]]];
      buffer_a[1] = mirror82x[inverse10[compact[row_pattern[7]] + 6561 * board[72] + 19683 * board[77]]];
      buffer_a[2] = mirror82x[inverse10[compact[col_pattern[0]] + 6561 * board[22] + 19683 * board[72]]];
      buffer_a[3] = mirror82x[inverse10[compact[col_pattern[7]] + 6561 * board[27] + 19683 * board[77]]];
    }
  }

  /* The B file */

  if ( USE_B_FILE ) {
    if ( side_to_move == BLACKSQ ) {
      buffer_b[0] = mirror[compact[row_pattern[1]]];
      buffer_b[1] = mirror[compact[row_pattern[6]]];
      buffer_b[2] = mirror[compact[col_pattern[1]]];
      buffer_b[3] = mirror[compact[col_pattern[6]]];
    }
    else {
      buffer_b[0] = mirror[inverse8[compact[row_pattern[1]]]];
      buffer_b[1] = mirror[inverse8[compact[row_pattern[6]]]];
      buffer_b[2] = mirror[inverse8[compact[col_pattern[1]]]];
      buffer_b[3] = mirror[inverse8[compact[col_pattern[6]]]];
    }
  }

  /* The C file */

  if ( USE_C_FILE ) {
    if ( side_to_move == BLACKSQ ) {
      buffer_c[0] = mirror[compact[row_pattern[2]]];
      buffer_c[1] = mirror[compact[row_pattern[5]]];
      buffer_c[2] = mirror[compact[col_pattern[2]]];
      buffer_c[3] = mirror[compact[col_pattern[5]]];
    }
    else {
      buffer_c[0] = mirror[inverse8[compact[row_pattern[2]]]];
      buffer_c[1] = mirror[inverse8[compact[row_pattern[5]]]];
      buffer_c[2] = mirror[inverse8[compact[col_pattern[2]]]];
      buffer_c[3] = mirror[inverse8[compact[col_pattern[5]]]];
    }
  }

  /* The D file */

  if ( USE_D_FILE ) {
    if ( side_to_move == BLACKSQ ) {
      buffer_d[0] = mirror[compact[row_pattern[3]]];
      buffer_d[1] = mirror[compact[row_pattern[4]]];
      buffer_d[2] = mirror[compact[col_pattern[3]]];
      buffer_d[3] = mirror[compact[col_pattern[4]]];
    }
    else {
      buffer_d[0] = mirror[inverse8[compact[row_pattern[3]]]];
      buffer_d[1] = mirror[inverse8[compact[row_pattern[4]]]];
      buffer_d[2] = mirror[inverse8[compact[col_pattern[3]]]];
      buffer_d[3] = mirror[inverse8[compact[col_pattern[4]]]];
    }
  }

  /* The 5*2 corner pattern */

  if ( USE_CORNER52 ) {
    if ( side_to_move == BLACKSQ ) {
      /* a1-e1 + a2-e2 */
      config52 = (row_pattern[0] & 1023) + ((row_pattern[1] & 1023) << 10);
      buffer_52[0] = compact[config52];
      /* a1-a5 + b1-b5 */
      config52 = (col_pattern[0] & 1023) + ((col_pattern[1] & 1023) << 10);
      buffer_52[1] = compact[config52];
      /* h1-d1 + h2-d2 */
      config52 = (row_pattern[0] >> 6) + ((row_pattern[1] >> 6) << 10);
      buffer_52[2] = flip52[compact[config52]];
      /* h1-h5 + g1-g5 */
      config52 = (col_pattern[7] & 1023) + ((col_pattern[6] & 1023) << 10);
      buffer_52[3] = compact[config52];
      /* a8-e8 + a7-e7 */
      config52 = (row_pattern[7] & 1023) + ((row_pattern[6] & 1023) << 10);
      buffer_52[4] = compact[config52];
      /* a8-a4 + b8-b4 */
      config52 = (col_pattern[0] >> 6) + ((col_pattern[1] >> 6) << 10);
      buffer_52[5] = flip52[compact[config52]];
      /* h8-d8 + h7-d7 */
      config52 = (row_pattern[7] >> 6) + ((row_pattern[6] >> 6) << 10);
      buffer_52[6] = flip52[compact[config52]];
      /* h8-h4 + g8-g4 */
      config52 = (col_pattern[7] >> 6) + ((col_pattern[6] >> 6) << 10);
      buffer_52[7] = flip52[compact[config52]];
    }
    else {
      /* a1-e1 + a2-e2 */
      config52 = (row_pattern[0] & 1023) + ((row_pattern[1] & 1023) << 10);
      buffer_52[0] = inverse10[compact[config52]];
      /* a1-a5 + b1-b5 */
      config52 = (col_pattern[0] & 1023) + ((col_pattern[1] & 1023) << 10);
      buffer_52[1] = inverse10[compact[config52]];
      /* h1-d1 + h2-d2 */
      config52 = (row_pattern[0] >> 6) + ((row_pattern[1] >> 6) << 10);
      buffer_52[2] = inverse10[flip52[compact[config52]]];
      /* h1-h5 + g1-g5 */
      config52 = (col_pattern[7] & 1023) + ((col_pattern[6] & 1023) << 10);
      buffer_52[3] = inverse10[compact[config52]];
      /* a8-e8 + a7-e7 */
      config52 = (row_pattern[7] & 1023) + ((row_pattern[6] & 1023) << 10);
      buffer_52[4] = inverse10[compact[config52]];
      /* a8-a4 + b8-b4 */
      config52 = (col_pattern[0] >> 6) + ((col_pattern[1] >> 6) << 10);
      buffer_52[5] = inverse10[flip52[compact[config52]]];
      /* h8-e8 + h7-e7 */
      config52 = (row_pattern[7] >> 6) + ((row_pattern[6] >> 6) << 10);
      buffer_52[6] = inverse10[flip52[compact[config52]]];
      /* h8-h4 + g8-g4 */
      config52 = (col_pattern[7] >> 6) + ((col_pattern[6] >> 6) << 10);
      buffer_52[7] = inverse10[flip52[compact[config52]]];
    }
  }

  /* The 3*3 corner pattern */

  if ( USE_CORNER33 ) {
    if ( side_to_move == BLACKSQ ) {
      /* a1-c1 + a2-c2 + a3-c3 */
      config33 = (row_pattern[0] & 63) +
	((row_pattern[1] & 63) << 6) +
	((row_pattern[2] & 63) << 12);
      buffer_33[0] = mirror33[compact[config33]];
      /* h1-f1 + h2-f2 + h3-f3 */
      config33 = (row_pattern[0] >> 10) +
	((row_pattern[1] >> 10) << 6) +
	((row_pattern[2] >> 10) << 12);
      buffer_33[1] = mirror33[flip33[compact[config33]]];
      /* a8-c8 + a7-c7 + a6-c6 */
      config33 = (row_pattern[7] & 63) +
	((row_pattern[6] & 63) << 6) +
	((row_pattern[5] & 63) << 12);
      buffer_33[2] = mirror33[compact[config33]];
      /* h8-f8 + h7-f7 + h6-f6 */
      config33 = (row_pattern[7] >> 10) +
	((row_pattern[6] >> 10) << 6) +
	((row_pattern[5] >> 10) << 12);
      buffer_33[3] = mirror33[flip33[compact[config33]]];
    }
    else {
      /* a1-c1 + a2-c2 + a3-c3 */
      config33 = (row_pattern[0] & 63) +
	((row_pattern[1] & 63) << 6) +
	((row_pattern[2] & 63) << 12);
      buffer_33[0] = mirror33[inverse9[compact[config33]]];
      /* h1-f1 + h2-f2 + h3-f3 */
      config33 = (row_pattern[0] >> 10) +
	((row_pattern[1] >> 10) << 6) +
	((row_pattern[2] >> 10) << 12);
      buffer_33[1] = mirror33[inverse9[flip33[compact[config33]]]];
      /* a8-c8 + a7-c7 + a6-c6 */
      config33 = (row_pattern[7] & 63) +
	((row_pattern[6] & 63) << 6) +
	((row_pattern[5] & 63) << 12);
      buffer_33[2] = mirror33[inverse9[compact[config33]]];
      /* h8-f8 + h7-f7 + h6-f6 */
      config33 = (row_pattern[7] >> 10) +
	((row_pattern[6] >> 10) << 6) +
	((row_pattern[5] >> 10) << 12);
      buffer_33[3] = mirror33[inverse9[flip33[compact[config33]]]];
    }
  }

  /* The diagonals of length 8 */

  if ( USE_DIAG8 ) {
    if ( side_to_move == BLACKSQ ) {
      buffer_8[0] = mirror[compact[diag1_pattern[7]]];
      buffer_8[1] = mirror[compact[diag2_pattern[7]]];
    }
    else {
      buffer_8[0] = mirror[inverse8[compact[diag1_pattern[7]]]];
      buffer_8[1] = mirror[inverse8[compact[diag2_pattern[7]]]];
    }
  }

  /* The diagonals of length 7 */

  if ( USE_DIAG7 ) {
    if ( side_to_move == BLACKSQ ) {
      buffer_7[0] = mirror7[compact[diag1_pattern[6]]];
      buffer_7[1] = mirror7[compact[diag1_pattern[8]]];
      buffer_7[2] = mirror7[compact[diag2_pattern[6]]];
      buffer_7[3] = mirror7[compact[diag2_pattern[8]]];
    }
    else {
      buffer_7[0] = mirror7[inverse7[compact[diag1_pattern[6]]]];
      buffer_7[1] = mirror7[inverse7[compact[diag1_pattern[8]]]];
      buffer_7[2] = mirror7[inverse7[compact[diag2_pattern[6]]]];
      buffer_7[3] = mirror7[inverse7[compact[diag2_pattern[8]]]];
    }
  }

  /* The diagonals of length 6 */

  if ( USE_DIAG6 ) {
    if ( side_to_move == BLACKSQ ) {
      buffer_6[0] = mirror6[compact[diag1_pattern[5]]];
      buffer_6[1] = mirror6[compact[diag1_pattern[9]]];
      buffer_6[2] = mirror6[compact[diag2_pattern[5]]];
      buffer_6[3] = mirror6[compact[diag2_pattern[9]]];
    }
    else {
      buffer_6[0] = mirror6[inverse6[compact[diag1_pattern[5]]]];
      buffer_6[1] = mirror6[inverse6[compact[diag1_pattern[9]]]];
      buffer_6[2] = mirror6[inverse6[compact[diag2_pattern[5]]]];
      buffer_6[3] = mirror6[inverse6[compact[diag2_pattern[9]]]];
    }
  }

  /* The diagonals of length 5 */

  if ( USE_DIAG5 ) {
    if ( side_to_move == BLACKSQ ) {
      buffer_5[0] = mirror5[compact[diag1_pattern[4]]];
      buffer_5[1] = mirror5[compact[diag1_pattern[10]]];
      buffer_5[2] = mirror5[compact[diag2_pattern[4]]];
      buffer_5[3] = mirror5[compact[diag2_pattern[10]]];
    }
    else {
      buffer_5[0] = mirror5[inverse5[compact[diag1_pattern[4]]]];
      buffer_5[1] = mirror5[inverse5[compact[diag1_pattern[10]]]];
      buffer_5[2] = mirror5[inverse5[compact[diag2_pattern[4]]]];
      buffer_5[3] = mirror5[inverse5[compact[diag2_pattern[10]]]];
    }
  }

  /* The diagonals of length 4 */

  if ( USE_DIAG4 ) {
    if (side_to_move == BLACKSQ) {
      buffer_4[0] = mirror4[compact[diag1_pattern[3]]];
      buffer_4[1] = mirror4[compact[diag1_pattern[11]]];
      buffer_4[2] = mirror4[compact[diag2_pattern[3]]];
      buffer_4[3] = mirror4[compact[diag2_pattern[11]]];
    }
    else {
      buffer_4[0] = mirror4[inverse4[compact[diag1_pattern[3]]]];
      buffer_4[1] = mirror4[inverse4[compact[diag1_pattern[11]]]];
      buffer_4[2] = mirror4[inverse4[compact[diag2_pattern[3]]]];
      buffer_4[3] = mirror4[inverse4[compact[diag2_pattern[11]]]];
    }
  }
}


/*
   PERFORM_ANALYSIS
   Updates frequency counts.
*/   

void perform_analysis( int index ) {
  int coeff, start, stop;
  int global_parity;
  int side_to_move, stage;
  int buffer_a[4], buffer_b[4], buffer_c[4], buffer_d[4];
  int buffer_52[8], buffer_33[4];
  int buffer_8[4], buffer_7[4], buffer_6[4], buffer_5[4], buffer_4[4];

  side_to_move = position_list[index].side_to_move;
  stage = position_list[index].stage;

  determine_features( side_to_move, stage,
		      &global_parity, buffer_a, buffer_b,
		      buffer_c, buffer_d, buffer_52,
		      buffer_33, buffer_8, buffer_7, buffer_6,
		      buffer_5, buffer_4 );

  /* The D file */

  sort( buffer_d, 4 );
  start = 0;
  do {
    stop = start + 1;
    while ( (stop < 4) && (buffer_d[stop] == buffer_d[start]) )
      stop++;
    coeff = stop - start;
    dfile[buffer_d[start]].frequency++;
    start = stop;
  } while (start < 4);

  /* The C file */

  sort(buffer_c, 4);
  start = 0;
  do {
    stop = start + 1;
    while ( (stop < 4) && (buffer_c[stop] == buffer_c[start]) )
      stop++;
    coeff = stop - start;
    cfile[buffer_c[start]].frequency++;
    start = stop;
  } while ( start < 4 );

  /* The B file */

  sort( buffer_b, 4 );
  start = 0;
  do {
    stop = start + 1;
    while ( (stop < 4) && (buffer_b[stop] == buffer_b[start]) )
      stop++;
    coeff = stop - start;
    bfile[buffer_b[start]].frequency++;
    start = stop;
  } while ( start < 4 );

  /* The A file */

  sort( buffer_a, 4 );
  start = 0;
  do {
    stop = start + 1;
    while ( (stop < 4) && (buffer_a[stop] == buffer_a[start]) )
      stop++;
    coeff = stop - start;
    if ( USE_A_FILE )
      afile[buffer_a[start]].frequency++;
    else if ( USE_A_FILE2X )
      afile2x[buffer_a[start]].frequency++;
    start = stop;
  } while ( start < 4 );

  /* The diagonals of length 8 */

  sort( buffer_8, 2 );
  start = 0;
  do {
    stop = start + 1;
    while ( (stop < 2) && (buffer_8[stop] == buffer_8[start]) )
      stop++;
    coeff = stop - start;
    diag8[buffer_8[start]].frequency++;
    start = stop;
  } while ( start < 2 );

  /* The diagonals of length 7 */

  sort( buffer_7, 4 );
  start = 0;
  do {
    stop = start + 1;
    while ( (stop < 4) && (buffer_7[stop] == buffer_7[start]) )
      stop++;
    coeff = stop - start;
    diag7[buffer_7[start]].frequency++;
    start = stop;
  } while ( start < 4 );

  /* The diagonals of length 6 */

  sort( buffer_6, 4 );
  start = 0;
  do {
    stop = start + 1;
    while ( (stop < 4) && (buffer_6[stop] == buffer_6[start]) )
      stop++;
    coeff = stop - start;
    diag6[buffer_6[start]].frequency++;
    start = stop;
  } while (start < 4 );

  /* The diagonals of length 5 */

  sort( buffer_5, 4 );
  start = 0;
  do {
    stop = start + 1;
    while ( (stop < 4) && (buffer_5[stop] == buffer_5[start]) )
      stop++;
    coeff = stop - start;
    diag5[buffer_5[start]].frequency++;
    start = stop;
  } while ( start < 4 );

  /* The diagonals of length 4 */

  sort( buffer_4, 4 );
  start = 0;
  do {
    stop = start + 1;
    while ( (stop < 4) && (buffer_4[stop] == buffer_4[start]) )
      stop++;
    coeff = stop - start;
    diag4[buffer_4[start]].frequency++;
    start = stop;
  } while ( start < 4 );

  /* The 5*2 corner pattern */

  sort( buffer_52, 8 );
  start = 0;
  do {
    stop = start + 1;
    while ( (stop < 8) && (buffer_52[stop] == buffer_52[start]) )
      stop++;
    coeff = stop - start;
    corner52[buffer_52[start]].frequency++;
    start = stop;
  } while ( start < 8 );

  /* The 3*3 corner pattern */

  sort( buffer_33, 4 );
  start = 0;
  do {
    stop = start + 1;
    while ( (stop < 4) && (buffer_33[stop] == buffer_33[start]) )
      stop++;
    coeff = stop - start;
    corner33[buffer_33[start]].frequency++;
    start = stop;
  } while ( start < 4 );
}


/*
   PERFORM_EVALUATION
   Updates the gradient based on the position BRANCH.
*/

void perform_evaluation( int index ) {
  double error;
  double grad_contrib;
  double curr_weight;
  int i;
  int global_parity;
  int side_to_move, stage;
  int buffer_a[4], buffer_b[4], buffer_c[4], buffer_d[4];
  int buffer_52[8], buffer_33[4];
  int buffer_8[4], buffer_7[4], buffer_6[4], buffer_5[4], buffer_4[4];

  /* Get the pattern values */

  side_to_move = position_list[index].side_to_move;
  stage = position_list[index].stage;

  determine_features( side_to_move, stage,
		      &global_parity, buffer_a, buffer_b,
		      buffer_c, buffer_d, buffer_52,
		      buffer_33, buffer_8, buffer_7, buffer_6,
		      buffer_5, buffer_4 );

  /* Calculate the contribution to the gradient and the objective */

  error = -position_list[index].score;

  curr_weight = weight[stage];
  total_weight += curr_weight;
  error += constant.solution;
  if ( USE_PARITY )
    error += parity.solution * global_parity;
  if ( USE_A_FILE )
    for ( i = 0; i < 4; i++ )
      error += afile[buffer_a[i]].solution;
  else if ( USE_A_FILE2X )
    for ( i = 0; i < 4; i++ )
      error += afile2x[buffer_a[i]].solution;
  if ( USE_B_FILE )
    for ( i = 0; i < 4; i++ )
      error += bfile[buffer_b[i]].solution;
  if ( USE_C_FILE )
    for ( i = 0; i < 4; i++ )
      error += cfile[buffer_c[i]].solution;
  if ( USE_D_FILE )
    for ( i = 0; i < 4; i++ )
      error += dfile[buffer_d[i]].solution;
  if ( USE_CORNER52 )
    for ( i = 0; i < 8; i++ )
      error += corner52[buffer_52[i]].solution;
  if ( USE_CORNER33 )
    for ( i = 0; i < 4; i++ )
      error += corner33[buffer_33[i]].solution;
  if ( USE_DIAG8 )
    for ( i = 0; i < 2; i++ )
      error += diag8[buffer_8[i]].solution;
  if ( USE_DIAG7 )
    for ( i = 0; i < 4; i++ )
      error += diag7[buffer_7[i]].solution;
  if ( USE_DIAG6 )
    for (i = 0; i < 4; i++)
      error += diag6[buffer_6[i]].solution;
  if ( USE_DIAG5 )
    for ( i = 0; i < 4; i++ )
      error += diag5[buffer_5[i]].solution;
  if ( USE_DIAG4 )
    for ( i = 0; i < 4; i++ )
      error += diag4[buffer_4[i]].solution;
  error *= curr_weight;
  objective += error * error;
  abs_error_sum += fabs(error);

  grad_contrib = 2.0 * curr_weight * error;
  constant.gradient += grad_contrib;
  if ( USE_PARITY )
    parity.gradient += grad_contrib * global_parity;
  if ( USE_A_FILE )
    for ( i = 0; i < 4; i++ )
      afile[buffer_a[i]].gradient += grad_contrib;
  else if ( USE_A_FILE2X )
    for ( i = 0; i < 4; i++ )
      afile2x[buffer_a[i]].gradient += grad_contrib;
  if ( USE_B_FILE )
    for ( i = 0; i < 4; i++ )
      bfile[buffer_b[i]].gradient += grad_contrib;
  if ( USE_C_FILE )
    for ( i = 0; i < 4; i++ )
      cfile[buffer_c[i]].gradient += grad_contrib;
  if ( USE_D_FILE )
    for ( i = 0; i < 4; i++ )
      dfile[buffer_d[i]].gradient += grad_contrib;
  if ( USE_CORNER52 )
    for (i = 0; i < 8; i++)
      corner52[buffer_52[i]].gradient += grad_contrib;
  if ( USE_CORNER33 )
    for ( i = 0; i < 4; i++ )
      corner33[buffer_33[i]].gradient += grad_contrib;
  if ( USE_DIAG8 )
    for ( i = 0; i < 2; i++ )
      diag8[buffer_8[i]].gradient += grad_contrib;
  if ( USE_DIAG7 )
    for ( i = 0; i < 4; i++ )
      diag7[buffer_7[i]].gradient += grad_contrib;
  if ( USE_DIAG6 )
    for ( i = 0; i < 4; i++ )
      diag6[buffer_6[i]].gradient += grad_contrib;
  if ( USE_DIAG5 )
    for ( i = 0; i < 4; i++ )
      diag5[buffer_5[i]].gradient += grad_contrib;
  if ( USE_DIAG4 )
    for ( i = 0; i < 4; i++ )
      diag4[buffer_4[i]].gradient += grad_contrib;
}


/*
   PERFORM_STEP_UPDATE
   Updates the parameters used to determine the optimal step length
   based on the position BRANCH.
*/

void perform_step_update( int index ) {
  double error;
  double grad_contrib;
  double curr_weight;
  int i;
  int global_parity;
  int side_to_move, stage;
  int buffer_a[4], buffer_b[4], buffer_c[4], buffer_d[4];
  int buffer_52[8], buffer_33[4];
  int buffer_8[4], buffer_7[4], buffer_6[4], buffer_5[4], buffer_4[4];

  /* Get the pattern values */

  side_to_move = position_list[index].side_to_move;
  stage = position_list[index].stage;

  determine_features( side_to_move, stage,
		      &global_parity, buffer_a, buffer_b,
		      buffer_c, buffer_d, buffer_52,
		      buffer_33, buffer_8, buffer_7, buffer_6,
		      buffer_5, buffer_4 );

  /* Calculate the contribution to the gradient and the objective */

  error = -position_list[index].score;

  grad_contrib = 0.0;

  error += constant.solution;
  grad_contrib += constant.direction;
  if ( USE_PARITY ) {
    error += parity.solution * global_parity;
    grad_contrib += parity.direction * global_parity;
  }
  if ( USE_A_FILE )
    for ( i = 0; i < 4; i++ ) {
      error += afile[buffer_a[i]].solution;
      grad_contrib += afile[buffer_a[i]].direction;
    }
  else if ( USE_A_FILE2X )
    for ( i = 0; i < 4; i++ ) {
      error += afile2x[buffer_a[i]].solution;
      grad_contrib += afile2x[buffer_a[i]].direction;
    }
  if ( USE_B_FILE )
    for ( i = 0; i < 4; i++ ) {
      error += bfile[buffer_b[i]].solution;
      grad_contrib += bfile[buffer_b[i]].direction;
    }
  if ( USE_C_FILE )
    for ( i = 0; i < 4; i++ ) {
      error += cfile[buffer_c[i]].solution;
      grad_contrib += cfile[buffer_c[i]].direction;
    }
  if ( USE_D_FILE )
    for ( i = 0; i < 4; i++ ) {
      error += dfile[buffer_d[i]].solution;
      grad_contrib += dfile[buffer_d[i]].direction;
    }
  if ( USE_CORNER52 )
    for ( i = 0; i < 8; i++ ) {
      error += corner52[buffer_52[i]].solution;
      grad_contrib += corner52[buffer_52[i]].direction;
    }
  if ( USE_CORNER33 )
    for ( i = 0; i < 4; i++ ) {
      error += corner33[buffer_33[i]].solution;
      grad_contrib += corner33[buffer_33[i]].direction;
    }
  if ( USE_DIAG8 )
    for ( i = 0; i < 2; i++ ) {
      error += diag8[buffer_8[i]].solution;
      grad_contrib += diag8[buffer_8[i]].direction;
    }
  if ( USE_DIAG7 )
    for ( i = 0; i < 4; i++ ) {
      error += diag7[buffer_7[i]].solution;
      grad_contrib += diag7[buffer_7[i]].direction;
    }
  if ( USE_DIAG6 )
    for ( i = 0; i < 4; i++ ) {
      error += diag6[buffer_6[i]].solution;
      grad_contrib += diag6[buffer_6[i]].direction;
    }
  if ( USE_DIAG5 )
    for ( i = 0; i < 4; i++ ) {
      error += diag5[buffer_5[i]].solution;
      grad_contrib += diag5[buffer_5[i]].direction;
    }
  if ( USE_DIAG4 )
    for ( i = 0; i < 4; i++ ) {
      error += diag4[buffer_4[i]].solution;
      grad_contrib += diag4[buffer_4[i]].direction;
    }
  curr_weight = weight[stage];
  error *= curr_weight;
  grad_contrib *= curr_weight;
  grad_contrib /= total_weight;

  quad_coeff += grad_contrib * grad_contrib;
  lin_coeff += 2.0 * grad_contrib * error;
  const_coeff += error * error;
}


/*
   PERFORM_ACTION
   A wrapper to the function given by the function pointer BFUNC.
*/

INLINE
void perform_action( void (*bfunc)(int), int index ) {
  node_count++;
  if ( active[position_list[index].stage] ) {
    relevant_count++;
    if ( (interval != 0) && (relevant_count % interval == 0) ) {
      printf( " %d", relevant_count );
      fflush( stdout );
    }
    unpack_position( index );
    bfunc( index );
  }
}


/*
   ITERATE
   Applies the function BFUNC to all the (relevant)
   positions in the position list.
*/

void iterate( void (*bfunc)(int) ) {
  int index;

  for ( index = 0; index < position_count; index++ )
    perform_action( bfunc, index );
}


/*
   ANALYZE_GAMES
   Creates frequency statistics.
*/

void analyze_games( void ) {
  node_count = 0;
  relevant_count = 0;
  interval = 0;
  iterate( perform_analysis );
}


/*
   EVALUATE_GAMES
   Determines the gradient for all patterns.
*/   

void evaluate_games( void ) {
  node_count = 0;
  relevant_count = 0;
  iterate( perform_evaluation );
}


/*
   DETERMINE_GAMES
   Determines the optimal step length.
*/   

void determine_games( void ) {
  node_count = 0;
  relevant_count = 0;
  iterate( perform_step_update );
}


/*
   PATTERN_SETUP
   Creates a bunch of maps between patterns.
*/

void pattern_setup( void ) {
  int i, j, k;
  int pos;
  int pattern, mirror_pattern;
  int power3;
  int flip8[6561], flip5[81], flip3[27];
  int row[10];

  /* The inverse patterns */

  for ( i = 0; i < 81; i++ )
    inverse4[i] = 80 - i;

  for ( i = 0; i < 243; i++ )
    inverse5[i] = 242 - i;

  for ( i = 0; i < 729; i++ )
    inverse6[i] = 728 - i;

  for ( i = 0; i < 2187; i++ )
    inverse7[i] = 2186 - i;

  for ( i = 0; i < 6561; i++ )
    inverse8[i] = 6560 - i;

  for ( i = 0; i < 19683; i++ )
    inverse9[i] = 19682 - i;

  for ( i = 0; i < 59049; i++ )
    inverse10[i] = 59048 - i;

  /* Build the common pattern maps */

  for ( i = 0; i < 10; i++ )
    row[i] = 0;

  for ( i = 0; i < 59049; i++ ) {
    pattern = 0;
    for ( j = 0; j < 10; j++ )  /* Create the corresponding pattern. */
      pattern |= (row[j] << (j << 1));

    /* Map the base-4 pattern onto the corresponding base-3 pattern */
    compact[pattern] = i;

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if (row[j] == 3)
	row[j] = 0;
      j++;
    } while (row[j - 1] == 0 && j < 10);
  }

  /* Build the pattern tables for 8*1-patterns */   

  for (i = 0; i < 8; i++)
    row[i] = 0;

  for (i = 0; i < 6561; i++) {
    mirror_pattern = 0;
    power3 = 1;
    for (j = 7; j >= 0; j--) {
      mirror_pattern += row[j] * power3;
      power3 *= 3;
    }
    /* Create the symmetry map */
    mirror[i] = MIN( i, mirror_pattern );
    flip8[i] = mirror_pattern;

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if (row[j] == 3)
	row[j] = 0;
      j++;
    } while (row[j - 1] == 0 && j < 8);
  }

  /* Build the tables for 7*1-patterns */

  for (i = 0; i < 7; i++)
    row[i] = 0;
  for (i = 0; i < 2187; i++) {
    mirror_pattern = 0;
    power3 = 1;
    for (j = 6; j >= 0; j--) {
      mirror_pattern += row[j] * power3;
      power3 *= 3;
    }
    mirror7[i] = MIN(i, mirror_pattern);

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if (row[j] == 3)
	row[j] = 0;
      j++;
    } while (row[j - 1] == 0 && j < 7);
  }

  /* Build the tables for 6*1-patterns */   

  for (i = 0; i < 6; i++)
    row[i] = 0;
  for (i = 0; i < 729; i++) {
    mirror_pattern = 0;
    power3 = 1;
    for (j = 5; j >= 0; j--) {
      mirror_pattern += row[j] * power3;
      power3 *= 3;
    }
    mirror6[i] = MIN(i, mirror_pattern);

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if (row[j] == 3)
	row[j] = 0;
      j++;
    } while (row[j - 1] == 0 && j < 6);
  }

  /* Build the tables for 5*1-patterns */   

  for (i = 0; i < 5; i++)
    row[i] = 0;
  for (i = 0; i < 243; i++) {
    mirror_pattern = 0;
    power3 = 1;
    for (j = 4; j >= 0; j--) {
      mirror_pattern += row[j] * power3;
      power3 *= 3;
    }
    mirror5[i] = MIN(i, mirror_pattern);
    flip5[i] = mirror_pattern;

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while (row[j - 1] == 0 && j < 5);
  }

  /* Build the tables for 4*1-patterns */   

  for ( i = 0; i < 4; i++ )
    row[i] = 0;
  for ( i = 0; i < 81; i++ ) {
    mirror_pattern = 0;
    power3 = 1;
    for ( j = 3; j >= 0; j-- ) {
      mirror_pattern += row[j] * power3;
      power3 *= 3;
    }
    mirror4[i] = MIN( i, mirror_pattern );

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while ( (row[j - 1] == 0) && (j < 4) );
  }

  /* Build the tables for 3*1-patterns */   

  for ( i = 0; i < 3; i++ )
    row[i] = 0;
  for ( i = 0; i < 27; i++ ) {
    mirror_pattern = 0;
    power3 = 1;
    for ( j = 2; j >= 0; j-- ) {
      mirror_pattern += row[j] * power3;
      power3 *= 3;
    }
    mirror3[i] = MIN( i, mirror_pattern );
    flip3[i] = mirror_pattern;

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while ( (row[j - 1] == 0) && (j < 3) );
  }

  /* Build the tables for 5*2-patterns */

  for ( i = 0; i < 243; i++ )
    for ( j = 0; j < 243; j++ )
      flip52[243 * i + j] = 243 * flip5[i] + flip5[j];
  for ( i = 0; i < 59049; i++ )
    identity10[i] = i;

  /* Build the tables for 3*3-patterns */

  for ( i = 0; i < 27; i++ )
    for ( j = 0; j < 27; j++ )
      for ( k = 0; k < 27; k++ )
	flip33[729 * i + 27 * j + k] =
	  729 * flip3[i] + 27 * flip3[j] + flip3[k];

  for ( i = 0; i < 9; i++ )
    row[i] = 0;

  for ( i = 0; i < 19683; i++ ) {
    mirror_pattern =
      row[0] + 3 * row[3] + 9 * row[6] +
      27 * row[1] + 81 * row[4] + 243 * row[7] +
      729 * row[2] + 2187 * row[5] + 6561 * row[8];
    mirror33[i] = MIN( i, mirror_pattern );

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while ( (row[j - 1] == 0) && (j < 9) );
  }

  /* Build the tables for edge2X-patterns */

  for ( i = 0; i < 6561; i++ )
    for ( j = 0; j < 3; j++ )
      for ( k = 0; k < 3; k++ )
	mirror82x[i + 6561 * j + 19683 * k] =
	  MIN( flip8[i] + 6561 * k + 19683 * j, i + 6561 * j + 19683 * k );
   /* Create the connections position <--> patterns affected */

  for ( i = 1; i <= 8; i++ )
    for (j  = 1; j <= 8; j++ ) {
      pos = 10 * i + j;
      row_no[pos] = i - 1;
      row_index[pos] = j - 1;
      col_no[pos] = j - 1;
      col_index[pos] = i - 1;
      diag1_no[pos] = j - i + 7;
      if ( i >= j )  /* First 8 diagonals */
	diag1_index[pos] = j - 1;
      else
	diag1_index[pos] = i - 1;            
      diag2_no[pos] = j + i - 2;
      if ( i + j <= 9 )  /* First 8 diagonals */
	diag2_index[pos] = j - 1;
      else
	diag2_index[pos] = 8 - i;
    }

  /* Reset the coefficients for the different patterns */

  constant.solution = 0.0;
  constant.direction = 0.0;
  parity.solution = 0.0;
  parity.direction = 0.0;
  for ( i = 0; i < 59049; i++ ) {
    afile2x[i].pattern = i;
    afile2x[i].frequency = 0;
    afile2x[i].direction = 0.0;
    afile2x[i].most_common = 0;
    corner52[i].pattern = i;
    corner52[i].frequency = 0;
    corner52[i].direction = 0.0;
    corner52[i].most_common = 0;
  }
  for ( i = 0; i < 19683; i++ ) {
    corner33[i].pattern = i;
    corner33[i].frequency = 0;
    corner33[i].direction = 0.0;
    corner33[i].most_common = 0;
  }
  for ( i = 0; i < 6561; i++ ) {
    afile[i].pattern = i;
    afile[i].frequency = 0;
    afile[i].direction = 0.0;
    afile[i].most_common = 0;
    bfile[i].pattern = i;
    bfile[i].frequency = 0;
    bfile[i].direction = 0.0;
    bfile[i].most_common = 0;
    cfile[i].pattern = i;
    cfile[i].frequency = 0;
    cfile[i].direction = 0.0;
    cfile[i].most_common = 0;
    dfile[i].pattern = i;
    dfile[i].frequency = 0;
    dfile[i].direction = 0.0;
    dfile[i].most_common = 0;
    diag8[i].pattern = i;
    diag8[i].frequency = 0;
    diag8[i].direction = 0.0;
    diag8[i].most_common = 0;
  }
  for ( i = 0; i < 2187; i++ ) {
    diag7[i].pattern = i;
    diag7[i].frequency = 0;
    diag7[i].direction = 0.0;
    diag7[i].most_common = 0;
  }
  for ( i = 0; i < 729; i++ ) {
    diag6[i].pattern = i;
    diag6[i].frequency = 0;
    diag6[i].direction = 0.0;
    diag6[i].most_common = 0;
  }
  for ( i = 0; i < 243; i++ ) {
    diag5[i].pattern = i;
    diag5[i].frequency = 0;
    diag5[i].direction = 0.0;
    diag5[i].most_common = 0;
  }
  for ( i = 0; i < 81; i++ ) {
    diag4[i].pattern = i;
    diag4[i].frequency = 0;
    diag4[i].direction = 0.0;
    diag4[i].most_common = 0;
  }
}


/*
   SAVE
   Writes a set of pattern values to disc.
*/

void save( const char *base, char *suffix, InfoItem *items, int count ) {
  char file_name[32];
  float vals[59049];
  int i;
  int freq[59049];
  FILE *stream;

  sprintf( file_name, "%s%s", base, suffix );
  stream = fopen( file_name, "wb" );
  if ( stream == NULL )
    printf( "Error creating '%s'\n", file_name );
  else {
    for (i = 0; i < count; i++) {
      vals[i] = items[i].solution;
      freq[i] = items[i].frequency;
    }
    fwrite( vals, sizeof( float ), count, stream );
    fwrite( freq, sizeof( int ), count, stream);
    fclose( stream );
  }
}


/*
   STORE_PATTERNS
   Writes all sets of feature values to disc.
*/   

void store_patterns( void ) {
  char suffix[8];
  char file_name[16];
  FILE *stream;

  printf( "Saving patterns..." );
  fflush( stdout );

  sprintf( suffix, ".b%d", analysis_stage );

  if ( USE_A_FILE )
    save( "afile", suffix, afile, 6561 );
  else if ( USE_A_FILE2X )
    save( "afile2x", suffix, afile2x, 59049 );
  if ( USE_B_FILE )
    save( "bfile", suffix, bfile, 6561 );
  if ( USE_C_FILE )
    save( "cfile", suffix, cfile, 6561 );
  if ( USE_D_FILE )
    save( "dfile", suffix, dfile, 6561 );
  if ( USE_DIAG8 )
    save( "diag8", suffix, diag8, 6561 );
  if ( USE_DIAG7 )
    save( "diag7", suffix, diag7, 2187 );
  if ( USE_DIAG6 )
    save( "diag6", suffix, diag6, 729 );
  if ( USE_DIAG5 )
    save( "diag5", suffix, diag5, 243 );
  if ( USE_DIAG4 )
    save( "diag4", suffix, diag4, 81 );
  if ( USE_CORNER33 )
    save( "corner33", suffix, corner33, 19683 );
  if ( USE_CORNER52 )
    save( "corner52", suffix, corner52, 59049 );

  sprintf( file_name, "main.s%d", analysis_stage );
  stream = fopen( file_name, "w" );
  if ( stream == NULL )
    printf( "Error creating '%s'\n", file_name );
  else {
    fprintf( stream, "%.8f\n", constant.solution );
    if ( USE_PARITY )
      fprintf( stream, "%.8f\n", parity.solution );
    fprintf( stream, "\n" );
    fprintf( stream, "Target value: %.8f\n", objective );
    fprintf( stream, "Average error: %.8f\n", abs_error_sum );
    fprintf( stream, "Maximum change: %.8f\n", max_delta );
    fprintf( stream, "Average change: %.8f\n", average_delta );
    fclose( stream );
  }

  puts( " done" );
}


/*
   WRITE_LOG
   Saves info on the state of the optimization to disc.
*/

void write_log( int iteration ) {
  char file_name[32];
  FILE *stream;

  sprintf( file_name, "log.s%d", analysis_stage );
  stream = fopen( file_name, "a" );
  if ( stream == NULL )
    printf( "Error appending to '%s'\n", file_name );
  else {
    fprintf( stream, "#%3d  Obj=%.8f  Error=%.8f  Max=%.6f  Av=%.6f\n",
	     iteration, objective, abs_error_sum, max_delta, average_delta );
    fclose( stream );
  }
}


/*
   INITIALIZE_SOLUTION
   Reads the starting point from disc if available, otherwise
   zeroes all values.
   Note: One-dimensional linear regression is no longer performed
   due to its poor performance.
*/   

void initialize_solution( const char *base, InfoItem *item, int count,
			  int *my_mirror ) {
  char file_name[32];
  float *vals;
  int i;
  int *freq;
  FILE *stream;

  sprintf( file_name, "%s.b%d", base, analysis_stage );
  stream = fopen( file_name, "rb" );
  if ( stream == NULL )
    for ( i = 0; i < count; i++ )
      item[i].solution = 0.0;
  else {
    vals = malloc(count * sizeof( float ) );
    freq = malloc(count * sizeof( int ) );
    fread( vals, sizeof( float ), count, stream );
    fread( freq, sizeof( int ), count, stream );
    fclose( stream );
    for ( i = 0; i < count; i++ )
      if ( freq[i] > 0 ) {
	item[i].solution = vals[i];
	if ( vals[i] > 100.0 )
	  printf( "i=%d, strange value %.2f, freq=%d\n",
		  i, vals[i], freq[i] );
      }
      else
	item[i].solution = 0.0;
    free( freq );
    free( vals );
  }
}


/*
   FIND_MOST_COMMON
   Finds and marks the most common pattern of a feature.
*/

void find_most_common( InfoItem *item, int count ) {
  int i;
  int index, value;

  value = -1;
  index = 0;
  for ( i = 0; i < count; i++ )
    if ( item[i].frequency > value ) {
      index = i;
      value = item[i].frequency;
    }
  item[index].most_common = 1;
  item[index].solution = 0.0;
}


/*
   INITIALIZE_NON_PATTERNS
   Reads or calculates the starting point for features not
   corresponding to patterns in the board.
*/

void initialize_non_patterns( const char *base ) {
  char file_name[32];
  FILE *stream;

  sprintf( file_name, "%s.s%d", base, analysis_stage );
  stream = fopen( file_name, "r" );
  if ( stream == NULL ) {
    if ( USE_PARITY )
      parity.solution = 0.0;
    constant.solution = 0.0;
  }
  else {
    fscanf( stream, "%lf", &constant.solution );
    if ( USE_PARITY )
      fscanf( stream, "%lf", &parity.solution );
    fclose( stream );
  }
}


/*
   LIMIT_CHANGE
   Change one feature value, but not more than the damping specifies.
*/

void limit_change( double *value, float change ) {
  if ( change > +UPDATE_BOUND )
    change = +UPDATE_BOUND;
  else if ( change < -UPDATE_BOUND )
    change = -UPDATE_BOUND;
  *value += change;
}

/*
   UPDATE_SOLUTION
   Changes a specific set of pattern using a specified scale.
   Notice that pattern 0 is not updated; it is removed to
   obtain linear independence,
*/

void update_solution( InfoItem *item, int count, double scale ) {
  double change, abs_change;
  int i;

  for ( i = 0; i < count; i++ )
    if ( (item[i].frequency > 0) && (!item[i].most_common) ) {
      change = scale * item[i].direction;
      abs_change = fabs( change );
      if ( abs_change > max_delta )
	max_delta = abs_change;
      delta_sum += abs_change;
      if ( change > +UPDATE_BOUND )
	change = +UPDATE_BOUND;
      else if ( change < -UPDATE_BOUND )
	change = -UPDATE_BOUND;
      limit_change( &item[i].solution, change );
      update_count++;
    }
}


/*
   UPDATE_SEARCH_DIRECTION
   Update the search direction for a set of pattern using
   Fletcher-Reeves' update rule.
*/

void update_search_direction(InfoItem *item, int count, double beta) {
  int i;

  for (i = 0; i < count; i++)
    if (!item[i].most_common)
      item[i].direction =
	beta * item[i].direction - item[i].gradient;
    else
      item[i].direction = 0.0;
}


int main(int argc, char *argv[]) {
  char *game_file, *option_file;
  char prefix[32];
  double alpha, beta;
  double predicted_objective;
  double grad_sum, old_grad_sum;
  int i;
  int iteration, max_iterations;
  int count;
  time_t start_time, curr_time;
  FILE *option_stream;

  time(&start_time);

  if ( (argc < 4) || (argc > 7) ) {
    puts( "Usage:" );
    puts( "  tune8dbs <position file> <option file> <stage>" );
    puts( "           [<max #positions>] [<iterations>] [<max diff>]" );
    puts( "" );
    puts( "Gunnar Andersson, July 19, 1999" );
    exit( EXIT_FAILURE );
  }

  game_file = argv[1];
  option_file = argv[2];
  analysis_stage = atoi( argv[3] );
  if ( argc >= 5 )
    max_positions = atoi( argv[4] );
  else
    max_positions = DEFAULT_MAX_POSITIONS;
  if ( argc >= 6 )
    max_iterations = atoi( argv[5] );
  else
    max_iterations = 100000000;
  if ( argc >= 7 )
    max_diff = atoi( argv[6] );
  else
    max_diff = 64;

  /* Create pattern tables and reset all feature values */

  printf( "Building pattern tables... " );
  fflush( stdout );
  pattern_setup();
  puts( "done" );

  /* Parse the option file */

  option_stream = fopen( option_file, "r" );
  if ( option_stream == NULL ) {
    printf( "Unable to open option file '%s'\n", option_file );
    exit( EXIT_FAILURE );
  }
  fscanf( option_stream, "%s", prefix );
  fscanf( option_stream, "%d", &stage_count );
  for ( i = 0; i < stage_count; i++ )
    fscanf( option_stream, "%d", &stage[i] );
  fclose( option_stream );

  for ( i = 0; i <= 60; i++ )
    active[i] = FALSE;

  if ( analysis_stage != 0 )
    for ( i = stage[analysis_stage - 1] + 1;
	  i <= stage[analysis_stage]; i++ ) {
      active[i] = TRUE;
      weight[i] = sqrt( 1.0 * (i - stage[analysis_stage - 1]) /
			(stage[analysis_stage] - stage[analysis_stage - 1]) );
    }
  if ( analysis_stage != stage_count - 1 )
    for ( i = stage[analysis_stage]; i < stage[analysis_stage + 1]; i++ ) {
      active[i] = TRUE;
      weight[i] = sqrt( 1.0 * (stage[analysis_stage + 1] - i) /
			(stage[analysis_stage + 1] - stage[analysis_stage]) );
    }
  for ( i = 0; i <= 60; i++ )
    if ( active[i] )
      last_active = i;
  printf( "Last active phase: %d\n", last_active );

  /* Initialize the database */   

  read_position_file( game_file );

  /* Determine pattern frequencies */

  printf( "\nPreparing..." );
  fflush( stdout );
  analyze_games();
  printf( " done (%d relevant nodes out of %d)\n", relevant_count,
	  node_count );

  interval = (((relevant_count / 5) + 9) / 10) * 10;

  printf( "Reading pattern values... " );
  fflush( stdout );
  initialize_non_patterns( "main" );
  if ( USE_A_FILE ) {
    initialize_solution("afile", afile, 6561, mirror);
    find_most_common(afile, 6561);
  }
  else if ( USE_A_FILE2X ) {
    initialize_solution("afile2x", afile2x, 59049, mirror82x);
    find_most_common(afile2x, 59049);
  }
  if ( USE_B_FILE ) {
    initialize_solution("bfile", bfile, 6561, mirror);
    find_most_common(bfile, 6561);
  }
  if ( USE_C_FILE ) {
    initialize_solution("cfile", cfile, 6561, mirror);
    find_most_common(cfile, 6561);
  }
  if ( USE_D_FILE ) {
    initialize_solution("dfile", dfile, 6561, mirror);
    find_most_common(dfile, 6561);
  }
  if ( USE_DIAG8 ) {
    initialize_solution("diag8", diag8, 6561, mirror);
    find_most_common(diag8, 6561);
  }
  if ( USE_DIAG7 ) {
    initialize_solution("diag7", diag7, 2187, mirror7);
    find_most_common(diag7, 2187);
  }
  if ( USE_DIAG6 ) {
    initialize_solution("diag6", diag6, 729, mirror6);
    find_most_common(diag6, 729);
  }
  if ( USE_DIAG5 ) {
    initialize_solution("diag5", diag5, 243, mirror5);
    find_most_common(diag5, 243);
  }
  if ( USE_DIAG4 ) {
    initialize_solution("diag4", diag4, 81, mirror4);
    find_most_common(diag4, 81);
  }
  if ( USE_CORNER33 ) {
    initialize_solution("corner33", corner33, 19683, mirror33);
    find_most_common(corner33, 19683);
  }
  if ( USE_CORNER52 ) {
    initialize_solution( "corner52", corner52, 59049, identity10 );
    find_most_common( corner52, 59049 );
  }
  puts("done");

  /* Scan through the database and generate the data points */

  grad_sum = 0.0;
  max_delta = 0.0;
  average_delta = 0.0;
  for (iteration = 1; iteration <= max_iterations; iteration++) {
    constant.gradient = 0.0;
    if ( USE_PARITY )
      parity.gradient = 0.0;
    if ( USE_A_FILE2X )
      for ( i = 0; i < 59059; i++ )
	afile2x[i].gradient = 0.0;
    if ( USE_CORNER52 )
      for ( i = 0; i < 59049; i++ )
	corner52[i].gradient = 0.0;
    if ( USE_CORNER33 )
      for ( i = 0; i < 19683; i++ )
	corner33[i].gradient = 0.0;
    for ( i = 0; i < 6561; i++ ) {
      if ( USE_A_FILE )
	afile[i].gradient = 0.0;
      if ( USE_B_FILE )
	bfile[i].gradient = 0.0;
      if ( USE_C_FILE )
	cfile[i].gradient = 0.0;
      if ( USE_D_FILE )
	dfile[i].gradient = 0.0;
      if ( USE_DIAG8 )
	diag8[i].gradient = 0.0;
      }
    if ( USE_DIAG7 )
      for ( i = 0; i < 2187; i++ )
	diag7[i].gradient = 0.0;
    if ( USE_DIAG6 )
      for ( i = 0; i < 729; i++ )
	diag6[i].gradient = 0.0;
    if ( USE_DIAG5 )
      for ( i = 0; i < 243; i++ )
	diag5[i].gradient = 0.0;
    if ( USE_DIAG4 )
      for ( i = 0; i < 81; i++ )
	diag4[i].gradient = 0.0;
      
    objective = 0.0;
    abs_error_sum = 0.0;
    printf( "\nDetermining gradient:      " );
    fflush( stdout );
    count = 0;
    total_weight = 0.0;
    evaluate_games();
    printf( " %d\n", relevant_count );
    objective /= total_weight;
    abs_error_sum /= total_weight;
    store_patterns();
    time(&curr_time);
    printf( "Objective: %.8f    Av. error: %.8f    Time: %ld s    Iter %d\n",
	    objective, abs_error_sum, (long) (curr_time - start_time),
	    iteration );

    /* Measure the gradient */

    printf( "Updating the gradient length... " );
    fflush( stdout );
    old_grad_sum = grad_sum;
    grad_sum = 0.0;
    grad_sum += SQR( constant.gradient );
    if ( USE_PARITY )
      grad_sum += SQR(parity.gradient);
    if ( USE_A_FILE2X )
      for (i = 0; i < 59049; i++)
	if (!afile2x[i].most_common)
	  grad_sum += SQR(afile2x[i].gradient);
    if ( USE_CORNER52 )
      for ( i = 0; i < 59049; i++ )
	if ( !corner52[i].most_common )
	  grad_sum += SQR( corner52[i].gradient );
    if ( USE_CORNER33 )
      for (i = 0; i < 19683; i++)
	if (!corner33[i].most_common)
	  grad_sum += SQR(corner33[i].gradient);
    for (i = 0; i < 6561; i++) {
      if ( USE_A_FILE )
	if (!afile[i].most_common)
	  grad_sum += SQR(afile[i].gradient);
      if ( USE_B_FILE )
	if (!bfile[i].most_common)
	  grad_sum += SQR(bfile[i].gradient);
      if ( USE_C_FILE )
	if (!cfile[i].most_common)
	  grad_sum += SQR(cfile[i].gradient);
      if ( USE_D_FILE )
	if (!dfile[i].most_common)
	  grad_sum += SQR(dfile[i].gradient);
      if ( USE_DIAG8 )
	if (!diag8[i].most_common)
	  grad_sum += SQR(diag8[i].gradient);
    }
    if ( USE_DIAG7 )
      for (i = 0; i < 2187; i++)
	if (!diag7[i].most_common)
	  grad_sum += SQR(diag7[i].gradient);
    if ( USE_DIAG6 )
      for (i = 0; i < 729; i++)
	if (!diag6[i].most_common)
	  grad_sum += SQR(diag6[i].gradient);
    if ( USE_DIAG5 )
      for (i = 0; i < 243; i++)
	if (!diag5[i].most_common)
	  grad_sum += SQR(diag5[i].gradient);
    if ( USE_DIAG4 )
      for (i = 0; i < 81; i++)
	if (!diag4[i].most_common)
	  grad_sum += SQR(diag4[i].gradient);

    /* Determine the current search direction */

    if (iteration > 1)
      beta = grad_sum / old_grad_sum;
    else
      beta = 0.0;
    printf("beta=%.8f\n", beta);
    constant.direction = beta * constant.direction - constant.gradient;
    if ( USE_PARITY )
      parity.direction = beta * parity.direction - parity.gradient;
    if ( USE_A_FILE )
      update_search_direction(afile, 6561, beta);
    else if ( USE_A_FILE2X )
      update_search_direction(afile2x, 59049, beta);
    if ( USE_B_FILE )
      update_search_direction(bfile, 6561, beta);
    if ( USE_C_FILE )
      update_search_direction(cfile, 6561, beta);
    if ( USE_D_FILE )
      update_search_direction(dfile, 6561, beta);
    if ( USE_DIAG8 )
      update_search_direction(diag8, 6561, beta);
    if ( USE_DIAG7 )
      update_search_direction(diag7, 2187, beta);
    if ( USE_DIAG6 )
      update_search_direction(diag6, 729, beta);
    if ( USE_DIAG5 )
      update_search_direction(diag5, 243, beta);
    if ( USE_DIAG4 )
      update_search_direction(diag4, 81, beta);
    if ( USE_CORNER33 )
      update_search_direction(corner33, 19683, beta);
    if ( USE_CORNER52 )
      update_search_direction(corner52, 59049, beta);

    /* Find the best one-dimensional step */

    printf("Determining step:          ");
    fflush(stdout);
    count = 0;
    quad_coeff = 0.0;
    lin_coeff = 0.0;
    const_coeff = 0.0;
    determine_games();
    printf(" %d\n", relevant_count);
    quad_coeff /= total_weight;
    lin_coeff /= total_weight;
    const_coeff /= total_weight;
    alpha = -lin_coeff / (2.0 * quad_coeff);
    predicted_objective = const_coeff - quad_coeff * alpha * alpha;
    printf("alpha=%.8f predicts %.8f\n", alpha, predicted_objective);

    /* Update the solution */
    max_delta = 0.0;
    delta_sum = 0.0;
    update_count = 0;
    limit_change(&constant.solution,
		 alpha * constant.direction / total_weight);
    if ( USE_PARITY )
      limit_change(&parity.solution,
		   alpha * parity.direction / total_weight);
    if ( USE_A_FILE )
      update_solution(afile, 6561, alpha / total_weight);
    else if ( USE_A_FILE2X )
      update_solution(afile2x, 59049, alpha / total_weight);
    if ( USE_B_FILE )
      update_solution(bfile, 6561, alpha / total_weight);
    if ( USE_C_FILE )
      update_solution(cfile, 6561, alpha / total_weight);
    if ( USE_D_FILE )
      update_solution(dfile, 6561, alpha / total_weight);
    if ( USE_DIAG8 )
      update_solution(diag8, 6561, alpha / total_weight);
    if ( USE_DIAG7 )
      update_solution(diag7, 2187, alpha / total_weight);
    if ( USE_DIAG6 )
      update_solution(diag6, 729, alpha / total_weight);
    if ( USE_DIAG5 )
      update_solution(diag5, 243, alpha / total_weight);
    if ( USE_DIAG4 )
      update_solution(diag4, 81, alpha / total_weight);
    if ( USE_CORNER33 )
      update_solution(corner33, 19683, alpha / total_weight);
    if ( USE_CORNER52 )
      update_solution( corner52, 59049, alpha / total_weight );

    average_delta = delta_sum / update_count;
    printf("Constant: %.4f  Parity: %.4f  Max change: %.5f  Av. change: %.5f\n",
	   constant.solution, parity.solution, max_delta, average_delta);
    if (iteration % 10 == 0)
      write_log(iteration);
  }

  return EXIT_SUCCESS;
}
