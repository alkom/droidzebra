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

package com.shurik.droidzebra;

import android.content.Context;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.LinkedList;
import java.util.List;
//import android.util.Log;

// DroidZebra -> ZebraEngine:public -async-> ZebraEngine thread(jni) -> Callback() -async-> DroidZebra:Handler 
public class ZebraEngine extends Thread {

	static public final int BOARD_SIZE = 8;

	static public String PATTERNS_FILE = "coeffs2.bin"; 
	static public String BOOK_FILE = "book.bin";
	static public String BOOK_FILE_COMPRESSED = "book.cmp.z";
	
	// board colors
	static public final byte PLAYER_BLACK = 0;
	static public final byte PLAYER_EMPTY = 1; // for board color
	static public final byte PLAYER_WHITE = 2;

	static public final int PLAYER_ZEBRA = 1; // for zebra skill in PlayerInfo

	// default parameters
	static public final int INFINIT_TIME = 10000000;

	static private int SELFPLAY_MOVE_DELAY = 500; // ms
	private int mMoveDelay = 0;
	private long mMoveStartTime = 0; //ms
	private int mMovesWithoutInput = 0;
	
	// messages
	static public final int 
	MSG_ERROR = 0,
	MSG_BOARD = 1,
	MSG_CANDIDATE_MOVES = 2,
	MSG_GET_USER_INPUT = 3,
	MSG_PASS = 4,
	MSG_OPENING_NAME = 5,
	MSG_LAST_MOVE = 6,
	MSG_GAME_START = 7,
	MSG_GAME_OVER = 8,
	MSG_MOVE_START = 9,
	MSG_MOVE_END = 10,
	MSG_EVAL_TEXT = 11,
	MSG_PV = 12,
	MSG_CANDIDATE_EVALS = 13,
	MSG_DEBUG = 65535;

	// engine state
	static public final int 
	ES_INITIAL = 0,
	ES_READY2PLAY = 1,
	ES_PLAY = 2,
	ES_PLAYINPROGRESS = 3,
	ES_USER_INPUT_WAIT = 4
	;

	static public final int
	UI_EVENT_EXIT = 0,
	UI_EVENT_MOVE = 1,
	UI_EVENT_UNDO = 2,
	UI_EVENT_SETTINGS_CHANGE = 3,
	UI_EVENT_REDO = 4
	;

	private static final String[] coeffAssets = { "coeffs2.bin" };
	private static final String[] bookCompressedAssets = { "book.cmp.z",};

    public void clean() {
        mHandler = null;
    }


	private transient GameState mInitialGameState;
	private transient GameState mCurrentGameState;
	
	// current move
	private JSONObject mPendingEvent= null;
	private int mValidMoves[] = null;

	// player info
	private PlayerInfo[] mPlayerInfo = {
			new PlayerInfo(PLAYER_BLACK, 0, 0, 0, INFINIT_TIME, 0),
			new PlayerInfo(PLAYER_ZEBRA, 4, 12, 12, INFINIT_TIME, 0),
			new PlayerInfo(PLAYER_WHITE, 0, 0, 0, INFINIT_TIME, 0)
	};
	private boolean mPlayerInfoChanged = false;
	private int mSideToMove = PLAYER_ZEBRA;

	// context
	private Context mContext;
	
	// message sink
	private Handler mHandler;

	// files folder
	private File mFilesDir;

	// synchronization
	static private final Object  mJNILock = new Object();

	private transient Object mEngineStateEvent = new Object();

	private int mEngineState = ES_INITIAL;

	private boolean mRun = false;
	
	private boolean bInCallback = false;

	public ZebraEngine(Context context, Handler handler)
	{
		mContext = context;
		mHandler = handler;
	}

	public boolean initFiles()
	{
		mFilesDir = null;

		// first check if files exist on internal device
		File pattern = new File(mContext.getFilesDir(), PATTERNS_FILE);
		File book = new File(mContext.getFilesDir(), BOOK_FILE);
		if( pattern.exists() && book.exists() ) {
			mFilesDir = mContext.getFilesDir();
			return true;
		}
		
		// if not - try external folder
		try {
			if(android.os.Environment.getExternalStorageState().equals(android.os.Environment.MEDIA_MOUNTED)) {
				File extDir = new File(android.os.Environment.getExternalStorageDirectory(), "/DroidZebra/files/");
				_prepareZebraFolder(extDir); //may throw
				mFilesDir = extDir;
			}
		} catch (IOException e) {
			mFilesDir = null;
		}
		
		// if external did not work out - try internal
		if(mFilesDir == null) {
			try {
				_prepareZebraFolder(mContext.getFilesDir());
				mFilesDir = mContext.getFilesDir();
			} catch (IOException e) {
				fatalError(e.getMessage());
				return false;
			}
		}
		return true;
	}

	public void waitForEngineState( int state, int milliseconds )
	{
		synchronized(mEngineStateEvent) {
			if(mEngineState!=state)
				try {
					mEngineStateEvent.wait(milliseconds);
				} catch (InterruptedException e) {
				}
		}
	}

	public void waitForEngineState( int state )
	{
		synchronized(mEngineStateEvent) {
			while(mEngineState!=state && mRun)
				try {
					mEngineStateEvent.wait();
				} catch (InterruptedException e) {
				}
		}
	}

	public void setEngineState( int state )
	{
		synchronized(mEngineStateEvent)
		{
			mEngineState = state;
			mEngineStateEvent.notifyAll();
		}
	}

	public int getEngineState()
	{
		return mEngineState;
	}

	public boolean gameInProgress() {
		return zeGameInProgress();
	}
	
	public void setRunning(boolean b) {
		boolean oldRun = mRun;
		mRun = b;
		if( oldRun && !mRun ) stopGame();
	}

	// tell zebra to stop thinking
	public void stopMove() {
		zeForceReturn();
	}

	// tell zebra to end current game
	public void stopGame() {
		zeForceExit();
		// if waiting for move - get back into the engine
		// every other state should work itself out
		if(getEngineState()==ES_USER_INPUT_WAIT) {
			mPendingEvent = new JSONObject();
			try {
				mPendingEvent.put("type", UI_EVENT_EXIT);
			} catch (JSONException e) {
				// Log.getStackTraceString(e);
			}			
			setEngineState(ES_PLAY);
		}
	}

	public void makeMove(Move move) throws InvalidMove, EngineError
	{
		if(!isValidMove(move))
			throw new InvalidMove();

		// if thinking on human time - stop
		if( mPlayerInfo[mSideToMove].skill==0
			&& getEngineState()==ZebraEngine.ES_PLAYINPROGRESS ) {
			stopMove();
			waitForEngineState(ES_USER_INPUT_WAIT, 1000);
		}
		
		if( getEngineState()!=ES_USER_INPUT_WAIT) {
			// Log.d("ZebraEngine", "Invalid Engine State");
			return;
		}

		// add move the the pending event and tell zebra to pick it up
		mPendingEvent = new JSONObject();
		try {
			mPendingEvent.put("type", UI_EVENT_MOVE);
			mPendingEvent.put("move", move.mMove);
		} catch (JSONException e) {
			// Log.getStackTraceString(e);
		}
		setEngineState(ES_PLAY);
	}

	public void undoMove() throws EngineError
	{
		// if thinking on human time - stop
		if( mPlayerInfo[mSideToMove].skill==0
			&& getEngineState()==ZebraEngine.ES_PLAYINPROGRESS ) {
			stopMove();
			waitForEngineState(ES_USER_INPUT_WAIT, 1000);
		}

		if( getEngineState()!=ES_USER_INPUT_WAIT) {
			// Log.d("ZebraEngine", "Invalid Engine State");
			return;
		}

		// create pending event and tell zebra to pick it up
		mPendingEvent = new JSONObject();
		try {
			mPendingEvent.put("type", UI_EVENT_UNDO);
		} catch (JSONException e) {
			// Log.getStackTraceString(e);
		}
		setEngineState(ES_PLAY);
	}

	public void redoMove() throws EngineError
	{
		// if thinking on human time - stop
		if( mPlayerInfo[mSideToMove].skill==0
			&& getEngineState()==ZebraEngine.ES_PLAYINPROGRESS ) {
			stopMove();
			waitForEngineState(ES_USER_INPUT_WAIT, 1000);
		}

		if( getEngineState()!=ES_USER_INPUT_WAIT) {
			// Log.d("ZebraEngine", "Invalid Engine State");
			return;
		}

		// create pending event and tell zebra to pick it up
		mPendingEvent = new JSONObject();
		try {
			mPendingEvent.put("type", UI_EVENT_REDO);
		} catch (JSONException e) {
			// Log.getStackTraceString(e);
		}
		setEngineState(ES_PLAY);
	}
	
	// notifications that some settings have changes - see if we care
	public void sendSettingsChanged()
	{
		// if we are waiting for input - restart the move (e.g. if sides switched)
		if( getEngineState()==ES_USER_INPUT_WAIT) { 
			mPendingEvent = new JSONObject();
			try {
				mPendingEvent.put("type", UI_EVENT_SETTINGS_CHANGE);
			} catch (JSONException e) {
				// Log.getStackTraceString(e);
			}
			setEngineState(ES_PLAY);
		}
	}

	public void sendReplayMoves(List<Move> moves) {
            if(getEngineState()!=ZebraEngine.ES_READY2PLAY) {
                stopGame();
                waitForEngineState(ZebraEngine.ES_READY2PLAY);
            }
            mInitialGameState = new GameState();
            mInitialGameState.mDisksPlayed = moves.size();
            mInitialGameState.mMoveSequence = toByte(moves);
            setEngineState(ES_PLAY);
    }

	// settings helpers
	public void setAutoMakeMoves(boolean _settingAutoMakeForcedMoves) {
		if(_settingAutoMakeForcedMoves)
			zeSetAutoMakeMoves(1);
		else
			zeSetAutoMakeMoves(0);
	}

	public void setSlack(float _slack) {
		zeSetSlack(_slack);
	}

	public void setPerturbation(float _perturbation) {
		zeSetPerturbation(_perturbation);
	}

	public void setForcedOpening(String _openingName) {
		zeSetForcedOpening(_openingName);
	}

	public void setHumanOpenings(boolean _enable) {
		if(_enable)
			zeSetHumanOpenings(1);
		else
			zeSetHumanOpenings(0);
	}
	
	public void setPracticeMode(boolean _enable) {
		if(_enable)
			zeSetPracticeMode(1);
		else
			zeSetPracticeMode(0);
	}

	public void setUseBook(boolean _enable) {
		if(_enable)
			zeSetUseBook(1);
		else
			zeSetUseBook(0);
	}

	public void setPlayerInfo(PlayerInfo playerInfo)  throws EngineError
	{
		if( playerInfo.playerColor!=PLAYER_BLACK && playerInfo.playerColor!=PLAYER_WHITE && playerInfo.playerColor!=PLAYER_ZEBRA)
			throw new EngineError(String.format("Invalid player type %d", playerInfo.playerColor));

		mPlayerInfo[playerInfo.playerColor] = playerInfo;

		mPlayerInfoChanged = true;
	}

	public void setMoveDelay(int delay) {
		mMoveDelay = delay;
	}

	public void setInitialGameState(LinkedList<Move> moves) {
		byte[] bytes = toByte(moves);
		setInitialGameState(moves.size(), bytes);
	}
	
	// gamestate manipulators
	public void setInitialGameState(int moveCount, byte[] moves) {
		mInitialGameState = new GameState();
		mInitialGameState.mDisksPlayed = moveCount; 
		mInitialGameState.mMoveSequence = new byte[moveCount];
		for(int i=0; i<moveCount; i++) {
			mInitialGameState.mMoveSequence[i] = moves[i];
		}
	}
	
	public GameState getGameState() {
		return mCurrentGameState;
	}
	
	// zebra thread
	@Override
	public void run() {
		setRunning(true);

		setEngineState(ES_INITIAL);

		// init data files
		if( !initFiles() ) return;

		synchronized(mJNILock) {
			zeGlobalInit(mFilesDir.getAbsolutePath());
			zeSetPlayerInfo(PLAYER_BLACK, 0, 0, 0, INFINIT_TIME, 0);
			zeSetPlayerInfo(PLAYER_WHITE, 0, 0, 0, INFINIT_TIME, 0);
		}

		setEngineState(ES_READY2PLAY);

		while (mRun) {
			waitForEngineState(ES_PLAY);

			if( !mRun ) break; // something may have happened while we were waiting

			setEngineState(ES_PLAYINPROGRESS);

			synchronized(mJNILock) {
				zeSetPlayerInfo(
						PLAYER_BLACK, 
						mPlayerInfo[PLAYER_BLACK].skill, 
						mPlayerInfo[PLAYER_BLACK].exactSolvingSkill, 
						mPlayerInfo[PLAYER_BLACK].wldSolvingSkill,
						mPlayerInfo[PLAYER_BLACK].playerTime,
						mPlayerInfo[PLAYER_BLACK].playerTimeIncrement
				);
				zeSetPlayerInfo(
						PLAYER_WHITE, 
						mPlayerInfo[PLAYER_WHITE].skill, 
						mPlayerInfo[PLAYER_WHITE].exactSolvingSkill, 
						mPlayerInfo[PLAYER_WHITE].wldSolvingSkill,
						mPlayerInfo[PLAYER_WHITE].playerTime,
						mPlayerInfo[PLAYER_WHITE].playerTimeIncrement
				);
				zeSetPlayerInfo(
						PLAYER_ZEBRA, 
						mPlayerInfo[PLAYER_ZEBRA].skill, 
						mPlayerInfo[PLAYER_ZEBRA].exactSolvingSkill, 
						mPlayerInfo[PLAYER_ZEBRA].wldSolvingSkill,
						mPlayerInfo[PLAYER_ZEBRA].playerTime,
						mPlayerInfo[PLAYER_ZEBRA].playerTimeIncrement
				);
				
				mCurrentGameState = new GameState();
				mCurrentGameState.mDisksPlayed = 0;
				mCurrentGameState.mMoveSequence = new byte[2*BOARD_SIZE*BOARD_SIZE];

				if( mInitialGameState != null )
					zePlay(mInitialGameState.mDisksPlayed, mInitialGameState.mMoveSequence);
				else
					zePlay(0, null);
				
				mInitialGameState = null;
			}

			setEngineState(ES_READY2PLAY);
			//setEngineState(ES_PLAY);  // test
		}

		synchronized(mJNILock) {
			zeGlobalTerminate();
		}
	}
	


	private byte[] toByte(List<Move> moves) {
		byte[] moveBytes = new byte[moves.size()];
		for(int i = 0; i < moves.size(); i++) {
			moveBytes[i] = (byte)moves.get(i).mMove;
		}
		return moveBytes;
	}


	// called by native code
	//public void Error(String msg) throws EngineError
	//{
	//	throw new EngineError(msg); 
	//}

	// called by native code - see droidzebra-msg.c
	private JSONObject Callback(int msgcode, JSONObject data) {
		JSONObject retval = null;
		Message msg = mHandler.obtainMessage(msgcode);
		Bundle b = new Bundle();
		msg.setData(b);
		// Log.d("ZebraEngine", String.format("Callback(%d,%s)", msgcode, data.toString()));
		if (bInCallback && msgcode != MSG_ERROR) {
			fatalError("Recursive callback call");
			new Exception().printStackTrace();
		}



		try {
			bInCallback = true;
			switch (msgcode) {
			case MSG_ERROR: { 
				b.putString("error", data.getString("error"));
				if(getEngineState()==ES_INITIAL) {
					// delete .bin files if initialization failed 
					// will be recreated from resources
					new File(mFilesDir, PATTERNS_FILE).delete();
					new File(mFilesDir, BOOK_FILE).delete();
					new File(mFilesDir, BOOK_FILE_COMPRESSED).delete();
				}
				mHandler.sendMessage(msg);
			} break;

			case MSG_DEBUG: {
				b.putString("message", data.getString("message"));
				mHandler.sendMessage(msg);
			} break;

			case MSG_BOARD: {
				int len;
				JSONObject info;
				JSONArray zeArray;
				byte[] moves;

				JSONArray zeboard = data.getJSONArray("board");
				byte newBoard[] = new byte[BOARD_SIZE*BOARD_SIZE];
				for( int i=0; i<zeboard.length(); i++) {
					JSONArray row = zeboard.getJSONArray(i);
					for( int j=0; j<row.length(); j++) {
						newBoard[i*BOARD_SIZE+j] = (byte)row.getInt(j);
					}
				}
				b.putByteArray("board", newBoard);
				b.putInt("side_to_move", data.getInt("side_to_move"));
				mCurrentGameState.mDisksPlayed = data.getInt("disks_played");
				
				// black info
				{
					Bundle black = new Bundle();
					info = data.getJSONObject("black");
					black.putString("time", info.getString("time"));
					black.putFloat("eval", (float)info.getDouble("eval"));
					black.putInt("disc_count", info.getInt("disc_count"));
					black.putString("time", info.getString("time"));
					
					zeArray = info.getJSONArray("moves");
					len = zeArray.length();
					moves = new byte[len];
					assert (2 * len <= mCurrentGameState.mMoveSequence.length);
					for( int i=0; i<len; i++) {
						moves[i] = (byte)zeArray.getInt(i);
						mCurrentGameState.mMoveSequence[2*i] = moves[i];
					}
					black.putByteArray("moves", moves);
					b.putBundle("black", black);
				}

				// white info
				{
					Bundle white = new Bundle();
					info = data.getJSONObject("white");
					white.putString("time", info.getString("time"));
					white.putFloat("eval", (float)info.getDouble("eval"));
					white.putInt("disc_count", info.getInt("disc_count"));
					white.putString("time", info.getString("time"));
					
					zeArray = info.getJSONArray("moves");
					len = zeArray.length();
					moves = new byte[len];
					assert ((2 * len + 1) <= mCurrentGameState.mMoveSequence.length);
					for( int i=0; i<len; i++) {
						moves[i] = (byte)zeArray.getInt(i);
						mCurrentGameState.mMoveSequence[2*i+1] = moves[i];
					}
					white.putByteArray("moves", moves);
					b.putBundle("white", white);
				}
				
				mHandler.sendMessage(msg);
			} break;

			case MSG_CANDIDATE_MOVES: {
				JSONArray jscmoves = data.getJSONArray("moves");
				CandidateMove cmoves[] = new CandidateMove[jscmoves.length()];
				mValidMoves = new int[jscmoves.length()];
				for( int i=0; i<jscmoves.length(); i++ ) {
					JSONObject jscmove = jscmoves.getJSONObject(i);
					mValidMoves[i] = jscmoves.getJSONObject(i).getInt("move");
					cmoves[i] = new CandidateMove(new Move(jscmove.getInt("move")));
				}
				msg.obj = cmoves;
				mHandler.sendMessage(msg);
			} break;

			case MSG_GET_USER_INPUT: {
				mMovesWithoutInput = 0;
				
				setEngineState(ES_USER_INPUT_WAIT);

				waitForEngineState(ES_PLAY);

				while(mPendingEvent==null) {
					setEngineState(ES_USER_INPUT_WAIT);
					waitForEngineState(ES_PLAY);
				}

				retval = mPendingEvent;

				setEngineState(ES_PLAYINPROGRESS);

				mValidMoves = null;
				mPendingEvent = null;
			} break;

			case MSG_PASS: {
				setEngineState(ES_USER_INPUT_WAIT);
				mHandler.sendMessage(msg);
				waitForEngineState(ES_PLAY);				
				setEngineState(ES_PLAYINPROGRESS);
			} break;

			case MSG_OPENING_NAME: {
				b.putString("opening", data.getString("opening"));
				mHandler.sendMessage(msg);
			} break;

			case MSG_LAST_MOVE: {
				b.putInt("move", data.getInt("move"));
				mHandler.sendMessage(msg);
			} break;

			case MSG_GAME_START: {
				mHandler.sendMessage(msg);
			} break;

			case MSG_GAME_OVER: {
				mHandler.sendMessage(msg);
			} break;

			case MSG_MOVE_START: {
				mMoveStartTime = android.os.SystemClock.uptimeMillis();
				
				mSideToMove = data.getInt("side_to_move");
				
				// can change player info here
				if( mPlayerInfoChanged ) {
					zeSetPlayerInfo(
							PLAYER_BLACK, 
							mPlayerInfo[PLAYER_BLACK].skill, 
							mPlayerInfo[PLAYER_BLACK].exactSolvingSkill, 
							mPlayerInfo[PLAYER_BLACK].wldSolvingSkill,
							mPlayerInfo[PLAYER_BLACK].playerTime,
							mPlayerInfo[PLAYER_BLACK].playerTimeIncrement
					);
					zeSetPlayerInfo(
							PLAYER_WHITE, 
							mPlayerInfo[PLAYER_WHITE].skill, 
							mPlayerInfo[PLAYER_WHITE].exactSolvingSkill, 
							mPlayerInfo[PLAYER_WHITE].wldSolvingSkill,
							mPlayerInfo[PLAYER_WHITE].playerTime,
							mPlayerInfo[PLAYER_WHITE].playerTimeIncrement
					);
					zeSetPlayerInfo(
							PLAYER_ZEBRA, 
							mPlayerInfo[PLAYER_ZEBRA].skill, 
							mPlayerInfo[PLAYER_ZEBRA].exactSolvingSkill, 
							mPlayerInfo[PLAYER_ZEBRA].wldSolvingSkill,
							mPlayerInfo[PLAYER_ZEBRA].playerTime,
							mPlayerInfo[PLAYER_ZEBRA].playerTimeIncrement
					);
				}
				mHandler.sendMessage(msg);
			} break;

			case MSG_MOVE_END: {
				// introduce delay between moves made by the computer without user input 
				// so we can actually to see that the game is being played :)
				if( mMoveDelay>0 || (mMovesWithoutInput>1 && mPlayerInfo[mSideToMove].skill>0) ) {
					long moveEnd = android.os.SystemClock.uptimeMillis();
					int delay = mMoveDelay>0? mMoveDelay : SELFPLAY_MOVE_DELAY;
					if( (moveEnd - mMoveStartTime)<delay ) {
						android.os.SystemClock.sleep(delay - (moveEnd - mMoveStartTime));
					}
				}
				
				// this counter is reset by user input
				mMovesWithoutInput += 1;

				mHandler.sendMessage(msg);
			} break;			

			case MSG_EVAL_TEXT: {
				b.putString("eval", data.getString("eval"));
				mHandler.sendMessage(msg);
			} break;			

			case MSG_PV: {
				JSONArray zeArray = data.getJSONArray("pv");
				int len = zeArray.length();
				byte[] moves = new byte[len];
				for( int i=0; i<len; i++)
					moves[i] = (byte)zeArray.getInt(i);
				b.putByteArray("pv", moves);
				mHandler.sendMessage(msg);
			} break;
			
			case MSG_CANDIDATE_EVALS: {
				JSONArray jscevals = data.getJSONArray("evals");
				CandidateMove cmoves[] = new CandidateMove[jscevals.length()];
				for( int i=0; i<jscevals.length(); i++ ) {
					JSONObject jsceval = jscevals.getJSONObject(i);
					cmoves[i] = new CandidateMove(
							new Move(jsceval.getInt("move")),
							jsceval.getString("eval_s"),
							jsceval.getString("eval_l"),
							(jsceval.getInt("best")!=0)
						);
				}
				msg.obj = cmoves;
				mHandler.sendMessage(msg);
				
			} break;
			
			default: {
				b.putString("error", String.format("Unkown message ID %d", msgcode));
				msg.setData(b);
				mHandler.sendMessage(msg);
			} break;
			}
		} catch (JSONException e) {
			msg.what = MSG_ERROR;
			b.putString( "error", "JSONException:" + e.getMessage());
			msg.setData(b);
			mHandler.sendMessage(msg);
		} finally {
			bInCallback = false;
		}
		return retval;
	}

	public boolean isThinking() {
		return getEngineState()==ZebraEngine.ES_PLAYINPROGRESS;
	}

	public boolean isValidMove(Move move) {
		if(mValidMoves==null)
			return false;

		boolean valid = false;
		for(int m : mValidMoves) 
			if(m==move.mMove) {
				valid = true;
				break;
			}
		return valid;
	}
	
	public boolean isHumanToMove() {
		return mPlayerInfo[mSideToMove].skill==0;
	}
	
	private void _prepareZebraFolder(File dir) throws IOException
	{
		File pattern = new File(dir, PATTERNS_FILE);
		File book = new File(dir, BOOK_FILE);
		File bookCompressed = new File(dir, BOOK_FILE_COMPRESSED);
		
		if( pattern.exists() && book.exists() )
			return;

		if( !dir.exists() && !dir.mkdirs() )
			throw new IOException(String.format("Unable to create %s", dir));
			
		try {
			_asset2File(coeffAssets, pattern);
			_asset2File(bookCompressedAssets, bookCompressed);
		} catch (IOException e) {
			pattern.delete();
			book.delete();
			bookCompressed.delete();
			throw e;
		}
	}
	
	private void _asset2File(String[] assets, File file) throws IOException {
		if( !file.exists() || file.length()==0 ) {
			// copy files
			AssetManager assetManager = mContext.getAssets();
			InputStream source = null;
			OutputStream destination = null;
			try {
				destination = new FileOutputStream(file);
				for(String sourceAsset:assets) {
					source = assetManager.open(sourceAsset);
					byte[] buffer = new byte[1024];
					int len = source.read(buffer);
					while (len != -1) {
						destination.write(buffer, 0, len);
						len = source.read(buffer);
					}	
				}
			}
			finally {
				if(destination != null) {
					try {
						destination.close();
					} catch (IOException e) {
						fatalError(e.getMessage());
					}
				}
			}
		}
	}

	private void fatalError(String message) {
		try {
			JSONObject json = new JSONObject();
			json.put("error", message);
			Callback(MSG_ERROR, json);
		} catch (JSONException e) {
			e.printStackTrace();
		}
		
	}

	// JNI
	private native void zeGlobalInit(String filesDir);
	private native void zeGlobalTerminate();
	private native void zeForceReturn();
	private native void zeForceExit();
	private native void zeSetPlayerInfo(
			int player,
			int skill,
			int exactSkill,
			int wldSkill,
			int time,
			int timeIncrement
	);    
	private native void zePlay(int providedMoveCount, byte[] providedMoves);
	private native void zeSetAutoMakeMoves(int auto_make_moves);
	private native void zeSetSlack(float slack);
	private native void zeSetPerturbation(float perturbation);
	private native void zeSetForcedOpening(String opening_name);
	private native void zeSetHumanOpenings(int enable);
	private native void zeSetPracticeMode(int enable);
	private native void zeSetUseBook(int enable);
	private native boolean zeGameInProgress();
	
	public native void zeJsonTest(JSONObject json);

	static {
		System.loadLibrary("droidzebra");
	}

}
