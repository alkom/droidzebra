/*
   File:           patterns.h

   Created:        July 4, 1997

   Modified:       August 1, 2002

   Author:         Gunnar Andersson (gunnar@radagast.se)

   Contents:       The patterns.
*/



#ifndef PATTERNS_H
#define PATTERNS_H



#include "constant.h"



#ifdef __cplusplus
extern "C" {
#endif



/* Predefined two-bit patterns. */

#define  EMPTY_PATTERN               0
#define  BLACK_PATTERN               1
#define  WHITE_PATTERN               2


/* Board patterns used in position evaluation */

#define  PATTERN_COUNT               46

#define  AFILEX1                     0
#define  AFILEX2                     1
#define  AFILEX3                     2
#define  AFILEX4                     3
#define  BFILE1                      4
#define  BFILE2                      5
#define  BFILE3                      6
#define  BFILE4                      7
#define  CFILE1                      8
#define  CFILE2                      9
#define  CFILE3                      10
#define  CFILE4                      11
#define  DFILE1                      12
#define  DFILE2                      13
#define  DFILE3                      14
#define  DFILE4                      15
#define  DIAG8_1                     16
#define  DIAG8_2                     17
#define  DIAG7_1                     18
#define  DIAG7_2                     19
#define  DIAG7_3                     20
#define  DIAG7_4                     21
#define  DIAG6_1                     22
#define  DIAG6_2                     23
#define  DIAG6_3                     24
#define  DIAG6_4                     25
#define  DIAG5_1                     26
#define  DIAG5_2                     27
#define  DIAG5_3                     28
#define  DIAG5_4                     29
#define  DIAG4_1                     30
#define  DIAG4_2                     31
#define  DIAG4_3                     32
#define  DIAG4_4                     33
#define  CORNER33_1                  34
#define  CORNER33_2                  35
#define  CORNER33_3                  36
#define  CORNER33_4                  37
#define  CORNER42_1                  38
#define  CORNER42_2                  39
#define  CORNER42_3                  40
#define  CORNER42_4                  41
#define  CORNER42_5                  42
#define  CORNER42_6                  43
#define  CORNER42_7                  44
#define  CORNER42_8                  45

#define  CORNER52_1                  38
#define  CORNER52_2                  39
#define  CORNER52_3                  40
#define  CORNER52_4                  41
#define  CORNER52_5                  42
#define  CORNER52_6                  43
#define  CORNER52_7                  44
#define  CORNER52_8                  45



extern int pow3[10];

/* Connections between the squares and the bit masks */

extern int row_no[100];
extern int row_index[100];
extern int col_no[100];
extern int col_index[100];

extern int color_pattern[3];

/* The patterns describing the current state of the board. */

extern int row_pattern[8];
extern int col_pattern[8];

/* Symmetry maps */

extern int flip8[6561];

/* Masks which represent dependencies between discs and patterns */

extern unsigned int depend_lo[100];
extern unsigned int depend_hi[100];

/* Bit masks that show what patterns have been modified */

extern unsigned int modified_lo;
extern unsigned int modified_hi;



void
init_patterns( void );

void
compute_line_patterns( int *in_board );



#ifdef __cplusplus
}
#endif



#endif  /* PATTERNS_H */
