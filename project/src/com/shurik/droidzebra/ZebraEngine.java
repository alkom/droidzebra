package com.shurik.droidzebra;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;


public class ZebraEngine extends Thread {

	static public final int BOARD_SIZE = 8;

	static public String PATTERNS_FILE = "coeffs2.bin"; 
	static public String BOOK_FILE = "book.bin";
	
	// board colors
	static public final int PLAYER_BLACK = 0;
	static public final int PLAYER_EMPTY = 1;
	static public final int PLAYER_WHITE = 2;

	// default parameters
	static public final int INFINIT_TIME = 10000000;

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
	MSG_DEBUG = 65535;

	// engine state
	static public final int 
	ES_INITIAL = 0,
	ES_READY2PLAY = 1,
	ES_PLAY = 2,
	ES_PLAYINPROGRESS = 3,
	ES_USER_INPUT_WAIT = 4,
	ES_USER_INPUT_RESUME = 5;

	static public final int
	UI_EVENT_EXIT = 0,
	UI_EVENT_MOVE = 1,
	UI_EVENT_UNDO = 2,
	UI_EVENT_SETTINGS_CHANGE = 3
	;

	private static final String[] coeffAssets = { "coeffs2.bin" };
	private static final String[] bookAssets = { "book.bin.0", "book.bin.1", "book.bin.2", "book.bin.3", "book.bin.4", "book.bin.5", "book.bin.6", "book.bin.7", "book.bin.8",};
	//private static final String[] bookAssets = { "book.bin",};

	// player info
	public static class PlayerInfo {
		public PlayerInfo(int _player, int _skill, int _exact_skill, int _wld_skill, int _player_time, int _increment) {
			assert(_player==PLAYER_BLACK || _player==PLAYER_WHITE);
			playerColor = _player;
			skill = _skill;
			exactSolvingSkill = _exact_skill;
			wldSolvingSkill = _wld_skill;
			playerTime = _player_time;
			playerTimeIncrement = _increment;

		}
		public int playerColor;
		public int skill;
		public int exactSolvingSkill;
		public int wldSolvingSkill;
		public int playerTime;
		public int playerTimeIncrement;
	};

	public static class InvalidMove extends Exception {
		private static final long serialVersionUID = 8970579827351672330L;
	};

	public static class Move {
		public int mMove;
		public Move(int move) {
			mMove = move;
		}
		public Move(int x, int y) {
			mMove = (x+1)*10+y+1;
		}
		public int getY() { return mMove%10-1; }
		public int getX() { return mMove/10-1; }
		public String getText() {
			if (mMove == -1) return "--";
			
			byte m[] = new byte[2];
			m[0] = (byte) ('a' + getX());
			m[1] = (byte) ('1' + getY());
			return new String(m);
		} 
	};

	public class CandidateMove {
		public Move mMove;
		public float mScore;  
		public CandidateMove(Move move, float score) {
			mMove = move;
			mScore = score;
		}
	}

	// current move
	private JSONObject mPendingEvent= null;
	private int mValidMoves[] = null;

	// player info
	private PlayerInfo[] mPlayerInfo = {
			new PlayerInfo(PLAYER_BLACK, 0, 0, 0, INFINIT_TIME, 0),
			null,
			new PlayerInfo(PLAYER_WHITE, 0, 0, 0, INFINIT_TIME, 0)
	};
	private boolean mPlayerInfoChanged = false;
	private int mSideToMove = PLAYER_EMPTY;

	// context
	private Context mContext;
	
	// message sink
	private Handler mHandler;

	// files folder
	private File mFilesDir;

	// syncronization
	static private final Object  mJNILock = new Object();

	private Object mEngineStateEvent = new Object();

	private int mEngineState = ES_INITIAL;

	private boolean mRun = false;

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

	public void setRunning(boolean b) {
		boolean oldRun = mRun;
		mRun = b;
		if( oldRun && !mRun ) stopGame();
	}

	public void stopMove() {
		zeForceReturn();
	}

	public void stopGame() {
		zeForceExit();
		// if waiting for move - get back into the engine
		// every other state should work itself out
		if(getEngineState()==ES_USER_INPUT_WAIT) {
			mPendingEvent = new JSONObject();
			try {
				mPendingEvent.put("type", UI_EVENT_EXIT);
			} catch (JSONException e) {
				Log.getStackTraceString(e);
			}			
			setEngineState(ES_USER_INPUT_RESUME);
		}
	}

	public void makeMove(Move move) throws InvalidMove, EngineError
	{
		if( getEngineState()!=ES_USER_INPUT_WAIT) {
			Log.d("ZebraEngine", "Invalid Engine State");
			return;
		}

		if(mValidMoves==null) 
			throw new InvalidMove();

		boolean valid = false;
		for(int m : mValidMoves) 
			if(m==move.mMove) {
				valid = true;
				break;
			}
		if(!valid) throw new InvalidMove();

		mPendingEvent = new JSONObject();
		try {
			mPendingEvent.put("type", UI_EVENT_MOVE);
			mPendingEvent.put("move", move.mMove);
		} catch (JSONException e) {
			Log.getStackTraceString(e);
		}
		setEngineState(ES_USER_INPUT_RESUME);
	}

	public void undoMove() throws EngineError
	{
		if( getEngineState()!=ES_USER_INPUT_WAIT) {
			Log.d("ZebraEngine", "Invalid Engine State");
			return;
		}

		mPendingEvent = new JSONObject();
		try {
			mPendingEvent.put("type", UI_EVENT_UNDO);
		} catch (JSONException e) {
			Log.getStackTraceString(e);
		}
		setEngineState(ES_USER_INPUT_RESUME);
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
				Log.getStackTraceString(e);
			}
			setEngineState(ES_USER_INPUT_RESUME);
		}
	}

	public void setAutoMakeMoves(boolean mSettingAutoMakeForcedMoves) {
		if(mSettingAutoMakeForcedMoves)
			zeSetAutoMakeMoves(1);
		else
			zeSetAutoMakeMoves(0);
	}

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
				zePlay();
			}

			setEngineState(ES_READY2PLAY);
			//setEngineState(ES_PLAY);  // test
		}

		synchronized(mJNILock) {
			zeGlobalTerminate();
		}
	}


	public void setPlayerInfo(PlayerInfo playerInfo)  throws EngineError
	{
		if( playerInfo.playerColor!=PLAYER_BLACK && playerInfo.playerColor!=PLAYER_WHITE )
			throw new EngineError(String.format("Invalid player type %d", playerInfo.playerColor));

		mPlayerInfo[playerInfo.playerColor] = playerInfo;

		mPlayerInfoChanged = true;
	}

	// called by native code
	//public void Error(String msg) throws EngineError
	//{
	//	throw new EngineError(msg); 
	//}

	// called by native code
	private JSONObject Callback(int msgcode, JSONObject data) {
		JSONObject retval = null;
		Message msg = mHandler.obtainMessage(msgcode);
		Bundle b = new Bundle();
		msg.setData(b);
		// Log.d("ZebraEngine", String.format("Callback(%d,%s)", msgcode, data.toString()));
		try {
			switch(msgcode) {
			case MSG_ERROR: {
				b.putString("error", data.getString("error"));
				if(getEngineState()==ES_INITIAL) {
					// delete .bin files
					new File(mFilesDir, PATTERNS_FILE).delete();
					new File(mFilesDir, BOOK_FILE).delete();
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
					for( int i=0; i<len; i++)
						moves[i] = (byte)zeArray.getInt(i);
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
					for( int i=0; i<len; i++)
						moves[i] = (byte)zeArray.getInt(i);
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
					cmoves[i] = new CandidateMove(
							new Move(jscmove.getInt("move")),
							(float)jscmove.getDouble("score")
					);
				}
				msg.obj = cmoves;
				mHandler.sendMessage(msg);
			} break;

			case MSG_GET_USER_INPUT: {
				setEngineState(ES_USER_INPUT_WAIT);

				waitForEngineState(ES_USER_INPUT_RESUME);

				while(mPendingEvent==null) {
					setEngineState(ES_USER_INPUT_WAIT);
					waitForEngineState(ES_USER_INPUT_RESUME);
				}

				retval = mPendingEvent;

				setEngineState(ES_PLAYINPROGRESS);

				mValidMoves = null;
				mPendingEvent = null;
			} break;

			case MSG_PASS: {
				setEngineState(ES_USER_INPUT_WAIT);
				mHandler.sendMessage(msg);
				waitForEngineState(ES_USER_INPUT_RESUME);				
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
				}
				mHandler.sendMessage(msg);
			} break;

			case MSG_MOVE_END: {
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
			
			default: {
				b.putString( "error", String.format("Unkown message ID %d", msgcode) );
				msg.setData(b);
				mHandler.sendMessage(msg);
			} break;
			}
		} catch (JSONException e) {
			msg.what = MSG_ERROR;
			b.putString( "error", "JSONException:" + e.getMessage());
			msg.setData(b);
			mHandler.sendMessage(msg);
		}
		return retval;
	}

	public boolean isThinking() {
		return getEngineState()==ZebraEngine.ES_PLAYINPROGRESS;
	}

	private void _prepareZebraFolder(File dir) throws IOException
	{
		File pattern = new File(dir, PATTERNS_FILE);
		File book = new File(dir, BOOK_FILE);
		
		if( pattern.exists() && book.exists() )
			return;

		if( !dir.exists() && !dir.mkdirs() )
			throw new IOException(String.format("Unable to create %s", dir));
			
		
		try {
			_asset2File(coeffAssets, pattern);
			_asset2File(bookAssets, book);
		} catch (IOException e) {
			pattern.delete();
			book.delete();
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
	private native void zePlay();
	private native void zeSetAutoMakeMoves(int auto_make_moves);

	public native void zeJsonTest(JSONObject json);

	static {
		System.loadLibrary("droidzebra");
	}

}
