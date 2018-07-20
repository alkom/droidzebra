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

package de.earthlingz.oerszebra;

import android.app.ActionBar;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.ClipboardManager;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.util.Log;
import android.view.*;
import android.widget.Button;
import android.widget.TextView;
import com.shurik.droidzebra.*;
import de.earthlingz.oerszebra.BoardView.BoardView;
import de.earthlingz.oerszebra.BoardView.BoardViewModel;
import de.earthlingz.oerszebra.parser.GameParser;

import java.lang.ref.WeakReference;
import java.util.Calendar;
import java.util.Date;
import java.util.LinkedList;
import java.util.Locale;

import static de.earthlingz.oerszebra.GameSettingsConstants.*;
import static de.earthlingz.oerszebra.GlobalSettingsLoader.*;


public class DroidZebra extends FragmentActivity implements MoveStringConsumer,
        OnSettingsChangedListener, BoardView.OnMakeMoveListener, GameStateListener, ZebraEngine.OnEngineErrorListener {
    private ClipboardManager clipboard;
    private ZebraEngine engine;


    private boolean mBusyDialogUp = false;
    private boolean isHintUp = false;
    private boolean mIsInitCompleted = false;
    private boolean mActivityActive = false;

    private BoardView mBoardView;
    private StatusView mStatusView;

    private BoardViewModel state = ZebraServices.getBoardState();

    private GameParser parser = ZebraServices.getGameParser();
    private WeakReference<AlertDialog> alert = null;

    public SettingsProvider settingsProvider;
    private GameStateListener handler = new GameStateHandlerProxy(this);
    private GameState gameState;
    private EngineConfig engineConfig;


    public void resetStateAndStatusView() {
        getState().reset();
        if (mStatusView != null)
            mStatusView.clear();
    }

    public boolean evalsDisplayEnabled() {
        return settingsProvider.isSettingPracticeMode() || isHintUp;
    }

    public void startNewGameAndResetUI() {
        startNewGame();

        resetStateAndStatusView();
        loadUISettings();

    }

    private void startNewGame() {
        engine.newGame(engineConfig, new ZebraEngine.OnGameStateReadyListener() {
            @Override
            public void onGameStateReady(GameState gameState) {
                DroidZebra.this.gameState = gameState;
                gameState.setHandler(handler);
            }
        });
    }

    /* Creates the menu items */
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.menu, menu);
        return super.onCreateOptionsMenu(menu);
    }

    public boolean initialized() {
        return mIsInitCompleted;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (!mIsInitCompleted) return false;
        switch (item.getItemId()) {
            case R.id.menu_new_game:
                startNewGameAndResetUI();
                return true;
            case R.id.menu_quit:
                showQuitDialog();
                return true;
            case R.id.menu_take_back:
                engine.undoMove(gameState);
                return true;
            case R.id.menu_take_redo:
                engine.redoMove(gameState);
                return true;
            case R.id.menu_settings: {
                // Launch Preference activity
                Intent i = new Intent(this, SettingsPreferences.class);
                startActivity(i);
            }
            return true;
            case R.id.menu_switch_sides: {
                switchSides();
            }
            break;
            case R.id.menu_enter_moves: {
                enterMoves();
            }
            break;
            case R.id.menu_mail: {
                sendMail();
            }
            return true;
            case R.id.menu_hint: {
                showHint();
            }
            return true;
        }
        return false;
    }

    @Override
    protected void onNewIntent(Intent intent) {

        String action = intent.getAction();
        String type = intent.getType();

        Log.i("Intent", type + " " + action);

        if (Intent.ACTION_SEND.equals(action) && type != null) {
            switch (type) {
                case "text/plain":
                    consumeMovesString(intent.getDataString()); // Handle text being sent

                    break;
                case "message/rfc822":
                    Log.i("Intent", intent.getStringExtra(Intent.EXTRA_TEXT));
                    consumeMovesString(intent.getStringExtra(Intent.EXTRA_TEXT)); // Handle text being sent

                    break;
                default:
                    Log.e("intent", "unknown intent");
                    break;
            }
        } else {
            Log.e("intent", "unknown intent");
        }
    }

    /**
     * Called when the activity is first created.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        resetStateAndStatusView();

        clipboard = (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);

        setContentView(R.layout.spash_layout);
        hideActionBar();

        engine = ZebraEngine.get(new AndroidContext(getApplicationContext()));
        this.settingsProvider = new GlobalSettingsLoader(this);
        this.settingsProvider.setOnSettingsChangedListener(this);
        this.engineConfig = settingsProvider.createEngineConfig();

        Intent intent = getIntent();
        String action = intent.getAction();
        String type = intent.getType();

        Log.i("Intent", type + " " + action);
        engine.setOnErrorListener(this); //TODO don't forget to remove later to avoid memory leak

        engine.onReady(() -> {
            setContentView(R.layout.board_layout);
            showActionBar();
            mBoardView = (BoardView) findViewById(R.id.board);
            mStatusView = (StatusView) findViewById(R.id.status_panel);
            mBoardView.setBoardViewModel(getState());
            mBoardView.setOnMakeMoveListener(this);
            mBoardView.requestFocus();


            if (Intent.ACTION_SEND.equals(action) && type != null) {
                if ("text/plain".equals(type) || "message/rfc822".equals(type)) {
                    startNewGameAndResetUI(parser.makeMoveList(intent.getStringExtra(Intent.EXTRA_TEXT)));
                }
            } else if (savedInstanceState != null
                    && savedInstanceState.containsKey("moves_played_count")
                    && savedInstanceState.getInt("moves_played_count") > 0) {
                startNewGameAndResetUI(savedInstanceState.getInt("moves_played_count"), savedInstanceState.getByteArray("moves_played"));
            } else {
                startNewGameAndResetUI();
            }


            mIsInitCompleted = true;
        });
    }


    private void startNewGameAndResetUI(LinkedList<Move> moves) {
        engine.newGame(moves, engineConfig, new ZebraEngine.OnGameStateReadyListener() {
            @Override
            public void onGameStateReady(GameState gameState1) {
                DroidZebra.this.gameState = gameState1;
                gameState1.setHandler(handler);
            }
        });

        resetStateAndStatusView();
        loadUISettings();

    }

    private void startNewGameAndResetUI(int moves_played_count, byte[] moves_played) {
        engine.newGame(moves_played, moves_played_count, engineConfig, new ZebraEngine.OnGameStateReadyListener() {
            @Override
            public void onGameStateReady(GameState gameState1) {
                DroidZebra.this.gameState = gameState1;
                gameState1.setHandler(handler);
            }
        });

        resetStateAndStatusView();
        loadUISettings();

    }

    private void showActionBar() {
        ActionBar actionBar = getActionBar();
        if (actionBar != null) {
            actionBar.show();
        }
    }

    private void hideActionBar() {
        ActionBar actionBar = getActionBar();
        if (actionBar != null) {
            actionBar.hide();
        }
    }

    private void loadEngineSettings() {
        this.engineConfig = settingsProvider.createEngineConfig();
        if (engine != null) {
            engine.updateConfig(gameState, engineConfig);
        }
    }

    private void loadUISettings() {
        if (mBoardView != null) {
            mBoardView.setDisplayAnimations(settingsProvider.isSettingDisplayEnableAnimations());
            mBoardView.setAnimationDuration(settingsProvider.getSettingAnimationDuration());
            mBoardView.setDisplayLastMove(settingsProvider.isSettingDisplayLastMove());
            mBoardView.setDisplayMoves(settingsProvider.isSettingDisplayMoves());
            mBoardView.setDisplayEvals(evalsDisplayEnabled());
        }


        int depth = settingsProvider.getSettingZebraDepth();
        int depthExact = settingsProvider.getSettingZebraDepthExact();
        int depthWLD = settingsProvider.getSettingZebraDepthWLD();

        mStatusView.setTextForID(
                StatusView.ID_SCORE_SKILL,
                String.format(getString(R.string.display_depth), depth, depthExact, depthWLD)
        );


        if (!settingsProvider.isSettingDisplayPv()) {
            mStatusView.setTextForID(StatusView.ID_STATUS_PV, "");
            mStatusView.setTextForID(StatusView.ID_STATUS_EVAL, "");
        }
    }


    private void sendMail() {
        //GetNowTime
        Calendar calendar = Calendar.getInstance();
        Date nowTime = calendar.getTime();
        StringBuilder sbBlackPlayer = new StringBuilder();
        StringBuilder sbWhitePlayer = new StringBuilder();
        GameState gs = gameState;
        SharedPreferences settings = getSharedPreferences(SHARED_PREFS_NAME, 0);


        Intent intent = new Intent();
        Intent chooser = Intent.createChooser(intent, "");

        intent.setAction(Intent.ACTION_SEND);
        intent.setType("message/rfc822");
        intent.putExtra(
                Intent.EXTRA_EMAIL,
                new String[]{settings.getString(SETTINGS_KEY_SENDMAIL, DEFAULT_SETTING_SENDMAIL)});

        intent.putExtra(Intent.EXTRA_SUBJECT, getResources().getString(R.string.app_name));

        //get BlackPlayer and WhitePlayer
        switch (settingsProvider.getSettingFunction()) { //TODO this might cause a problem, because settings provider is not a source of truth here. It should be taken from ZebraEngine
            case FUNCTION_HUMAN_VS_HUMAN:
                sbBlackPlayer.append("Player");
                sbWhitePlayer.append("Player");
                break;
            case FUNCTION_ZEBRA_BLACK:
                sbBlackPlayer.append("DroidZebra-");
                sbBlackPlayer.append(settingsProvider.getSettingZebraDepth());
                sbBlackPlayer.append("/");
                sbBlackPlayer.append(settingsProvider.getSettingZebraDepthExact());
                sbBlackPlayer.append("/");
                sbBlackPlayer.append(settingsProvider.getSettingZebraDepthWLD());

                sbWhitePlayer.append("Player");
                break;
            case FUNCTION_ZEBRA_WHITE:
                sbBlackPlayer.append("Player");

                sbWhitePlayer.append("DroidZebra-");
                sbWhitePlayer.append(settingsProvider.getSettingZebraDepth());
                sbWhitePlayer.append("/");
                sbWhitePlayer.append(settingsProvider.getSettingZebraDepthExact());
                sbWhitePlayer.append("/");
                sbWhitePlayer.append(settingsProvider.getSettingZebraDepthWLD());
                break;
            case FUNCTION_ZEBRA_VS_ZEBRA:
                sbBlackPlayer.append("DroidZebra-");
                sbBlackPlayer.append(settingsProvider.getSettingZebraDepth());
                sbBlackPlayer.append("/");
                sbBlackPlayer.append(settingsProvider.getSettingZebraDepthExact());
                sbBlackPlayer.append("/");
                sbBlackPlayer.append(settingsProvider.getSettingZebraDepthWLD());

                sbWhitePlayer.append("DroidZebra-");
                sbWhitePlayer.append(settingsProvider.getSettingZebraDepth());
                sbWhitePlayer.append("/");
                sbWhitePlayer.append(settingsProvider.getSettingZebraDepthExact());
                sbWhitePlayer.append("/");
                sbWhitePlayer.append(settingsProvider.getSettingZebraDepthWLD());
            default:
        }
        StringBuilder sb = new StringBuilder();
        sb.append(getResources().getString(R.string.mail_generated));
        sb.append("\r\n");
        sb.append(getResources().getString(R.string.mail_date));
        sb.append(" ");
        sb.append(nowTime);
        sb.append("\r\n\r\n");
        sb.append(getResources().getString(R.string.mail_move));
        sb.append(" ");
        String sbMovesString = gs.getMoveSequenceAsString();
        sb.append(sbMovesString);
        sb.append("\r\n\r\n");
        sb.append(sbBlackPlayer.toString());
        sb.append("  (B)  ");
        sb.append(getState().getBlackScore());
        sb.append(":");
        sb.append(getState().getWhiteScore());
        sb.append("  (W)  ");
        sb.append(sbWhitePlayer.toString());

        intent.putExtra(Intent.EXTRA_TEXT, sb.toString());
        // Intent
        // Verify the original intent will resolve to at least one activity
        if (intent.resolveActivity(getPackageManager()) != null) {
            startActivity(chooser);
        }
    }

    private void switchSides() {
        int newFunction = -1;

        if (settingsProvider.getSettingFunction() == FUNCTION_ZEBRA_WHITE)
            newFunction = FUNCTION_ZEBRA_BLACK;
        else if (settingsProvider.getSettingFunction() == FUNCTION_ZEBRA_BLACK)
            newFunction = FUNCTION_ZEBRA_WHITE;

        if (newFunction > 0) {
            SharedPreferences settings = getSharedPreferences(SHARED_PREFS_NAME, 0);
            SharedPreferences.Editor editor = settings.edit();
            editor.putString(SETTINGS_KEY_FUNCTION, String.format(Locale.getDefault(), "%d", newFunction));
            editor.apply();
        }

        loadUISettings();
        loadEngineSettings();

    }

    private void showHint() {
        if (!settingsProvider.isSettingPracticeMode()) {
            setHintUp(true);
            engine.loadEvals(gameState, engineConfig);
        }
    }

    @Override
    protected void onDestroy() {
        engine.disconnect(gameState);
        gameState.removeHandler();

        super.onDestroy();
    }

    void showDialog(DialogFragment dialog, String tag) {
        if (mActivityActive) {
            runOnUiThread(() -> dialog.show(getSupportFragmentManager(), tag));
        }
    }

    public void showPassDialog() {
        DialogFragment newFragment = DialogPass.newInstance();
        showDialog(newFragment, "dialog_pass");
    }

    public void showGameOverDialog() {
        DialogFragment newFragment = DialogGameOver.newInstance();
        showDialog(newFragment, "dialog_gameover");
    }

    public void showQuitDialog() {
        DialogFragment newFragment = DialogQuit.newInstance();
        showDialog(newFragment, "dialog_quit");
    }

    private void enterMoves() {
        DialogFragment newFragment = EnterMovesDialog.newInstance(clipboard);
        showDialog(newFragment, "dialog_moves");
    }

    public void consumeMovesString(String s) {
        final LinkedList<Move> moves = parser.makeMoveList(s);
        startNewGameAndResetUI(moves);
    }

    private void showBusyDialog() {
        if (!mBusyDialogUp && engine.isThinking(gameState)) {
            DialogFragment newFragment = DialogBusy.newInstance();
            mBusyDialogUp = true;
            showDialog(newFragment, "dialog_busy");
        }
    }

    public void dismissBusyDialog() {
        if (mBusyDialogUp) {
            Fragment prev = getSupportFragmentManager().findFragmentByTag("dialog_busy");
            if (prev != null) {
                DialogFragment df = (DialogFragment) prev;
                df.dismiss();
            }
            mBusyDialogUp = false;
        }
    }

    public void setHintUp(boolean value) {
        isHintUp = value;
        this.mBoardView.setDisplayEvals(evalsDisplayEnabled());
    }

    public void showAlertDialog(String msg) {
        DroidZebra.this.startNewGameAndResetUI();
        AlertDialog.Builder alertDialog = new AlertDialog.Builder(this);
        alertDialog.setTitle("Zebra Error");
        alertDialog.setMessage(msg);
        alertDialog.setPositiveButton("OK", (dialog, id) -> alert = null);
        runOnUiThread(() -> alert = new WeakReference<>(alertDialog.show()));
    }

    public WeakReference<AlertDialog> getAlert() {
        return alert;
    }

    @Override
    protected void onPause() {
        super.onPause();
        mActivityActive = false;
    }

    @Override
    protected void onResume() {
        super.onResume();
        mActivityActive = true;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK && event.getRepeatCount() == 0) {
            engine.undoMove(gameState);
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        GameState gs = gameState;
        if (gs != null) {
            outState.putByteArray("moves_played", gs.exportMoveSequence());
            outState.putInt("moves_played_count", gs.getDisksPlayed());
            outState.putInt("version", 1);
        }
    }

    public BoardViewModel getState() {
        return state;
    }

    @Override
    public void onSettingsChanged() {
        loadUISettings();
        loadEngineSettings();
    }

    @Override
    public void onMakeMove(Move move) {
        if (getState().isValidMove(move)) {
            // if zebra is still thinking - no move is possible yet - throw a busy dialog
            if (engine.isThinking(gameState) && !engine.isHumanToMove(gameState, engineConfig)) {
                showBusyDialog();
            } else {
                try {
                    engine.makeMove(gameState, move);
                } catch (InvalidMove e) {
                    Log.e("Invalid Move", e.getMessage(), e);
                }
            }
        }
    }

    @Override
    public void onError(String error) {
        this.showAlertDialog(error);
    }

    @Override
    public void onBoard(GameState gameState) {
        int sideToMove = gameState.getSideToMove();

        //triggers animations
        boolean boardChanged = state.update(gameState);

        setStatusViewScores(sideToMove);

        int iStart, iEnd;
        MoveList black_moves = gameState.getBlackPlayer().getMoveList();
        MoveList white_moves = gameState.getWhitePlayer().getMoveList();

        iEnd = black_moves.length();
        iStart = Math.max(0, iEnd - 4);
        for (int i = 0; i < 4; i++) {
            mStatusView.setTextForID(
                    StatusView.ID_SCORELINE_NUM_1 + i,
                    String.format(Locale.getDefault(), "%d", i + iStart + 1)
            );
        }

        for (int i = 0; i < 4; i++) {
            String move_text;
            if (i + iStart < iEnd) {
                move_text = black_moves.getMoveText(i + iStart);
            } else {
                move_text = "";
            }

            mStatusView.setTextForID(
                    StatusView.ID_SCORELINE_BLACK_1 + i,
                    move_text
            );
        }

        iEnd = white_moves.length();
        iStart = Math.max(0, iEnd - 4);
        for (int i = 0; i < 4; i++) {
            String move_text;
            if (i + iStart < iEnd) {
                move_text = white_moves.getMoveText(i + iStart);
            } else {
                move_text = "";
            }
            mStatusView.setTextForID(
                    StatusView.ID_SCORELINE_WHITE_1 + i,
                    move_text
            );
        }


        if (mStatusView != null && gameState.getOpening() != null) {
            mStatusView.setTextForID(
                    StatusView.ID_STATUS_OPENING,
                    gameState.getOpening()
            );
        }
        if (!boardChanged) {
            Log.v("Handler", "invalidate");
            mBoardView.invalidate();
        }
    }

    private void setStatusViewScores(int sideToMove) {
        String scoreText;
        if (sideToMove == ZebraEngine.PLAYER_BLACK) {
            scoreText = String.format(Locale.getDefault(), "•%d", state.getBlackScore());
        } else {
            scoreText = String.format(Locale.getDefault(), "%d", state.getBlackScore());
        }
        mStatusView.setTextForID(
                StatusView.ID_SCORE_BLACK,
                scoreText
        );

        if (sideToMove == ZebraEngine.PLAYER_WHITE) {
            scoreText = String.format(Locale.getDefault(), "%d•", state.getWhiteScore());
        } else {
            scoreText = String.format(Locale.getDefault(), "%d", state.getWhiteScore());
        }
        mStatusView.setTextForID(
                StatusView.ID_SCORE_WHITE,
                scoreText
        );
    }

    @Override
    public void onPass() {
        this.showPassDialog();
    }

    @Override
    public void onGameOver() {
        state.processGameOver();
        runOnUiThread(() -> mBoardView.invalidate());//TODO Id doubt runOnUIThread is necessary here
        this.showGameOverDialog();
    }

    @Override
    public void onMoveEnd() {
        this.dismissBusyDialog();
        if (isHintUp) {
            this.setHintUp(false);
            engine.updateConfig(gameState, engineConfig);
        }

    }

    @Override
    public void onEval(String eval) {
        if (settingsProvider.isSettingDisplayPv()) {
            mStatusView.setTextForID(
                    StatusView.ID_STATUS_EVAL,
                    eval
            );
        }
    }

    @Override
    public void onPv(byte[] pv) {
        if (settingsProvider.isSettingDisplayPv() && pv != null) {
            StringBuilder pvText = new StringBuilder();
            for (byte move : pv) {
                pvText.append(new Move(move).getText());
                pvText.append(" ");
            }
            mStatusView.setTextForID(
                    StatusView.ID_STATUS_PV,
                    pvText.toString()
            );
        }

    }


    //-------------------------------------------------------------------------
    // Pass Dialog
    public static class DialogPass extends DialogFragment {

        public static DialogPass newInstance() {
            return new DialogPass();
        }

        public DroidZebra getDroidZebra() {
            return (DroidZebra) getActivity();
        }

        @Override
        @NonNull
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            return new AlertDialog.Builder(getActivity())
                    .setTitle(R.string.app_name)
                    .setMessage(R.string.dialog_pass_text)
                    .setPositiveButton(R.string.dialog_ok, (dialog, id) -> getDroidZebra().engine.pass(getDroidZebra().gameState, getDroidZebra().engineConfig))
                    .create();
        }
    }

    //-------------------------------------------------------------------------
    // Game Over Dialog
    public static class DialogGameOver extends DialogFragment {

        public static DialogGameOver newInstance() {
            return new DialogGameOver();
        }

        public DroidZebra getDroidZebra() {
            return (DroidZebra) getActivity();
        }

        public void refreshContent(View dialog) {
            int winner;
            int blackScore = getDroidZebra().getState().getBlackScore();
            int whiteScore = getDroidZebra().getState().getWhiteScore();
            if (whiteScore > blackScore)
                winner = R.string.gameover_text_white_wins;
            else if (whiteScore < blackScore)
                winner = R.string.gameover_text_black_wins;
            else
                winner = R.string.gameover_text_draw;

            ((TextView) dialog.findViewById(R.id.gameover_text)).setText(winner);

            ((TextView) dialog.findViewById(R.id.gameover_score)).setText(String.format(Locale.getDefault(), "%d : %d", blackScore, whiteScore));
        }

        @Override
        public View onCreateView(LayoutInflater inflater, ViewGroup container,
                                 Bundle savedInstanceState) {

            getDialog().setTitle(R.string.gameover_title);

            View v = inflater.inflate(R.layout.gameover, container, false);

            Button button;
            button = (Button) v.findViewById(R.id.gameover_choice_new_game);
            button.setOnClickListener(
                    v15 -> {
                        dismiss();
                        getDroidZebra().startNewGameAndResetUI();
                    });

            button = (Button) v.findViewById(R.id.gameover_choice_switch);
            button.setOnClickListener(
                    v12 -> {
                        dismiss();
                        getDroidZebra().switchSides();
                    });

            button = (Button) v.findViewById(R.id.gameover_choice_cancel);
            button.setOnClickListener(
                    v1 -> dismiss());

            button = (Button) v.findViewById(R.id.gameover_choice_options);
            button.setOnClickListener(
                    v13 -> {
                        dismiss();

                        // start settings
                        Intent i = new Intent(getDroidZebra(), SettingsPreferences.class);
                        startActivity(i);
                    });

            button = (Button) v.findViewById(R.id.gameover_choice_email);
            button.setOnClickListener(
                    v14 -> {
                        dismiss();
                        getDroidZebra().sendMail();
                    });

            refreshContent(v);

            return v;
        }

        @Override
        public void onResume() {
            super.onResume();
            refreshContent(getView());
        }
    }

    //-------------------------------------------------------------------------
    // Pass Dialog
    public static class DialogQuit extends DialogFragment {

        public static DialogQuit newInstance() {
            return new DialogQuit();
        }

        public DroidZebra getDroidZebra() {
            return (DroidZebra) getActivity();
        }

        @NonNull
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            return new AlertDialog.Builder(getActivity())
                    .setTitle(R.string.dialog_quit_title)
                    .setPositiveButton(R.string.dialog_quit_button_quit, (dialog, id) -> getDroidZebra().finish()
                    )
                    .setNegativeButton(R.string.dialog_quit_button_cancel, null)
                    .create();
        }
    }

    //-------------------------------------------------------------------------
    // Pass Dialog
    public static class DialogBusy extends DialogFragment {

        public static DialogBusy newInstance() {
            return new DialogBusy();
        }

        public DroidZebra getDroidZebra() {
            return (DroidZebra) getActivity();
        }

        @NonNull
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            ProgressDialog pd = new ProgressDialog(getActivity()) {
                @Override
                public boolean onKeyDown(int keyCode, KeyEvent event) {
                    stopZebra();
                    return super.onKeyDown(keyCode, event);
                }

                @Override
                public boolean onTouchEvent(MotionEvent event) {
                    if (event.getAction() == MotionEvent.ACTION_DOWN) {
                        stopZebra();
                        return true;
                    }
                    return super.onTouchEvent(event);
                }

                private void stopZebra() {
                    ZebraEngine engine = getDroidZebra().engine;
                    engine.stopIfThinking(getDroidZebra().gameState);

                    getDroidZebra().mBusyDialogUp = false;
                    cancel();
                }
            };
            pd.setProgressStyle(ProgressDialog.STYLE_SPINNER);
            pd.setMessage(getResources().getString(R.string.dialog_busy_message));
            return pd;
        }
    }
}
