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
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.*;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;

import static de.earthlingz.oerszebra.GameSettingsConstants.*;

// DroidZebra -> ZebraEngine:public -async-> ZebraEngine thread(jni) -> Callback() -async-> DroidZebra:Handler
public class ZebraEngine {

    private static final int BOARD_SIZE = 8;

    private static String PATTERNS_FILE = "coeffs2.bin";
    private static String BOOK_FILE = "book.bin";
    private static String BOOK_FILE_COMPRESSED = "book.cmp.z";

    // board colors
    static public final byte PLAYER_BLACK = 0;
    static public final byte PLAYER_EMPTY = 1; // for board color
    static public final byte PLAYER_WHITE = 2;

    private static final int PLAYER_ZEBRA = 1; // for zebra skill in PlayerInfo

    // default parameters
    private static final int INFINITE_TIME = 10000000;
    private static ZebraEngine engine;
    private final EngineThread engineThread = new EngineThread();
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
    private static final int
            ES_INITIAL = 0,
            ES_READY2PLAY = 1,
            ES_PLAY = 2,
            ES_PLAY_IN_PROGRESS = 3,
            ES_USER_INPUT_WAIT = 4;

    private static final int
            UI_EVENT_EXIT = 0,
            UI_EVENT_MOVE = 1,
            UI_EVENT_UNDO = 2,
            UI_EVENT_SETTINGS_CHANGE = 3,
            UI_EVENT_REDO = 4;
    private PlayerInfo blackPlayerInfo = new PlayerInfo(0, 0, 0);
    private PlayerInfo zebraPlayerInfo = new PlayerInfo(4, 12, 12);
    private PlayerInfo whitePlayerInfo = new PlayerInfo(0, 0, 0);


    private transient GameState initialGameState;
    private transient GameState currentGameState;

    // current move
    private JSONObject mPendingEvent = null;
    private int mValidMoves[] = null;

    // mPlayerInfoChanged must be always inside synchronized block with playerInfoLock
    // :/ we must change how this work in the future (or we could make whole class synchronized? is it slow?)
    private boolean mPlayerInfoChanged = false;
    private final Object playerInfoLock = new Object();

    private int mSideToMove = PLAYER_ZEBRA;

    // context
    private GameContext mContext;

    // files folder
    private File mFilesDir;

    // synchronization
    static private final Object mJNILock = new Object();

    private final transient Object engineStateEventLock = new Object();

    private int mEngineState = ES_INITIAL;

    private boolean isRunning = false;

    private boolean bInCallback = false;
    private OnEngineErrorListener onErrorListener = new OnEngineErrorListener() {
    };
    private OnEngineDebugListener onDebugListener = new OnEngineDebugListener() {
    };
    private OnGameStateReadyListener onGameStateReadyListener = new OnGameStateReadyListener() {
    };

    private ZebraEngine(GameContext context) {
        mContext = context;
        engineThread.start();
    }


    private boolean initFiles() {
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

    private void waitForEngineState(int state, int milliseconds) {
        synchronized (engineStateEventLock) {
            if (mEngineState != state)
                try {
                    engineStateEventLock.wait(milliseconds);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }

        }
    }

    private void waitForEngineState(int state) {
        synchronized (engineStateEventLock) {
            while (mEngineState != state && isRunning)
                try {
                    engineStateEventLock.wait();
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
        }
    }

    private void setEngineStatePlay() {
        setEngineState(ZebraEngine.ES_PLAY);
    }

    private void setEngineState(int state) {
        synchronized (engineStateEventLock) {
            mEngineState = state;
            engineStateEventLock.notifyAll();
        }
    }

    private void setRunning(boolean b) {
        boolean wasRunning = isRunning;
        isRunning = b;
        if (wasRunning && !isRunning) stopGame();
    }

    // tell zebra to stop thinking
    private void stopMove() {
        zeForceReturn();
    }

    // tell zebra to end current game
    private void stopGame() {
        zeForceExit();
        // if waiting for move - get back into the engine
        // every other state should work itself out
        if (mEngineState == ES_USER_INPUT_WAIT) {
            mPendingEvent = new JSONObject();
            try {
                mPendingEvent.put("type", UI_EVENT_EXIT);
            } catch (JSONException e) {
                // Log.getStackTraceString(e);
            }
            setEngineState(ES_PLAY);
        }
    }

    public void makeMove(GameState gameState, Move move) throws InvalidMove {
        if (gameState != currentGameState) {
            //TODO switch context and play
            return;
        }
        if (!isValidMove(move))
            throw new InvalidMove();

        // if thinking on human time - stop
        stopIfThinkingOnHumanTime();

        if (mEngineState != ES_USER_INPUT_WAIT) {
            // Log.d("ZebraEngine", "Invalid Engine State");
            return;
        }

        // add move the the pending event and tell zebra to pick it up
        mPendingEvent = new JSONObject();
        try {
            mPendingEvent.put("type", UI_EVENT_MOVE);
            mPendingEvent.put("move", move.getMoveInt());
        } catch (JSONException e) {
            // Log.getStackTraceString(e);
        }
        setEngineState(ES_PLAY);
    }

    /**
     * This is needed when zebra is thinking on practice mode but user wants to play - as I found out, maybe it is needed for something else too
     */
    private void stopIfThinkingOnHumanTime() {
        if (isThinkingOnHumanTime()) {
            stopMove();
            waitForEngineState(ES_USER_INPUT_WAIT, 1000);
        }
    }

    public void undoMove(GameState gameState) {
        if (currentGameState != gameState) {
            //TODO switch context and undo on that board - later we might do it completely without engine -
            // TODO why would you need it here right?
            return;
        }
        // if thinking on human time - stop
        stopIfThinkingOnHumanTime();

        if (mEngineState != ES_USER_INPUT_WAIT) {
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

    public void redoMove(GameState gameState) {
        if (currentGameState != gameState) {
            //TODO switch context
            return;
        }

        // if thinking on human time - stop
        stopIfThinkingOnHumanTime();

        if (mEngineState != ES_USER_INPUT_WAIT) {
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

    private boolean isThinkingOnHumanTime() {
        return isHumanToMove(currentGameState)
                && isThinking();
    }

    // notifications that some settings have changes - see if we care
    private void sendSettingsChanged() {
        // if we are waiting for input - restart the move (e.g. if sides switched)
        if (mEngineState == ES_USER_INPUT_WAIT) {
            mPendingEvent = new JSONObject();
            try {
                mPendingEvent.put("type", UI_EVENT_SETTINGS_CHANGE);
            } catch (JSONException e) {
                // Log.getStackTraceString(e);
            }
            setEngineState(ES_PLAY);
        }
    }

    private void sendReplayMoves(List<Move> moves) {
        if (mEngineState != ZebraEngine.ES_READY2PLAY) {
            stopGame();
            waitForEngineState(ZebraEngine.ES_READY2PLAY);
        }
        initialGameState = new GameState(BOARD_SIZE, moves);

        setEngineState(ES_PLAY);
    }

    // settings helpers


    private void setEngineFunction(int settingFunction, int depth, int depthExact, int depthWLD) {
        switch (settingFunction) {
            case FUNCTION_HUMAN_VS_HUMAN:
                setBlackPlayerInfo(new PlayerInfo(0, 0, 0));
                setWhitePlayerInfo(new PlayerInfo(0, 0, 0));
                break;
            case FUNCTION_ZEBRA_BLACK:
                setBlackPlayerInfo(new PlayerInfo(depth, depthExact, depthWLD));
                setWhitePlayerInfo(new PlayerInfo(0, 0, 0));
                break;
            case FUNCTION_ZEBRA_VS_ZEBRA:
                setBlackPlayerInfo(new PlayerInfo(depth, depthExact, depthWLD));
                setWhitePlayerInfo(new PlayerInfo(depth, depthExact, depthWLD));
                break;
            case FUNCTION_ZEBRA_WHITE:
            default:
                setBlackPlayerInfo(new PlayerInfo(0, 0, 0));
                setWhitePlayerInfo(new PlayerInfo(depth, depthExact, depthWLD));
                break;
        }
        setZebraPlayerInfo(new PlayerInfo(depth + 1, depthExact + 1, depthWLD + 1));
    }

    private void setAutoMakeMoves(boolean _settingAutoMakeForcedMoves) {
        if (_settingAutoMakeForcedMoves)
            zeSetAutoMakeMoves(1);
        else
            zeSetAutoMakeMoves(0);
    }

    private void setSlack(int _slack) {
        zeSetSlack(_slack);
    }

    private void setPerturbation(int _perturbation) {
        zeSetPerturbation(_perturbation);
    }

    private void setForcedOpening(String _openingName) {
        zeSetForcedOpening(_openingName);
    }

    private void setHumanOpenings(boolean _enable) {
        if (_enable)
            zeSetHumanOpenings(1);
        else
            zeSetHumanOpenings(0);
    }

    private void setPracticeMode(boolean _enable) {
        if (_enable)
            zeSetPracticeMode(1);
        else
            zeSetPracticeMode(0);
    }

    private void setUseBook(boolean _enable) {
        if (_enable)
            zeSetUseBook(1);
        else
            zeSetUseBook(0);
    }

    private void setZebraPlayerInfo(PlayerInfo playerInfo) {
        synchronized (playerInfoLock) {
            zebraPlayerInfo = playerInfo;
            mPlayerInfoChanged = true;
        }
    }

    private void setWhitePlayerInfo(PlayerInfo playerInfo) {
        synchronized (playerInfoLock) {
            whitePlayerInfo = playerInfo;
            mPlayerInfoChanged = true;
        }
    }

    private void setBlackPlayerInfo(PlayerInfo playerInfo) {
        synchronized (playerInfoLock) {
            blackPlayerInfo = playerInfo;
            mPlayerInfoChanged = true;
        }
    }

    private void setComputerMoveDelay(int delay) {
        computerMoveDelay = delay;
    }

    private void setInitialGameState(LinkedList<Move> moves) {
        ZebraEngine.this.initialGameState = new GameState(BOARD_SIZE, moves);
    }

    // gamestate manipulators
    private void setInitialGameState(int moveCount, byte[] moves) {
        initialGameState = new GameState(BOARD_SIZE, moves, moveCount);
    }

    private void setPlayerInfos() {
        zeSetPlayerInfo(
                PLAYER_BLACK,
                blackPlayerInfo.skill,
                blackPlayerInfo.exactSolvingSkill,
                blackPlayerInfo.wldSolvingSkill,
                INFINITE_TIME,
                0
        );
        zeSetPlayerInfo(
                PLAYER_WHITE,
                whitePlayerInfo.skill,
                whitePlayerInfo.exactSolvingSkill,
                whitePlayerInfo.wldSolvingSkill,
                INFINITE_TIME,
                0
        );
        zeSetPlayerInfo(
                PLAYER_ZEBRA,
                zebraPlayerInfo.skill,
                zebraPlayerInfo.exactSolvingSkill,
                zebraPlayerInfo.wldSolvingSkill,
                INFINITE_TIME,
                0
        );
    }

    // TODO when we want it, then we do it ;), for now it's not clear how it should work so I commented it out
//    public void analyzeGame(List<Move> moves) {
//        byte[] bytes = toByte(moves);
//        zeAnalyzeGame(moves.size(), bytes);
//    }

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
                    if (mEngineState == ES_INITIAL) {
                        // delete .bin files if initialization failed
                        // will be recreated from resources
                        new File(mFilesDir, PATTERNS_FILE).delete();
                        new File(mFilesDir, BOOK_FILE).delete();
                        new File(mFilesDir, BOOK_FILE_COMPRESSED).delete();
                    }
                    String error = data.getString("error");
                    ZebraEngine.this.onErrorListener.onError(error);
                }
                break;

                case MSG_DEBUG: {
                    String message = data.getString("message");
                    ZebraEngine.this.onDebugListener.onDebug(message);
                }
                break;

                case MSG_BOARD: {

                    JSONArray boardJSON = data.getJSONArray("board");
                    JSONObject whiteInfoJSON = data.getJSONObject("white");
                    JSONObject blackInfoJSON = data.getJSONObject("black");
                    JSONArray blackMovesJSON = blackInfoJSON.getJSONArray("moves");
                    JSONArray whiteMovesJSON = whiteInfoJSON.getJSONArray("moves");
                    int sideToMove = data.getInt("side_to_move");
                    int disksPlayed = data.getInt("disks_played");
                    String blackTime = blackInfoJSON.getString("time");
                    float blackEval = (float) blackInfoJSON.getDouble("eval");
                    int blackDiscCount = blackInfoJSON.getInt("disc_count");
                    String whiteTime = whiteInfoJSON.getString("time");
                    float whiteEval = (float) whiteInfoJSON.getDouble("eval");
                    int whiteDiscCOunt = whiteInfoJSON.getInt("disc_count");

                    MoveList blackMoveList = new MoveList(blackMovesJSON);
                    MoveList whiteMoveList = new MoveList(whiteMovesJSON);

                    ByteBoard byteBoard = new ByteBoard(boardJSON, BOARD_SIZE);

                    currentGameState.updateGameState(sideToMove, disksPlayed, blackTime, blackEval, blackDiscCount, whiteTime, whiteEval, whiteDiscCOunt, blackMoveList, whiteMoveList, byteBoard);

                }
                break;

                case MSG_CANDIDATE_MOVES: {
                    JSONArray jscmoves = data.getJSONArray("moves");
                    CandidateMove cmoves[] = new CandidateMove[jscmoves.length()];
                    mValidMoves = new int[jscmoves.length()];
                    for (int i = 0; i < jscmoves.length(); i++) {
                        int move = jscmoves.getJSONObject(i).getInt("move");
                        mValidMoves[i] = move;
                        cmoves[i] = new CandidateMove(move);
                    }
                    currentGameState.setCandidateMoves(cmoves);
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

                    setEngineState(ES_PLAY_IN_PROGRESS);

                    mValidMoves = null;
                    mPendingEvent = null;
                }
                break;

                case MSG_PASS: {
                    setEngineState(ES_USER_INPUT_WAIT);
                    currentGameState.sendPass();
                    waitForEngineState(ES_PLAY);
                    setEngineState(ES_PLAY_IN_PROGRESS);
                }
                break;
                case MSG_ANALYZE_GAME: {
                    setEngineState(ES_USER_INPUT_WAIT);
                    //currentGameState.sendMessage(msg);
                    waitForEngineState(ES_PLAY);
                    setEngineState(ES_PLAY_IN_PROGRESS);
                }
                break;

                case MSG_OPENING_NAME: {
                    currentGameState.setOpening(data.getString("opening"));

                }
                break;

                case MSG_LAST_MOVE: {
                    currentGameState.setLastMove(data.getInt("move"));
                }
                break;

                case MSG_NEXT_MOVE: {
                    currentGameState.setNextMove(data.getInt("move"));

                }
                break;

                case MSG_GAME_START: {
                    currentGameState.sendGameStart();
                }
                break;

                case MSG_GAME_OVER: {
                    currentGameState.sendGameOver();
                }
                break;

                case MSG_MOVE_START: {
                    mMoveStartTime = android.os.SystemClock.uptimeMillis();

                    mSideToMove = data.getInt("side_to_move");//TODO shouldn't this be set on GameState now?

                    // can change player info here
                    synchronized (playerInfoLock) {
                        if (mPlayerInfoChanged) {
                            setPlayerInfos();
                            mPlayerInfoChanged = false;
                        }
                    }

                    currentGameState.sendMoveStart();
                }
                break;

                case MSG_MOVE_END: {
                    // introduce delay between moves made by the computer without user input
                    // so we can actually to see that the game is being played :)
                    // TODO thanks a lot :D, now it will be way more problematic to handle shared use of the engine
                    // TODO this should be handled entirely by UI. Stopping time-critical engine thread for the sake of UI is just ridiculous

                    if (computerMoveDelay > 0 && !isHumanToMove(currentGameState)) {
                        long moveEnd = android.os.SystemClock.uptimeMillis();
                        if ((moveEnd - mMoveStartTime) < computerMoveDelay) {
                            android.os.SystemClock.sleep(computerMoveDelay - (moveEnd - mMoveStartTime));
                        }
                    }
                    currentGameState.sendMoveEnd();
                }
                break;

                case MSG_EVAL_TEXT: {
                    String eval = data.getString("eval");
                    currentGameState.sendEval(eval);
                }
                break;

                case MSG_PV: {
                    JSONArray zeArray = data.getJSONArray("pv");
                    int len = zeArray.length();
                    byte[] moves = new byte[len];
                    for (int i = 0; i < len; i++)
                        moves[i] = (byte) zeArray.getInt(i);
                    currentGameState.sendPv(moves);
                }
                break;

                case MSG_CANDIDATE_EVALS: {
                    JSONArray jscevals = data.getJSONArray("evals");
                    CandidateMove cmoves[] = new CandidateMove[jscevals.length()];
                    for (int i = 0; i < jscevals.length(); i++) {
                        JSONObject jsceval = jscevals.getJSONObject(i);
                        cmoves[i] = new CandidateMove(
                                (jsceval.getInt("move")),
                                jsceval.getString("eval_s"),
                                jsceval.getString("eval_l"),
                                (jsceval.getInt("best") != 0)
                        );
                    }
                    currentGameState.addCandidateMoveEvals(cmoves);

                }
                break;

                default: {
                    onErrorListener.onError(String.format(Locale.getDefault(), "Unknown message ID %d", msgcode));
                }
                break;
            }
        } catch (JSONException e) {
            onErrorListener.onError("JSONException:" + e.getMessage());
        } finally {
            bInCallback = false;
        }
        return retval;
    }

    private boolean isThinking() {
        return mEngineState == ZebraEngine.ES_PLAY_IN_PROGRESS;
    }

    private boolean isValidMove(Move move) {
        if (mValidMoves == null)
            return false;

        boolean valid = false;
        for (int m : mValidMoves)
            if (m == move.getMoveInt()) {
                valid = true;
                break;
            }
        return valid;
    }

    public boolean isHumanToMove(GameState gameState, EngineConfig config) {
        if (gameState != currentGameState) {
            return false; //TODO switch context
        }
        return isHumanToMove(gameState);
    }

    private boolean isHumanToMove(GameState gameState) {
        return getSideToMovePlayerInfo().skill == 0;
    }

    private PlayerInfo getSideToMovePlayerInfo() {
        if (mSideToMove == PLAYER_BLACK) {
            return blackPlayerInfo;
        }
        if (mSideToMove == PLAYER_WHITE) {
            return whitePlayerInfo;
        }
        return zebraPlayerInfo;
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

    private native void zeJsonTest(JSONObject json);

    static {
        System.loadLibrary("droidzebra");
    }

    void waitForReadyToPlay() {
        waitForEngineState(ZebraEngine.ES_READY2PLAY);
    }

    private boolean isReadyToPlay() {
        return mEngineState == ZebraEngine.ES_READY2PLAY;
    }

    private void kill() { //TODO remove this, no one should be able to kill the engine
        boolean retry = true;
        setRunning(false);
        engineThread.interrupt(); // if waiting
        while (retry) {
            try {
                engineThread.join();
                retry = false;
            } catch (InterruptedException e) {
                Log.wtf("wtf", e);
            }
        }
    }

    public static synchronized ZebraEngine get(GameContext ctx) { //TODO this is still kinda risky..
        if (engine == null ) {
            engine = new ZebraEngine(ctx);
        }
        return engine;

    }

    public void setOnErrorListener(OnEngineErrorListener onErrorListener) {
        if (onErrorListener == null) {
            ZebraEngine.this.onErrorListener = new OnEngineErrorListener() {
            };
        } else {
            ZebraEngine.this.onErrorListener = onErrorListener;
        }
    }

    public void setOnDebugListener(OnEngineDebugListener listener) {
        if (listener == null)
            ZebraEngine.this.onDebugListener = new OnEngineDebugListener() {
            };
        else
            ZebraEngine.this.onDebugListener = listener;
    }

    private void loadConfig(EngineConfig cfg) {
        setEngineFunction(cfg.engineFunction, cfg.depth, cfg.depthExact, cfg.depthWLD);

        setAutoMakeMoves(cfg.autoForcedMoves);
        setForcedOpening(cfg.forcedOpening);
        setHumanOpenings(cfg.humanOpenings);
        setPracticeMode(cfg.practiceMode);
        setUseBook(cfg.useBook);

        setSlack(cfg.slack);
        setPerturbation(cfg.perturbation);

        setComputerMoveDelay((cfg.engineFunction != FUNCTION_HUMAN_VS_HUMAN) ? cfg.computerMoveDelay : 0);
        sendSettingsChanged();
    }

    private void waitForReadyToPlay(final Runnable completion) {
        new CompletionAsyncTask(completion, this)
                .execute();
    }

    public void newGame(EngineConfig engineConfig, OnGameStateReadyListener onGameStateReadyListener) {
        this.onGameStateReadyListener = onGameStateReadyListener;
        if (!isReadyToPlay()) {
            stopGame();
        }
        waitForReadyToPlay(
                () -> {
                    loadConfig(engineConfig);
                    setEngineStatePlay();
                }
        );
    }

    public void newGame(LinkedList<Move> fromMoves, EngineConfig engineConfig, OnGameStateReadyListener onGameStateReadyListener) {
        engine.setInitialGameState(fromMoves);
        newGame(engineConfig, onGameStateReadyListener);
    }

    public void newGame(byte[] fromMoves, int movesCount, EngineConfig engineConfig, OnGameStateReadyListener onGameStateReadyListener) {
        engine.setInitialGameState(movesCount, fromMoves);
        newGame(engineConfig, onGameStateReadyListener);
    }

    public void onReady(Runnable onEngineReady) {
        new CompletionAsyncTask(onEngineReady, this)
                .execute();
    }

    public void stopIfThinking(GameState gameState) {
        if (gameState != currentGameState) {
            return; //TODO switch context
        }
        if (engine.isThinking()) {
            engine.stopMove();
        }
    }

    public void loadEvals(GameState gameState, EngineConfig engineConfig) {
        if (currentGameState != gameState) {
            return; //TODO switch context
        }
        loadConfig(engineConfig.practiceMode ? engineConfig : engineConfig.alterPracticeMode(true));
    }

    public void updateConfig(GameState gameState, EngineConfig engineConfig) {
        if (currentGameState != gameState) {
            return; //TODO switch context
        }
        loadConfig(engineConfig);
    }

    public void pass(GameState gameState, EngineConfig config) {
        if (currentGameState != gameState) {
            return; //TODO switch context
        }
        setEngineStatePlay();
    }

    public boolean isThinking(GameState gameState) {
        if (currentGameState != gameState) {
            return false; //TODO switch context
        }
        return isThinking();
    }

    public void disconnect(GameState gameState) {
        if (gameState != currentGameState) {
            //already disconnected
            return;
        }
        currentGameState = new GameState(BOARD_SIZE);
        stopGame();
    }

    public interface OnEngineErrorListener {
        default void onError(String error) {
            Log.v("OersZebra", error);
        }
    }

    public interface OnEngineDebugListener {
        default void onDebug(String message) {
        }
    }

    public interface OnGameStateReadyListener {
        default void onGameStateReady(GameState gameState) {
        }
    }


    private class EngineThread extends Thread {
        // zebra thread
        @Override
        public void run() {
            setRunning(true);

            setEngineState(ES_INITIAL);

            // init data files
            if (!initFiles()) return;

            synchronized (mJNILock) {
                zeGlobalInit(mFilesDir.getAbsolutePath());
                zeSetPlayerInfo(PLAYER_BLACK, 0, 0, 0, INFINITE_TIME, 0);
                zeSetPlayerInfo(PLAYER_WHITE, 0, 0, 0, INFINITE_TIME, 0);
            }

            setEngineState(ES_READY2PLAY);

            while (isRunning) {
                waitForEngineState(ES_PLAY);

                if (!isRunning) break; // something may have happened while we were waiting

                setEngineState(ES_PLAY_IN_PROGRESS);

                synchronized (mJNILock) {
                    setPlayerInfos();

                    currentGameState = new GameState(BOARD_SIZE);
                    OnGameStateReadyListener listener = onGameStateReadyListener;
                    onGameStateReadyListener = new OnGameStateReadyListener() {
                    };
                    listener.onGameStateReady(currentGameState);


                    if (initialGameState != null) {
                        GameState initialGameState = ZebraEngine.this.initialGameState;
                        ZebraEngine.this.initialGameState = null;
                        byte[] providedMoves = initialGameState.exportMoveSequence();
                        zePlay(providedMoves.length, providedMoves);
                    } else
                        zePlay(0, null);

                }

                setEngineState(ES_READY2PLAY);
                //setEngineState(ES_PLAY);  // test
            }

            synchronized (mJNILock) {
                zeGlobalTerminate();
            }
        }
    }
}
