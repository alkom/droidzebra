/*
   File:       globals.c

   Created:    June 30, 1997

   Modified:   October 30, 2001

   Author:     Gunnar Andersson (gunnar@radagast.se)

   Contents:   Global state variables.
*/



#include "globals.h"


/* Global variables */

int pv[MAX_SEARCH_DEPTH][MAX_SEARCH_DEPTH];
int pv_depth[MAX_SEARCH_DEPTH];
int score_sheet_row;
int piece_count[3][MAX_SEARCH_DEPTH];
int black_moves[60];
int white_moves[60];
Board board;
