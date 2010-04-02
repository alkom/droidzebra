/*
   File:          patterns.c

   Created:       July 4, 1997

   Modified:      December 25, 1999
   
   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      The patterns.
*/



#include <stdio.h>
#include <stdlib.h>
#include "constant.h"
#include "display.h"
#include "globals.h"
#include "macros.h"
#include "patterns.h"



/* Global variables */

int pow3[10] = { 1, 3, 9, 27, 81, 243, 729, 2187, 6561, 19683 };

/* Connections between the squares and the bit masks */

int row_no[100];
int row_index[100];
int col_no[100];
int col_index[100];

int color_pattern[3];

/* The patterns describing the current state of the board. */

int row_pattern[8];
int col_pattern[8];

/* Symmetry maps */

int flip8[6561];

/* Bit masks which represent dependencies between discs and patterns */

unsigned int depend_lo[100];
unsigned int depend_hi[100];

/* Bit masks that show what patterns have been modified */

unsigned int modified_lo;
unsigned int modified_hi;



/*
   TRANSFORMATION_SET_UP
   Calculate the various symmetry and color transformations.
*/

static void
transformation_setup( void ) {
  int i, j;
  int row[10];

  /* Build the pattern tables for 8*1-patterns */

  for ( i = 0; i < 8; i++ )
    row[i] = 0;

  for ( i = 0; i < 6561; i++ ) {
    /* Create the symmetry map */
    flip8[i] = 0;
    for ( j = 0; j < 8; j++ )
      flip8[i] += row[j] * pow3[7 - j];

    /* Next configuration */
    j = 0;
    do {  /* The odometer principle */
      row[j]++;
      if ( row[j] == 3 )
	row[j] = 0;
      j++;
    } while ( (row[j - 1] == 0) && (j < 8) );
  }
}


/*
  ADD_SINGLE
  Mark board position POS as depending on pattern # MASK.
*/

static void
add_single( int mask, int pos ) {
  if ( mask < 32 )
    depend_lo[pos] |= 1 << mask;
  else
    depend_hi[pos] |= 1 << (mask - 32);
}


/*
  ADD_MULTIPLE
  Mark board positions POS, POS+STEP, ..., POS+(COUNT-1)STEP as
  depending on pattern # MASK.
*/

static void
add_multiple( int mask, int pos, int count, int step ) {
  int i;

  for ( i = 0; i < count; i++ )
    add_single( mask, pos + i * step );
}


/*
  PATTERN_DEPENDENCY
  Fill the dependency masks for each square with the bit masks
  for the patterns which it depends.
  Note: The definitions of the patterns and their corresponding name
        must match the order given in endmacro.c.
*/


static void
pattern_dependency( void ) {

  /* A-file+2X: a1-a8 + b2,b7 */

  add_multiple( AFILEX1, 11, 8, 10 );
  add_single( AFILEX1, 22 );
  add_single( AFILEX1, 72 );

  /* A-file+2X: h1-h8 + g2,g7 */

  add_multiple( AFILEX2, 18, 8, 10 );
  add_single( AFILEX2, 27 );
  add_single( AFILEX2, 77 );

  /* A-file+2X: a1-h1 + b2,g2 */

  add_multiple( AFILEX3, 11, 8, 1 );
  add_single( AFILEX3, 22 );
  add_single( AFILEX3, 27 );

  /* A-file+2X: a8-h8 + b7,g7 */

  add_multiple( AFILEX4, 81, 8, 1 );
  add_single( AFILEX4, 72 );
  add_single( AFILEX4, 77 );

  /* B-file: b1-b8 */

  add_multiple( BFILE1, 12, 8, 10 );

  /* B-file: g1-g8 */

  add_multiple( BFILE2, 17, 8, 10 );

  /* B-file: a2-h2 */

  add_multiple( BFILE3, 21, 8, 1 );

  /* B-file: a7-h7 */

  add_multiple( BFILE4, 71, 8, 1 );

  /* C-file: c1-c8 */

  add_multiple( CFILE1, 13, 8, 10 );

  /* C-file: f1-f8 */

  add_multiple( CFILE2, 16, 8, 10 );

  /* C-file: a3-h3 */

  add_multiple( CFILE3, 31, 8, 1 );

  /* C-file: a6-h6 */

  add_multiple( CFILE4, 61, 8, 1);

  /* D-file: d1-d8 */

  add_multiple( DFILE1, 14, 8, 10 );

  /* D-file: e1-e8 */

  add_multiple( DFILE2, 15, 8, 10 );

  /* D-file: a4-h4 */

  add_multiple( DFILE3, 41, 8, 1 );

  /* D-file: a5-h5 */

  add_multiple( DFILE4, 51, 8, 1 );

  /* Diag8: a1-h8 */

  add_multiple( DIAG8_1, 11, 8, 11 );

  /* Diag8: h1-a8 */

  add_multiple( DIAG8_2, 18, 8, 9 );

  /* Diag7: b1-h7 */

  add_multiple( DIAG7_1, 12, 7, 11 );

  /* Diag7: a2-g8 */

  add_multiple( DIAG7_2, 21, 7, 11 );

  /* Diag7: a7-g1 */

  add_multiple( DIAG7_3, 17, 7, 9 );

  /* Diag7: b8-h2 */

  add_multiple( DIAG7_4, 28, 7, 9 );

  /* Diag6: c1-h6 */

  add_multiple( DIAG6_1, 13, 6, 11 );

  /* Diag6: a3-f8 */

  add_multiple( DIAG6_2, 31, 6, 11 );

  /* Diag6: a6-f1 */

  add_multiple( DIAG6_3, 16, 6, 9 );

  /* Diag6: c8-h3 */

  add_multiple( DIAG6_4, 38, 6, 9 );

  /* Diag5: d1-h5 */

  add_multiple( DIAG5_1, 14, 5, 11 );

  /* Diag5: a4-e8 */

  add_multiple( DIAG5_2, 41, 5, 11 );

  /* Diag5: a5-e1 */

  add_multiple( DIAG5_3, 15, 5, 9 );

  /* Diag5: d8-h4 */

  add_multiple( DIAG5_4, 48, 5, 9 );

  /* Diag4: e1-h4 */

  add_multiple( DIAG4_1, 15, 4, 11 );

  /* Diag4: a5-d8 */

  add_multiple( DIAG4_2, 51, 4, 11 );

  /* Diag4: a4-d1 */

  add_multiple( DIAG4_3, 14, 4, 9 );

  /* Diag4: e8-h5 */

  add_multiple( DIAG4_4, 58, 4, 9 );

  /* Corner3x3: a1-c1 + a2-c2 + a3-c3 */

  add_multiple( CORNER33_1, 11, 3, 1 );
  add_multiple( CORNER33_1, 21, 3, 1 );
  add_multiple( CORNER33_1, 31, 3, 1 );

  /* Corner3x3: a8-c8 + a7-c7 + a6-c6 */

  add_multiple( CORNER33_2, 81, 3, 1 );
  add_multiple( CORNER33_2, 71, 3, 1 );
  add_multiple( CORNER33_2, 61, 3, 1 );

  /* Corner3x3: f1-h1 + f2-h2 + f3-h3 */

  add_multiple( CORNER33_3, 18, 3, -1 );
  add_multiple( CORNER33_3, 28, 3, -1 );
  add_multiple( CORNER33_3, 38, 3, -1 );

  /* Corner3x3: f8-h8 + f7-h7 + f6-h6 */

  add_multiple( CORNER33_4, 88, 3, -1 );
  add_multiple( CORNER33_4, 78, 3, -1 );
  add_multiple( CORNER33_4, 68, 3, -1 );

  /* Corner4x2: a1-d1 + a2-d2 */

  add_multiple( CORNER42_1, 11, 4, 1 );
  add_multiple( CORNER42_1, 21, 4, 1 );

  /* Corner4x2: a8-d8 + a7-d7 */

  add_multiple( CORNER42_2, 81, 4, 1 );
  add_multiple( CORNER42_2, 71, 4, 1 );

  /* Corner4x2: e1-h1 + e2-h2 */

  add_multiple( CORNER42_3, 18, 4, -1 );
  add_multiple( CORNER42_3, 28, 4, -1 );

  /* Corner4x2: e8-h8 + e7-h7 */

  add_multiple( CORNER42_4, 88, 4, -1 );
  add_multiple( CORNER42_4, 78, 4, -1 );

  /* Corner4x2: a1-a4 + b1-b4 */

  add_multiple( CORNER42_5, 11, 4, 10 );
  add_multiple( CORNER42_5, 12, 4, 10 );

  /* Corner4x2: h1-h4 + g1-g4 */ 

  add_multiple( CORNER42_6, 18, 4, 10 );
  add_multiple( CORNER42_6, 17, 4, 10 );

  /* Corner4x2: a8-a5 + b8-b5 */

  add_multiple( CORNER42_7, 81, 4, -10 );
  add_multiple( CORNER42_7, 82, 4, -10 );

  /* Corner4x2: h8-h5 + g8-g5 */

  add_multiple( CORNER42_8, 88, 4, -10 );
  add_multiple( CORNER42_8, 87, 4, -10 );
}



/*
   INIT_PATTERNS
   Pre-computes some tables needed for fast pattern access.
*/   

void
init_patterns( void ) {
  int i, j;
  int pos;

  transformation_setup();

  for ( i = 1; i <= 8; i++ )
    for ( j = 1; j <= 8; j++ ) {
      pos = 10 * i + j;
      row_no[pos] = i - 1;
      row_index[pos] = j - 1;
      col_no[pos] = j - 1;
      col_index[pos] = i - 1;
    }

  pattern_dependency();

  /* These values needed for compatibility with the old book format */

  color_pattern[BLACKSQ] = BLACK_PATTERN;
  color_pattern[WHITESQ] = WHITE_PATTERN;
}


/*
   COMPUTE_LINE_PATTERNS
   Translate the current board configuration into patterns.
*/

void
compute_line_patterns( int *in_board ) {
  int i, j;
  int pos;
  int mask;

  for ( i = 0; i < 8; i++ ) {
    row_pattern[i] = 0;
    col_pattern[i] = 0;
  }

  for ( i = 1; i <= 8; i++ )
    for ( j = 1; j <= 8; j++ ) {
      pos = 10 * i + j;
      if ( in_board[pos] == EMPTY )
	mask = EMPTY_PATTERN;
      else               
	mask = color_pattern[in_board[pos]];
      row_pattern[row_no[pos]] += mask * pow3[row_index[pos]];
      col_pattern[col_no[pos]] += mask * pow3[col_index[pos]];
    }
}
