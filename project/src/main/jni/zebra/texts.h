/*
   File:         texts.h

   Created:      September 22, 1999

   Modified:     July 20, 2002

   Author:       Gunnar Andersson (gunnar@radagast.se)

   Contents:     All the string constants used in Zebra's output.
*/



#ifndef TEXTS_H
#define TEXTS_H



/* Error messages in cntflip.c */
#define  COUNT_FLIPS_ERROR     "CountFlips called with sqnum="
#define  ANY_FLIPS_ERROR       "AnyFlips called with sqnum="

/* Abbreviations for kilo, mega and giga in display.c */
#define  KILO_ABBREV           'k'
#define  MEGA_ABBREV           'M'
#define  GIGA_ABBREV           'G'

/* Abbreviation for the time unit "second" in display.c */
#define  SECOND_ABBREV         's'

/* Abbreviation for "Principal variation" in display.c */
#define  PV_ABBREV             "PV"

/* The colors used in display.c */
#define  BLACK_TEXT            "Black"
#define  WHITE_TEXT            "White"

/* Game outcomes in display.c and game.c */
#define  WIN_TEXT              "Win"
#define  WIN_BY_TEXT           "Win by"
#define  WIN_BY_BOUND_TEXT     "Win by at least"
#define  DRAW_TEXT             "Draw"
#define  LOSS_TEXT             "Loss"
#define  LOSS_BY_TEXT          "Loss by"
#define  LOSS_BY_BOUND_TEXT    "Loss by at least"
#define  DISCS_TEXT            "discs"
#define  FORCED_TEXT           "forced"
#define  PASS_TEXT             "pass"
#define  INCOMPLETE_TEXT       "incompl"
#define  BOOK_TEXT             "book"
#define  INTERRUPT_TEXT        "Interrupted move"

/* Error messages in doflip.c */
#define  DOFLIPS_ERROR         "DoFlips called with sqnum="

/* Error messages in end.c and midgame.c */
#define  HASH_BEFORE           "Hash value before"
#define  HASH_AFTER            "Hash value after"

/* Abbreviation for "nodes per second" in end.c */
#define  NPS_ABBREV            "nps"

/* Timeout messages in end.c */
#define  PANIC_ABORT_TEXT      "Panic abort after"
#define  SEMI_PANIC_ABORT_TEXT "Semi-panic abort after"
#define  SEL_SEARCH_TEXT       "in selective search"
#define  WLD_SEARCH_TEXT       "in WLD search"
#define  EXACT_SEARCH_TEXT     "in exact search"

/* Error header in error.c */
#define  FATAL_ERROR_TEXT      "Fatal error"

/* The header of the log file from game.c */
#define  LOG_TEXT              "Log file created"
#define  ENGINE_TEXT           "Engine compiled"

/* Error messages in game.c */
#define  GAME_LOAD_ERROR       "Cannot open game file"
#define  BAD_CHARACTER_ERROR   "Unrecognized character"
#define  GAME_FILE_TEXT        "in game file"

/* Status messages in game.c and thordb.c */
#define  BEST_MOVE_TEXT        "Best move"
#define  MOVE_GEN_TEXT         "moves generated"
#define  MOVE_CHOICE_TEXT      "Move chosen"
#define  THOR_TEXT             "Thor database"
#define  HASH_MOVE_TEXT        "hash move"

/* Error messages in getcoeff.c */
#define  READ_ERROR_HI         "Error reading HI"
#define  READ_ERROR_LO         "Error reading LO"
#define  MIRROR_ERROR          "Mirror symmetry error"
#define  MEMORY_ERROR          "Memory allocation failure"
#define  FILE_ERROR            "Unable to open coefficient file"
#define  CHECKSUM_ERROR        "Wrong checksum in , might be an old version"

/* Prompts in moves.c */
#define  BLACK_PROMPT          "Black move"
#define  WHITE_PROMPT          "White move"

/* Error messages in osfbook.c */
#define  BOOK_HASH_ALLOC_ERROR "Book hash table: Failed to allocate"
#define  BOOK_ALLOC_ERROR      "Book node list: Failed to allocate"
#define  BOOK_INVALID_MOVE     "Invalid move generated"
#define  NO_GAME_FILE_ERROR    "Could not open game file"
#define  NO_DB_FILE_ERROR      "Could not open database file"
#define  BOOK_CHECKSUM_ERROR   "Wrong checksum, might be an old version"
#define  DB_WRITE_ERROR        "Could not create database file"

/* Error message in safemem.c */
#define  SAFEMEM_FAILURE       "Memory allocation failure when allocating"

/* Error message in search.c */
#define  PV_ERROR              "Error in PV completion"

/* Various texts and error messages in thordb.c */
#define  NA_ERROR              "Not available"
#define  TOURNAMENTS_TEXT      "tournaments"
#define  PLAYERS_TEXT          "players"
#define  DATABASES_TEXT        "databases"
#define  GAMES_TEXT            "games"
#define  VERSUS_TEXT           "vs"
#define  PERFECT_TEXT          "perfect"



#endif  /* TEXTS_H */
