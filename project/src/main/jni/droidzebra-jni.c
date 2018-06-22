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


#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <setjmp.h>
#include <jni.h>

#include "droidzebra.h"
#include "droidzebra-json.h"
#include "droidzebra-msg.h"

/*--------- zebra ----------*/
#include "zebra/globals.h"
#include "zebra/game.h"
#include "zebra/display.h"
#include "zebra/myrandom.h"
#include "zebra/learn.h"
#include "zebra/error.h"
#include "zebra/osfbook.h"
#include "zebra/hash.h"
#include "zebra/moves.h"
#include "zebra/getcoeff.h"
#include "zebra/timer.h"
#include "zebra/eval.h"
#include "zebra/midgame.h"
#include "zebra/opname.h"
#include "zebra/thordb.h"
/*--------- zebra ----------*/

#define DEFAULT_HASH_BITS         18
#define DEFAULT_RANDOM            TRUE
#define DEFAULT_SLACK             0
#define DEFAULT_PERTURBATION      0
#define DEFAULT_WLD_ONLY          FALSE
#define SEPARATE_TABLES           FALSE
#define INFINIT_TIME              10000000

#define USE_LOG					  FALSE

// --
static int player_time[3], player_increment[3];
static int skill[3];
static int wld_skill[3], exact_skill[3];
static int force_exit = 0;
static int auto_make_forced_moves = 0;
static int s_slack = DEFAULT_SLACK;
static int s_perturbation = DEFAULT_PERTURBATION;
static int s_human_opening = FALSE;
static int s_practice_mode = FALSE;
static const char* s_forced_opening_seq = NULL;
static int s_use_book = TRUE;
static int s_enable_msg = TRUE;
static int s_undo_stack[64];
static int s_undo_stack_pointer = 0;
// --

#define JNIFn(Package, Class, Fname) JNICALL Java_com_shurik_##Package##_##Class##_##Fname

// these are only for callback function:
// first outer JNI call stores the environment that will be used by callback function
// from deep within zebra code (with the context of outer JNI call)
// if JNI function does not need to call "Callback" (like simple setters/getters)
// then it should not use it. Nesting is explicitly disabled.
static JNIEnv* s_env = NULL;
static jobject s_thiz = NULL;
static jmp_buf s_err_jmp;
#define DROIDZEBRA_JNI_SETUP \
	assert(s_env==NULL && s_thiz==NULL); \
	if( setjmp(s_err_jmp) ) return; \
	s_env = env; \
	s_thiz = thiz;
#define DROIDZEBRA_JNI_CLEAN \
	s_env = NULL; \
	s_thiz = NULL;
#define DROIDZEBRA_JNI_BREAK longjmp(s_err_jmp, -1);
#define DROIDZEBRA_CHECK_JNI { assert(s_env!=NULL); if(!s_env) exit( EXIT_FAILURE ); }

#define DROIDZEBRA_JNI_THROW(msg) \
	{ _droidzebra_throw_engine_error(env, (msg)); return; }

char android_files_dir[256];

static void _droidzebra_undo_turn(int* side_to_move);
static void _droidzebra_redo_turn(int* side_to_move);
static void _droidzebra_on_settings_change(void);
static void _droidzebra_compute_evals(int side_to_move);
static void _droidzebra_throw_engine_error(JNIEnv* env, const char* msg);

// undo stack helpers
static void _droidzebra_undo_stack_push(int val);
static int _droidzebra_undo_stack_pop(void);
static void _droidzebra_undo_stack_clear(void);
static int _droidzebra_can_redo(void);

JNIEnv* droidzebra_jnienv(void)
{
	DROIDZEBRA_CHECK_JNI;
	return s_env;
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeJsonTest)( JNIEnv* env, jobject thiz, jobject json )
{
	DROIDZEBRA_JNI_SETUP;
	char* buf = (char*)malloc(500000);
	char* str = droidzebra_json_get_string(env, json, "testin", buf, 500000);
	if(str) droidzebra_json_put_string(env, json, "testout", str );
	free(buf);
	DROIDZEBRA_JNI_CLEAN;
}

JNIEXPORT void
JNIFn(droidzebra, ZebraEngine, zeGlobalInit)(
		JNIEnv* env,
		jobject thiz,
		jstring files_dir
)
{
	DROIDZEBRA_JNI_SETUP;

	char binbookpath[FILENAME_MAX], cmpbookpath[FILENAME_MAX];
	time_t timer;

	echo = TRUE;
	display_pv = DEFAULT_DISPLAY_PV;
	skill[BLACKSQ] = skill[WHITESQ] = -1;
	player_time[BLACKSQ] = player_time[WHITESQ] = INFINIT_TIME;
    player_increment[BLACKSQ] = player_increment[WHITESQ] = 0;


	const char* str;
	str = (*env)->GetStringUTFChars(env, files_dir, NULL);
	if (str == NULL) {
		DROIDZEBRA_JNI_CLEAN;
		return; /* OutOfMemoryError already thrown */
	}
	strncpy(android_files_dir, str, sizeof(android_files_dir)-1);
	(*env)->ReleaseStringUTFChars(env, files_dir, str);

	toggle_status_log(USE_LOG);

	global_setup( DEFAULT_RANDOM, DEFAULT_HASH_BITS );
	init_thor_database();

	sprintf(cmpbookpath, "%s/book.cmp.z", android_files_dir);
	sprintf(binbookpath, "%s/book.bin", android_files_dir);
	if(access(cmpbookpath, R_OK)==0) {
		init_osf(FALSE);
		unpack_compressed_database_gz(cmpbookpath, binbookpath);
		unlink(cmpbookpath);
	}

	init_learn(binbookpath, TRUE);

	time(&timer);
    my_srandom((int) timer);
	DROIDZEBRA_JNI_CLEAN;
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeGlobalTerminate)( JNIEnv* env, jobject thiz )
{
	global_terminate();
}

JNIEXPORT void
JNIFn(droidzebra, ZebraEngine, zeForceReturn)(JNIEnv *env, jobject instance)
{
	force_return = 1;
}

JNIEXPORT void
JNIFn(droidzebra, ZebraEngine, zeForceExit)(JNIEnv *env, jobject instance)
{
	force_return = 1;
	force_exit = 1;
}

/* callback */
void
fatal_error( const char *format, ... ) {
	char errmsg[1024];
	va_list arg_ptr;
	jobject json;

	DROIDZEBRA_CHECK_JNI;

	va_start( arg_ptr, format );
	vsprintf( errmsg, format, arg_ptr );
	va_end( arg_ptr );

	json = droidzebra_json_create(s_env, NULL);
	if( !json ) exit( EXIT_FAILURE );
	droidzebra_json_put_string(s_env, json, "error", errmsg);
	json = droidzebra_RPC_callback(MSG_ERROR, json);
	(*s_env)->DeleteLocalRef(s_env, json);
	DROIDZEBRA_JNI_BREAK;

	/* not reached (theoretically) */
	exit( EXIT_FAILURE );
}

/* callback */
void
critical_error(const char *format, ...) {
	char errmsg[1024];
	va_list arg_ptr;
	jobject json;

	DROIDZEBRA_CHECK_JNI;

	va_start(arg_ptr, format);
	vsprintf(errmsg, format, arg_ptr);
	va_end(arg_ptr);

	json = droidzebra_json_create(s_env, NULL);
	if (!json) exit(EXIT_FAILURE);
	droidzebra_json_put_string(s_env, json, "error", errmsg);
	json = droidzebra_RPC_callback(MSG_ERROR, json);
	(*s_env)->DeleteLocalRef(s_env, json);
}

JNIEXPORT void
JNIFn(droidzebra, ZebraEngine, zeAnalyzeGame)(JNIEnv *env, jobject thiz, jint providedMoveCount,
                                              jbyteArray providedMoves) {
    EvaluationType best_info1, best_info2, played_info1, played_info2;
    const char *black_name, *white_name;
    const char *opening_name;
    double move_start, move_stop;
    int i;
    int side_to_move, opponent;
    int curr_move, resp_move;
    int timed_search;
    int black_hash1, black_hash2, white_hash1, white_hash2;
    int col, row;
    int empties;
    unsigned int best_trans1, best_trans2, played_trans1, played_trans2;
    char output_stream[1024];

    DROIDZEBRA_JNI_SETUP;



    /* copy provided moves */
    int provided_move_index = 0;
    int provided_move_count = 0;
    int provided_move[65];

    if (providedMoveCount > 0 && providedMoves) {
        jbyte *providedMovesJNI;
        provided_move_count = providedMoveCount;
        i = (*env)->GetArrayLength(env, providedMoves);
        if (provided_move_count > i)
            fatal_error("Provided move count is greater than array size %d>%d", provided_move_count,
                        i);
        if (provided_move_count > 64)
            fatal_error("Provided move count is greater that 64: %d", provided_move_count);
        providedMovesJNI = (*env)->GetByteArrayElements(env, providedMoves, 0);
        if (!providedMovesJNI)
            fatal_error("failed to get provide moves (jni)");
        for (i = 0; i < provided_move_count; i++) {
            provided_move[i] = providedMovesJNI[i];
        }
        (*env)->ReleaseByteArrayElements(env, providedMoves, providedMovesJNI, 0);
    }

    game_init(NULL, &side_to_move);
    setup_hash(TRUE);
    clear_stored_game();

    reset_book_search();
    //set_move_list( black_moves, white_moves, score_sheet_row );
    //set_evals( 0.0, 0.0 );

    for (i = 0; i < 60; i++) {
        black_moves[i] = PASS;
        white_moves[i] = PASS;
    }


    best_trans1 = (unsigned int) my_random();
    best_trans2 = (unsigned int) my_random();
    played_trans1 = (unsigned int) my_random();
    played_trans2 = (unsigned int) my_random();

    while (game_in_progress() && (disks_played < provided_move_count)) {
        remove_coeffs(disks_played);
        if (SEPARATE_TABLES) {  /* Computer players won't share hash tables */
            if (side_to_move == BLACKSQ) {
                hash1 ^= black_hash1;
                hash2 ^= black_hash2;
            } else {
                hash1 ^= white_hash1;
                hash2 ^= white_hash2;
            }
        }
        generate_all(side_to_move);

        if (side_to_move == BLACKSQ)
            score_sheet_row++;

        if (move_count[disks_played] != 0) {
            move_start = get_real_timer();
            clear_panic_abort();

/*			if ( echo ) {
				set_move_list( black_moves, white_moves, score_sheet_row );
				set_times( floor( player_time[BLACKSQ] ),
						   floor( player_time[WHITESQ] ) );
				opening_name = find_opening_name();
				if ( opening_name != NULL )
					printf( "\nOpening: %s\n", opening_name );

				display_board( stdout, board, side_to_move, TRUE, use_timer, TRUE );
			}*/

            /* Check what the Thor opening statistics has to say */

            (void) choose_thor_opening_move(board, side_to_move, FALSE);


            start_move(player_time[side_to_move],
                       player_increment[side_to_move],
                       disks_played + 4);
            determine_move_time(player_time[side_to_move],
                                player_increment[side_to_move],
                                disks_played + 4);
            timed_search = (skill[side_to_move] >= 60);
            toggle_experimental(FALSE);

            empties = 60 - disks_played;

            /* Determine the score for the move actually played.
               A private hash transformation is used so that the parallel
           search trees - "played" and "best" - don't clash. This way
           all scores are comparable. */

            set_hash_transformation(played_trans1, played_trans2);

            curr_move = provided_move[disks_played];
            opponent = OPP(side_to_move);
            (void) make_move(side_to_move, curr_move, TRUE);
            if (empties > wld_skill[side_to_move]) {
                reset_counter(&nodes);
                resp_move = compute_move(opponent, FALSE, player_time[opponent],
                                         player_increment[opponent], timed_search,
                                         s_use_book, skill[opponent] - 2,
                                         exact_skill[opponent] - 1,
                                         wld_skill[opponent] - 1, TRUE,
                                         &played_info1);
            }
            reset_counter(&nodes);
            resp_move = compute_move(opponent, FALSE, player_time[opponent],
                                     player_increment[opponent], timed_search,
                                     s_use_book, skill[opponent] - 1,
                                     exact_skill[opponent] - 1,
                                     wld_skill[opponent] - 1, TRUE, &played_info2);

            unmake_move(side_to_move, curr_move);

            /* Determine the 'best' move and its score. For midgame moves,
           search twice to dampen oscillations. Unless we're in the endgame
           region, a private hash transform is used - see above. */

            if (empties > wld_skill[side_to_move]) {
                set_hash_transformation(best_trans1, best_trans2);
                reset_counter(&nodes);
                curr_move =
                        compute_move(side_to_move, FALSE, player_time[side_to_move],
                                     player_increment[side_to_move], timed_search,
                                     s_use_book, skill[side_to_move] - 1,
                                     exact_skill[side_to_move], wld_skill[side_to_move],
                                     TRUE, &best_info1);
            }
            reset_counter(&nodes);
            curr_move =
                    compute_move(side_to_move, FALSE, player_time[side_to_move],
                                 player_increment[side_to_move], timed_search,
                                 s_use_book, skill[side_to_move],
                                 exact_skill[side_to_move], wld_skill[side_to_move],
                                 TRUE, &best_info2);

            /* Output the two score-move pairs */
            sprintf(output_stream, "%c%c ", TO_SQUARE(curr_move));
            if (empties <= exact_skill[side_to_move])
                sprintf(output_stream, "%+6d", best_info2.score / 128);
            else if (empties <= wld_skill[side_to_move]) {
                if (best_info2.res == WON_POSITION)
                    strcat("    +1", output_stream);
                else if (best_info2.res == LOST_POSITION)
                    strcat("    -1", output_stream);
                else
                    strcat("     0", output_stream);
            } else {
                /* If the played move is the best, output the already calculated
                   score for the best move - that way we avoid a subtle problem:
                   Suppose (N-1)-ply move is X but N-ply move is Y, where Y is
                   the best move. Then averaging the corresponding scores won't
                   coincide with the N-ply averaged score for Y. */
                if ((curr_move == provided_move[disks_played]) &&
                    (resp_move != PASS))
                    sprintf(output_stream, "%6.2f",
                            -(played_info1.score + played_info2.score) / (2 * 128.0));
                else
                    sprintf(output_stream, "%6.2f",
                            (best_info1.score + best_info2.score) / (2 * 128.0));
            }

            curr_move = provided_move[disks_played];

            sprintf(output_stream, "       %c%c ", TO_SQUARE(curr_move));
            if (resp_move == PASS)
                sprintf(output_stream, "     ?");
            else if (empties <= exact_skill[side_to_move])
                sprintf(output_stream, "%+6d", -played_info2.score / 128);
            else if (empties <= wld_skill[side_to_move]) {
                if (played_info2.res == WON_POSITION)
                    strcat("    -1", output_stream);
                else if (played_info2.res == LOST_POSITION)
                    strcat("    +1", output_stream);
                else
                    strcat("     0", output_stream);
            } else
                sprintf(output_stream, "%6.2f",
                        -(played_info1.score + played_info2.score) / (2 * 128.0));
            strcat("\n", output_stream);

            if (!valid_move(curr_move, side_to_move))
                fatal_error("Invalid move %c%c in move sequence",
                            TO_SQUARE(curr_move));

            move_stop = get_real_timer();
            if (player_time[side_to_move] != INFINIT_TIME)
                player_time[side_to_move] -= (move_stop - move_start);

            (void) make_move(side_to_move, curr_move, TRUE);
            if (side_to_move == BLACKSQ)
                black_moves[score_sheet_row] = curr_move;
            else {
                if (white_moves[score_sheet_row] != PASS)
                    score_sheet_row++;
                white_moves[score_sheet_row] = curr_move;
            }
        } else {
            if (side_to_move == BLACKSQ)
                black_moves[score_sheet_row] = PASS;
            else
                white_moves[score_sheet_row] = PASS;
        }

        side_to_move = OPP(side_to_move);
    } //END While

    if (side_to_move == BLACKSQ)
        score_sheet_row++;

    set_move_list(black_moves, white_moves, score_sheet_row);

    droidzebra_enable_messaging(TRUE);
    droidzebra_msg_analyze(output_stream);
    //TODO callback
    //display_board( stdout, board, side_to_move, TRUE, use_timer, TRUE );
    DROIDZEBRA_JNI_CLEAN

}

jobject droidzebra_RPC_callback(jint message, jobject json)
{
	DROIDZEBRA_CHECK_JNI;

	jobject out = NULL;
	jclass cls = (*s_env)->GetObjectClass(s_env, s_thiz);
	jmethodID mid = (*s_env)->GetMethodID(s_env, cls, "Callback", "(ILorg/json/JSONObject;)Lorg/json/JSONObject;");
	if (mid == 0) {
		DROIDZEBRA_JNI_BREAK;
		exit( EXIT_FAILURE ); // not reached
	}

	// create empty json object is not specified
	if(!json) {
		json = droidzebra_json_create(s_env, NULL);
		if( !json ) {
			DROIDZEBRA_JNI_BREAK;
			exit( EXIT_FAILURE ); // not reached
		}
	}

	out = (*s_env)->CallObjectMethod(s_env, s_thiz, mid, message, json);
	if ((*s_env)->ExceptionCheck(s_env)) {
		DROIDZEBRA_JNI_BREAK;
		exit( EXIT_FAILURE ); //not reached
	}
	(*s_env)->DeleteLocalRef(s_env, json);
	(*s_env)->DeleteLocalRef(s_env, cls);
	return out;
}

// enable/disable sending messsage to the java app
int droidzebra_enable_messaging(int enable)
{
	int prev = s_enable_msg;
	s_enable_msg = enable;
	return prev;
}

void droidzebra_message(int category, const char* json_str)
{
	jobject json;

	if(!s_enable_msg) return;

	DROIDZEBRA_CHECK_JNI;

	json = droidzebra_json_create(s_env, json_str);
	if( !json ) {
		fatal_error("failed to create JSON object");
		return; // not reached
	}
	json = droidzebra_RPC_callback(category, json);
	(*s_env)->DeleteLocalRef(s_env, json);
}


int droidzebra_message_debug(const char* format, ...)
{
	char errmsg[1024];

	va_list arg_ptr;
	jobject json;

	DROIDZEBRA_CHECK_JNI;

	va_start( arg_ptr, format );
    int retval = vsprintf(errmsg, format, arg_ptr);
	va_end( arg_ptr );

	json = droidzebra_json_create(s_env, NULL);
	if( !json ) {
		fatal_error("failed to create JSON object");
		return -1; // not reached
	}
	droidzebra_json_put_string(s_env, json, "message", errmsg);
	json = droidzebra_RPC_callback(MSG_DEBUG, json);
	(*s_env)->DeleteLocalRef(s_env, json);

	return retval;
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeSetPlayerInfo)(
        JNIEnv *env, jobject instance,
        jint _player,
        jint _skill,
        jint _exact_skill,
        jint _wld_skill,
        jint _time,
        jint _time_increment
)
{
	if( _player!=BLACKSQ && _player!=WHITESQ && _player!=EMPTY ) {
		char errmsg[128];
		sprintf(errmsg, "Invalid player ID: %d", _player);
		DROIDZEBRA_JNI_THROW(errmsg);
	}

	skill[_player] = _skill;
	exact_skill[_player] = _exact_skill;
	wld_skill[_player] = _wld_skill;

	player_time[_player] = _time;
	player_increment[_player] = _time_increment;
}


JNIEXPORT void JNICALL
Java_com_shurik_droidzebra_ZebraEngine_zeSetAutoMakeMoves(JNIEnv *env, jobject instance,
                                                          jint auto_make_moves) {

	auto_make_forced_moves = auto_make_moves;

}

JNIEXPORT void
JNIFn(droidzebra, ZebraEngine, zeSetSlack)(JNIEnv *env, jobject instance, jint slack)
{
	s_slack = slack;
}

JNIEXPORT void
JNIFn(droidzebra, ZebraEngine, zeSetPerturbation)(JNIEnv *env, jobject instance, jint perturbation)
{
	s_perturbation = perturbation;
}

JNIEXPORT void
JNIFn(droidzebra, ZebraEngine, zeSetHumanOpenings)(JNIEnv *env, jobject instance, jint enable)
{
	s_human_opening = enable;
}

JNIEXPORT void
JNIFn(droidzebra, ZebraEngine, zeSetPracticeMode)(JNIEnv *env, jobject instance, jint enable)
{
	s_practice_mode = enable;
}

JNIEXPORT void
JNIFn(droidzebra, ZebraEngine, zeSetUseBook)(JNIEnv *env, jobject instance, jint enable)
{
	s_use_book = enable;
}

JNIEXPORT void
JNIFn(droidzebra, ZebraEngine, zeSetForcedOpening)(JNIEnv *env, jobject instance,
                                                   jstring opening_name)
{
	int i;
	const char* str = NULL;

	s_forced_opening_seq = NULL;
	if( !opening_name ) return;
	str = (*env)->GetStringUTFChars(env, opening_name, NULL);
	if( !str ) return;
	for(i=0; i<OPENING_COUNT; i++) {
		if( strcmp(opening_list[i].name, str)==0 ) {
			s_forced_opening_seq = opening_list[i].sequence;
			break;
		}
	}
	(*env)->ReleaseStringUTFChars(env, opening_name, str);
}

JNIEXPORT jboolean
JNIFn(droidzebra, ZebraEngine, zeGameInProgress)(JNIEnv *env, jobject instance) {
    return (jboolean) (game_in_progress() ? JNI_TRUE : JNI_FALSE);
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zePlay)( JNIEnv* env, jobject thiz, jint providedMoveCount, jbyteArray providedMoves )
{
	EvaluationType eval_info;
	const char *black_name;
	const char *white_name;
	const char *opening_name;
	const char *op;
	double move_start, move_stop;
	int i;
	int side_to_move;
	int curr_move;
	int timed_search;
	int black_hash1, black_hash2, white_hash1, white_hash2;
	ui_event_t evt;
	int provided_move_count;
	int provided_move_index;
	int provided_move[65];
	int silent = FALSE;
	int use_book = s_use_book;

	DROIDZEBRA_JNI_SETUP;

	force_exit = 0;

	if(skill[BLACKSQ]<0  || skill[WHITESQ]<0 ) {
		fatal_error("Set Player Info first");
	}

	/* copy provided moves */
	provided_move_index = 0;
	provided_move_count = 0;
	if( providedMoveCount>0 && providedMoves ) {
		jbyte* providedMovesJNI;
		provided_move_count = providedMoveCount;
		i = (*env)->GetArrayLength(env, providedMoves);
		if( provided_move_count>i )
			fatal_error("Provided move count is greater than array size %d>%d", provided_move_count, i);
		if(provided_move_count>64)
			fatal_error("Provided move count is greater that 64: %d", provided_move_count);
		providedMovesJNI = (*env)->GetByteArrayElements(env, providedMoves, 0);
		if( !providedMovesJNI )
			fatal_error("failed to get provide moves (jni)");
		for(i=0; i<provided_move_count; i++) {
			provided_move[i] = providedMovesJNI[i];
		}
		(*env)->ReleaseByteArrayElements(env, providedMoves, providedMovesJNI, 0);
	}

	/* Set up the position and the search engine */
	game_init( NULL, &side_to_move );
	setup_hash( TRUE );
	clear_stored_game();
    set_slack(s_slack * 128);
    set_perturbation(s_perturbation * 128);
	toggle_human_openings( s_human_opening );
	set_forced_opening( s_forced_opening_seq );
	opening_name = NULL;

	reset_book_search();
	set_deviation_value(0, 0, 0.0);

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

	black_hash1 = my_random();
	black_hash2 = my_random();
	white_hash1 = my_random();
	white_hash2 = my_random();

	droidzebra_msg_game_start();

AGAIN:
    curr_move = PASS;
	while ( game_in_progress() && !force_exit ) {
		force_return = 0;
		silent = (provided_move_index < provided_move_count);

		droidzebra_enable_messaging(!silent);

		droidzebra_msg_move_start(side_to_move);

		remove_coeffs( disks_played );
		clear_evaluated();

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

		// echo
		droidzebra_msg_candidate_moves();
		set_move_list( black_moves, white_moves, score_sheet_row );
        set_times(player_time[BLACKSQ],
                  player_time[WHITESQ]);
		op = find_opening_name();
		if ( op != NULL && (!opening_name || strcmp(op, opening_name)) ) {
			opening_name = op;
		}
		droidzebra_msg_opening_name(opening_name);
		droidzebra_msg_last_move(disks_played>0? get_stored_move(disks_played-1) : PASS);
		display_board( stdout, board, side_to_move, TRUE, TRUE, TRUE );
		// echo

		if ( move_count[disks_played] != 0 ) {
			move_start = get_real_timer();
			clear_panic_abort();

			if ( provided_move_index >= provided_move_count ) {
				if ( skill[side_to_move] == 0 ) {
					curr_move = -1;

					if( auto_make_forced_moves && move_count[disks_played]==1 ) {
						curr_move = move_list[disks_played][0];
					} else {
						// compute evaluations
						if( s_practice_mode ) {
							_droidzebra_compute_evals(side_to_move);
							if(force_exit) break;
							if(force_return) force_return = 0; // interrupted by user input
						}

						// wait for user event
						droidzebra_msg_get_user_input( side_to_move, &evt );
						if( evt.type== UI_EVENT_EXIT ) {
							force_exit = 1;
							break;
						} else if( evt.type==UI_EVENT_MOVE ) {
							curr_move = evt.evt_move.move;
							_droidzebra_undo_stack_clear(); // once player makes the move undo info is stale
						} else if( evt.type==UI_EVENT_UNDO ) {
							_droidzebra_undo_turn(&side_to_move);
							// adjust for increment at the beginning of the game loop
							if ( side_to_move == BLACKSQ )
								score_sheet_row--;
							continue;
						} else if( evt.type==UI_EVENT_REDO ) {
							_droidzebra_redo_turn(&side_to_move);
							// adjust for increment at the beginning of the game loop
							if ( side_to_move == BLACKSQ )
								score_sheet_row--;
							continue;
						} else if( evt.type==UI_EVENT_SETTINGS_CHANGE ) {
							_droidzebra_on_settings_change();
							// repeat move on settings change
							if ( side_to_move == BLACKSQ )
								score_sheet_row--; // adjust for increment at the beginning of the game loop
							continue;
						} else {
							fatal_error("Unsupported UI event: %d", evt.type);
						}
					}
					assert(curr_move>=0);
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
				}
			} else {
				curr_move = provided_move[provided_move_index];
				if (!valid_move(curr_move, side_to_move)) {
					critical_error("Invalid move %c%c in move sequence", TO_SQUARE(curr_move));
					force_exit = 1;
					break;
				}
			}

			move_stop = get_real_timer();
			if ( player_time[side_to_move] != INFINIT_TIME )
				player_time[side_to_move] -= (move_stop - move_start);

			store_move( disks_played, curr_move );

			(void) make_move( side_to_move, curr_move, TRUE );
			if ( side_to_move == BLACKSQ )
				black_moves[score_sheet_row] = curr_move;
			else {
				white_moves[score_sheet_row] = curr_move;
			}
		}
		else {
			//if we still have moves to make continue and switch sides
			if (provided_move_index < provided_move_count) {
				side_to_move = OPP(side_to_move);
				continue;
			}
			// this is where we pass
			if ( side_to_move == BLACKSQ )
				black_moves[score_sheet_row] = PASS;
			else
				white_moves[score_sheet_row] = PASS;
			if ( !auto_make_forced_moves && skill[side_to_move] == 0 ) {
				droidzebra_msg_pass();
			}
		}

		droidzebra_msg_move_end(side_to_move);

		side_to_move = OPP( side_to_move );

		provided_move_index ++;
	}

	droidzebra_enable_messaging(TRUE);

	if ( side_to_move == BLACKSQ )
		score_sheet_row++;

	set_move_list( black_moves, white_moves, score_sheet_row );
    set_times(player_time[BLACKSQ], player_time[WHITESQ]);
	droidzebra_msg_opening_name(opening_name);
	display_board( stdout, board, side_to_move, TRUE, TRUE, TRUE );

	/*
	double node_val, eval_val;
	adjust_counter( &total_nodes );
	node_val = counter_value( &total_nodes );
	adjust_counter( &total_evaluations );
	eval_val = counter_value( &total_evaluations );
	printf( "\nBlack: %d   White: %d\n", disc_count( BLACKSQ ),
			disc_count( WHITESQ ) );
	printf( "Nodes searched:        %-10.0f\n", node_val );

	printf( "Positions evaluated:   %-10.0f\n", eval_val );

	printf( "Total time: %.1f s\n", total_time );
	*/

	if( !force_exit )
		droidzebra_msg_game_over();

	// loop here until we are told to exit so the user has a chance to undo
	while( !force_exit ) {
		droidzebra_msg_get_user_input( side_to_move, &evt );
		if( evt.type== UI_EVENT_EXIT ) {
			force_exit = 1;
			break;
		} else if( evt.type==UI_EVENT_UNDO ) {
			_droidzebra_undo_turn(&side_to_move);
			// adjust for increment at the beginning of the game loop
			if ( side_to_move == BLACKSQ )
				score_sheet_row--;
			goto AGAIN;
		} else if( evt.type==UI_EVENT_SETTINGS_CHANGE ) {
			_droidzebra_on_settings_change();
		}
	}

	DROIDZEBRA_JNI_CLEAN;
}

// undo moves until player is a human and he can make a move
void _droidzebra_undo_turn(int* side_to_move)
{
	int human_can_move = 0;
	int curr_move;

	if(score_sheet_row==0 && *side_to_move==BLACKSQ) return;

	_droidzebra_undo_stack_push(disks_played);

	do {
		*side_to_move = OPP(*side_to_move);

		if ( *side_to_move == WHITESQ )
			score_sheet_row--;

		human_can_move =
				skill[*side_to_move]==0 &&
				!(
					(auto_make_forced_moves && move_count[disks_played-1]==1)
					|| (*side_to_move==WHITESQ && white_moves[score_sheet_row]==PASS)
					|| (*side_to_move==BLACKSQ && black_moves[score_sheet_row]==PASS)
				);

		if ( *side_to_move == WHITESQ ) {
			curr_move = white_moves[score_sheet_row];
			if(white_moves[score_sheet_row]!=PASS)
				unmake_move(WHITESQ, white_moves[score_sheet_row] );
			white_moves[score_sheet_row] = PASS;
		} else {
			curr_move = black_moves[score_sheet_row];
			if(black_moves[score_sheet_row]!=PASS)
				unmake_move(BLACKSQ, black_moves[score_sheet_row] );
			black_moves[score_sheet_row] = PASS;
		}

		droidzebra_message_debug("undo: side_to_move %d, undo_move %d, score_sheet_row %d, disks_played %d, move_count %d", *side_to_move, curr_move, score_sheet_row, disks_played, move_count[disks_played]);
	} while( !(score_sheet_row==0 && *side_to_move==BLACKSQ) && !human_can_move );
	clear_endgame_performed();
}

void _droidzebra_redo_turn(int* side_to_move)
{
	int target_disks_played = 0;
	int curr_move;

	if(!_droidzebra_can_redo()) return;

	target_disks_played = _droidzebra_undo_stack_pop();
	droidzebra_message_debug("redo: score_sheet_row %d, disks_played %d, new_disks_played %d", score_sheet_row, disks_played, target_disks_played);
	while(disks_played<target_disks_played) {
		generate_all( *side_to_move );

		if( move_count[disks_played]>0 ) {
			curr_move = get_stored_move(disks_played);
			if( curr_move==ILLEGAL || !valid_move(curr_move, *side_to_move) ) {
				fatal_error( "Invalid move %c%c in redo sequence", TO_SQUARE( curr_move ));
			}
			(void) make_move( *side_to_move, curr_move, TRUE );
		} else {
			curr_move = PASS;
		}

		droidzebra_message_debug("redo: score_sheet_row %d, curr_move %d, side_to_move %d, disks_played %d", score_sheet_row, curr_move, *side_to_move, disks_played);

		if ( *side_to_move == BLACKSQ )
			black_moves[score_sheet_row] = curr_move;
		else {
			white_moves[score_sheet_row] = curr_move;
		}

		*side_to_move = OPP(*side_to_move);

		if ( *side_to_move == BLACKSQ )
			score_sheet_row++;
	}
}

void _droidzebra_on_settings_change(void)
{
    set_slack(s_slack * 128);
    set_perturbation(s_perturbation * 128);
	toggle_human_openings( s_human_opening );
	set_forced_opening( s_forced_opening_seq );
}

void _droidzebra_compute_evals(int side_to_move)
{
	int stored_pv[sizeof(full_pv)/sizeof(full_pv[0])];
	int stored_pv_depth = 0;

	set_slack(0);
	set_perturbation(0);
	toggle_human_openings(FALSE);
	set_forced_opening( NULL );
	memcpy(stored_pv, full_pv, sizeof(stored_pv));
	stored_pv_depth = full_pv_depth;

	extended_compute_move(side_to_move, FALSE, TRUE, skill[EMPTY], exact_skill[EMPTY], wld_skill[EMPTY]);

	memcpy(full_pv, stored_pv, sizeof(full_pv));
	full_pv_depth = stored_pv_depth;
    set_slack(s_slack * 128);
    set_perturbation(s_perturbation * 128);
	toggle_human_openings( s_human_opening );
	set_forced_opening( s_forced_opening_seq );

	display_status(stdout, FALSE);
}

void _droidzebra_throw_engine_error(JNIEnv* env, const char* msg)
{
    jclass exc = (*env)->FindClass(env, "com/shurik/droidzebra/EngineError");
    if(exc) (*env)->ThrowNew(env, exc, msg);
}

void _droidzebra_undo_stack_push(int val)
{
	assert(s_undo_stack_pointer<64);
	s_undo_stack[s_undo_stack_pointer++] = val;
}

int _droidzebra_undo_stack_pop(void)
{
	assert(s_undo_stack_pointer>0);
	return s_undo_stack[--s_undo_stack_pointer];
}

void _droidzebra_undo_stack_clear(void)
{
	s_undo_stack_pointer = 0;
}

int _droidzebra_can_redo(void)
{
	return s_undo_stack_pointer>0;
}
