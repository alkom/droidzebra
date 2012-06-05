/* Copyright (C) 2012 by Alex Kompel  */

#ifndef __DROIDZEBRA_MSG__
#define __DROIDZEBRA_MSG__

#include "droidzebra.h"

#define MSG_ERROR 0
#define MSG_BOARD 1
#define MSG_CANDIDATE_MOVES 2
#define MSG_GET_USER_INPUT 3
#define MSG_PASS 4
#define MSG_OPENING_NAME 5
#define MSG_LAST_MOVE 6
#define MSG_GAME_START 7
#define MSG_GAME_OVER 8
#define MSG_MOVE_START 9
#define MSG_MOVE_END 10
#define MSG_EVAL_TEXT 11
#define MSG_PV 12
#define MSG_CANDIDATE_EVALS 13
#define MSG_DEBUG 65535

#define UI_EVENT_EXIT  0
#define UI_EVENT_MOVE  1
#define UI_EVENT_UNDO  2
#define UI_EVENT_SETTINGS_CHANGE 3

typedef struct {
	int type;
	union {
		struct {
			int move;
		} evt_move;
	};
} ui_event_t;

void
droidzebra_msg_board(
	int *board,
	int side_to_move,
	double black_eval,
	double white_eval,
	int black_time,
	int white_time
);

int
droidzebra_msg_get_user_input( int side_to_move, ui_event_t* ui_event );

void
droidzebra_msg_candidate_moves(void);

void
droidzebra_msg_pass(void);

void
droidzebra_msg_opening_name(const char* opening_name);

void
droidzebra_msg_last_move(int move);

void
droidzebra_msg_game_start(void);

void
droidzebra_msg_game_over(void);

void
droidzebra_msg_move_start(int side_to_move);

void
droidzebra_msg_move_end(int side_to_move);

void
droidzebra_msg_eval(void);

void
droidzebra_msg_pv(void);

void
droidzebra_msg_candidate_evals(void);


#endif

