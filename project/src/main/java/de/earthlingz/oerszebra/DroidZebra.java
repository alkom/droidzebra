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

import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.ClipboardManager;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import android.view.*;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.view.menu.MenuBuilder;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.Fragment;
import com.joanzapata.iconify.IconDrawable;
import com.joanzapata.iconify.Iconify;
import com.joanzapata.iconify.fonts.FontAwesomeIcons;
import com.joanzapata.iconify.fonts.FontAwesomeModule;
import com.shurik.droidzebra.*;
import de.earthlingz.oerszebra.BoardView.BoardView;
import de.earthlingz.oerszebra.BoardView.GameStateBoardModel;
import de.earthlingz.oerszebra.guessmove.GuessMoveActivity;
import de.earthlingz.oerszebra.parser.GameParser;

import javax.annotation.Nonnull;
import java.lang.ref.WeakReference;
import java.util.Calendar;
import java.util.Date;
import java.util.LinkedList;
import java.util.Locale;

import static de.earthlingz.oerszebra.GameSettingsConstants.*;
import static de.earthlingz.oerszebra.GlobalSettingsLoader.*;


public class DroidZebra extends AppCompatActivity implements MoveStringConsumer,
        OnSettingsChangedListener, BoardView.OnMakeMoveListener, GameStateListener, ZebraEngine.OnEngineErrorListener {
    private ClipboardManager clipboard;
    private ZebraEngine engine;


    private boolean mBusyDialogUp = false;
    private boolean isHintUp = false;
    private boolean mIsInitCompleted = false;
    private boolean mActivityActive = false;

    private BoardView mBoardView;
    private GameStateBoardModel state = ZebraServices.getBoardState();

    private GameParser parser = ZebraServices.getGameParser();
    private WeakReference<AlertDialog> alert = null;

    public SettingsProvider settingsProvider;
    private GameStateListener handler = new GameStateHandlerProxy(this);
    private GameState gameState;
    private EngineConfig engineConfig;
    private Menu menu;


    public void resetStateAndStatusView() {
        getState().reset();
        ((TextView)findViewById(R.id.status_opening)).setText("");
        ((TextView)findViewById(R.id.status_moves)).setText("");
    }

    public boolean evalsDisplayEnabled() {
        return settingsProvider.isSettingPracticeMode() || isHintUp;
    }

    public void startNewGameAndResetUI() {
        startNewGame();

        resetAndLoadOnGuiThread();

    }

    private void startNewGame() {
        engine.newGame(engineConfig, new ZebraEngine.OnGameStateReadyListener() {
            @Override
            public void onGameStateReady(GameState gameState) {
                DroidZebra.this.gameState = gameState;
                gameState.setGameStateListener(handler);
            }
        });
    }

    @Override
    public void onStart() {
        super.onStart();
        Analytics.ask(this);
    }

    /* Creates the menu items */
    @Override
    @SuppressLint("RestrictedApi")
    public boolean onCreateOptionsMenu(Menu menu) {
        this.menu = menu;
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.menu, menu);

        if(menu instanceof MenuBuilder){
            MenuBuilder m = (MenuBuilder) menu;
            m.setOptionalIconsVisible(true);
        }

        menu.findItem(R.id.menu_take_back).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_undo)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        menu.findItem(R.id.menu_take_redo).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_repeat)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        menu.findItem(R.id.menu_new_game).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_play)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        menu.findItem(R.id.menu_goto_beginning).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_fast_backward)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        menu.findItem(R.id.menu_rotate).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_refresh)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        menu.findItem(R.id.menu_switch_sides).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_exchange)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        menu.findItem(R.id.menu_hint).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_info)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        menu.findItem(R.id.menu_settings).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_cog)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        menu.findItem(R.id.menu_enter_moves).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_file_text)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        menu.findItem(R.id.menu_guess_move).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_question)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        menu.findItem(R.id.menu_quit).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_close)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        menu.findItem(R.id.menu_mail).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_mail_forward)
                        .colorRes(R.color.white)
                        .sizeDp(12));

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
                undo();
                return true;
            case R.id.menu_goto_beginning:
                undoAll();
                return true;
            case R.id.menu_take_redo:
                redo();
                return true;
            case R.id.menu_settings: {
                // Launch Preference activity
                Intent i = new Intent(this, SettingsPreferences.class);
                startActivity(i);
            }
            return true;
            case R.id.menu_guess_move: {
                // Launch GuessMove activity
                Intent i = new Intent(this, GuessMoveActivity.class);
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
            case R.id.menu_rotate: {
                rotate();
            }
            return true;
        }
        return false;
    }

    void redo() {
        engine.redoMove(gameState);
    }

    void undo() {
        menu.findItem(R.id.menu_take_back).setEnabled(false);
        engine.undoMove(gameState);
    }

    @Override
    protected void onNewIntent(Intent intent) {

        String action = intent.getAction();
        String type = intent.getType();

        Log.i("Intent", type + " " + action);

        if (Intent.ACTION_SEND.equals(action) && type != null) {
            switch (type) {
                case "text/plain":

                    String dataString = intent.getDataString();
                    Analytics.converse("intent", null);
                    Analytics.log("intent", dataString);
                    consumeMovesString(dataString); // Handle text being sent

                    break;
                case "message/rfc822":
                    String text = intent.getStringExtra(Intent.EXTRA_TEXT);
                    Analytics.converse("intent", null);
                    Analytics.log("intent", text);
                    consumeMovesString(text); // Handle text being sent

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
        Analytics.setApp(this);
        Analytics.build();

        Iconify
                .with(new FontAwesomeModule());

        clipboard = (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);

        setContentView(R.layout.spash_layout);
        hideActionBar();

        engine = ZebraEngine.get(new AndroidContext(getApplicationContext()));
        engine.setOnDebugListener(new ZebraEngine.OnEngineDebugListener() {
            @Override
            public void onDebug(String message) {
                Log.d("engine", message);
            }
        });
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
            resetStateAndStatusView();
            showActionBar();
            mBoardView = findViewById(R.id.board);
            mBoardView.setBoardViewModel(getState());
            mBoardView.setOnMakeMoveListener(this);
            mBoardView.requestFocus();

            ((ImageButton)findViewById(R.id.status_undo)).setImageDrawable(new IconDrawable(this, FontAwesomeIcons.fa_undo)
                    .colorRes(R.color.white)
                    .sizeDp(30));

            ((ImageButton)findViewById(R.id.status_redo)).setImageDrawable(new IconDrawable(this, FontAwesomeIcons.fa_repeat)
                    .colorRes(R.color.white)
                    .sizeDp(30));

            ((ImageButton)findViewById(R.id.status_rotate)).setImageDrawable(new IconDrawable(this, FontAwesomeIcons.fa_refresh)
                    .colorRes(R.color.white)
                    .sizeDp(30));

            ((ImageButton)findViewById(R.id.status_first_move)).setImageDrawable(new IconDrawable(this, FontAwesomeIcons.fa_fast_backward)
                    .colorRes(R.color.white)
                    .sizeDp(30));

            if (Intent.ACTION_SEND.equals(action) && type != null) {
                if ("text/plain".equals(type) || "message/rfc822".equals(type)) {
                    String input = intent.getStringExtra(Intent.EXTRA_TEXT);
                    if(input != null) {
                        startNewGameAndResetUI(parser.makeMoveList(input));
                    }
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
        Analytics.log("new_game", new GameState(8, moves).getMoveSequenceAsString());
        engine.newGame(moves, engineConfig, new ZebraEngine.OnGameStateReadyListener() {
            @Override
            public void onGameStateReady(GameState gameState1) {
                DroidZebra.this.gameState = gameState1;
                gameState1.setGameStateListener(handler);

                resetAndLoadOnGuiThread();
            }
        });
    }

    public GameState getGameState() {
        return gameState;
    }

    private void startNewGameAndResetUI(int moves_played_count, byte[] moves_played) {
        Analytics.log("new_game", new GameState(8, moves_played, moves_played_count).getMoveSequenceAsString());
        engine.newGame(moves_played, moves_played_count, engineConfig, new ZebraEngine.OnGameStateReadyListener() {
            @Override
            public void onGameStateReady(GameState gameState1) {
                DroidZebra.this.gameState = gameState1;
                gameState1.setGameStateListener(handler);
                String moveSequenceAsString = gameState.getMoveSequenceAsString();
                Log.i("start", moveSequenceAsString);

                resetAndLoadOnGuiThread();
            }
        });
    }

    private void resetAndLoadOnGuiThread() {
        runOnUiThread(() -> {
            resetStateAndStatusView();
            loadUISettings();
        });
    }

    private void showActionBar() {
        ActionBar actionBar = getSupportActionBar();
        if (actionBar != null) {
            actionBar.show();
        }
    }

    private void hideActionBar() {
        ActionBar actionBar = getSupportActionBar();
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

        ((TextView)findViewById(R.id.status_settings)).setText(
                String.format(getString(R.string.display_depth), depth, depthExact, depthWLD)
        );
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

        Analytics.converse("send_mail", null);

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
        if(gameState != null) {
            engine.disconnect(gameState);
            gameState.removeGameStateListener();
        }

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
        Analytics.error(msg, gameState);
        runOnUiThread(DroidZebra.this::startNewGameAndResetUI);
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
            undo();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        GameState gs = gameState;
        if (gs != null) {
            byte[] moves = gs.exportMoveSequence();
            outState.putByteArray("moves_played", moves);
            outState.putInt("moves_played_count", moves.length);
            outState.putInt("version", 1);
        }
    }

    public GameStateBoardModel getState() {
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
        menu.findItem(R.id.menu_take_back).setEnabled(true);
        int sideToMove = gameState.getSideToMove();

        //triggers animations
        boolean boardChanged = state.update(gameState);

        setStatusViewScores(sideToMove);

        if (gameState.getOpening() != null) {
            ((TextView)findViewById(R.id.status_opening)).setText(gameState.getOpening());
        }

        if (!boardChanged) {
            Log.v("Handler", "invalidate");
            mBoardView.invalidate();
        }
    }

    private void setStatusViewScores(int sideToMove) {
        String scoreText;
        if (sideToMove == ZebraEngine.PLAYER_BLACK) {
            //with dot before
            scoreText = String.format(Locale.getDefault(), "•%d", state.getBlackScore());
        } else {
            scoreText = String.format(Locale.getDefault(), "%d", state.getBlackScore());
        }
        TextView black = findViewById(R.id.blackscore);
        black.setText(scoreText);

        if (sideToMove == ZebraEngine.PLAYER_WHITE) {
            //with dot behind
            scoreText = String.format(Locale.getDefault(), "%d•", state.getWhiteScore());
        } else {
            scoreText = String.format(Locale.getDefault(), "%d", state.getWhiteScore());
        }

        TextView white = findViewById(R.id.whitescore);
        white.setText(scoreText);
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
        //do nothing
    }

    @Override
    public void onPv(byte[] pv) {
        //do nothing, happens too fast
    }

    public void rotate() {
        byte[] rotate = gameState.rotate();
        startNewGameAndResetUI(gameState.getDisksPlayed(), rotate);
    }

    public void undo(View view) {
        undo();
    }

    public void undoAll(View view) {
        undoAll();
    }

    public void redo(View view) {
        redo();
    }

    public void rotate(View view) {
        rotate();
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
        @Nonnull
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
            button = v.findViewById(R.id.gameover_choice_new_game);
            button.setOnClickListener(
                    v15 -> {
                        dismiss();
                        getDroidZebra().startNewGameAndResetUI();
                    });

            button = v.findViewById(R.id.gameover_choice_rotate);
            button.setOnClickListener(
                    click -> {
                        dismiss();
                        getDroidZebra().rotate();
                    });

            button = v.findViewById(R.id.gameover_choice_beginning);
            button.setOnClickListener(
                    click -> {
                        dismiss();
                        getDroidZebra().undoAll();
                    });

            button = v.findViewById(R.id.gameover_choice_switch);
            button.setOnClickListener(
                    v12 -> {
                        dismiss();
                        getDroidZebra().switchSides();
                    });

            button = v.findViewById(R.id.gameover_choice_cancel);
            button.setOnClickListener(
                    v1 -> dismiss());

            button = v.findViewById(R.id.gameover_choice_options);
            button.setOnClickListener(
                    v13 -> {
                        dismiss();

                        // start settings
                        Intent i = new Intent(getDroidZebra(), SettingsPreferences.class);
                        startActivity(i);
                    });

            button = v.findViewById(R.id.gameover_choice_email);
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

    private void undoAll() {
        engine.undoAll(gameState);
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

        @Nonnull
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

        @Nonnull
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
