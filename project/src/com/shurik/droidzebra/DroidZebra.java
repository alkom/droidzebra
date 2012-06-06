/* Copyright (C) 2012 by Alex Kompel  */
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

package com.shurik.droidzebra;

import com.shurik.droidzebra.ZebraEngine.CandidateMove;
import com.shurik.droidzebra.ZebraEngine.Move;
import com.shurik.droidzebra.ZebraEngine.PlayerInfo;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
//import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;

public class DroidZebra extends Activity
implements SharedPreferences.OnSharedPreferenceChangeListener
{
	public static final String SHARED_PREFS_NAME="droidzebrasettings";

	final public static int boardSize = 8;

	private static final int
	MENU_NEW_GAME = 1,
	MENU_QUIT = 2,
	MENU_TAKE_BACK = 3,
	MENU_SETTINGS = 4,
	MENU_SWITCH_SIDES = 5,
	MENU_DONATE = 6
	;

	private static final int
	DIALOG_PASS_ID = 1,
	DIALOG_GAME_OVER = 2,
	DIALOG_QUIT = 3,
	DIALOG_BUSY = 4,
	DIALOG_DONATE = 5
	;

	private static final int
	FUNCTION_HUMAN_VS_HUMAN = 0,
	FUNCTION_ZEBRA_WHITE = 1,
	FUNCTION_ZEBRA_BLACK = 2,
	FUNCTION_ZEBRA_VS_ZEBRA = 3;

	private static final int
	RANDOMNESS_NONE = 0,
	RANDOMNESS_SMALL = 1,
	RANDOMNESS_MEDIUM = 2,
	RANDOMNESS_LARGE = 3,
	RANDOMNESS_HUGE = 4;
	
	public ZebraEngine mZebraThread;

	public static final int DEFAULT_SETTING_FUNCTION = FUNCTION_ZEBRA_WHITE;
	public static final String DEFAULT_SETTING_STRENGTH  = "1|1|1";
	public static final boolean DEFAULT_SETTING_AUTO_MAKE_FORCED_MOVES  = false;
	public static final int DEFAULT_SETTING_RANDOMNESS = RANDOMNESS_LARGE;
	public static final String DEFAULT_SETTING_FORCE_OPENING = "None";
	public static final boolean DEFAULT_SETTING_HUMAN_OPENINGS = false;
	public static final boolean DEFAULT_SETTING_PRACTICE_MODE = false;
	public static final boolean DEFAULT_SETTING_USE_BOOK = true;
	public static final boolean DEFAULT_SETTING_DISPLAY_PV = true;
	public static final boolean DEFAULT_SETTING_DISPLAY_MOVES = true;
	public static final boolean DEFAULT_SETTING_DISPLAY_LAST_MOVE = true;

	public static final String 
	SETTINGS_KEY_FUNCTION = "settings_engine_function",
	SETTINGS_KEY_STRENGTH = "settings_engine_strength",
	SETTINGS_KEY_AUTO_MAKE_FORCED_MOVES = "settings_engine_auto_make_moves",
	SETTINGS_KEY_RANDOMNESS = "settings_engine_randomness",
	SETTINGS_KEY_FORCE_OPENING = "settings_engine_force_opening",
	SETTINGS_KEY_HUMAN_OPENINGS = "settings_engine_human_openings",
	SETTINGS_KEY_PRACTICE_MODE = "settings_engine_practice_mode",
	SETTINGS_KEY_USE_BOOK = "settings_engine_use_book",
	SETTINGS_KEY_DISPLAY_PV = "settings_ui_display_pv",
	SETTINGS_KEY_DISPLAY_MOVES = "settings_ui_display_moves",
	SETTINGS_KEY_DISPLAY_LAST_MOVE = "settings_ui_display_last_move"
	;

	private byte mBoard[][] = new byte[boardSize][boardSize];

	private CandidateMove[] mCandidateMoves = null;

	private Move mLastMove = null;
	private int mWhiteScore = 0;
	private int mBlackScore = 0;
	private String mOpeningName;

	private BoardView mBoardView;
	private StatusView mStatusView;
	
	private boolean mBusyDialogUp = false;
	
	private SharedPreferences mSettings;
	public int mSettingFunction = DEFAULT_SETTING_FUNCTION;
	private int mSettingZebraDepth = 1;
	private int mSettingZebraDepthExact = 1;
	private int mSettingZebraDepthWLD = 1;
	public boolean mSettingAutoMakeForcedMoves = DEFAULT_SETTING_AUTO_MAKE_FORCED_MOVES;
	public int mSettingZebraRandomness = DEFAULT_SETTING_RANDOMNESS;
	public String mSettingZebraForceOpening = DEFAULT_SETTING_FORCE_OPENING;
	public boolean mSettingZebraHumanOpenings = DEFAULT_SETTING_HUMAN_OPENINGS;
	public boolean mSettingZebraPracticeMode = DEFAULT_SETTING_PRACTICE_MODE;
	public boolean mSettingZebraUseBook = DEFAULT_SETTING_USE_BOOK;
	public boolean mSettingDisplayPV = DEFAULT_SETTING_DISPLAY_PV;
	public boolean mSettingDisplayMoves = DEFAULT_SETTING_DISPLAY_MOVES;
	public boolean mSettingDisplayLastMove = DEFAULT_SETTING_DISPLAY_LAST_MOVE;

	private void newCompletionPort(final int zebraEngineStatus, final Runnable completion) {
		new AsyncTask<Void, Void, Void>() {
			@Override 
			protected Void doInBackground(Void... p) {
				mZebraThread.waitForEngineState(zebraEngineStatus);
				return null;
			}
			@Override
			protected void onPostExecute(Void result) {
				completion.run();
			}
		}
		.execute();
	}

	public DroidZebra() {
		initBoard();
	}

	public byte[][] getBoard() {
		return mBoard;
	}

	public void initBoard() {
		mCandidateMoves = null;
		mLastMove = null;
		mWhiteScore = mBlackScore = 0;
		for(int i=0; i<boardSize; i++)
			for(int j=0; j<boardSize; j++)
				mBoard[i][j] = ZebraEngine.PLAYER_EMPTY;
		if( mStatusView!=null ) 
			mStatusView.clear();
	}

	public void setBoard(byte[] board) {
		for(int i=0; i<boardSize; i++)
			for(int j=0; j<boardSize; j++)
				mBoard[i][j] = board[i*boardSize + j];
		mBoardView.invalidate();
	}

	public CandidateMove[] getCandidateMoves() {
		return mCandidateMoves;
	}

	public void setCandidateMoves(CandidateMove[] cmoves) {
		mCandidateMoves = cmoves;
		mBoardView.invalidate();
	}

	public void setLastMove(Move move) {
		mLastMove = move;
	}

	public Move getLastMove() {
		return mLastMove;

	}

	public boolean isValidMove(Move move) {
		if( mCandidateMoves==null ) 
			return false;
		for(CandidateMove m:mCandidateMoves) {
			if(m.mMove.getX()==move.getX() && m.mMove.getY()==move.getY() ) return true;
		}
		return false;
	}

	public void newGame() {
		if(mZebraThread.getEngineState()!=ZebraEngine.ES_READY2PLAY) {
			mZebraThread.stopGame();
		}
		newCompletionPort(
				ZebraEngine.ES_READY2PLAY,
				new Runnable() {
					@Override public void run() {
						DroidZebra.this.initBoard();
						DroidZebra.this.loadSettings();
						DroidZebra.this.mZebraThread.setEngineState(ZebraEngine.ES_PLAY);                
					}
				}
		);
	}

	public void busyDialog() {
		if( !mBusyDialogUp && mZebraThread.isThinking() ) {
			mBusyDialogUp = true;
			showDialog(DIALOG_BUSY);
		}
	}
	
	class DroidZebraHandler extends Handler {
		@Override
		public void handleMessage(Message m) {
			// block messages if waiting for something
			switch(m.what) {
			case ZebraEngine.MSG_ERROR: {
				FatalError(m.getData().getString("error"));
			} break;
			case ZebraEngine.MSG_BOARD: {
				String score;
				int sideToMove = m.getData().getInt("side_to_move");
				
				setBoard(m.getData().getByteArray("board"));
				
				mBlackScore = m.getData().getBundle("black").getInt("disc_count");
				mWhiteScore = m.getData().getBundle("white").getInt("disc_count");
				
				if(sideToMove==ZebraEngine.PLAYER_BLACK)
					score = String.format("•%d", mBlackScore);
				else
					score = String.format("%d", mBlackScore);
				mStatusView.setTextForID(
					StatusView.ID_SCORE_BLACK, 
					score
				);

				if(sideToMove==ZebraEngine.PLAYER_WHITE)
					score = String.format("•%d", mWhiteScore);
				else
					score = String.format("%d", mWhiteScore);
				mStatusView.setTextForID(
						StatusView.ID_SCORE_WHITE,
						score
					);
				
				int iStart, iEnd;
				byte[] black_moves = m.getData().getBundle("black").getByteArray("moves");
				byte[] white_moves = m.getData().getBundle("white").getByteArray("moves");
				
				iEnd = black_moves.length;
				iStart = Math.max(0, iEnd-4);
				for( int i=0; i<4; i++ ) {
					String num_text = String.format("%d", i+iStart+1);
					String move_text;
					if( i+iStart<iEnd ) {
						Move move = new Move(black_moves[i+iStart]);
						move_text = move.getText();
					} else {
						move_text = "";
					}
					mStatusView.setTextForID(
							StatusView.ID_SCORELINE_NUM_1 + i,
							num_text
						);
					mStatusView.setTextForID(
							StatusView.ID_SCORELINE_BLACK_1 + i,
							move_text
						);
				}

				iEnd = white_moves.length;
				iStart = Math.max(0, iEnd-4);
				for( int i=0; i<4; i++ ) {
					String move_text;
					if( i+iStart<iEnd ) {
						Move move = new Move(white_moves[i+iStart]);
						move_text = move.getText();
					} else {
						move_text = "";
					}
					mStatusView.setTextForID(
							StatusView.ID_SCORELINE_WHITE_1 + i,
							move_text
						);
				}
			} break;
			
			case ZebraEngine.MSG_CANDIDATE_MOVES: {
				setCandidateMoves((CandidateMove[]) m.obj);
			} break;
			
			case ZebraEngine.MSG_PASS: {
				showDialog(DIALOG_PASS_ID);
			} break;
			
			case ZebraEngine.MSG_OPENING_NAME: {
				mOpeningName = m.getData().getString("opening");
				mStatusView.setTextForID(
						StatusView.ID_STATUS_OPENING, 
						mOpeningName
					);
			} break;
			
			case ZebraEngine.MSG_LAST_MOVE: {
				byte move =  (byte)m.getData().getInt("move");
				setLastMove(new Move(move));
			} break;
			
			case ZebraEngine.MSG_GAME_OVER: {
				showDialog(DIALOG_GAME_OVER);
			} break;
			
			case ZebraEngine.MSG_EVAL_TEXT: {
				mStatusView.setTextForID(
						StatusView.ID_STATUS_EVAL, 
						m.getData().getString("eval")
					);
			} break;
			
			case ZebraEngine.MSG_PV: {
				if( mSettingDisplayPV ) {
					byte[] pv = m.getData().getByteArray("pv");
					String pvText = new String();
					for( byte move:pv ) {
						pvText += new Move(move).getText();
						pvText += " ";
					}
					mStatusView.setTextForID(
							StatusView.ID_STATUS_PV, 
							pvText
						);
				}
			} break;
			
			case ZebraEngine.MSG_MOVE_END: {
				if( mBusyDialogUp ) {
					dismissDialog(DIALOG_BUSY);
					mBusyDialogUp = false;
				}
			} break;
			
			case ZebraEngine.MSG_CANDIDATE_EVALS: {
				CandidateMove[] evals = (CandidateMove[]) m.obj;
				for(CandidateMove eval:evals) {
					for(int i=0; i<mCandidateMoves.length; i++) {
						if(mCandidateMoves[i].mMove.mMove == eval.mMove.mMove) {
							mCandidateMoves[i] = eval;
							break;
						}
					}
				}
				mBoardView.invalidate();
			} break;
			
			case ZebraEngine.MSG_DEBUG: {
				//Log.d("DroidZebra", m.getData().getString("message"));
			} break;
			}
		}
	}
	final private DroidZebraHandler mDroidZebraHandler = new DroidZebraHandler();
	
	/* Creates the menu items */
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		menu.add(0, MENU_NEW_GAME, 0, R.string.menu_item_new_game).setIcon(R.drawable.ic_menu_play);
		menu.add(0, MENU_SWITCH_SIDES, 0, R.string.menu_item_switch_sides).setIcon(R.drawable.ic_menu_switch_sides);
		menu.add(0, MENU_TAKE_BACK, 0, R.string.menu_item_undo).setIcon(android.R.drawable.ic_menu_revert);
		menu.add(0, MENU_SETTINGS, 0, R.string.menu_item_settings).setIcon(android.R.drawable.ic_menu_preferences);
		menu.add(0, MENU_DONATE, 0, R.string.menu_item_donate).setIcon(android.R.drawable.ic_menu_send);
		menu.add(0, MENU_QUIT, 0, R.string.menu_item_quit).setIcon(android.R.drawable.ic_menu_close_clear_cancel);
		return true;
	}	

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		try {
			switch (item.getItemId()) {
			case MENU_NEW_GAME:
				newGame();
				return true;
			case MENU_QUIT:
				showDialog(DIALOG_QUIT);
				return true;
			case MENU_TAKE_BACK:
				mZebraThread.undoMove();
				return true;
			case MENU_SETTINGS: {
				// Launch Preference activity
				Intent i = new Intent(this, SettingsPreferences.class);
				startActivity(i);
			} return true;
			case MENU_SWITCH_SIDES: {
				switchSides();
			} break;
			case MENU_DONATE: {
				showDialog(DIALOG_DONATE);
			} return true;
			}
		} catch (EngineError e) {
			FatalError(e.msg);
		}
		return false;
	}

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		setContentView(R.layout.spash_layout);

		// start your engines
		mZebraThread = new ZebraEngine(this, mDroidZebraHandler);

		// preferences
		mSettings = getSharedPreferences(SHARED_PREFS_NAME, 0);
		mSettings.registerOnSharedPreferenceChangeListener(this);

		if( savedInstanceState != null 
			&& savedInstanceState.containsKey("moves_played_count") ) {
			mZebraThread.setInitialGameState(savedInstanceState.getInt("moves_played_count"), savedInstanceState.getByteArray("moves_played"));
		} 
		/*else {
			//byte[] init = {56,66,65,46,35,64,47,34,33,36,57,24,43,25,37,23,63,26,16,15,14,13,12,53, 52, 62, 75, 41, 42, 74, 51, 31, 32, 61, 83, 84, 73, 82, 17, 21, 72, 68, 58, 85, 76, 67, 86, 87, 78, 38, 48, 88, 27, 77 };
			byte[] init = {
					65 , 46 , 35 , 64 , 53 , 36 , 56 , 24 , 27 , 34 , 26 , 43 , 33 , 25 , 47 , 18 ,
					15 , 14 , 37 , 16 , 17 , 62 , 23 , 52 , 13 , 66 , 74 , 12 , 63 , 42 , 32 , 41 ,
					31 , 51 , 22 , 21 , 72 , 11 , 67 , 28 , 38 , 58 , 48 , 61 , 68 , 57 , -1 , 71 ,
					-1 , 81 , 82 , 78 , -1 , 77 , -1 , 83 , 84 , 73 , 75 , 86 , 76 , 87 
			};
			mZebraThread.setInitialGameState(init.length, init);
		}*/
		
		mZebraThread.start();

		newCompletionPort(
				ZebraEngine.ES_READY2PLAY,
				new Runnable() {
					@Override public void run() {
						DroidZebra.this.setContentView(R.layout.board_layout);
						DroidZebra.this.mBoardView = (BoardView)DroidZebra.this.findViewById(R.id.board);
						DroidZebra.this.mStatusView  = (StatusView)DroidZebra.this.findViewById(R.id.status_panel);
						DroidZebra.this.mBoardView.setDroidZebra(DroidZebra.this);
						DroidZebra.this.mBoardView.requestFocus();
						DroidZebra.this.initBoard();
						DroidZebra.this.loadSettings();
						DroidZebra.this.mZebraThread.setEngineState(ZebraEngine.ES_PLAY);                
					}
				}
		);
	}

	private void loadSettings() {
		int settingsFunction, settingZebraDepth, settingZebraDepthExact, settingZebraDepthWLD;
		int settingRandomness;
		boolean settingAutoMakeForcedMoves;
		String settingZebraForceOpening;
		boolean settingZebraHumanOpenings;
		boolean settingZebraPracticeMode;
		boolean settingZebraUseBook;
		
		SharedPreferences settings = getSharedPreferences(SHARED_PREFS_NAME, 0);

		settingsFunction = Integer.parseInt(settings.getString(SETTINGS_KEY_FUNCTION, String.format("%d", DEFAULT_SETTING_FUNCTION)));
		String[] strength = settings.getString(SETTINGS_KEY_STRENGTH, DEFAULT_SETTING_STRENGTH).split("\\|");
		// Log.d("DroidZebra", String.format("settings %s:%s|%s|%s", SETTINGS_KEY_STRENGTH, strength[0], strength[1], strength[2]));

		settingZebraDepth = Integer.parseInt(strength[0]);
		settingZebraDepthExact = Integer.parseInt(strength[1]);
		settingZebraDepthWLD = Integer.parseInt(strength[2]);
		//Log.d( "DroidZebra", 
		//		String.format("Function: %d; depth: %d; exact: %d; wld %d",
		//				mSettingFunction, mSettingZebraDepth, mSettingZebraDepthExact, mSettingZebraDepthWLD)
		//);

		settingAutoMakeForcedMoves = settings.getBoolean(SETTINGS_KEY_AUTO_MAKE_FORCED_MOVES, DEFAULT_SETTING_AUTO_MAKE_FORCED_MOVES);
		settingRandomness = Integer.parseInt(settings.getString(SETTINGS_KEY_RANDOMNESS, String.format("%d", DEFAULT_SETTING_RANDOMNESS)));
		settingZebraForceOpening = settings.getString(SETTINGS_KEY_FORCE_OPENING, DEFAULT_SETTING_FORCE_OPENING);
		settingZebraHumanOpenings = settings.getBoolean(SETTINGS_KEY_HUMAN_OPENINGS, DEFAULT_SETTING_HUMAN_OPENINGS);
		settingZebraPracticeMode = settings.getBoolean(SETTINGS_KEY_PRACTICE_MODE, DEFAULT_SETTING_PRACTICE_MODE);
		settingZebraUseBook = settings.getBoolean(SETTINGS_KEY_USE_BOOK, DEFAULT_SETTING_USE_BOOK);

		
		boolean bZebraSettingChanged = (
			mSettingFunction != settingsFunction 
			|| mSettingZebraDepth != settingZebraDepth
			|| mSettingZebraDepthExact != settingZebraDepthExact
			|| mSettingZebraDepthWLD != settingZebraDepthWLD
			|| mSettingAutoMakeForcedMoves != settingAutoMakeForcedMoves
			|| mSettingZebraRandomness != settingRandomness
			|| mSettingZebraForceOpening != settingZebraForceOpening
			|| mSettingZebraHumanOpenings != settingZebraHumanOpenings
			|| mSettingZebraPracticeMode != settingZebraPracticeMode
			|| mSettingZebraUseBook != settingZebraUseBook
			);
		
		mSettingFunction = settingsFunction;
		mSettingZebraDepth = settingZebraDepth;
		mSettingZebraDepthExact = settingZebraDepthExact;
		mSettingZebraDepthWLD = settingZebraDepthWLD;
		mSettingAutoMakeForcedMoves = settingAutoMakeForcedMoves;
		mSettingZebraRandomness = settingRandomness;
		mSettingZebraForceOpening = settingZebraForceOpening;
		mSettingZebraHumanOpenings = settingZebraHumanOpenings;
		mSettingZebraPracticeMode = settingZebraPracticeMode;
		mSettingZebraUseBook = settingZebraUseBook;
		
		try {
			mZebraThread.setAutoMakeMoves(mSettingAutoMakeForcedMoves);
			mZebraThread.setForcedOpening(mSettingZebraForceOpening);
			mZebraThread.setHumanOpenings(mSettingZebraHumanOpenings);
			mZebraThread.setPracticeMode(mSettingZebraPracticeMode);
			mZebraThread.setUseBook(mSettingZebraUseBook);
			
			switch( mSettingFunction ) {
			case FUNCTION_HUMAN_VS_HUMAN:
				mZebraThread.setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_BLACK, 0, 0, 0, ZebraEngine.INFINIT_TIME, 0));
				mZebraThread.setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_WHITE, 0, 0, 0, ZebraEngine.INFINIT_TIME, 0));
				break;
			case FUNCTION_ZEBRA_BLACK:
				mZebraThread.setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_BLACK, mSettingZebraDepth, mSettingZebraDepthExact, mSettingZebraDepthWLD, ZebraEngine.INFINIT_TIME, 0));
				mZebraThread.setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_WHITE, 0, 0, 0, ZebraEngine.INFINIT_TIME, 0));
				break;
			case FUNCTION_ZEBRA_VS_ZEBRA:
				mZebraThread.setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_BLACK, mSettingZebraDepth, mSettingZebraDepthExact, mSettingZebraDepthWLD, ZebraEngine.INFINIT_TIME, 0));
				mZebraThread.setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_WHITE, mSettingZebraDepth, mSettingZebraDepthExact, mSettingZebraDepthWLD, ZebraEngine.INFINIT_TIME, 0));
				break;
			case FUNCTION_ZEBRA_WHITE:
			default:
				mZebraThread.setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_BLACK, 0, 0, 0, ZebraEngine.INFINIT_TIME, 0));
				mZebraThread.setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_WHITE, mSettingZebraDepth, mSettingZebraDepthExact, mSettingZebraDepthWLD, ZebraEngine.INFINIT_TIME, 0));
				break;
			}
			mZebraThread.setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_ZEBRA, mSettingZebraDepth+1, mSettingZebraDepthExact+1, mSettingZebraDepthWLD+1, ZebraEngine.INFINIT_TIME, 0));
			
			switch(mSettingZebraRandomness) {
			case RANDOMNESS_SMALL:
				mZebraThread.setSlack(1.5f);
				mZebraThread.setPerturbation(1.0f);
				break;
			case RANDOMNESS_MEDIUM:
				mZebraThread.setSlack(4.0f);
				mZebraThread.setPerturbation(2.5f);
				break;
			case RANDOMNESS_LARGE:
				mZebraThread.setSlack(6.0f);
				mZebraThread.setPerturbation(6.0f);
				break;
			case RANDOMNESS_HUGE:
				mZebraThread.setSlack(10.0f);
				mZebraThread.setPerturbation(16.0f);
				break;
			case RANDOMNESS_NONE:
			default:
				mZebraThread.setSlack(0.0f);
				mZebraThread.setPerturbation(0.0f);
				break;
			}
		} catch (EngineError e) {
			FatalError(e.msg);
		}
		
		mStatusView.setTextForID(
			StatusView.ID_SCORE_SKILL, 
			String.format(getString(R.string.display_depth), mSettingZebraDepth, mSettingZebraDepthExact, mSettingZebraDepthWLD)
			);

		mSettingDisplayPV = settings.getBoolean(SETTINGS_KEY_DISPLAY_PV, DEFAULT_SETTING_DISPLAY_PV);
		if(!mSettingDisplayPV ) {
			mStatusView.setTextForID(StatusView.ID_STATUS_PV, "");
		}

		mSettingDisplayMoves = settings.getBoolean(SETTINGS_KEY_DISPLAY_MOVES, DEFAULT_SETTING_DISPLAY_MOVES);
		mSettingDisplayLastMove = settings.getBoolean(SETTINGS_KEY_DISPLAY_LAST_MOVE, DEFAULT_SETTING_DISPLAY_LAST_MOVE);
		
		if( bZebraSettingChanged ) {
			mZebraThread.sendSettingsChanged();
		}
	}

	private void switchSides() {
		int newFunction = -1;
		
		if( mSettingFunction == FUNCTION_ZEBRA_WHITE )
			newFunction = FUNCTION_ZEBRA_BLACK;
		else if( mSettingFunction == FUNCTION_ZEBRA_BLACK )
			newFunction = FUNCTION_ZEBRA_WHITE;
		
		if(newFunction>0) {
			SharedPreferences settings = getSharedPreferences(SHARED_PREFS_NAME, 0);
			SharedPreferences.Editor editor = settings.edit();
			editor.putString(SETTINGS_KEY_FUNCTION, String.format("%d", newFunction));
			editor.commit();
		}
		
		loadSettings();
		
		// start a new game if not playing
		if( !mZebraThread.gameInProgress() )
			newGame();
	}
	
	@Override
	protected void onDestroy() {
		boolean retry = true;
		mZebraThread.setRunning(false);
		mZebraThread.interrupt(); // if waiting
		while (retry) {
			try {
				mZebraThread.join();
				retry = false;
			} catch (InterruptedException e) {
			}
		}
		super.onDestroy();
	}

	@Override
	protected Dialog onCreateDialog(int id) {
		Dialog dialog;
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		switch(id) {
		case DIALOG_PASS_ID: {
			builder.setTitle(R.string.app_name);
			builder.setMessage(R.string.dialog_pass_text);
			builder.setPositiveButton( R.string.dialog_ok, new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int id) {
					mZebraThread.setEngineState(ZebraEngine.ES_USER_INPUT_RESUME);
				}
			}
			);
			dialog = builder.create();
		} break;
		case DIALOG_GAME_OVER: {
			dialog = new Dialog(this);

			dialog.setContentView(R.layout.gameover);
			dialog.setTitle(R.string.gameover_title);

			Button button;
			button = (Button)dialog.findViewById(R.id.gameover_choice_new_game);
			button.setOnClickListener(
					new View.OnClickListener() {
						public void onClick(View v) {
							newGame();
							dismissDialog(DIALOG_GAME_OVER);
						}
					});

			button = (Button)dialog.findViewById(R.id.gameover_choice_switch);
			button.setOnClickListener(
					new View.OnClickListener() {
						public void onClick(View v) {
							switchSides();
							dismissDialog(DIALOG_GAME_OVER);
						}
					});

			button = (Button)dialog.findViewById(R.id.gameover_choice_cancel);
			button.setOnClickListener(
					new View.OnClickListener() {
						public void onClick(View v) {
							dismissDialog(DIALOG_GAME_OVER);
						}
					});

			button = (Button)dialog.findViewById(R.id.gameover_choice_options);
			button.setOnClickListener(
					new View.OnClickListener() {
						public void onClick(View v) {
							// close the dialog
							dismissDialog(DIALOG_GAME_OVER);
							
							// start settings
							Intent i = new Intent(DroidZebra.this, SettingsPreferences.class);
							startActivity(i);
						}
					});
			
		} break;
		case DIALOG_QUIT: {
			dialog = builder
				.setTitle(R.string.dialog_quit_title)
				.setPositiveButton( R.string.dialog_quit_button_quit, new DialogInterface.OnClickListener() {
							public void onClick(DialogInterface dialog, int id) {
								DroidZebra.this.finish();
							}
						}
					)
				.setNegativeButton( R.string.dialog_quit_button_cancel, null )
				.create();
		} break;
		case DIALOG_BUSY: {
			ProgressDialog pd = new ProgressDialog(this) {
				@Override 
				public boolean onKeyDown(int keyCode, KeyEvent event) {
					 if( mZebraThread.isThinking() ) {
						 mZebraThread.stopMove();
					 }
					 mBusyDialogUp = false;
					 cancel();
					 return super.onKeyDown(keyCode, event);
				}
				@Override 
				public boolean onTouchEvent(MotionEvent event) {
					 if(event.getAction()==MotionEvent.ACTION_DOWN) {
						 if( mZebraThread.isThinking() ) {
							 mZebraThread.stopMove();
						 }
						 mBusyDialogUp = false;
						 cancel();
						 return true;
					 }
					return super.onTouchEvent(event);
				 }
			};
			pd.setProgressStyle(ProgressDialog.STYLE_SPINNER);
			pd.setMessage(getResources().getString(R.string.dialog_busy_message));			
			dialog = pd;
		} break;
		case DIALOG_DONATE: {
			builder
			.setTitle(R.string.dialog_donate_title)
			.setMessage(R.string.dialog_donate_message)
			.setIcon(R.drawable.icon)
			.setPositiveButton( R.string.dialog_donate_doit, new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int id) {
					String url = getResources().getString(R.string.donate_url);
					Intent i = new Intent(Intent.ACTION_VIEW);
					i.setData(Uri.parse(url));
					startActivity(i);
				}
			}
			)
			.setNegativeButton(R.string.dialog_donate_cancel, null);
			dialog = builder.create();
		} break;
		default:
			dialog = null;
		}
		return dialog;
	}


	@Override
	protected void onPrepareDialog(int id, Dialog dialog) {
		super.onPrepareDialog(id, dialog);
		switch(id) {
		case DIALOG_GAME_OVER: {
			int winner;
			if( mWhiteScore>mBlackScore ) 
				winner =R.string.gameover_text_white_wins;
			else if( mWhiteScore<mBlackScore ) 
				winner = R.string.gameover_text_black_wins;
			else 
				winner = R.string.gameover_text_draw;

			((TextView)dialog.findViewById(R.id.gameover_text)).setText(winner);

			((TextView)dialog.findViewById(R.id.gameover_score)).setText(String.format("%d : %d", mBlackScore, mWhiteScore));
		} break;
		}
	}
	
	public void onSharedPreferenceChanged(SharedPreferences prefs, String key) {
		if(mZebraThread!=null) 
			loadSettings();
	}

	public void FatalError(String msg) {
		AlertDialog.Builder alertDialog = new AlertDialog.Builder(this);
		alertDialog.setTitle("Zebra Error");
		alertDialog.setMessage(msg);
		alertDialog.setPositiveButton( "OK", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				DroidZebra.this.finish();
			}
		}
		);
		alertDialog.show();
	}

	@Override
	protected void onPause() {
		// TODO Auto-generated method stub
		super.onPause();
	}

	@Override
	protected void onResume() {
		// TODO Auto-generated method stub
		super.onResume();
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
	    if( keyCode==KeyEvent.KEYCODE_BACK && event.getRepeatCount()==0) {
			try {
				mZebraThread.undoMove();
			} catch (EngineError e) {
				FatalError(e.msg);
			}
	        return true;
	    }
	    return super.onKeyDown(keyCode, event);
	}
	
	/* requires api level 5 
	@Override
	public void onBackPressed() {
		try {
			mZebraThread.undoMove();
		} catch (EngineError e) {
			FatalError(e.msg);
		}
	} */

	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		// TODO Auto-generated method stub
		super.onConfigurationChanged(newConfig);
	}

	@Override
	protected void onSaveInstanceState(Bundle outState) {
		ZebraEngine.GameState gs = mZebraThread.getGameState(); 
		if( gs != null ) {
			outState.putByteArray("moves_played", gs.mMoveSequence);
			outState.putInt("moves_played_count", gs.mDisksPlayed);
			outState.putInt("version", 1);
		}
	}



}
