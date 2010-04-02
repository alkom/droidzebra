#ifndef __DROIDZEBRA__
#define __DROIDZEBRA__

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


int droidzebra_message_debug(const char* format, ...);
void droidzebra_message(int category, const char* json_str);

#endif

