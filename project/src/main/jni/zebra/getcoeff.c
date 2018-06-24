/*
   File:          getcoeff.c

   Created:       November 19, 1997

   Modified:      January 3, 2003
   
   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      Unpacks the coefficient file, computes final-stage
                  pattern values and performs pattern evaluation.
*/


#include "porting.h"


/* #define __linux__ */


#if !defined( _WIN32_WCE ) && !defined( __linux__ )
#include <dir.h>
#endif

#ifdef _WIN32_WCE
#include "../zlib113/zlib.h"
#else
#include <assert.h>
#include <zlib.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "constant.h"
#include "error.h"
#include "eval.h"
#include "getcoeff.h"
#include "macros.h"
#include "magic.h"
#include "moves.h"
#include "patterns.h"
#include "safemem.h"
#include "search.h"
#include "texts.h"



/* An upper limit on the number of coefficient blocks in the arena */
#define MAX_BLOCKS            200

/* The file containing the feature values (really shouldn't be #define'd) */
#define PATTERN_FILE          "coeffs2.bin"

/* Calculate cycle counts for the eval function? */
#define TIME_EVAL             0



typedef struct {
  int permanent;
  int loaded;
  int prev, next;
  int block;
  short parity_constant[2];
  short parity;
  short constant;
  short *afile2x, *bfile, *cfile, *dfile;
  short *diag8, *diag7, *diag6, *diag5, *diag4;
  short *corner33, *corner52;
  short *afile2x_last, *bfile_last, *cfile_last, *dfile_last;
  short *diag8_last, *diag7_last, *diag6_last, *diag5_last, *diag4_last;
  short *corner33_last, *corner52_last;
  char alignment_padding[12];  /* In order to achieve 128-byte alignment */
} CoeffSet;


typedef struct {
  short afile2x_block[59049];
  short bfile_block[6561];
  short cfile_block[6561];
  short dfile_block[6561];
  short diag8_block[6561];
  short diag7_block[2187];
  short diag6_block[729];
  short diag5_block[243];
  short diag4_block[81];
  short corner33_block[19683];
  short corner52_block[59049];
} AllocationBlock;



static int stage_count;
static int block_count;
static int stage[61];
static int block_allocated[MAX_BLOCKS], block_set[MAX_BLOCKS];
static int eval_map[61];
static AllocationBlock *block_list[MAX_BLOCKS];
static CoeffSet set[61];



/*
   TERMINAL_PATTERNS
   Calculates the patterns associated with a filled board,
   only counting discs.
*/

static void
terminal_patterns( void ) {
  double result;
  double value[8][8];
  int i, j, k;
  int row[10];
  int hit[8][8];

  /* Count the number of times each square is counted */

  for ( i = 0; i < 8; i++ )
    for ( j = 0; j < 8; j++ )
      hit[i][j] = 0;
  for ( i = 0; i < 8; i++ ) {
    hit[0][i]++;
    hit[i][0]++;
    hit[7][i]++;
    hit[i][7]++;
  }
  for ( i = 0; i < 8; i++ ) {
    hit[1][i]++;
    hit[i][1]++;
    hit[6][i]++;
    hit[i][6]++;
  }
  for ( i = 0; i < 8; i++ ) {
    hit[2][i]++;
    hit[i][2]++;
    hit[5][i]++;
    hit[i][5]++;
  }
  for ( i = 0; i < 8; i++ ) {
    hit[3][i]++;
    hit[i][3]++;
    hit[4][i]++;
    hit[i][4]++;
  }
  for ( i = 0; i < 3; i++ )
    for ( j = 0; j < 3; j++ ) {
      hit[i][j]++;
      hit[i][7 - j]++;
      hit[7 - i][j]++;
      hit[7 - i][7 - j]++;
    }
  for ( i = 0; i < 2; i++ )
    for ( j = 0; j < 5; j++ ) {
      hit[i][j]++;
      hit[j][i]++;
      hit[i][7 - j]++;
      hit[j][7 - i]++;
      hit[7 - i][j]++;
      hit[7 - j][i]++;
      hit[7 - i][7 - j]++;
      hit[7 - j][7 - i]++;
    }
  for ( i = 0; i < 8; i++ ) {
    hit[i][i]++;
    hit[i][7 - i]++;
  }
  for ( i = 0; i < 7; i++ ) {
    hit[i][i + 1]++;
    hit[i + 1][i]++;
    hit[i][6 - i]++;
    hit[i + 1][7 - i]++;
  }
  for ( i = 0; i < 6; i++ ) {
    hit[i][i + 2]++;
    hit[i + 2][i]++;
    hit[i][5 - i]++;
    hit[i + 2][7 - i]++;
  }
  for ( i = 0; i < 5; i++ ) {
    hit[i][i + 3]++;
    hit[i + 3][i]++;
    hit[i][4 - i]++;
    hit[i + 3][7 - i]++;
  }
  for ( i = 0; i < 4; i++ ) {
    hit[i][i + 4]++;
    hit[i + 4][i]++;
    hit[i][3 - i]++;
    hit[i + 4][7 - i]++;
  }
  hit[1][1] += 2;
  hit[1][6] += 2;
  hit[6][1] += 2;
  hit[6][6] += 2;

  for ( i = 0; i < 8; i++ )
    for ( j = 0; j < 8; j++ )
      value[i][j] = 1.0 / hit[i][j];

  for ( i = 0; i < 10; i++ )
    row[i] = 0;

  for ( i = 0; i < 59049; i++ ) {
    result = 0.0;
    for ( j = 0; j < 8; j++ )
      if ( row[j] == BLACKSQ )
	result += value[0][j];
      else if ( row[j] == WHITESQ )
	result -= value[0][j];
    if ( row[8] == BLACKSQ )
      result += value[1][1];
    else if ( row[8] == WHITESQ )
      result -= value[1][1];
    if ( row[9] == BLACKSQ )
      result += value[1][6];
    else if ( row[9] == WHITESQ )
      result -= value[1][6];
    set[60].afile2x[i] = floor( result * 128.0 + 0.5 );

    result = 0.0;
    for ( j = 0; j < 5; j++ )
      for ( k = 0; k < 2; k++ )
	if ( row[5 * k + j] == BLACKSQ )
	  result += value[j][k];
	else if ( row[5 * k + j] == WHITESQ )
	  result -= value[j][k];
    set[60].corner52[i] = floor( result * 128.0 + 0.5 );

    if ( i < 19683 ) {
      result = 0.0;
      for ( j = 0; j < 3; j++ )
	for ( k = 0; k < 3; k++ )
	  if ( row[3 * j + k] == BLACKSQ )
	    result += value[j][k];
	  else if ( row[3 * j + k] == WHITESQ )
	    result -= value[j][k];
      set[60].corner33[i] = floor( result * 128.0 + 0.5 );
    }
    if ( i < 6561 ) {
      result = 0.0;
      for ( j = 0; j < 8; j++ )
	if ( row[j] == BLACKSQ )
	  result += value[1][j];
	else if ( row[j] == WHITESQ )
	  result -= value[1][j];
      set[60].bfile[i] = floor( result * 128.0 + 0.5 );
         
      result = 0.0;
      for ( j = 0; j < 8; j++ )
	if ( row[j] == BLACKSQ )
	  result += value[2][j];
	else if ( row[j] == WHITESQ )
	  result -= value[2][j];
      set[60].cfile[i] = floor( result * 128.0 + 0.5 );
         
      result = 0.0;
      for ( j = 0; j < 8; j++ )
	if ( row[j] == BLACKSQ )
	  result += value[3][j];
	else if ( row[j] == WHITESQ )
	  result -= value[3][j];
      set[60].dfile[i] = floor( result * 128.0 + 0.5 );
         
      result = 0.0;
      for ( j = 0; j < 8; j++ )
	if ( row[j] == BLACKSQ )
	  result += value[j][j];
	else if ( row[j] == WHITESQ )
	  result -= value[j][j];
      set[60].diag8[i] = floor( result * 128.0 + 0.5 );
    }
    if ( i < 2187 ) {
      result = 0.0;
      for ( j = 0; j < 7; j++ )
	if ( row[j] == BLACKSQ )
	  result += value[j][j + 1];
	else if ( row[j] == WHITESQ )
	  result -= value[j][j + 1];
      set[60].diag7[i] = floor( result * 128.0 + 0.5 );
    }
    if ( i < 729 ) {
      result = 0.0;
      for ( j = 0; j < 6; j++ )
	if ( row[j] == BLACKSQ )
	  result += value[j][j + 2];
	else if ( row[j] == WHITESQ )
	  result -= value[j][j + 2];
      set[60].diag6[i] = floor( result * 128.0 + 0.5 );
    }
    if ( i < 243 ) {
      result = 0.0;
      for ( j = 0; j < 5; j++ )
	if ( row[j] == BLACKSQ )
	  result += value[j][j + 3];
	else if ( row[j] == WHITESQ )
	  result -= value[j][j + 3];
      set[60].diag5[i] = floor( result * 128.0 + 0.5 );
    }
    if ( i < 81 ) {
      result = 0.0;
      for ( j = 0; j < 4; j++ )
	if ( row[j] == BLACKSQ )
	  result += value[j][j + 4];
	else if ( row[j] == WHITESQ )
	  result -= value[j][j + 4];
      set[60].diag4[i] = floor( result * 128.0 + 0.5 );
    }

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while ( (row[j - 1] == 0) && (j < 10) );
  }
}


/*
   GET_WORD
   Reads a 16-bit signed integer from a file.
*/   

static short
get_word( gzFile stream ) {
  union {
    short signed_val;
    unsigned short unsigned_val;
  } val;

#if 0
  int received;
  unsigned char hi, lo;

  received = fread( &hi, sizeof( unsigned char ), 1, stream );
  if ( received != 1 )
    puts( READ_ERROR_HI );
  received = fread( &lo, sizeof( unsigned char ), 1, stream );
  if ( received != 1 )
    puts( READ_ERROR_LO );
#else
  int hi, lo;

  hi = gzgetc( stream );
  assert( hi != -1 );

  lo = gzgetc( stream );
  assert( lo != -1 );
#endif

  val.unsigned_val = (hi << 8) + lo;

  return val.signed_val;
}


/*
   UNPACK_BATCH
   Reads feature values for one specific pattern
*/

static void
unpack_batch( short *item, int *mirror, int count, gzFile stream ) {
  int i;
  short *buffer;
  
  buffer = (short *) safe_malloc( count * sizeof( short ) );
  
  /* Unpack the coefficient block where the score is scaled
     so that 512 units corresponds to one disk. */

  for ( i = 0; i < count; i++ )
    if ( (mirror == NULL) || (mirror[i] == i) )
      buffer[i] = get_word( stream ) / 4;
    else
      buffer[i] = buffer[mirror[i]];

  for ( i = 0; i < count; i++ )
    item[i] = buffer[i];
  if ( mirror != NULL )
    for ( i = 0; i < count; i++ )
      if ( item[i] != item[mirror[i]] ) {
	printf( "%s @ %d <--> %d of %d\n", MIRROR_ERROR,
		i, mirror[i], count );
	printf( "%d <--> %d\n", item[i], item[mirror[i]] );
	exit( EXIT_FAILURE );
      }

  free( buffer );
}


/*
   UNPACK_COEFFS
   Reads all feature values for a certain stage. To take care of
   symmetric patterns, mirror tables are calculated.
*/

static void
unpack_coeffs( gzFile stream ) {
  int i, j, k;
  int mirror_pattern;
  int row[10];
  int *map_mirror3;
  int *map_mirror4;
  int *map_mirror5;
  int *map_mirror6;
  int *map_mirror7;
  int *map_mirror8;
  int *map_mirror33;
  int *map_mirror8x2;

  /* Allocate the memory needed for the temporary mirror maps from the
     heap rather than the stack to reduce memory requirements. */

  map_mirror3 = (int *) safe_malloc( 27 * sizeof( int ) );
  map_mirror4 = (int *) safe_malloc( 81 * sizeof( int ) );
  map_mirror5 = (int *) safe_malloc( 243 * sizeof( int ) );
  map_mirror6 = (int *) safe_malloc( 729 * sizeof( int ) );
  map_mirror7 = (int *) safe_malloc( 2187 * sizeof( int ) );
  map_mirror8 = (int *) safe_malloc( 6561 * sizeof( int ) );
  map_mirror33 = (int *) safe_malloc( 19683 * sizeof( int ) );
  map_mirror8x2 = (int *) safe_malloc( 59049 * sizeof( int ) );

  /* Build the pattern tables for 8*1-patterns */

  for ( i = 0; i < 8; i++ )
    row[i] = 0;

  for ( i = 0; i < 6561; i++ ) {
    mirror_pattern = 0;
    for ( j = 0; j < 8; j++ )
      mirror_pattern += row[j] * pow3[7 - j];
    /* Create the symmetry map */
    map_mirror8[i] = MIN( i, mirror_pattern );

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while ( (row[j - 1] == 0) && (j < 8) );
  }

  /* Build the tables for 7*1-patterns */

  for ( i = 0; i < 7; i++ )
    row[i] = 0;

  for ( i = 0; i < 2187; i++ ) {
    mirror_pattern = 0;
    for ( j = 0; j < 7; j++ )
      mirror_pattern += row[j] * pow3[6 - j];
    map_mirror7[i] = MIN( i, mirror_pattern );

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while ( (row[j - 1]) == 0 && (j < 7) );
  }

  /* Build the tables for 6*1-patterns */

  for ( i = 0; i < 6; i++ )
    row[i] = 0;
  for ( i = 0; i < 729; i++ ) {
    mirror_pattern = 0;
    for ( j = 0; j < 6; j++ )
      mirror_pattern += row[j] * pow3[5 - j];
    map_mirror6[i] = MIN( i, mirror_pattern );

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while ( (row[j - 1]) == 0 && (j < 6) );
  }

  /* Build the tables for 5*1-patterns */

  for ( i = 0; i < 5; i++ )
    row[i] = 0;

  for ( i = 0; i < 243; i++ ) {
    mirror_pattern = 0;
    for ( j = 0; j < 5; j++ )
      mirror_pattern += row[j] * pow3[4 - j];
    map_mirror5[i] = MIN( mirror_pattern, i );

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while ( (row[j - 1] == 0) && (j < 5) );
  }

  /* Build the tables for 4*1-patterns */   

  for ( i = 0; i < 4; i++ )
    row[i] = 0;

  for ( i = 0; i < 81; i++ ) {
    mirror_pattern = 0;
    for ( j = 0; j < 4; j++ )
      mirror_pattern += row[j] * pow3[3 - j];
    map_mirror4[i] = MIN( i, mirror_pattern );

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while ( (row[j - 1]) == 0 && (j < 4) );
  }

  /* Build the tables for 3*1-patterns */   

  for ( i = 0; i < 3; i++ )
    row[i] = 0;

  for ( i = 0; i < 27; i++ ) {
    mirror_pattern = 0;
    for ( j = 0; j < 3; j++ )
      mirror_pattern += row[j] * pow3[2 - j];
    map_mirror3[i] = MIN( i, mirror_pattern );

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

  /* --- none needed --- */

  /* Build the tables for edge2X-patterns */

  for ( i = 0; i < 6561; i++ )
    for ( j = 0; j < 3; j++ )
      for ( k = 0; k < 3; k++ )
	map_mirror8x2[i + 6561 * j + 19683 * k] =
	  MIN( flip8[i] + 6561 * k + 19683 * j, i + 6561 * j + 19683 * k );

  /* Build the tables for 3*3-patterns */

  for ( i = 0; i < 9; i++ )
    row[i] = 0;

  for ( i = 0; i < 19683; i++ ) {
    mirror_pattern =
      row[0] + 3 * row[3] + 9 * row[6] +
      27 * row[1] + 81 * row[4] + 243 * row[7] +
      729 * row[2] + 2187 * row[5] + 6561 * row[8];
    map_mirror33[i] = MIN( i, mirror_pattern );

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while ( (row[j - 1] == 0) && (j < 9) );
  }

  /* Read and unpack - using symmetries - the coefficient tables. */

  for ( i = 0; i < stage_count - 1; i++ ) {
    set[stage[i]].constant = get_word( stream ) / 4;
    set[stage[i]].parity = get_word( stream ) / 4;
    set[stage[i]].parity_constant[0] = set[stage[i]].constant;
    set[stage[i]].parity_constant[1] =
      set[stage[i]].constant + set[stage[i]].parity;
    unpack_batch( set[stage[i]].afile2x, map_mirror8x2, 59049, stream );
    unpack_batch( set[stage[i]].bfile, map_mirror8, 6561, stream );
    unpack_batch( set[stage[i]].cfile, map_mirror8, 6561, stream );
    unpack_batch( set[stage[i]].dfile, map_mirror8, 6561, stream );
    unpack_batch( set[stage[i]].diag8, map_mirror8, 6561, stream );
    unpack_batch( set[stage[i]].diag7, map_mirror7, 2187, stream );
    unpack_batch( set[stage[i]].diag6, map_mirror6, 729, stream );
    unpack_batch( set[stage[i]].diag5, map_mirror5, 243, stream );
    unpack_batch( set[stage[i]].diag4, map_mirror4, 81, stream );
    unpack_batch( set[stage[i]].corner33, map_mirror33, 19683, stream );
    unpack_batch( set[stage[i]].corner52, NULL, 59049, stream );
  }

  /* Free the mirror tables - the symmetries are now implicit
     in the coefficient tables. */

  free( map_mirror3 );
  free( map_mirror4 );
  free( map_mirror5 );
  free( map_mirror6 );
  free( map_mirror7 );
  free( map_mirror8 );
  free( map_mirror33 );
  free( map_mirror8x2 );
}


/*
   GENERATE_BATCH
   Interpolates between two stages.
*/   

static void
generate_batch( short *target, int count, short *source1, int weight1,
		short *source2, int weight2 ) {
  int i;
  int total_weight;

  total_weight = weight1 + weight2;
  for ( i = 0; i < count; i++ )
    target[i] = (weight1 * source1[i] + weight2 * source2[i]) /
      total_weight;
}


/*
   FIND_MEMORY_BLOCK
   Maintains an internal memory handler to boost
   performance and avoid heap fragmentation.
*/

static int
find_memory_block( short **afile2x, short **bfile, short **cfile,
		   short **dfile, short **diag8, short **diag7,
		   short **diag6, short **diag5, short **diag4,
		   short **corner33, short **corner52, int index ) {
  int i;
  int found_free, free_block;

  found_free = FALSE;
  free_block = -1;
  for ( i = 0; (i < block_count) && !found_free; i++ )
    if (!block_allocated[i] ) {
      found_free = TRUE;
      free_block = i;
    }
  if ( !found_free ) {
    if ( block_count < MAX_BLOCKS )
      block_list[block_count] =
	(AllocationBlock *) safe_malloc( sizeof( AllocationBlock ) );
    if ( (block_count == MAX_BLOCKS) ||
	 (block_list[block_count] == NULL) )
      fatal_error( "%s @ #%d\n", MEMORY_ERROR, block_count );
    free_block = block_count;
    block_count++;
  }
  *afile2x = block_list[free_block]->afile2x_block;
  *bfile = block_list[free_block]->bfile_block;
  *cfile = block_list[free_block]->cfile_block;
  *dfile = block_list[free_block]->dfile_block;
  *diag8 = block_list[free_block]->diag8_block;
  *diag7 = block_list[free_block]->diag7_block;
  *diag6 = block_list[free_block]->diag6_block;
  *diag5 = block_list[free_block]->diag5_block;
  *diag4 = block_list[free_block]->diag4_block;
  *corner33 = block_list[free_block]->corner33_block;
  *corner52 = block_list[free_block]->corner52_block;
  block_allocated[free_block] = TRUE;
  block_set[free_block] = index;

  return free_block;
}


/*
   FREE_MEMORY_BLOCK
   Marks a memory block as no longer in use.
*/

static void
free_memory_block( int block ) {
  block_allocated[block] = FALSE;
}


/*
   INIT_MEMORY_HANDLER
   Mark all blocks in the memory arena as "not used".
*/

static void
init_memory_handler( void ) {
  int i;

  block_count = 0;
  for ( i = 0; i < MAX_BLOCKS; i++ )
    block_allocated[i] = FALSE;
}


/*
   ALLOCATE_SET
   Finds memory for all patterns belonging to a certain stage.
*/

static void
allocate_set( int index ) {
  set[index].block =
    find_memory_block( &set[index].afile2x, &set[index].bfile,
		       &set[index].cfile, &set[index].dfile,
		       &set[index].diag8, &set[index].diag7,
		       &set[index].diag6, &set[index].diag5, &set[index].diag4,
		       &set[index].corner33, &set[index].corner52, index );
}


/*
   LOAD_SET
   Performs linear interpolation between the nearest stages to
   obtain the feature values for the stage in question.
   Also calculates the offset pointers to the last elements in each block
   (used for the inverted patterns when white is to move).
*/   

static void
load_set( int index ) {
  int prev, next;
  int weight1, weight2, total_weight;

  if ( !set[index].permanent ) {
    prev = set[index].prev;
    next = set[index].next;
    if ( prev == next ) {
      weight1 = 1;
      weight2 = 1;
    }
    else {
      weight1 = next - index;
      weight2 = index - prev;
    }
    total_weight = weight1 + weight2;
    set[index].constant =
      (weight1 * set[prev].constant + weight2 * set[next].constant) /
      total_weight;
    set[index].parity =
      (weight1 * set[prev].parity + weight2 * set[next].parity) /
      total_weight;
    set[index].parity_constant[0] = set[index].constant;
    set[index].parity_constant[1] =
      set[index].constant + set[index].parity;
    allocate_set( index );
    generate_batch( set[index].afile2x, 59049,
		    set[prev].afile2x, weight1,
		    set[next].afile2x, weight2 );
    generate_batch( set[index].bfile, 6561,
		    set[prev].bfile, weight1,
		    set[next].bfile, weight2 );
    generate_batch( set[index].cfile, 6561,
		    set[prev].cfile, weight1,
		    set[next].cfile, weight2 );
    generate_batch( set[index].dfile, 6561,
		    set[prev].dfile, weight1,
		    set[next].dfile, weight2 );
    generate_batch( set[index].diag8, 6561,
		    set[prev].diag8, weight1,
		    set[next].diag8, weight2 );
    generate_batch( set[index].diag7, 2187,
		    set[prev].diag7, weight1,
		    set[next].diag7, weight2 );
    generate_batch( set[index].diag6, 729,
		    set[prev].diag6, weight1,
		    set[next].diag6, weight2 );
    generate_batch( set[index].diag5, 243,
		    set[prev].diag5, weight1,
		    set[next].diag5, weight2 );
    generate_batch( set[index].diag4, 81,
		    set[prev].diag4, weight1,
		    set[next].diag4, weight2 );
    generate_batch( set[index].corner33, 19683,
		    set[prev].corner33, weight1,
		    set[next].corner33, weight2 );
    generate_batch( set[index].corner52, 59049,
		    set[prev].corner52, weight1,
		    set[next].corner52, weight2 );
  }

  set[index].afile2x_last = set[index].afile2x + 59048;
  set[index].bfile_last = set[index].bfile + 6560;
  set[index].cfile_last = set[index].cfile + 6560;
  set[index].dfile_last = set[index].dfile + 6560;
  set[index].diag8_last = set[index].diag8 + 6560;
  set[index].diag7_last = set[index].diag7 + 2186;
  set[index].diag6_last = set[index].diag6 + 728;
  set[index].diag5_last = set[index].diag5 + 242;
  set[index].diag4_last = set[index].diag4 + 80;
  set[index].corner33_last = set[index].corner33 + 19682;
  set[index].corner52_last = set[index].corner52 + 59048;

  set[index].loaded = 1;
}



/*
  DISC_COUNT_ADJUSTMENT
*/

static void
eval_adjustment( double disc_adjust, double edge_adjust,
		 double corner_adjust, double x_adjust ) {
  int i, j, k;
  int adjust;
  int row[10];

  for ( i = 0; i < stage_count - 1; i++ ) {

    /* Bonuses for having more discs */

    for ( j = 0; j < 59049; j++ ) {
      set[stage[i]].afile2x[j] += set[60].afile2x[j] * disc_adjust;
      set[stage[i]].corner52[j] += set[60].corner52[j] * disc_adjust;
    }
    for ( j = 0; j < 19683; j++ )
      set[stage[i]].corner33[j] += set[60].corner33[j] * disc_adjust;
    for ( j = 0; j < 6561; j++ ) {
      set[stage[i]].bfile[j] += set[60].bfile[j] * disc_adjust;
      set[stage[i]].cfile[j] += set[60].cfile[j] * disc_adjust;
      set[stage[i]].dfile[j] += set[60].dfile[j] * disc_adjust;
      set[stage[i]].diag8[j] += set[60].diag8[j] * disc_adjust;
    }
    for ( j = 0; j < 2187; j++ )
      set[stage[i]].diag7[j] += set[60].diag7[j] * disc_adjust;
    for ( j = 0; j < 729; j++ )
      set[stage[i]].diag6[j] += set[60].diag6[j] * disc_adjust;
    for ( j = 0; j < 243; j++ )
      set[stage[i]].diag5[j] += set[60].diag5[j] * disc_adjust;
    for ( j = 0; j < 81; j++ )
      set[stage[i]].diag4[j] += set[60].diag4[j] * disc_adjust;

    for ( j = 0; j < 10; j++ )
      row[j] = 0;

    for ( j = 0; j < 59049; j++ ) {
      adjust = 0;

      /* Bonus for having edge discs */

      for ( k = 1; k <= 6; k++ )
	if ( row[k] == BLACKSQ )
	  adjust += 128.0 * edge_adjust;
	else if ( row[k] == WHITESQ )
	  adjust -= 128.0 * edge_adjust;

      /* Bonus for having corners.  The "0.5 *" is because corners are part
	 of two A-file+2X patterns. */

      if ( row[0] == BLACKSQ )
	adjust += 0.5 * 128.0 * corner_adjust;
      else if ( row[0] == WHITESQ )
	adjust -= 0.5 * 128.0 * corner_adjust;
      if ( row[7] == BLACKSQ )
	adjust += 0.5 * 128.0 * corner_adjust;
      else if ( row[7] == WHITESQ )
	adjust -= 0.5 * 128.0 * corner_adjust;

      /* Bonus for having X-squares when the adjacent corners are empty.
	 Scaling by 0.5 applies here too. */

      if ( (row[8] == BLACKSQ) && (row[0] == EMPTY) )
	adjust += 0.5 * 128.0 * x_adjust;
      else if ( (row[8] == WHITESQ) && (row[0] == EMPTY) )
	adjust -= 0.5 * 128.0 * x_adjust;
      if ( (row[9] == BLACKSQ) && (row[7] == EMPTY) )
	adjust += 0.5 * 128.0 * x_adjust;
      else if ( (row[9] == WHITESQ) && (row[7] == EMPTY) )
	adjust -= 0.5 * 128.0 * x_adjust;

      set[stage[i]].afile2x[j] += adjust;

      /* Next configuration */
      k = 0;
      do {  /* The odometer principle */
	row[k]++;
	if ( row[k] == 3 )
	  row[k] = 0;
	k++;
      } while ( (row[k - 1] == 0) && (k < 10) );
    }
  }
}



/*
   INIT_COEFFS
   Manages the initialization of all relevant tables.
*/   

void
init_coeffs( void ) {
  int i, j;
  int word1, word2;
  int subsequent_stage;
  int curr_stage;
  gzFile coeff_stream;
  FILE *adjust_stream;
  char sPatternFile[260];

  init_memory_handler();

#if defined( _WIN32_WCE )
  /* Special hack for CE. */
  getcwd(sPatternFile, sizeof(sPatternFile));
  strcat(sPatternFile, PATTERN_FILE);
#elif defined(ANDROID)
  sprintf(sPatternFile, "%s/%s", android_files_dir, PATTERN_FILE);
#elif defined( __linux__ )
  /* Linux don't support current directory. */
  strcpy( sPatternFile, PATTERN_FILE );
#else
  getcwd(sPatternFile, sizeof(sPatternFile));
  strcat(sPatternFile, "/" PATTERN_FILE);
#endif

  coeff_stream = gzopen( sPatternFile, "rb" );
  if ( coeff_stream == NULL )
    fatal_error( "%s '%s'\n", FILE_ERROR, sPatternFile );

  /* Check the magic values in the beginning of the file to make sure
     the file format is right */

  word1 = get_word( coeff_stream );
  word2 = get_word( coeff_stream );

  if ( (word1 != EVAL_MAGIC1) || (word2 != EVAL_MAGIC2) )
    fatal_error( "%s: %s", sPatternFile, CHECKSUM_ERROR );

  /* Read the different stages for which the evaluation function
     was tuned and mark the other stages with pointers to the previous
     and next stages. */

  for ( i = 0; i <= 60; i++ ) {
    set[i].permanent = 0;
    set[i].loaded = 0;
  }

  stage_count = get_word( coeff_stream );
  for ( i = 0; i < stage_count - 1; i++ ) {
    stage[i] = get_word( coeff_stream );
    curr_stage = stage[i];
    if ( i == 0 )
      for ( j = 0; j < stage[0]; j++ ) {
	set[j].prev = stage[0];
	set[j].next = stage[0];
      }
    else
      for ( j = stage[i - 1]; j < stage[i]; j++ ) {
	set[j].prev = stage[i - 1];
	set[j].next = stage[i];
      }
    set[curr_stage].permanent = 1;
    allocate_set( curr_stage );
  }
  stage[stage_count - 1] = 60;   
  for ( j = stage[stage_count - 2]; j < 60; j++ ) {
    set[j].prev = stage[stage_count - 2];
    set[j].next = 60;
  }

  set[60].permanent = 1;
  allocate_set(60);

  /* Read the pattern values */

  unpack_coeffs( coeff_stream );
  gzclose( coeff_stream );

  /* Calculate the patterns which correspond to the board being filled */

  terminal_patterns();
  set[60].constant = 0;
  set[60].parity = 0;
  set[60].parity_constant[0] = set[60].constant;
  set[60].parity_constant[1] = set[60].constant + set[60].parity;

  /* Adjust the coefficients so as to reflect the encouragement for
     having lots of discs */

  adjust_stream = fopen( "adjust.txt", "r" );
  if ( adjust_stream != NULL ) {
    double disc_adjust = 0.0;
    double edge_adjust = 0.0;
    double corner_adjust = 0.0;
    double x_adjust = 0.0;

    fscanf( adjust_stream, "%lf %lf %lf %lf", &disc_adjust, &edge_adjust,
	    &corner_adjust, &x_adjust );
    eval_adjustment( disc_adjust, edge_adjust, corner_adjust, x_adjust );
    fclose( adjust_stream );
  }

  /* For each of number of disks played, decide on what set of evaluation
     patterns to use.
     The following rules apply:
     - Stages from the tuning are used as evaluation stages
     - Intermediate evaluation stages are introduced 2 stages before
     each tuning stage.
     - Other stages are mapped onto the next evaluation stage
     (which may be either from the tuning or an intermediate stage).
  */

  for ( i = 0; i < stage[0]; i++ )
    eval_map[i] = stage[0];
  for ( i = 0; i < stage_count; i++ )
    eval_map[stage[i]] = stage[i];

  for ( i = subsequent_stage = 60; i >= stage[0]; i-- ) {
    if ( eval_map[i] == i )
      subsequent_stage = i;
    else if ( i == subsequent_stage - 2 ) {
      eval_map[i] = i;
      subsequent_stage = i;
    }
    else
      eval_map[i] = subsequent_stage;
  }
}



static long long int
rdtsc( void ) {
/*
 I don't know what this does, but the ndk compiler will fail here
 because the A output constraint cannot be applied/found
#if defined(__GNUC__)
  long long a;
  asm volatile("rdtsc":"=A" (a));
  return a;
#else
  return 0;
#endif
 */
  return 0;
}



/*
   PATTERN_EVALUATION
   Calculates the static evaluation of the position using
   the statistically optimized pattern tables.
*/   

#ifdef LOG_EVAL
#include "display.h"
#endif

short pattern_score;

INLINE int
pattern_evaluation( int side_to_move ) {
  int eval_phase;
  short score;
#if TIME_EVAL
  long long t0 = rdtsc();
#endif

#ifdef LOG_EVAL
  FILE *stream = fopen( "eval.log", "a" );

  display_board( stream, board, side_to_move, FALSE, FALSE, FALSE );
#endif

  /* Any player wiped out? Game over then... */

  if ( piece_count[BLACKSQ][disks_played] == 0 ) {
    if ( side_to_move == BLACKSQ )
      return -(MIDGAME_WIN + 64);
    else
      return +(MIDGAME_WIN + 64);
  }
  else if ( piece_count[WHITESQ][disks_played] == 0 ) {
    if ( side_to_move == BLACKSQ )
      return +(MIDGAME_WIN + 64);
    else
      return -(MIDGAME_WIN + 64);
  }

  /* Load and/or initialize the pattern coefficients */

  eval_phase = eval_map[disks_played];
  if ( !set[eval_phase].loaded )
    load_set( eval_phase );

  /* The constant feature and the parity feature */

  score = set[eval_phase].parity_constant[disks_played & 1];

#ifdef LOG_EVAL
  fprintf( stream, "parity=%d\n", score );
  fprintf( stream, "disks_played=%d (%d)\n", disks_played,
	disks_played & 1 );
#endif

  /* The pattern features. */

  if ( side_to_move == BLACKSQ ) {
#ifdef USE_PENTIUM_ASM
    int pattern0;
    int pattern1;
    int pattern2;
    int pattern3;
    asm( 
         "movl _board+288,%0\n\t"
         "movl _board+308,%1\n\t"
         "movl _board+108,%2\n\t"
         "movl _board+308,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+88,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+108,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+88,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+288,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+324,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+352,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+72,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+352,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+284,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+312,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+68,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+348,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+244,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+272,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+64,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+344,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+204,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+232,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+60,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+340,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+164,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+192,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+56,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+336,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+124,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+152,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+52,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+332,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+84,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+112,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+48,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+328,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+44,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+72,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+44,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+324,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].afile2x[pattern0];
    score += set[eval_phase].afile2x[pattern1];
    score += set[eval_phase].afile2x[pattern2];
    score += set[eval_phase].afile2x[pattern3];
    asm( 
         "movl _board+328,%0\n\t"
         "movl _board+348,%1\n\t"
         "movl _board+112,%2\n\t"
         "movl _board+312,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+288,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+308,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+108,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+308,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+248,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+268,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+104,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+304,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+208,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+228,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+100,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+300,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+168,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+188,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+96,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+296,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+128,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+148,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+92,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+292,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+88,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+108,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+88,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+288,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+48,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+68,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+84,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+284,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].bfile[pattern0];
    score += set[eval_phase].bfile[pattern1];
    score += set[eval_phase].bfile[pattern2];
    score += set[eval_phase].bfile[pattern3];
    asm( 
         "movl _board+332,%0\n\t"
         "movl _board+344,%1\n\t"
         "movl _board+152,%2\n\t"
         "movl _board+272,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+292,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+304,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+148,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+268,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+252,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+264,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+144,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+264,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+212,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+224,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+140,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+260,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+172,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+184,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+136,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+256,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+132,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+144,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+132,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+252,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+92,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+104,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+128,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+248,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+52,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+64,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+124,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+244,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].cfile[pattern0];
    score += set[eval_phase].cfile[pattern1];
    score += set[eval_phase].cfile[pattern2];
    score += set[eval_phase].cfile[pattern3];
    asm( 
         "movl _board+336,%0\n\t"
         "movl _board+340,%1\n\t"
         "movl _board+192,%2\n\t"
         "movl _board+232,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+296,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+300,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+188,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+228,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+256,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+260,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+184,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+224,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+216,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+220,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+180,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+220,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+176,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+180,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+176,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+216,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+136,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+140,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+172,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+212,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+96,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+100,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+168,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+208,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+56,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+60,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+164,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+204,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].dfile[pattern0];
    score += set[eval_phase].dfile[pattern1];
    score += set[eval_phase].dfile[pattern2];
    score += set[eval_phase].dfile[pattern3];
    asm( 
         "movl _board+352,%0\n\t"
         "movl _board+324,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+308,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+288,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+264,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+252,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+220,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+216,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+176,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+180,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+132,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+144,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+88,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+108,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+44,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+72,%1"
         : "=r" (pattern0), "=r" (pattern1) : );
    score += set[eval_phase].diag8[pattern0];
    score += set[eval_phase].diag8[pattern1];
    asm( 
         "movl _board+312,%0\n\t"
         "movl _board+348,%1\n\t"
         "movl _board+284,%2\n\t"
         "movl _board+328,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+268,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+304,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+248,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+292,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+224,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+260,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+212,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+256,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+180,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+216,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+176,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+220,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+136,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+172,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+140,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+184,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+92,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+128,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+104,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+148,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+48,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+84,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+68,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+112,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].diag7[pattern0];
    score += set[eval_phase].diag7[pattern1];
    score += set[eval_phase].diag7[pattern2];
    score += set[eval_phase].diag7[pattern3];
    asm( 
         "movl _board+272,%0\n\t"
         "movl _board+344,%1\n\t"
         "movl _board+244,%2\n\t"
         "movl _board+332,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+228,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+300,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+208,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+296,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+184,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+256,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+172,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+260,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+140,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+212,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+136,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+224,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+96,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+168,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+100,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+188,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+52,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+124,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+64,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+152,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].diag6[pattern0];
    score += set[eval_phase].diag6[pattern1];
    score += set[eval_phase].diag6[pattern2];
    score += set[eval_phase].diag6[pattern3];
    asm( 
         "movl _board+232,%0\n\t"
         "movl _board+340,%1\n\t"
         "movl _board+204,%2\n\t"
         "movl _board+336,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+188,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+296,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+168,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+300,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+144,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+252,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+132,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+264,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+100,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+208,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+96,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+228,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+56,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+164,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+60,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+192,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].diag5[pattern0];
    score += set[eval_phase].diag5[pattern1];
    score += set[eval_phase].diag5[pattern2];
    score += set[eval_phase].diag5[pattern3];
    asm( 
         "movl _board+192,%0\n\t"
         "movl _board+336,%1\n\t"
         "movl _board+164,%2\n\t"
         "movl _board+340,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+148,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+292,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+128,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+304,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+104,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+248,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+92,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+268,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+60,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+204,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+56,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+232,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].diag4[pattern0];
    score += set[eval_phase].diag4[pattern1];
    score += set[eval_phase].diag4[pattern2];
    score += set[eval_phase].diag4[pattern3];
    asm( 
         "movl _board+132,%0\n\t"
         "movl _board+252,%1\n\t"
         "movl _board+144,%2\n\t"
         "movl _board+264,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+128,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+248,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+148,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+268,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+124,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+244,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+152,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+272,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+92,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+292,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+104,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+304,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+88,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+288,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+108,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+308,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+84,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+284,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+112,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+312,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+52,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+332,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+64,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+344,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+48,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+328,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+68,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+348,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+44,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+324,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+72,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+352,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].corner33[pattern0];
    score += set[eval_phase].corner33[pattern1];
    score += set[eval_phase].corner33[pattern2];
    score += set[eval_phase].corner33[pattern3];
    asm( 
         "movl _board+100,%0\n\t"
         "movl _board+300,%1\n\t"
         "movl _board+96,%2\n\t"
         "movl _board+296,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+96,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+296,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+100,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+300,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+92,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+292,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+104,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+304,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+88,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+288,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+108,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+308,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+84,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+284,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+112,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+312,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+60,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+340,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+56,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+336,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+56,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+336,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+60,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+340,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+52,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+332,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+64,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+344,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+48,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+328,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+68,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+348,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+44,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+324,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+72,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+352,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].corner52[pattern0];
    score += set[eval_phase].corner52[pattern1];
    score += set[eval_phase].corner52[pattern2];
    score += set[eval_phase].corner52[pattern3];
    asm( 
         "movl _board+208,%0\n\t"
         "movl _board+228,%1\n\t"
         "movl _board+168,%2\n\t"
         "movl _board+188,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+168,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+188,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+208,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+228,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+128,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+148,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+248,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+268,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+88,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+108,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+288,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+308,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+48,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+68,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+328,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+348,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+204,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+232,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+164,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+192,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+164,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+192,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+204,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+232,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+124,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+152,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+244,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+272,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+84,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+112,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+284,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+312,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+44,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+72,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+324,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+352,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].corner52[pattern0];
    score += set[eval_phase].corner52[pattern1];
    score += set[eval_phase].corner52[pattern2];
    score += set[eval_phase].corner52[pattern3];
#else
  int pattern0;
  pattern0 = board[72];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[81];
  pattern0 = 3 * pattern0 + board[71];
  pattern0 = 3 * pattern0 + board[61];
  pattern0 = 3 * pattern0 + board[51];
  pattern0 = 3 * pattern0 + board[41];
  pattern0 = 3 * pattern0 + board[31];
  pattern0 = 3 * pattern0 + board[21];
  pattern0 = 3 * pattern0 + board[11];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].afile2x[pattern0] );
#endif
  score += set[eval_phase].afile2x[pattern0];
  pattern0 = board[77];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[88];
  pattern0 = 3 * pattern0 + board[78];
  pattern0 = 3 * pattern0 + board[68];
  pattern0 = 3 * pattern0 + board[58];
  pattern0 = 3 * pattern0 + board[48];
  pattern0 = 3 * pattern0 + board[38];
  pattern0 = 3 * pattern0 + board[28];
  pattern0 = 3 * pattern0 + board[18];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].afile2x[pattern0] );
#endif
  score += set[eval_phase].afile2x[pattern0];
  pattern0 = board[27];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[18];
  pattern0 = 3 * pattern0 + board[17];
  pattern0 = 3 * pattern0 + board[16];
  pattern0 = 3 * pattern0 + board[15];
  pattern0 = 3 * pattern0 + board[14];
  pattern0 = 3 * pattern0 + board[13];
  pattern0 = 3 * pattern0 + board[12];
  pattern0 = 3 * pattern0 + board[11];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].afile2x[pattern0] );
#endif
  score += set[eval_phase].afile2x[pattern0];
  pattern0 = board[77];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[88];
  pattern0 = 3 * pattern0 + board[87];
  pattern0 = 3 * pattern0 + board[86];
  pattern0 = 3 * pattern0 + board[85];
  pattern0 = 3 * pattern0 + board[84];
  pattern0 = 3 * pattern0 + board[83];
  pattern0 = 3 * pattern0 + board[82];
  pattern0 = 3 * pattern0 + board[81];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].afile2x[pattern0] );
#endif
  score += set[eval_phase].afile2x[pattern0];
  pattern0 = board[82];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[62];
  pattern0 = 3 * pattern0 + board[52];
  pattern0 = 3 * pattern0 + board[42];
  pattern0 = 3 * pattern0 + board[32];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[12];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].bfile[pattern0] );
#endif
  score += set[eval_phase].bfile[pattern0];
  pattern0 = board[87];
  pattern0 = 3 * pattern0 + board[77];
  pattern0 = 3 * pattern0 + board[67];
  pattern0 = 3 * pattern0 + board[57];
  pattern0 = 3 * pattern0 + board[47];
  pattern0 = 3 * pattern0 + board[37];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[17];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].bfile[pattern0] );
#endif
  score += set[eval_phase].bfile[pattern0];
  pattern0 = board[28];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[26];
  pattern0 = 3 * pattern0 + board[25];
  pattern0 = 3 * pattern0 + board[24];
  pattern0 = 3 * pattern0 + board[23];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[21];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].bfile[pattern0] );
#endif
  score += set[eval_phase].bfile[pattern0];
  pattern0 = board[78];
  pattern0 = 3 * pattern0 + board[77];
  pattern0 = 3 * pattern0 + board[76];
  pattern0 = 3 * pattern0 + board[75];
  pattern0 = 3 * pattern0 + board[74];
  pattern0 = 3 * pattern0 + board[73];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[71];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].bfile[pattern0] );
#endif
  score += set[eval_phase].bfile[pattern0];
  pattern0 = board[83];
  pattern0 = 3 * pattern0 + board[73];
  pattern0 = 3 * pattern0 + board[63];
  pattern0 = 3 * pattern0 + board[53];
  pattern0 = 3 * pattern0 + board[43];
  pattern0 = 3 * pattern0 + board[33];
  pattern0 = 3 * pattern0 + board[23];
  pattern0 = 3 * pattern0 + board[13];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].cfile[pattern0] );
#endif
  score += set[eval_phase].cfile[pattern0];
  pattern0 = board[86];
  pattern0 = 3 * pattern0 + board[76];
  pattern0 = 3 * pattern0 + board[66];
  pattern0 = 3 * pattern0 + board[56];
  pattern0 = 3 * pattern0 + board[46];
  pattern0 = 3 * pattern0 + board[36];
  pattern0 = 3 * pattern0 + board[26];
  pattern0 = 3 * pattern0 + board[16];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].cfile[pattern0] );
#endif
  score += set[eval_phase].cfile[pattern0];
  pattern0 = board[38];
  pattern0 = 3 * pattern0 + board[37];
  pattern0 = 3 * pattern0 + board[36];
  pattern0 = 3 * pattern0 + board[35];
  pattern0 = 3 * pattern0 + board[34];
  pattern0 = 3 * pattern0 + board[33];
  pattern0 = 3 * pattern0 + board[32];
  pattern0 = 3 * pattern0 + board[31];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].cfile[pattern0] );
#endif
  score += set[eval_phase].cfile[pattern0];
  pattern0 = board[68];
  pattern0 = 3 * pattern0 + board[67];
  pattern0 = 3 * pattern0 + board[66];
  pattern0 = 3 * pattern0 + board[65];
  pattern0 = 3 * pattern0 + board[64];
  pattern0 = 3 * pattern0 + board[63];
  pattern0 = 3 * pattern0 + board[62];
  pattern0 = 3 * pattern0 + board[61];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].cfile[pattern0] );
#endif
  score += set[eval_phase].cfile[pattern0];
  pattern0 = board[84];
  pattern0 = 3 * pattern0 + board[74];
  pattern0 = 3 * pattern0 + board[64];
  pattern0 = 3 * pattern0 + board[54];
  pattern0 = 3 * pattern0 + board[44];
  pattern0 = 3 * pattern0 + board[34];
  pattern0 = 3 * pattern0 + board[24];
  pattern0 = 3 * pattern0 + board[14];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].dfile[pattern0] );
#endif
  score += set[eval_phase].dfile[pattern0];
  pattern0 = board[85];
  pattern0 = 3 * pattern0 + board[75];
  pattern0 = 3 * pattern0 + board[65];
  pattern0 = 3 * pattern0 + board[55];
  pattern0 = 3 * pattern0 + board[45];
  pattern0 = 3 * pattern0 + board[35];
  pattern0 = 3 * pattern0 + board[25];
  pattern0 = 3 * pattern0 + board[15];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].dfile[pattern0] );
#endif
  score += set[eval_phase].dfile[pattern0];
  pattern0 = board[48];
  pattern0 = 3 * pattern0 + board[47];
  pattern0 = 3 * pattern0 + board[46];
  pattern0 = 3 * pattern0 + board[45];
  pattern0 = 3 * pattern0 + board[44];
  pattern0 = 3 * pattern0 + board[43];
  pattern0 = 3 * pattern0 + board[42];
  pattern0 = 3 * pattern0 + board[41];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].dfile[pattern0] );
#endif
  score += set[eval_phase].dfile[pattern0];
  pattern0 = board[58];
  pattern0 = 3 * pattern0 + board[57];
  pattern0 = 3 * pattern0 + board[56];
  pattern0 = 3 * pattern0 + board[55];
  pattern0 = 3 * pattern0 + board[54];
  pattern0 = 3 * pattern0 + board[53];
  pattern0 = 3 * pattern0 + board[52];
  pattern0 = 3 * pattern0 + board[51];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].dfile[pattern0] );
#endif
  score += set[eval_phase].dfile[pattern0];
  pattern0 = board[88];
  pattern0 = 3 * pattern0 + board[77];
  pattern0 = 3 * pattern0 + board[66];
  pattern0 = 3 * pattern0 + board[55];
  pattern0 = 3 * pattern0 + board[44];
  pattern0 = 3 * pattern0 + board[33];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[11];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag8[pattern0] );
#endif
  score += set[eval_phase].diag8[pattern0];
  pattern0 = board[81];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[63];
  pattern0 = 3 * pattern0 + board[54];
  pattern0 = 3 * pattern0 + board[45];
  pattern0 = 3 * pattern0 + board[36];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[18];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag8[pattern0] );
#endif
  score += set[eval_phase].diag8[pattern0];
  pattern0 = board[78];
  pattern0 = 3 * pattern0 + board[67];
  pattern0 = 3 * pattern0 + board[56];
  pattern0 = 3 * pattern0 + board[45];
  pattern0 = 3 * pattern0 + board[34];
  pattern0 = 3 * pattern0 + board[23];
  pattern0 = 3 * pattern0 + board[12];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag7[pattern0] );
#endif
  score += set[eval_phase].diag7[pattern0];
  pattern0 = board[87];
  pattern0 = 3 * pattern0 + board[76];
  pattern0 = 3 * pattern0 + board[65];
  pattern0 = 3 * pattern0 + board[54];
  pattern0 = 3 * pattern0 + board[43];
  pattern0 = 3 * pattern0 + board[32];
  pattern0 = 3 * pattern0 + board[21];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag7[pattern0] );
#endif
  score += set[eval_phase].diag7[pattern0];
  pattern0 = board[71];
  pattern0 = 3 * pattern0 + board[62];
  pattern0 = 3 * pattern0 + board[53];
  pattern0 = 3 * pattern0 + board[44];
  pattern0 = 3 * pattern0 + board[35];
  pattern0 = 3 * pattern0 + board[26];
  pattern0 = 3 * pattern0 + board[17];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag7[pattern0] );
#endif
  score += set[eval_phase].diag7[pattern0];
  pattern0 = board[82];
  pattern0 = 3 * pattern0 + board[73];
  pattern0 = 3 * pattern0 + board[64];
  pattern0 = 3 * pattern0 + board[55];
  pattern0 = 3 * pattern0 + board[46];
  pattern0 = 3 * pattern0 + board[37];
  pattern0 = 3 * pattern0 + board[28];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag7[pattern0] );
#endif
  score += set[eval_phase].diag7[pattern0];
  pattern0 = board[68];
  pattern0 = 3 * pattern0 + board[57];
  pattern0 = 3 * pattern0 + board[46];
  pattern0 = 3 * pattern0 + board[35];
  pattern0 = 3 * pattern0 + board[24];
  pattern0 = 3 * pattern0 + board[13];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag6[pattern0] );
#endif
  score += set[eval_phase].diag6[pattern0];
  pattern0 = board[86];
  pattern0 = 3 * pattern0 + board[75];
  pattern0 = 3 * pattern0 + board[64];
  pattern0 = 3 * pattern0 + board[53];
  pattern0 = 3 * pattern0 + board[42];
  pattern0 = 3 * pattern0 + board[31];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag6[pattern0] );
#endif
  score += set[eval_phase].diag6[pattern0];
  pattern0 = board[61];
  pattern0 = 3 * pattern0 + board[52];
  pattern0 = 3 * pattern0 + board[43];
  pattern0 = 3 * pattern0 + board[34];
  pattern0 = 3 * pattern0 + board[25];
  pattern0 = 3 * pattern0 + board[16];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag6[pattern0] );
#endif
  score += set[eval_phase].diag6[pattern0];
  pattern0 = board[83];
  pattern0 = 3 * pattern0 + board[74];
  pattern0 = 3 * pattern0 + board[65];
  pattern0 = 3 * pattern0 + board[56];
  pattern0 = 3 * pattern0 + board[47];
  pattern0 = 3 * pattern0 + board[38];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag6[pattern0] );
#endif
  score += set[eval_phase].diag6[pattern0];
  pattern0 = board[58];
  pattern0 = 3 * pattern0 + board[47];
  pattern0 = 3 * pattern0 + board[36];
  pattern0 = 3 * pattern0 + board[25];
  pattern0 = 3 * pattern0 + board[14];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag5[pattern0] );
#endif
  score += set[eval_phase].diag5[pattern0];
  pattern0 = board[85];
  pattern0 = 3 * pattern0 + board[74];
  pattern0 = 3 * pattern0 + board[63];
  pattern0 = 3 * pattern0 + board[52];
  pattern0 = 3 * pattern0 + board[41];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag5[pattern0] );
#endif
  score += set[eval_phase].diag5[pattern0];
  pattern0 = board[51];
  pattern0 = 3 * pattern0 + board[42];
  pattern0 = 3 * pattern0 + board[33];
  pattern0 = 3 * pattern0 + board[24];
  pattern0 = 3 * pattern0 + board[15];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag5[pattern0] );
#endif
  score += set[eval_phase].diag5[pattern0];
  pattern0 = board[84];
  pattern0 = 3 * pattern0 + board[75];
  pattern0 = 3 * pattern0 + board[66];
  pattern0 = 3 * pattern0 + board[57];
  pattern0 = 3 * pattern0 + board[48];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag5[pattern0] );
#endif
  score += set[eval_phase].diag5[pattern0];
  pattern0 = board[48];
  pattern0 = 3 * pattern0 + board[37];
  pattern0 = 3 * pattern0 + board[26];
  pattern0 = 3 * pattern0 + board[15];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag4[pattern0] );
#endif
  score += set[eval_phase].diag4[pattern0];
  pattern0 = board[84];
  pattern0 = 3 * pattern0 + board[73];
  pattern0 = 3 * pattern0 + board[62];
  pattern0 = 3 * pattern0 + board[51];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag4[pattern0] );
#endif
  score += set[eval_phase].diag4[pattern0];
  pattern0 = board[41];
  pattern0 = 3 * pattern0 + board[32];
  pattern0 = 3 * pattern0 + board[23];
  pattern0 = 3 * pattern0 + board[14];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag4[pattern0] );
#endif
  score += set[eval_phase].diag4[pattern0];
  pattern0 = board[85];
  pattern0 = 3 * pattern0 + board[76];
  pattern0 = 3 * pattern0 + board[67];
  pattern0 = 3 * pattern0 + board[58];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag4[pattern0] );
#endif
  score += set[eval_phase].diag4[pattern0];
  pattern0 = board[33];
  pattern0 = 3 * pattern0 + board[32];
  pattern0 = 3 * pattern0 + board[31];
  pattern0 = 3 * pattern0 + board[23];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[21];
  pattern0 = 3 * pattern0 + board[13];
  pattern0 = 3 * pattern0 + board[12];
  pattern0 = 3 * pattern0 + board[11];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner33[pattern0] );
#endif
  score += set[eval_phase].corner33[pattern0];
  pattern0 = board[63];
  pattern0 = 3 * pattern0 + board[62];
  pattern0 = 3 * pattern0 + board[61];
  pattern0 = 3 * pattern0 + board[73];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[71];
  pattern0 = 3 * pattern0 + board[83];
  pattern0 = 3 * pattern0 + board[82];
  pattern0 = 3 * pattern0 + board[81];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner33[pattern0] );
#endif
  score += set[eval_phase].corner33[pattern0];
  pattern0 = board[36];
  pattern0 = 3 * pattern0 + board[37];
  pattern0 = 3 * pattern0 + board[38];
  pattern0 = 3 * pattern0 + board[26];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[28];
  pattern0 = 3 * pattern0 + board[16];
  pattern0 = 3 * pattern0 + board[17];
  pattern0 = 3 * pattern0 + board[18];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner33[pattern0] );
#endif
  score += set[eval_phase].corner33[pattern0];
  pattern0 = board[66];
  pattern0 = 3 * pattern0 + board[67];
  pattern0 = 3 * pattern0 + board[68];
  pattern0 = 3 * pattern0 + board[76];
  pattern0 = 3 * pattern0 + board[77];
  pattern0 = 3 * pattern0 + board[78];
  pattern0 = 3 * pattern0 + board[86];
  pattern0 = 3 * pattern0 + board[87];
  pattern0 = 3 * pattern0 + board[88];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner33[pattern0] );
#endif
  score += set[eval_phase].corner33[pattern0];
  pattern0 = board[25];
  pattern0 = 3 * pattern0 + board[24];
  pattern0 = 3 * pattern0 + board[23];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[21];
  pattern0 = 3 * pattern0 + board[15];
  pattern0 = 3 * pattern0 + board[14];
  pattern0 = 3 * pattern0 + board[13];
  pattern0 = 3 * pattern0 + board[12];
  pattern0 = 3 * pattern0 + board[11];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52[pattern0] );
#endif
  score += set[eval_phase].corner52[pattern0];
  pattern0 = board[75];
  pattern0 = 3 * pattern0 + board[74];
  pattern0 = 3 * pattern0 + board[73];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[71];
  pattern0 = 3 * pattern0 + board[85];
  pattern0 = 3 * pattern0 + board[84];
  pattern0 = 3 * pattern0 + board[83];
  pattern0 = 3 * pattern0 + board[82];
  pattern0 = 3 * pattern0 + board[81];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52[pattern0] );
#endif
  score += set[eval_phase].corner52[pattern0];
  pattern0 = board[24];
  pattern0 = 3 * pattern0 + board[25];
  pattern0 = 3 * pattern0 + board[26];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[28];
  pattern0 = 3 * pattern0 + board[14];
  pattern0 = 3 * pattern0 + board[15];
  pattern0 = 3 * pattern0 + board[16];
  pattern0 = 3 * pattern0 + board[17];
  pattern0 = 3 * pattern0 + board[18];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52[pattern0] );
#endif
  score += set[eval_phase].corner52[pattern0];
  pattern0 = board[74];
  pattern0 = 3 * pattern0 + board[75];
  pattern0 = 3 * pattern0 + board[76];
  pattern0 = 3 * pattern0 + board[77];
  pattern0 = 3 * pattern0 + board[78];
  pattern0 = 3 * pattern0 + board[84];
  pattern0 = 3 * pattern0 + board[85];
  pattern0 = 3 * pattern0 + board[86];
  pattern0 = 3 * pattern0 + board[87];
  pattern0 = 3 * pattern0 + board[88];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52[pattern0] );
#endif
  score += set[eval_phase].corner52[pattern0];
  pattern0 = board[52];
  pattern0 = 3 * pattern0 + board[42];
  pattern0 = 3 * pattern0 + board[32];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[12];
  pattern0 = 3 * pattern0 + board[51];
  pattern0 = 3 * pattern0 + board[41];
  pattern0 = 3 * pattern0 + board[31];
  pattern0 = 3 * pattern0 + board[21];
  pattern0 = 3 * pattern0 + board[11];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52[pattern0] );
#endif
  score += set[eval_phase].corner52[pattern0];
  pattern0 = board[57];
  pattern0 = 3 * pattern0 + board[47];
  pattern0 = 3 * pattern0 + board[37];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[17];
  pattern0 = 3 * pattern0 + board[58];
  pattern0 = 3 * pattern0 + board[48];
  pattern0 = 3 * pattern0 + board[38];
  pattern0 = 3 * pattern0 + board[28];
  pattern0 = 3 * pattern0 + board[18];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52[pattern0] );
#endif
  score += set[eval_phase].corner52[pattern0];
  pattern0 = board[42];
  pattern0 = 3 * pattern0 + board[52];
  pattern0 = 3 * pattern0 + board[62];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[82];
  pattern0 = 3 * pattern0 + board[41];
  pattern0 = 3 * pattern0 + board[51];
  pattern0 = 3 * pattern0 + board[61];
  pattern0 = 3 * pattern0 + board[71];
  pattern0 = 3 * pattern0 + board[81];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52[pattern0] );
#endif
  score += set[eval_phase].corner52[pattern0];
  pattern0 = board[47];
  pattern0 = 3 * pattern0 + board[57];
  pattern0 = 3 * pattern0 + board[67];
  pattern0 = 3 * pattern0 + board[77];
  pattern0 = 3 * pattern0 + board[87];
  pattern0 = 3 * pattern0 + board[48];
  pattern0 = 3 * pattern0 + board[58];
  pattern0 = 3 * pattern0 + board[68];
  pattern0 = 3 * pattern0 + board[78];
  pattern0 = 3 * pattern0 + board[88];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52[pattern0] );
#endif
  score += set[eval_phase].corner52[pattern0];
#endif
  }
  else {
#ifdef USE_PENTIUM_ASM
    int pattern0;
    int pattern1;
    int pattern2;
    int pattern3;
    asm( 
         "movl _board+288,%0\n\t"
         "movl _board+308,%1\n\t"
         "movl _board+108,%2\n\t"
         "movl _board+308,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+88,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+108,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+88,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+288,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+324,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+352,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+72,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+352,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+284,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+312,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+68,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+348,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+244,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+272,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+64,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+344,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+204,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+232,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+60,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+340,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+164,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+192,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+56,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+336,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+124,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+152,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+52,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+332,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+84,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+112,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+48,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+328,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+44,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+72,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+44,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+324,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].afile2x_last[-pattern0];
    score += set[eval_phase].afile2x_last[-pattern1];
    score += set[eval_phase].afile2x_last[-pattern2];
    score += set[eval_phase].afile2x_last[-pattern3];
    asm( 
         "movl _board+328,%0\n\t"
         "movl _board+348,%1\n\t"
         "movl _board+112,%2\n\t"
         "movl _board+312,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+288,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+308,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+108,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+308,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+248,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+268,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+104,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+304,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+208,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+228,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+100,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+300,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+168,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+188,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+96,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+296,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+128,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+148,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+92,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+292,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+88,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+108,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+88,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+288,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+48,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+68,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+84,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+284,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].bfile_last[-pattern0];
    score += set[eval_phase].bfile_last[-pattern1];
    score += set[eval_phase].bfile_last[-pattern2];
    score += set[eval_phase].bfile_last[-pattern3];
    asm( 
         "movl _board+332,%0\n\t"
         "movl _board+344,%1\n\t"
         "movl _board+152,%2\n\t"
         "movl _board+272,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+292,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+304,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+148,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+268,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+252,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+264,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+144,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+264,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+212,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+224,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+140,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+260,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+172,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+184,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+136,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+256,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+132,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+144,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+132,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+252,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+92,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+104,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+128,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+248,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+52,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+64,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+124,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+244,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].cfile_last[-pattern0];
    score += set[eval_phase].cfile_last[-pattern1];
    score += set[eval_phase].cfile_last[-pattern2];
    score += set[eval_phase].cfile_last[-pattern3];
    asm( 
         "movl _board+336,%0\n\t"
         "movl _board+340,%1\n\t"
         "movl _board+192,%2\n\t"
         "movl _board+232,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+296,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+300,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+188,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+228,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+256,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+260,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+184,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+224,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+216,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+220,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+180,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+220,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+176,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+180,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+176,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+216,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+136,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+140,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+172,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+212,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+96,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+100,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+168,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+208,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+56,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+60,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+164,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+204,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].dfile_last[-pattern0];
    score += set[eval_phase].dfile_last[-pattern1];
    score += set[eval_phase].dfile_last[-pattern2];
    score += set[eval_phase].dfile_last[-pattern3];
    asm( 
         "movl _board+352,%0\n\t"
         "movl _board+324,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+308,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+288,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+264,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+252,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+220,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+216,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+176,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+180,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+132,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+144,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+88,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+108,%1\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+44,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+72,%1"
         : "=r" (pattern0), "=r" (pattern1) : );
    score += set[eval_phase].diag8_last[-pattern0];
    score += set[eval_phase].diag8_last[-pattern1];
    asm( 
         "movl _board+312,%0\n\t"
         "movl _board+348,%1\n\t"
         "movl _board+284,%2\n\t"
         "movl _board+328,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+268,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+304,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+248,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+292,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+224,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+260,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+212,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+256,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+180,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+216,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+176,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+220,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+136,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+172,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+140,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+184,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+92,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+128,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+104,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+148,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+48,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+84,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+68,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+112,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].diag7_last[-pattern0];
    score += set[eval_phase].diag7_last[-pattern1];
    score += set[eval_phase].diag7_last[-pattern2];
    score += set[eval_phase].diag7_last[-pattern3];
    asm( 
         "movl _board+272,%0\n\t"
         "movl _board+344,%1\n\t"
         "movl _board+244,%2\n\t"
         "movl _board+332,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+228,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+300,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+208,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+296,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+184,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+256,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+172,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+260,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+140,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+212,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+136,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+224,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+96,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+168,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+100,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+188,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+52,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+124,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+64,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+152,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].diag6_last[-pattern0];
    score += set[eval_phase].diag6_last[-pattern1];
    score += set[eval_phase].diag6_last[-pattern2];
    score += set[eval_phase].diag6_last[-pattern3];
    asm( 
         "movl _board+232,%0\n\t"
         "movl _board+340,%1\n\t"
         "movl _board+204,%2\n\t"
         "movl _board+336,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+188,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+296,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+168,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+300,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+144,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+252,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+132,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+264,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+100,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+208,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+96,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+228,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+56,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+164,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+60,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+192,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].diag5_last[-pattern0];
    score += set[eval_phase].diag5_last[-pattern1];
    score += set[eval_phase].diag5_last[-pattern2];
    score += set[eval_phase].diag5_last[-pattern3];
    asm( 
         "movl _board+192,%0\n\t"
         "movl _board+336,%1\n\t"
         "movl _board+164,%2\n\t"
         "movl _board+340,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+148,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+292,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+128,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+304,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+104,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+248,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+92,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+268,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+60,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+204,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+56,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+232,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].diag4_last[-pattern0];
    score += set[eval_phase].diag4_last[-pattern1];
    score += set[eval_phase].diag4_last[-pattern2];
    score += set[eval_phase].diag4_last[-pattern3];
    asm( 
         "movl _board+132,%0\n\t"
         "movl _board+252,%1\n\t"
         "movl _board+144,%2\n\t"
         "movl _board+264,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+128,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+248,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+148,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+268,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+124,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+244,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+152,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+272,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+92,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+292,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+104,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+304,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+88,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+288,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+108,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+308,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+84,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+284,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+112,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+312,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+52,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+332,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+64,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+344,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+48,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+328,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+68,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+348,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+44,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+324,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+72,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+352,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].corner33_last[-pattern0];
    score += set[eval_phase].corner33_last[-pattern1];
    score += set[eval_phase].corner33_last[-pattern2];
    score += set[eval_phase].corner33_last[-pattern3];
    asm( 
         "movl _board+100,%0\n\t"
         "movl _board+300,%1\n\t"
         "movl _board+96,%2\n\t"
         "movl _board+296,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+96,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+296,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+100,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+300,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+92,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+292,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+104,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+304,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+88,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+288,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+108,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+308,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+84,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+284,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+112,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+312,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+60,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+340,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+56,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+336,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+56,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+336,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+60,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+340,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+52,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+332,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+64,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+344,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+48,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+328,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+68,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+348,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+44,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+324,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+72,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+352,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].corner52_last[-pattern0];
    score += set[eval_phase].corner52_last[-pattern1];
    score += set[eval_phase].corner52_last[-pattern2];
    score += set[eval_phase].corner52_last[-pattern3];
    asm( 
         "movl _board+208,%0\n\t"
         "movl _board+228,%1\n\t"
         "movl _board+168,%2\n\t"
         "movl _board+188,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+168,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+188,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+208,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+228,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+128,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+148,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+248,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+268,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+88,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+108,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+288,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+308,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+48,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+68,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+328,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+348,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+204,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+232,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+164,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+192,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+164,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+192,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+204,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+232,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+124,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+152,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+244,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+272,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+84,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+112,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+284,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+312,%3\n\t"
         "leal (%0,%0,2),%0\n\t"
         "addl _board+44,%0\n\t"
         "leal (%1,%1,2),%1\n\t"
         "addl _board+72,%1\n\t"
         "leal (%2,%2,2),%2\n\t"
         "addl _board+324,%2\n\t"
         "leal (%3,%3,2),%3\n\t"
         "addl _board+352,%3"
         : "=r" (pattern0), "=r" (pattern1), "=r" (pattern2), "=r" (pattern3) : );
    score += set[eval_phase].corner52_last[-pattern0];
    score += set[eval_phase].corner52_last[-pattern1];
    score += set[eval_phase].corner52_last[-pattern2];
    score += set[eval_phase].corner52_last[-pattern3];
#else
  int pattern0;
  pattern0 = board[72];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[81];
  pattern0 = 3 * pattern0 + board[71];
  pattern0 = 3 * pattern0 + board[61];
  pattern0 = 3 * pattern0 + board[51];
  pattern0 = 3 * pattern0 + board[41];
  pattern0 = 3 * pattern0 + board[31];
  pattern0 = 3 * pattern0 + board[21];
  pattern0 = 3 * pattern0 + board[11];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].afile2x_last[-pattern0] );
#endif
  score += set[eval_phase].afile2x_last[-pattern0];
  pattern0 = board[77];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[88];
  pattern0 = 3 * pattern0 + board[78];
  pattern0 = 3 * pattern0 + board[68];
  pattern0 = 3 * pattern0 + board[58];
  pattern0 = 3 * pattern0 + board[48];
  pattern0 = 3 * pattern0 + board[38];
  pattern0 = 3 * pattern0 + board[28];
  pattern0 = 3 * pattern0 + board[18];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].afile2x_last[-pattern0] );
#endif
  score += set[eval_phase].afile2x_last[-pattern0];
  pattern0 = board[27];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[18];
  pattern0 = 3 * pattern0 + board[17];
  pattern0 = 3 * pattern0 + board[16];
  pattern0 = 3 * pattern0 + board[15];
  pattern0 = 3 * pattern0 + board[14];
  pattern0 = 3 * pattern0 + board[13];
  pattern0 = 3 * pattern0 + board[12];
  pattern0 = 3 * pattern0 + board[11];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].afile2x_last[-pattern0] );
#endif
  score += set[eval_phase].afile2x_last[-pattern0];
  pattern0 = board[77];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[88];
  pattern0 = 3 * pattern0 + board[87];
  pattern0 = 3 * pattern0 + board[86];
  pattern0 = 3 * pattern0 + board[85];
  pattern0 = 3 * pattern0 + board[84];
  pattern0 = 3 * pattern0 + board[83];
  pattern0 = 3 * pattern0 + board[82];
  pattern0 = 3 * pattern0 + board[81];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].afile2x_last[-pattern0] );
#endif
  score += set[eval_phase].afile2x_last[-pattern0];
  pattern0 = board[82];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[62];
  pattern0 = 3 * pattern0 + board[52];
  pattern0 = 3 * pattern0 + board[42];
  pattern0 = 3 * pattern0 + board[32];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[12];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].bfile_last[-pattern0] );
#endif
  score += set[eval_phase].bfile_last[-pattern0];
  pattern0 = board[87];
  pattern0 = 3 * pattern0 + board[77];
  pattern0 = 3 * pattern0 + board[67];
  pattern0 = 3 * pattern0 + board[57];
  pattern0 = 3 * pattern0 + board[47];
  pattern0 = 3 * pattern0 + board[37];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[17];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].bfile_last[-pattern0] );
#endif
  score += set[eval_phase].bfile_last[-pattern0];
  pattern0 = board[28];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[26];
  pattern0 = 3 * pattern0 + board[25];
  pattern0 = 3 * pattern0 + board[24];
  pattern0 = 3 * pattern0 + board[23];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[21];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].bfile_last[-pattern0] );
#endif
  score += set[eval_phase].bfile_last[-pattern0];
  pattern0 = board[78];
  pattern0 = 3 * pattern0 + board[77];
  pattern0 = 3 * pattern0 + board[76];
  pattern0 = 3 * pattern0 + board[75];
  pattern0 = 3 * pattern0 + board[74];
  pattern0 = 3 * pattern0 + board[73];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[71];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].bfile_last[-pattern0] );
#endif
  score += set[eval_phase].bfile_last[-pattern0];
  pattern0 = board[83];
  pattern0 = 3 * pattern0 + board[73];
  pattern0 = 3 * pattern0 + board[63];
  pattern0 = 3 * pattern0 + board[53];
  pattern0 = 3 * pattern0 + board[43];
  pattern0 = 3 * pattern0 + board[33];
  pattern0 = 3 * pattern0 + board[23];
  pattern0 = 3 * pattern0 + board[13];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].cfile_last[-pattern0] );
#endif
  score += set[eval_phase].cfile_last[-pattern0];
  pattern0 = board[86];
  pattern0 = 3 * pattern0 + board[76];
  pattern0 = 3 * pattern0 + board[66];
  pattern0 = 3 * pattern0 + board[56];
  pattern0 = 3 * pattern0 + board[46];
  pattern0 = 3 * pattern0 + board[36];
  pattern0 = 3 * pattern0 + board[26];
  pattern0 = 3 * pattern0 + board[16];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].cfile_last[-pattern0] );
#endif
  score += set[eval_phase].cfile_last[-pattern0];
  pattern0 = board[38];
  pattern0 = 3 * pattern0 + board[37];
  pattern0 = 3 * pattern0 + board[36];
  pattern0 = 3 * pattern0 + board[35];
  pattern0 = 3 * pattern0 + board[34];
  pattern0 = 3 * pattern0 + board[33];
  pattern0 = 3 * pattern0 + board[32];
  pattern0 = 3 * pattern0 + board[31];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].cfile_last[-pattern0] );
#endif
  score += set[eval_phase].cfile_last[-pattern0];
  pattern0 = board[68];
  pattern0 = 3 * pattern0 + board[67];
  pattern0 = 3 * pattern0 + board[66];
  pattern0 = 3 * pattern0 + board[65];
  pattern0 = 3 * pattern0 + board[64];
  pattern0 = 3 * pattern0 + board[63];
  pattern0 = 3 * pattern0 + board[62];
  pattern0 = 3 * pattern0 + board[61];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].cfile_last[-pattern0] );
#endif
  score += set[eval_phase].cfile_last[-pattern0];
  pattern0 = board[84];
  pattern0 = 3 * pattern0 + board[74];
  pattern0 = 3 * pattern0 + board[64];
  pattern0 = 3 * pattern0 + board[54];
  pattern0 = 3 * pattern0 + board[44];
  pattern0 = 3 * pattern0 + board[34];
  pattern0 = 3 * pattern0 + board[24];
  pattern0 = 3 * pattern0 + board[14];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].dfile_last[-pattern0] );
#endif
  score += set[eval_phase].dfile_last[-pattern0];
  pattern0 = board[85];
  pattern0 = 3 * pattern0 + board[75];
  pattern0 = 3 * pattern0 + board[65];
  pattern0 = 3 * pattern0 + board[55];
  pattern0 = 3 * pattern0 + board[45];
  pattern0 = 3 * pattern0 + board[35];
  pattern0 = 3 * pattern0 + board[25];
  pattern0 = 3 * pattern0 + board[15];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].dfile_last[-pattern0] );
#endif
  score += set[eval_phase].dfile_last[-pattern0];
  pattern0 = board[48];
  pattern0 = 3 * pattern0 + board[47];
  pattern0 = 3 * pattern0 + board[46];
  pattern0 = 3 * pattern0 + board[45];
  pattern0 = 3 * pattern0 + board[44];
  pattern0 = 3 * pattern0 + board[43];
  pattern0 = 3 * pattern0 + board[42];
  pattern0 = 3 * pattern0 + board[41];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].dfile_last[-pattern0] );
#endif
  score += set[eval_phase].dfile_last[-pattern0];
  pattern0 = board[58];
  pattern0 = 3 * pattern0 + board[57];
  pattern0 = 3 * pattern0 + board[56];
  pattern0 = 3 * pattern0 + board[55];
  pattern0 = 3 * pattern0 + board[54];
  pattern0 = 3 * pattern0 + board[53];
  pattern0 = 3 * pattern0 + board[52];
  pattern0 = 3 * pattern0 + board[51];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].dfile_last[-pattern0] );
#endif
  score += set[eval_phase].dfile_last[-pattern0];
  pattern0 = board[88];
  pattern0 = 3 * pattern0 + board[77];
  pattern0 = 3 * pattern0 + board[66];
  pattern0 = 3 * pattern0 + board[55];
  pattern0 = 3 * pattern0 + board[44];
  pattern0 = 3 * pattern0 + board[33];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[11];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag8_last[-pattern0] );
#endif
  score += set[eval_phase].diag8_last[-pattern0];
  pattern0 = board[81];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[63];
  pattern0 = 3 * pattern0 + board[54];
  pattern0 = 3 * pattern0 + board[45];
  pattern0 = 3 * pattern0 + board[36];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[18];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag8_last[-pattern0] );
#endif
  score += set[eval_phase].diag8_last[-pattern0];
  pattern0 = board[78];
  pattern0 = 3 * pattern0 + board[67];
  pattern0 = 3 * pattern0 + board[56];
  pattern0 = 3 * pattern0 + board[45];
  pattern0 = 3 * pattern0 + board[34];
  pattern0 = 3 * pattern0 + board[23];
  pattern0 = 3 * pattern0 + board[12];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag7_last[-pattern0] );
#endif
  score += set[eval_phase].diag7_last[-pattern0];
  pattern0 = board[87];
  pattern0 = 3 * pattern0 + board[76];
  pattern0 = 3 * pattern0 + board[65];
  pattern0 = 3 * pattern0 + board[54];
  pattern0 = 3 * pattern0 + board[43];
  pattern0 = 3 * pattern0 + board[32];
  pattern0 = 3 * pattern0 + board[21];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag7_last[-pattern0] );
#endif
  score += set[eval_phase].diag7_last[-pattern0];
  pattern0 = board[71];
  pattern0 = 3 * pattern0 + board[62];
  pattern0 = 3 * pattern0 + board[53];
  pattern0 = 3 * pattern0 + board[44];
  pattern0 = 3 * pattern0 + board[35];
  pattern0 = 3 * pattern0 + board[26];
  pattern0 = 3 * pattern0 + board[17];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag7_last[-pattern0] );
#endif
  score += set[eval_phase].diag7_last[-pattern0];
  pattern0 = board[82];
  pattern0 = 3 * pattern0 + board[73];
  pattern0 = 3 * pattern0 + board[64];
  pattern0 = 3 * pattern0 + board[55];
  pattern0 = 3 * pattern0 + board[46];
  pattern0 = 3 * pattern0 + board[37];
  pattern0 = 3 * pattern0 + board[28];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag7_last[-pattern0] );
#endif
  score += set[eval_phase].diag7_last[-pattern0];
  pattern0 = board[68];
  pattern0 = 3 * pattern0 + board[57];
  pattern0 = 3 * pattern0 + board[46];
  pattern0 = 3 * pattern0 + board[35];
  pattern0 = 3 * pattern0 + board[24];
  pattern0 = 3 * pattern0 + board[13];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag6_last[-pattern0] );
#endif
  score += set[eval_phase].diag6_last[-pattern0];
  pattern0 = board[86];
  pattern0 = 3 * pattern0 + board[75];
  pattern0 = 3 * pattern0 + board[64];
  pattern0 = 3 * pattern0 + board[53];
  pattern0 = 3 * pattern0 + board[42];
  pattern0 = 3 * pattern0 + board[31];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag6_last[-pattern0] );
#endif
  score += set[eval_phase].diag6_last[-pattern0];
  pattern0 = board[61];
  pattern0 = 3 * pattern0 + board[52];
  pattern0 = 3 * pattern0 + board[43];
  pattern0 = 3 * pattern0 + board[34];
  pattern0 = 3 * pattern0 + board[25];
  pattern0 = 3 * pattern0 + board[16];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag6_last[-pattern0] );
#endif
  score += set[eval_phase].diag6_last[-pattern0];
  pattern0 = board[83];
  pattern0 = 3 * pattern0 + board[74];
  pattern0 = 3 * pattern0 + board[65];
  pattern0 = 3 * pattern0 + board[56];
  pattern0 = 3 * pattern0 + board[47];
  pattern0 = 3 * pattern0 + board[38];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag6_last[-pattern0] );
#endif
  score += set[eval_phase].diag6_last[-pattern0];
  pattern0 = board[58];
  pattern0 = 3 * pattern0 + board[47];
  pattern0 = 3 * pattern0 + board[36];
  pattern0 = 3 * pattern0 + board[25];
  pattern0 = 3 * pattern0 + board[14];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag5_last[-pattern0] );
#endif
  score += set[eval_phase].diag5_last[-pattern0];
  pattern0 = board[85];
  pattern0 = 3 * pattern0 + board[74];
  pattern0 = 3 * pattern0 + board[63];
  pattern0 = 3 * pattern0 + board[52];
  pattern0 = 3 * pattern0 + board[41];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag5_last[-pattern0] );
#endif
  score += set[eval_phase].diag5_last[-pattern0];
  pattern0 = board[51];
  pattern0 = 3 * pattern0 + board[42];
  pattern0 = 3 * pattern0 + board[33];
  pattern0 = 3 * pattern0 + board[24];
  pattern0 = 3 * pattern0 + board[15];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag5_last[-pattern0] );
#endif
  score += set[eval_phase].diag5_last[-pattern0];
  pattern0 = board[84];
  pattern0 = 3 * pattern0 + board[75];
  pattern0 = 3 * pattern0 + board[66];
  pattern0 = 3 * pattern0 + board[57];
  pattern0 = 3 * pattern0 + board[48];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag5_last[-pattern0] );
#endif
  score += set[eval_phase].diag5_last[-pattern0];
  pattern0 = board[48];
  pattern0 = 3 * pattern0 + board[37];
  pattern0 = 3 * pattern0 + board[26];
  pattern0 = 3 * pattern0 + board[15];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag4_last[-pattern0] );
#endif
  score += set[eval_phase].diag4_last[-pattern0];
  pattern0 = board[84];
  pattern0 = 3 * pattern0 + board[73];
  pattern0 = 3 * pattern0 + board[62];
  pattern0 = 3 * pattern0 + board[51];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag4_last[-pattern0] );
#endif
  score += set[eval_phase].diag4_last[-pattern0];
  pattern0 = board[41];
  pattern0 = 3 * pattern0 + board[32];
  pattern0 = 3 * pattern0 + board[23];
  pattern0 = 3 * pattern0 + board[14];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag4_last[-pattern0] );
#endif
  score += set[eval_phase].diag4_last[-pattern0];
  pattern0 = board[85];
  pattern0 = 3 * pattern0 + board[76];
  pattern0 = 3 * pattern0 + board[67];
  pattern0 = 3 * pattern0 + board[58];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].diag4_last[-pattern0] );
#endif
  score += set[eval_phase].diag4_last[-pattern0];
  pattern0 = board[33];
  pattern0 = 3 * pattern0 + board[32];
  pattern0 = 3 * pattern0 + board[31];
  pattern0 = 3 * pattern0 + board[23];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[21];
  pattern0 = 3 * pattern0 + board[13];
  pattern0 = 3 * pattern0 + board[12];
  pattern0 = 3 * pattern0 + board[11];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner33_last[-pattern0] );
#endif
  score += set[eval_phase].corner33_last[-pattern0];
  pattern0 = board[63];
  pattern0 = 3 * pattern0 + board[62];
  pattern0 = 3 * pattern0 + board[61];
  pattern0 = 3 * pattern0 + board[73];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[71];
  pattern0 = 3 * pattern0 + board[83];
  pattern0 = 3 * pattern0 + board[82];
  pattern0 = 3 * pattern0 + board[81];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner33_last[-pattern0] );
#endif
  score += set[eval_phase].corner33_last[-pattern0];
  pattern0 = board[36];
  pattern0 = 3 * pattern0 + board[37];
  pattern0 = 3 * pattern0 + board[38];
  pattern0 = 3 * pattern0 + board[26];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[28];
  pattern0 = 3 * pattern0 + board[16];
  pattern0 = 3 * pattern0 + board[17];
  pattern0 = 3 * pattern0 + board[18];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner33_last[-pattern0] );
#endif
  score += set[eval_phase].corner33_last[-pattern0];
  pattern0 = board[66];
  pattern0 = 3 * pattern0 + board[67];
  pattern0 = 3 * pattern0 + board[68];
  pattern0 = 3 * pattern0 + board[76];
  pattern0 = 3 * pattern0 + board[77];
  pattern0 = 3 * pattern0 + board[78];
  pattern0 = 3 * pattern0 + board[86];
  pattern0 = 3 * pattern0 + board[87];
  pattern0 = 3 * pattern0 + board[88];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner33_last[-pattern0] );
#endif
  score += set[eval_phase].corner33_last[-pattern0];
  pattern0 = board[25];
  pattern0 = 3 * pattern0 + board[24];
  pattern0 = 3 * pattern0 + board[23];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[21];
  pattern0 = 3 * pattern0 + board[15];
  pattern0 = 3 * pattern0 + board[14];
  pattern0 = 3 * pattern0 + board[13];
  pattern0 = 3 * pattern0 + board[12];
  pattern0 = 3 * pattern0 + board[11];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52_last[-pattern0] );
#endif
  score += set[eval_phase].corner52_last[-pattern0];
  pattern0 = board[75];
  pattern0 = 3 * pattern0 + board[74];
  pattern0 = 3 * pattern0 + board[73];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[71];
  pattern0 = 3 * pattern0 + board[85];
  pattern0 = 3 * pattern0 + board[84];
  pattern0 = 3 * pattern0 + board[83];
  pattern0 = 3 * pattern0 + board[82];
  pattern0 = 3 * pattern0 + board[81];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52_last[-pattern0] );
#endif
  score += set[eval_phase].corner52_last[-pattern0];
  pattern0 = board[24];
  pattern0 = 3 * pattern0 + board[25];
  pattern0 = 3 * pattern0 + board[26];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[28];
  pattern0 = 3 * pattern0 + board[14];
  pattern0 = 3 * pattern0 + board[15];
  pattern0 = 3 * pattern0 + board[16];
  pattern0 = 3 * pattern0 + board[17];
  pattern0 = 3 * pattern0 + board[18];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52_last[-pattern0] );
#endif
  score += set[eval_phase].corner52_last[-pattern0];
  pattern0 = board[74];
  pattern0 = 3 * pattern0 + board[75];
  pattern0 = 3 * pattern0 + board[76];
  pattern0 = 3 * pattern0 + board[77];
  pattern0 = 3 * pattern0 + board[78];
  pattern0 = 3 * pattern0 + board[84];
  pattern0 = 3 * pattern0 + board[85];
  pattern0 = 3 * pattern0 + board[86];
  pattern0 = 3 * pattern0 + board[87];
  pattern0 = 3 * pattern0 + board[88];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52_last[-pattern0] );
#endif
  score += set[eval_phase].corner52_last[-pattern0];
  pattern0 = board[52];
  pattern0 = 3 * pattern0 + board[42];
  pattern0 = 3 * pattern0 + board[32];
  pattern0 = 3 * pattern0 + board[22];
  pattern0 = 3 * pattern0 + board[12];
  pattern0 = 3 * pattern0 + board[51];
  pattern0 = 3 * pattern0 + board[41];
  pattern0 = 3 * pattern0 + board[31];
  pattern0 = 3 * pattern0 + board[21];
  pattern0 = 3 * pattern0 + board[11];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52_last[-pattern0] );
#endif
  score += set[eval_phase].corner52_last[-pattern0];
  pattern0 = board[57];
  pattern0 = 3 * pattern0 + board[47];
  pattern0 = 3 * pattern0 + board[37];
  pattern0 = 3 * pattern0 + board[27];
  pattern0 = 3 * pattern0 + board[17];
  pattern0 = 3 * pattern0 + board[58];
  pattern0 = 3 * pattern0 + board[48];
  pattern0 = 3 * pattern0 + board[38];
  pattern0 = 3 * pattern0 + board[28];
  pattern0 = 3 * pattern0 + board[18];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52_last[-pattern0] );
#endif
  score += set[eval_phase].corner52_last[-pattern0];
  pattern0 = board[42];
  pattern0 = 3 * pattern0 + board[52];
  pattern0 = 3 * pattern0 + board[62];
  pattern0 = 3 * pattern0 + board[72];
  pattern0 = 3 * pattern0 + board[82];
  pattern0 = 3 * pattern0 + board[41];
  pattern0 = 3 * pattern0 + board[51];
  pattern0 = 3 * pattern0 + board[61];
  pattern0 = 3 * pattern0 + board[71];
  pattern0 = 3 * pattern0 + board[81];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52_last[-pattern0] );
#endif
  score += set[eval_phase].corner52_last[-pattern0];
  pattern0 = board[47];
  pattern0 = 3 * pattern0 + board[57];
  pattern0 = 3 * pattern0 + board[67];
  pattern0 = 3 * pattern0 + board[77];
  pattern0 = 3 * pattern0 + board[87];
  pattern0 = 3 * pattern0 + board[48];
  pattern0 = 3 * pattern0 + board[58];
  pattern0 = 3 * pattern0 + board[68];
  pattern0 = 3 * pattern0 + board[78];
  pattern0 = 3 * pattern0 + board[88];
#ifdef LOG_EVAL
  fprintf( stream, "pattern=%d\n", pattern0 );
  fprintf( stream, "score=%d\n", set[eval_phase].corner52_last[-pattern0] );
#endif
  score += set[eval_phase].corner52_last[-pattern0];
#endif
  }

#ifdef LOG_EVAL
  fclose( stream );
#endif

#if TIME_EVAL
  {
    static long long sum = 0;
    static int calls = 0;

    calls++;
    sum += (rdtsc() - t0);
    if ( (calls & ((1 << 22) - 1)) == 0 ) {
      printf( "%lld cycles / eval\n", sum / calls );
    }
  }
#endif

  return score;
}


/*
   REMOVE_SPECIFIC_COEFFS
   Removes the interpolated coefficients for a
   specific game phase from memory.
*/

static void
remove_specific_coeffs( int phase ) {
  if ( set[phase].loaded ) {
    if ( !set[phase].permanent )
      free_memory_block( set[phase].block );
    set[phase].loaded = 0;
  }
}


/*
   REMOVE_COEFFS
   Removes pattern tables which have gone out of scope from memory.
*/   

void
remove_coeffs( int phase ) {
  int i;

  for ( i = 0; i < phase; i++ )
    remove_specific_coeffs( i );
}


/*
   CLEAR_COEFFS
   Remove all coefficients loaded from memory.
*/

void
clear_coeffs( void ) {
  int i;

  for ( i = 0; i <= 60; i++ )
    remove_specific_coeffs( i );
}
