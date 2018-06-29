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

import android.util.Log;
import de.earthlingz.oerszebra.BuildConfig;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.*;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;

import static de.earthlingz.oerszebra.GameSettingsConstants.*;
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
    private int computerMoveDelay = 0;
    private long mMoveStartTime = 0; //ms

    // messages
    private static final int
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
            MSG_ANALYZE_GAME = 14,
            MSG_NEXT_MOVE = 15,
            MSG_DEBUG = 65535;

    // engine state
    static public final int
            ES_INITIAL = 0,
            ES_READY2PLAY = 1,
            ES_PLAY = 2,
            ES_PLAYINPROGRESS = 3,
            ES_USER_INPUT_WAIT = 4;

    static public final int
            UI_EVENT_EXIT = 0,
            UI_EVENT_MOVE = 1,
            UI_EVENT_UNDO = 2,
            UI_EVENT_SETTINGS_CHANGE = 3,
            UI_EVENT_REDO = 4;

    public void clean() {
        mHandler = null;
    }


    private transient ZebraBoard mInitialGameState;
    private transient ZebraBoard mCurrentGameState;

    // current move
    private JSONObject mPendingEvent = null;
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
    private GameContext mContext;

    // message sink
    private ZebraEngineMessageHander mHandler;

    // files folder
    private File mFilesDir;

    // synchronization
    static private final Object mJNILock = new Object();

    private final transient Object mEngineStateEvent = new Object();

    private int mEngineState = ES_INITIAL;

    private boolean mRun = false;

    private boolean bInCallback = false;

    public ZebraEngine(GameContext context) {
        mContext = context;
    }

    public void setHandler(ZebraEngineMessageHander mHandler) {
        this.mHandler = mHandler;
    }

    public boolean initFiles() {
        mFilesDir = null;

        // first check if files exist on internal device
        File pattern = new File(mContext.getFilesDir(), PATTERNS_FILE);
        File book = new File(mContext.getFilesDir(), BOOK_FILE_COMPRESSED);
        if (pattern.exists() && book.exists()) {
            mFilesDir = mContext.getFilesDir();
            return true;
        }

        // if not - try external folder
        copyAsset(mContext, PATTERNS_FILE, mContext.getFilesDir());
        copyAsset(mContext, BOOK_FILE_COMPRESSED, mContext.getFilesDir());

        if (!pattern.exists() && !book.exists()) {
            // will be recreated from resources, the next time, maybe
            new File(mContext.getFilesDir(), PATTERNS_FILE).delete();
            new File(mContext.getFilesDir(), BOOK_FILE).delete();
            new File(mContext.getFilesDir(), BOOK_FILE_COMPRESSED).delete();
            throw new IllegalStateException("Kann coeeffs.bin und book nicht finden");
        }

        mFilesDir = mContext.getFilesDir();
        return true;
    }

    private void copyAsset(GameContext assetManager,
                           String fromAssetPath, File filesdir) {
        File target = new File(filesdir, fromAssetPath);
        try (InputStream in = assetManager.open(fromAssetPath); OutputStream out = new FileOutputStream(target)) {
            copyFile(in, out);
        } catch (Exception e) {
            Log.e(ZebraEngine.class.getSimpleName(), "copyAsset: " + fromAssetPath, e);
            throw new IllegalStateException("Datei konnte nicht geladen werden: " + fromAssetPath, e);
        }
    }

    private static void copyFile(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1024];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
    }

    public void waitForEngineState(int state, int milliseconds) {
        synchronized (mEngineStateEvent) {
            if (mEngineState != state)
                try {
                    mEngineStateEvent.wait(milliseconds);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }

        }
    }

    public void waitForEngineState(int state) {
        synchronized (mEngineStateEvent) {
            while (mEngineState != state && mRun)
                try {
                    mEngineStateEvent.wait();
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
        }
    }

    public void setEngineState(int state) {
        synchronized (mEngineStateEvent) {
            mEngineState = state;
            mEngineStateEvent.notifyAll();
        }
    }

    public int getEngineState() {
        return mEngineState;
    }

    public boolean gameInProgress() {
        return zeGameInProgress();
    }

    public void setRunning(boolean b) {
        boolean oldRun = mRun;
        mRun = b;
        if (oldRun && !mRun) stopGame();
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
        if (getEngineState() == ES_USER_INPUT_WAIT) {
            mPendingEvent = new JSONObject();
            try {
                mPendingEvent.put("type", UI_EVENT_EXIT);
            } catch (JSONException e) {
                // Log.getStackTraceString(e);
            }
            setEngineState(ES_PLAY);
        }
    }

    public void makeMove(Move move) throws InvalidMove {
        if (!isValidMove(move))
            throw new InvalidMove();

        // if thinking on human time - stop
        if (isHumanToMove()
                && getEngineState() == ZebraEngine.ES_PLAYINPROGRESS) {
            stopMove();
            waitForEngineState(ES_USER_INPUT_WAIT, 1000);
        }

        if (getEngineState() != ES_USER_INPUT_WAIT) {
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

    public void undoMove() {
        // if thinking on human time - stop
        if (isHumanToMove()
                && getEngineState() == ZebraEngine.ES_PLAYINPROGRESS) {
            stopMove();
            waitForEngineState(ES_USER_INPUT_WAIT, 1000);
        }

        if (getEngineState() != ES_USER_INPUT_WAIT) {
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

    public void redoMove() {
        // if thinking on human time - stop
        if (isHumanToMove()
                && getEngineState() == ZebraEngine.ES_PLAYINPROGRESS) {
            stopMove();
            waitForEngineState(ES_USER_INPUT_WAIT, 1000);
        }

        if (getEngineState() != ES_USER_INPUT_WAIT) {
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
    public void sendSettingsChanged() {
        // if we are waiting for input - restart the move (e.g. if sides switched)
        if (getEngineState() == ES_USER_INPUT_WAIT) {
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
        if (getEngineState() != ZebraEngine.ES_READY2PLAY) {
            stopGame();
            waitForEngineState(ZebraEngine.ES_READY2PLAY);
        }
        mInitialGameState = new ZebraBoard();
        mInitialGameState.setDisksPlayed(moves.size());
        mInitialGameState.setMoveSequence(toByte(moves));
        setEngineState(ES_PLAY);
    }

    // settings helpers


    public void setSettingFunction(int settingFunction, int depth, int depthExact, int depthWLD) throws EngineError {
        switch (settingFunction) {
            case FUNCTION_HUMAN_VS_HUMAN:
                setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_BLACK, 0, 0, 0, ZebraEngine.INFINIT_TIME, 0));
                setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_WHITE, 0, 0, 0, ZebraEngine.INFINIT_TIME, 0));
                break;
            case FUNCTION_ZEBRA_BLACK:
                setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_BLACK, depth, depthExact, depthWLD, ZebraEngine.INFINIT_TIME, 0));
                setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_WHITE, 0, 0, 0, ZebraEngine.INFINIT_TIME, 0));
                break;
            case FUNCTION_ZEBRA_VS_ZEBRA:
                setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_BLACK, depth, depthExact, depthWLD, ZebraEngine.INFINIT_TIME, 0));
                setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_WHITE, depth, depthExact, depthWLD, ZebraEngine.INFINIT_TIME, 0));
                break;
            case FUNCTION_ZEBRA_WHITE:
            default:
                setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_BLACK, 0, 0, 0, ZebraEngine.INFINIT_TIME, 0));
                setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_WHITE, depth, depthExact, depthWLD, ZebraEngine.INFINIT_TIME, 0));
                break;
        }
        setPlayerInfo(new PlayerInfo(ZebraEngine.PLAYER_ZEBRA, depth + 1, depthExact + 1, depthWLD + 1, ZebraEngine.INFINIT_TIME, 0));
    }

    public void setAutoMakeMoves(boolean _settingAutoMakeForcedMoves) {
        if (_settingAutoMakeForcedMoves)
            zeSetAutoMakeMoves(1);
        else
            zeSetAutoMakeMoves(0);
    }

    public void setSlack(int _slack) {
        zeSetSlack(_slack);
    }

    public void setPerturbation(int _perturbation) {
        zeSetPerturbation(_perturbation);
    }

    public void setForcedOpening(String _openingName) {
        zeSetForcedOpening(_openingName);
    }

    public void setHumanOpenings(boolean _enable) {
        if (_enable)
            zeSetHumanOpenings(1);
        else
            zeSetHumanOpenings(0);
    }

    public void setPracticeMode(boolean _enable) {
        if (_enable)
            zeSetPracticeMode(1);
        else
            zeSetPracticeMode(0);
    }

    public void setUseBook(boolean _enable) {
        if (_enable)
            zeSetUseBook(1);
        else
            zeSetUseBook(0);
    }

    private void setPlayerInfo(PlayerInfo playerInfo) throws EngineError {
        if (playerInfo.playerColor != PLAYER_BLACK && playerInfo.playerColor != PLAYER_WHITE && playerInfo.playerColor != PLAYER_ZEBRA)
            throw new EngineError(String.format("Invalid player type %d", playerInfo.playerColor));

        mPlayerInfo[playerInfo.playerColor] = playerInfo;

        mPlayerInfoChanged = true;
    }

    public void setComputerMoveDelay(int delay) {
        computerMoveDelay = delay;
    }

    public void setInitialGameState(LinkedList<Move> moves) {
        byte[] bytes = toByte(moves);
        setInitialGameState(moves.size(), bytes);
    }

    // gamestate manipulators
    public void setInitialGameState(int moveCount, byte[] moves) {
        mInitialGameState = new ZebraBoard();
        mInitialGameState.setDisksPlayed(moveCount);
        mInitialGameState.setMoveSequence(new byte[moveCount]);
        for (int i = 0; i < moveCount; i++) {
            mInitialGameState.getMoveSequence()[i] = moves[i];
        }
    }

    public ZebraBoard getGameState() {
        return mCurrentGameState;
    }

    // zebra thread
    @Override
    public void run() {
        setRunning(true);

        setEngineState(ES_INITIAL);

        // init data files
        if (!initFiles()) return;

        synchronized (mJNILock) {
            zeGlobalInit(mFilesDir.getAbsolutePath());
            zeSetPlayerInfo(PLAYER_BLACK, 0, 0, 0, INFINIT_TIME, 0);
            zeSetPlayerInfo(PLAYER_WHITE, 0, 0, 0, INFINIT_TIME, 0);
        }

        setEngineState(ES_READY2PLAY);

        while (mRun) {
            waitForEngineState(ES_PLAY);

            if (!mRun) break; // something may have happened while we were waiting

            setEngineState(ES_PLAYINPROGRESS);

            synchronized (mJNILock) {
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

                mCurrentGameState = new ZebraBoard();
                mCurrentGameState.setDisksPlayed(0);
                mCurrentGameState.setMoveSequence(new byte[2 * BOARD_SIZE * BOARD_SIZE]);

                if (mInitialGameState != null)
                    zePlay(mInitialGameState.getDisksPlayed(), mInitialGameState.getMoveSequence());
                else
                    zePlay(0, null);

                mInitialGameState = null;
            }

            setEngineState(ES_READY2PLAY);
            //setEngineState(ES_PLAY);  // test
        }

        synchronized (mJNILock) {
            zeGlobalTerminate();
        }
    }

    public void analyzeGame(List<Move> moves) {
        byte[] bytes = toByte(moves);
        zeAnalyzeGame(moves.size(), bytes);
    }


    private byte[] toByte(List<Move> moves) {
        byte[] moveBytes = new byte[moves.size()];
        for (int i = 0; i < moves.size(); i++) {
            moveBytes[i] = (byte) moves.get(i).mMove;
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
        // Log.d("ZebraEngine", String.format("Callback(%d,%s)", msgcode, data.toString()));
        if (bInCallback && msgcode != MSG_ERROR) {
            fatalError("Recursive callback call");
            new Exception().printStackTrace();
        }


        try {
            bInCallback = true;
            switch (msgcode) {
                case MSG_ERROR: {
                    if (getEngineState() == ES_INITIAL) {
                        // delete .bin files if initialization failed
                        // will be recreated from resources
                        new File(mFilesDir, PATTERNS_FILE).delete();
                        new File(mFilesDir, BOOK_FILE).delete();
                        new File(mFilesDir, BOOK_FILE_COMPRESSED).delete();
                    }
                    String error = data.getString("error");
                    mHandler.sendError(error);
                }
                break;

                case MSG_DEBUG: {
                    String debug = data.getString("message");
                    mHandler.sendDebug(debug);
                }
                break;

                case MSG_BOARD: {
                    int len;
                    JSONObject info;
                    JSONArray zeArray;
                    byte[] moves;

                    JSONArray zeboard = data.getJSONArray("board");
                    byte newBoard[] = new byte[BOARD_SIZE * BOARD_SIZE];
                    for (int i = 0; i < zeboard.length(); i++) {
                        JSONArray row = zeboard.getJSONArray(i);
                        for (int j = 0; j < row.length(); j++) {
                            newBoard[i * BOARD_SIZE + j] = (byte) row.getInt(j);
                        }
                    }

                    //update the current game state
                    ZebraBoard board = getGameState();
                    board.setBoard(newBoard);
                    board.setSideToMove(data.getInt("side_to_move"));
                    board.setDisksPlayed(data.getInt("disks_played"));

                    // black info
                    {
                        ZebraPlayerStatus black = new ZebraPlayerStatus();
                        info = data.getJSONObject("black");
                        black.setTime(info.getString("time"));
                        black.setEval((float) info.getDouble("eval"));
                        black.setDiscCount(info.getInt("disc_count"));

                        zeArray = info.getJSONArray("moves");
                        len = zeArray.length();
                        moves = new byte[len];
                        if (BuildConfig.DEBUG && !(2 * len <= mCurrentGameState.getMoveSequence().length)) {
                            throw new AssertionError();
                        }
                        for (int i = 0; i < len; i++) {
                            moves[i] = (byte) zeArray.getInt(i);
                            mCurrentGameState.getMoveSequence()[2 * i] = moves[i];
                        }
                        black.setMoves(moves);
                        board.setBlackPlayer(black);
                    }

                    // white info
                    {
                        ZebraPlayerStatus white = new ZebraPlayerStatus();
                        info = data.getJSONObject("white");
                        white.setTime(info.getString("time"));
                        white.setEval((float) info.getDouble("eval"));
                        white.setDiscCount(info.getInt("disc_count"));

                        zeArray = info.getJSONArray("moves");
                        len = zeArray.length();
                        moves = new byte[len];
                        if (BuildConfig.DEBUG && !(2 * len <= mCurrentGameState.getMoveSequence().length)) {
                            throw new AssertionError();
                        }
                        for (int i = 0; i < len; i++) {
                            moves[i] = (byte) zeArray.getInt(i);
                            mCurrentGameState.getMoveSequence()[2 * i + 1] = moves[i];
                        }
                        white.setMoves(moves);
                        board.setWhitePlayer(white);
                    }
                    mHandler.sendBoard(board);
                }
                break;

                case MSG_CANDIDATE_MOVES: {
                    JSONArray jscmoves = data.getJSONArray("moves");
                    CandidateMove cmoves[] = new CandidateMove[jscmoves.length()];
                    mValidMoves = new int[jscmoves.length()];
                    for (int i = 0; i < jscmoves.length(); i++) {
                        JSONObject jscmove = jscmoves.getJSONObject(i);
                        mValidMoves[i] = jscmoves.getJSONObject(i).getInt("move");
                        cmoves[i] = new CandidateMove(new Move(jscmove.getInt("move")));
                    }
                    getGameState().setCandidateMoves(cmoves);
                }
                break;

                case MSG_GET_USER_INPUT: {

                    setEngineState(ES_USER_INPUT_WAIT);

                    waitForEngineState(ES_PLAY);

                    while (mPendingEvent == null) {
                        setEngineState(ES_USER_INPUT_WAIT);
                        waitForEngineState(ES_PLAY);
                    }

                    retval = mPendingEvent;

                    setEngineState(ES_PLAYINPROGRESS);

                    mValidMoves = null;
                    mPendingEvent = null;
                }
                break;

                case MSG_PASS: {
                    setEngineState(ES_USER_INPUT_WAIT);
                    mHandler.sendPass();
                    waitForEngineState(ES_PLAY);
                    setEngineState(ES_PLAYINPROGRESS);
                }
                break;
                case MSG_ANALYZE_GAME: {
                    setEngineState(ES_USER_INPUT_WAIT);
                    //mHandler.sendMessage(msg);
                    waitForEngineState(ES_PLAY);
                    setEngineState(ES_PLAYINPROGRESS);
                }
                break;

                case MSG_OPENING_NAME: {
                    getGameState().setOpening(data.getString("opening"));
                    mHandler.sendBoard(getGameState());
                }
                break;

                case MSG_LAST_MOVE: {
                    getGameState().setLastMove(data.getInt("move"));
                    mHandler.sendBoard(getGameState());
                }
                break;

                case MSG_NEXT_MOVE: {
                    getGameState().setNextMove(data.getInt("move"));
                    mHandler.sendBoard(getGameState());
                }
                break;

                case MSG_GAME_START: {
                    mHandler.sendGameStart();
                }
                break;

                case MSG_GAME_OVER: {
                    mHandler.sendGameOver();
                }
                break;

                case MSG_MOVE_START: {
                    mMoveStartTime = android.os.SystemClock.uptimeMillis();

                    mSideToMove = data.getInt("side_to_move");

                    // can change player info here
                    if (mPlayerInfoChanged) {
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
                        mPlayerInfoChanged = false;
                    }
                    mHandler.sendMoveStart();
                }
                break;

                case MSG_MOVE_END: {
                    // introduce delay between moves made by the computer without user input
                    // so we can actually to see that the game is being played :)
                    if (computerMoveDelay > 0 && (mPlayerInfo[mSideToMove].skill > 0)) {
                        long moveEnd = android.os.SystemClock.uptimeMillis();
                        if ((moveEnd - mMoveStartTime) < computerMoveDelay) {
                            android.os.SystemClock.sleep(computerMoveDelay - (moveEnd - mMoveStartTime));
                        }
                    }
                    mHandler.sendMoveEnd();
                }
                break;

                case MSG_EVAL_TEXT: {
                    String eval = data.getString("eval");
                    mHandler.sendEval(eval);
                }
                break;

                case MSG_PV: {
                    JSONArray zeArray = data.getJSONArray("pv");
                    int len = zeArray.length();
                    byte[] moves = new byte[len];
                    for (int i = 0; i < len; i++)
                        moves[i] = (byte) zeArray.getInt(i);
                    mHandler.sendPv(moves);
                }
                break;

                case MSG_CANDIDATE_EVALS: {
                    JSONArray jscevals = data.getJSONArray("evals");
                    CandidateMove cmoves[] = new CandidateMove[jscevals.length()];
                    for (int i = 0; i < jscevals.length(); i++) {
                        JSONObject jsceval = jscevals.getJSONObject(i);
                        cmoves[i] = new CandidateMove(
                                new Move(jsceval.getInt("move")),
                                jsceval.getString("eval_s"),
                                jsceval.getString("eval_l"),
                                (jsceval.getInt("best") != 0)
                        );
                    }
                    getGameState().addCandidateMoveEvals(cmoves);
                    mHandler.sendBoard(getGameState());
                }
                break;

                default: {
                    mHandler.sendError(String.format(Locale.getDefault(), "Unkown message ID %d", msgcode));
                }
                break;
            }
        } catch (JSONException e) {
            mHandler.sendError("JSONException:" + e.getMessage());
        } finally {
            bInCallback = false;
        }
        return retval;
    }

    public boolean isThinking() {
        return getEngineState() == ZebraEngine.ES_PLAYINPROGRESS;
    }

    public boolean isValidMove(Move move) {
        if (mValidMoves == null)
            return false;

        boolean valid = false;
        for (int m : mValidMoves)
            if (m == move.mMove) {
                valid = true;
                break;
            }
        return valid;
    }

    public boolean isHumanToMove() {
        return mPlayerInfo[mSideToMove].skill == 0;
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

    private native void zeSetSlack(int slack);

    private native void zeSetPerturbation(int perturbation);

    private native void zeSetForcedOpening(String opening_name);

    private native void zeSetHumanOpenings(int enable);

    private native void zeSetPracticeMode(int enable);

    private native void zeSetUseBook(int enable);

    private native boolean zeGameInProgress();

    private native void zeAnalyzeGame(int providedMoveCount, byte[] providedMoves);

    public native void zeJsonTest(JSONObject json);

    static {
        System.loadLibrary("droidzebra");
    }

}
