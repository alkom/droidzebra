/* Copyright (C) 2010 by Alex Kompel  */
/* This file is part of DroidZebra.

    DroidZebra is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    DroidZebra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DroidZebra.  If not, see <http://www.gnu.org/licenses/>
*/


#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "zebra/constant.h"
#include "zebra/game.h"
#include "zebra/globals.h"
#include "zebra/search.h"
#include "zebra/moves.h"
#include "zebra/display.h"
#include "zebra/safemem.h"
#include "zebra/texts.h"
#include "zebra/eval.h"

#include "droidzebra-json.h"
#include "droidzebra-msg.h"

// MSG_ERROR
// in droidzebra-jni.c

// MSG_BOARD
void
droidzebra_msg_board(
	int *board,
	int side_to_move,
	double black_eval,
	double white_eval,
	int black_time,
	int white_time
) {
	char buffer[1024];
	int buffer_pos = 0;
	int i, j;
	int len;

	buffer_pos = sprintf(buffer, "{ \"board\":[" );
	for ( i = 1; i <= 8; i++ ) {
		buffer_pos += sprintf(buffer+buffer_pos, "[" );
		for ( j = 1; j <= 8; j++ ) {
			switch ( board[10 * i + j] ) {
			case BLACKSQ:
				buffer_pos += sprintf(buffer+buffer_pos, "%d,", BLACKSQ );
				break;
			case WHITESQ:
				buffer_pos += sprintf(buffer+buffer_pos, "%d,", WHITESQ );
				break;
			default:
				buffer_pos += sprintf(buffer+buffer_pos, "%d,", EMPTY );
				break;
			}
		}
		// eat last comma
		buffer_pos--;
		buffer_pos += sprintf(buffer+buffer_pos, "]," );
	}
	buffer_pos--; // eat last comma
	buffer_pos += sprintf(buffer+buffer_pos, "]," );

	// side to move
	buffer_pos += sprintf(buffer+buffer_pos, "\"side_to_move\":%d,", side_to_move );
	buffer_pos += sprintf(buffer+buffer_pos, "\"disks_played\":%d,", side_to_move==BLACKSQ? 2*score_sheet_row : 2*score_sheet_row+1 );

	// black player info
	buffer_pos += sprintf(buffer+buffer_pos, "\"black\":{" );
	buffer_pos += sprintf(buffer+buffer_pos, "\"time\":\"%02d:%02d\",",
			black_time / 60, black_time % 60 );
	buffer_pos += sprintf(buffer+buffer_pos, "\"eval\":%6.2f,", black_eval );
	buffer_pos += sprintf(buffer+buffer_pos, "\"disc_count\":%d,", disc_count( BLACKSQ ));
	buffer_pos += sprintf(buffer+buffer_pos, "\"moves\":[ "); //space is important
	len = (side_to_move==BLACKSQ? score_sheet_row : score_sheet_row+1); // if it is white move then current score_sheet_row has the black move so we need to look at it as well
	for(i=0; i<len; i++)
		buffer_pos += sprintf(buffer+buffer_pos, "%d,", black_moves[i]);
	buffer_pos--; // eat last comma (or space - see above)
	buffer_pos += sprintf(buffer+buffer_pos, "],");
	buffer_pos--; // eat last comma
	buffer_pos += sprintf(buffer+buffer_pos, "}," );


	buffer_pos += sprintf(buffer+buffer_pos, "\"white\":{" );
	buffer_pos += sprintf(buffer+buffer_pos, "\"time\":\"%02d:%02d\",",
			white_time / 60, white_time % 60 );
	buffer_pos += sprintf(buffer+buffer_pos, "\"eval\":%6.2f,", white_eval );
	buffer_pos += sprintf(buffer+buffer_pos, "\"disc_count\":%d,", disc_count( WHITESQ ));
	buffer_pos += sprintf(buffer+buffer_pos, "\"moves\":[ "); //space is important
	for(i=0; i<score_sheet_row; i++) {
		buffer_pos += sprintf(buffer+buffer_pos, "%d,", white_moves[i]);
	}
	buffer_pos--; // eat last comma or space
	buffer_pos += sprintf(buffer+buffer_pos, "],");
	buffer_pos--; // eat last comma
	buffer_pos += sprintf(buffer+buffer_pos, "}," );

	buffer_pos--; // eat last comma
	buffer_pos += sprintf(buffer+buffer_pos, "}" );

	droidzebra_message(MSG_BOARD, buffer);
}

int
droidzebra_msg_get_user_input( int side_to_move, ui_event_t* ui_event )
{
	int type;
	int move;
	jobject json_move;
	int ready = 0;
	JNIEnv* env = droidzebra_jnienv();

	memset(ui_event, 0, sizeof(ui_event_t));

	while ( !ready ) {
		ready = 1;

		json_move = droidzebra_RPC_callback(MSG_GET_USER_INPUT, NULL);
		assert(json_move!=NULL);
		type = droidzebra_json_get_int(env, json_move, "type");
		ui_event->type = type;
		switch(type) {
		case UI_EVENT_MOVE:
			move = droidzebra_json_get_int(env, json_move, "move");
			ui_event->evt_move.move = move;
			ready = valid_move( move, side_to_move );
			break;
		}
		(*env)->DeleteLocalRef(env, json_move);
	}

	return 0;
}

// MSG_CANDIDATE_MOVES
void
droidzebra_msg_candidate_moves(void)
{
	int i;
	char buffer[128*60];
	int buffer_pos = 0;

	/* build list of alternative moves */
	buffer_pos = sprintf(buffer, "{\"moves\":[ " );

	for( i=0; i<move_count[disks_played]; i++ ) {
		buffer_pos += sprintf(buffer+buffer_pos,
				"{\"move\":%d},",
				move_list[disks_played][i]
		);
	}
	buffer_pos--; // erase last comma
	buffer_pos += sprintf(buffer+buffer_pos, "] }" );

	droidzebra_message(MSG_CANDIDATE_MOVES, buffer);
}

// MSG_PASS
void
droidzebra_msg_pass(void)
{
	droidzebra_message(MSG_PASS, NULL);
}

// MSG_OPENING_NAME
void
droidzebra_msg_opening_name(const char* opening_name)
{
	char buffer[256];
	if(!opening_name) return;
	sprintf(buffer, "{ \"opening\":\"%s\" }", opening_name);
	droidzebra_message(MSG_OPENING_NAME, buffer);
}

// MSG_LAST_MOVE
void
droidzebra_msg_last_move(int move)
{
	char json_buffer[64];
	sprintf(json_buffer, "{\"move\":%d}", move);
	droidzebra_message(MSG_LAST_MOVE, json_buffer);
}

// MSG_GAME_START
void
droidzebra_msg_game_start(void)
{
	droidzebra_message(MSG_GAME_START, NULL);
}

// MSG_GAME_OVER
void
droidzebra_msg_game_over(void)
{
	droidzebra_message(MSG_GAME_OVER, NULL);
}


void
droidzebra_msg_analyze(char *message) {
    char json_buffer[64];
    sprintf(json_buffer, "{\"analyze\":%d}", message);
    droidzebra_message(MSG_MOVE_START, json_buffer);
}

// MSG_MOVE_START
void
droidzebra_msg_move_start(int side_to_move)
{
	char json_buffer[64];
	sprintf(json_buffer, "{\"side_to_move\":%d}", side_to_move);
	droidzebra_message(MSG_MOVE_START, json_buffer);
}

// MSG_MOVE_END
void
droidzebra_msg_move_end(int side_to_move)
{
	char json_buffer[64];
	sprintf(json_buffer, "{\"side_to_move\":%d}", side_to_move);
	droidzebra_message(MSG_MOVE_END, json_buffer);
}


// MSG_EVALS
void
droidzebra_msg_eval(void)
{
	char buffer[128];
	char* eval_str = produce_eval_text(get_current_eval(),FALSE);
	sprintf(buffer, "{\"eval\":\"%s\"}", eval_str);
	free(eval_str);
	droidzebra_message(MSG_EVAL_TEXT, buffer);
}

// MSG_PV
void
droidzebra_msg_pv(void)
{
	char buffer[256];
	int buffer_pos = 0;
	int i;

	buffer_pos = sprintf(buffer, "{\"pv\":[ ");
	for ( i = 0; i < full_pv_depth; i++ )
		buffer_pos += sprintf(buffer+buffer_pos, "%d,", full_pv[i]);
	buffer_pos --; // kill last comma (or space)
	buffer_pos += sprintf(buffer+buffer_pos, "]}");

	droidzebra_message(MSG_PV, buffer);
}

/*
  COMPARE_EVAL
  Comparison function for two evals.  Same return value conventions
  as QuickSort.
*/

static int
compare_eval( EvaluationType e1, EvaluationType e2 ) {
  if ( (e1.type == WLD_EVAL) || (e1.type == EXACT_EVAL) )
    if ( e1.score > 0 )
      e1.score += 100000;
  if ( (e2.type == WLD_EVAL) || (e2.type == EXACT_EVAL) )
    if ( e2.score > 0 )
      e2.score += 100000;

  return e1.score - e2.score;
}

/*
  PRODUCE_EVAL_TEXT
  Convert a result descriptor into a string intended for output.
 */

static char *
candidate_eval_text( EvaluationType eval_info) {
	char *buffer;
	int disk_diff;
	int len;

	buffer = (char *) safe_malloc( 32 );
	len = 0;

	switch ( eval_info.type ) {
	case MIDGAME_EVAL:
		if ( eval_info.score >= MIDGAME_WIN )
			len = sprintf( buffer, WIN_TEXT );
		else if ( eval_info.score <= -MIDGAME_WIN )
			len = sprintf( buffer, LOSS_TEXT );
		else {
			disk_diff = (int)round(eval_info.score / 128.0);
			len = sprintf( buffer, "%+d", disk_diff );
		}
		break;

	case SELECTIVE_EVAL:
	case EXACT_EVAL:
		len = sprintf( buffer, "%+d", eval_info.score >> 7 );
		break;

	case WLD_EVAL:
		switch ( eval_info.res ) {

		case WON_POSITION:
			len = sprintf( buffer, WIN_TEXT );
			break;

		case DRAWN_POSITION:
			len = sprintf( buffer, DRAW_TEXT );
			break;

		case LOST_POSITION:
			len = sprintf( buffer, LOSS_TEXT );
			break;

		case UNSOLVED_POSITION:
			len = sprintf( buffer, "???" );
			break;
		}
		break;


	case FORCED_EVAL:
	case PASS_EVAL:
		len = sprintf( buffer, "-" );
		break;

	case INTERRUPTED_EVAL:
		len = sprintf( buffer, INCOMPLETE_TEXT );
		break;

	case UNDEFINED_EVAL:
		buffer[0] = 0;
		len = 0;
		break;

	case UNINITIALIZED_EVAL:
		len = sprintf( buffer, "--" );
		break;
	}
	return buffer;
}

// MSG_CANDIDATE_EVALS
void
droidzebra_msg_candidate_evals(void)
{
	int i;
	char buffer[128*60];
	int buffer_pos = 0;
	int evaluated_count = 0;
	char* eval_s;
	char* eval_l;
	EvaluationType best;

	buffer_pos = sprintf(buffer, "{\"evals\":[ " );

	evaluated_count = get_evaluated_count();
	if(evaluated_count==0)
		return;
	for( i=0; i<evaluated_count; i++ ) {
		EvaluatedMove emove = get_evaluated(i);
		if(i==0) {
			// evaluated moves are sorted best to worst
			best = emove.eval;
		}

		if( emove.eval.type == INTERRUPTED_EVAL
			|| emove.eval.type == UNDEFINED_EVAL
			|| emove.eval.type == UNINITIALIZED_EVAL
			) {
			continue;
		}

		eval_s = candidate_eval_text(emove.eval);
		eval_l = candidate_eval_text(emove.eval);

		buffer_pos += sprintf(buffer+buffer_pos,
				"{\"move\":%d,\"best\":%d,\"eval_s\":\"%s\",\"eval_l\":\"%s\"},",
				emove.move,
				compare_eval(best, emove.eval)==0? 1 : 0,
				eval_s,
				eval_l
		);

		free(eval_s);
		free(eval_l);
	}

	buffer_pos--; // erase last comma
	buffer_pos += sprintf(buffer+buffer_pos, "] }" );

	droidzebra_message(MSG_CANDIDATE_EVALS, buffer);
}

// MSG_DEBUG
// in droidzebra-jni.c

