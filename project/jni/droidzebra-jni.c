/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
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
#define DEFAULT_SLACK             0.25
#define DEFAULT_PERTURBATION      0
#define DEFAULT_WLD_ONLY          FALSE
#define SEPARATE_TABLES           FALSE
#define INFINIT_TIME              10000000.0

#define USE_LOG					  FALSE

// --
static double player_time[3], player_increment[3];
static int skill[3];
static int wld_skill[3], exact_skill[3];
static int force_exit = 0;
static int auto_make_forced_moves = 0;
static float s_slack = DEFAULT_SLACK;
static float s_perturbation = DEFAULT_PERTURBATION;
static int s_human_opening = FALSE;
static int s_practice_mode = FALSE;
static const char* s_forced_opening_seq = NULL;
static int s_use_book = TRUE;
static int s_enable_msg = TRUE;
// --

#define JNIFn(Package, Class, Fname) JNICALL Java_com_shurik_##Package##_##Class##_##Fname

static JNIEnv* s_env = NULL;
static jobject s_thiz = NULL;
static jmp_buf s_err_jmp;
#define DROIDZEBRA_JNI_SETUP if( setjmp(s_err_jmp) ) return; \
		s_env = env; \
		s_thiz = thiz;
#define DROIDZEBRA_JNI_BREAK longjmp(s_err_jmp, -1);
#define DROIDZEBRA_CHECK_JNI { assert(s_env!=NULL); if(!s_env) exit( EXIT_FAILURE ); }

char android_files_dir[256];

static void _droidzebra_undo_turn(int* side_to_move);
static void _droidzebra_on_settings_change(void);
static void _droidzebra_compute_evals(int side_to_move);

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
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeGlobalInit)( 
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
	player_increment[BLACKSQ] = player_increment[WHITESQ] = 0.0;


	const char* str;
	str = (*env)->GetStringUTFChars(env, files_dir, NULL);
	if (str == NULL) {
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
	my_srandom(timer);
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeGlobalTerminate)( JNIEnv* env, jobject thiz )
{
	DROIDZEBRA_JNI_SETUP;

	global_terminate();

	s_env = NULL;
	s_thiz = NULL;
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeForceReturn)(JNIEnv* env, jobject thiz)
{
	force_return = 1;
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeForceExit)(JNIEnv* env, jobject thiz)
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
	int retval = -1;

	va_list arg_ptr;
	jobject json;

	DROIDZEBRA_CHECK_JNI;

	va_start( arg_ptr, format );
	retval = vsprintf( errmsg, format, arg_ptr );
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
		JNIEnv* env,
		jobject thiz,
		jint _player,
		jint _skill,
		jint _exact_skill,
		jint _wld_skill,
		jint _time,
		jint _time_increment
)
{
	DROIDZEBRA_JNI_SETUP;

	if( _player!=BLACKSQ && _player!=WHITESQ && _player!=EMPTY ) {
		fatal_error("Invalid player ID: %d", _player);
	}

	skill[_player] = _skill;
	exact_skill[_player] = _exact_skill;
	wld_skill[_player] = _wld_skill;

	player_time[_player] = _time;
	player_increment[_player] = _time_increment;
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeSetAutoMakeMoves)( JNIEnv* env, jobject thiz, int _auto_make_moves )
{
	auto_make_forced_moves = _auto_make_moves;
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeSetSlack)( JNIEnv* env, jobject thiz, jfloat slack )
{
	s_slack = slack;
	//set_slack( floor( s_slack * 128.0 ) );
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeSetPerturbation)( JNIEnv* env, jobject thiz, jfloat perturbation )
{
	s_perturbation = perturbation;
	//set_perturbation( floor( s_perturbation * 128.0 ) );
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeSetHumanOpenings)( JNIEnv* env, jobject thiz, int enable )
{
	s_human_opening = enable;
	//toggle_human_openings(s_human_opening);
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeSetPracticeMode)( JNIEnv* env, jobject thiz, int enable )
{
	s_practice_mode = enable;
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeSetUseBook)( JNIEnv* env, jobject thiz, int enable )
{
	s_use_book = enable;
}

JNIEXPORT void
JNIFn(droidzebra,ZebraEngine,zeSetForcedOpening)( JNIEnv* env, jobject thiz, jobject opening_name )
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
JNIFn(droidzebra,ZebraEngine,zeGameInProgress)( JNIEnv* env, jobject thiz )
{
	return game_in_progress()? JNI_TRUE : JNI_FALSE;
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
	set_slack( floor( s_slack * 128.0 ) );
	set_perturbation( floor( s_perturbation * 128.0 ) );
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
		set_times( floor( player_time[BLACKSQ] ),
				floor( player_time[WHITESQ] ) );
		op = find_opening_name();
		if ( op != NULL && (!opening_name || strcmp(op, opening_name)) ) {
			opening_name = op;
		}
		droidzebra_msg_opening_name(opening_name);
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
						} else if( evt.type==UI_EVENT_UNDO ) {
							_droidzebra_undo_turn(&side_to_move);
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
				if ( !valid_move( curr_move, side_to_move ) )
					fatal_error( "Invalid move %c%c in move sequence", TO_SQUARE( curr_move ));
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

			droidzebra_msg_last_move(curr_move);
		}
		else {
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
	set_times( floor( player_time[BLACKSQ] ), floor( player_time[WHITESQ] ) );
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
}
// undo moves until player is a human and he can make a move
void _droidzebra_undo_turn(int* side_to_move)
{
	int human_can_move = 0;

	if(score_sheet_row==0 && *side_to_move==BLACKSQ) return;

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
			if(white_moves[score_sheet_row]!=PASS)
				unmake_move(WHITESQ, white_moves[score_sheet_row] );
			white_moves[score_sheet_row] = PASS;
		} else {
			if(black_moves[score_sheet_row]!=PASS)
				unmake_move(BLACKSQ, black_moves[score_sheet_row] );
			black_moves[score_sheet_row] = PASS;
		}

		droidzebra_message_debug("undo: score_sheet_row %d, disks_played %d, move_count %d", score_sheet_row, disks_played, move_count[disks_played]);
	} while( !(score_sheet_row==0 && *side_to_move==BLACKSQ) && !human_can_move );
	clear_endgame_performed();
}

void _droidzebra_on_settings_change(void)
{
	set_slack( floor( s_slack * 128.0 ) );
	set_perturbation( floor( s_perturbation * 128.0 ) );
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
	set_slack( floor( s_slack * 128.0 ) );
	set_perturbation( floor( s_perturbation * 128.0 ) );
	toggle_human_openings( s_human_opening );
	set_forced_opening( s_forced_opening_seq );

	display_status(stdout, FALSE);
}
