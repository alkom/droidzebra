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

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.ClipboardManager;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.util.Log;
import android.view.*;
import android.widget.Button;
import android.widget.TextView;
import com.google.common.base.Objects;
import com.shurik.droidzebra.*;
import de.earthlingz.oerszebra.parser.Gameparser;

import java.lang.ref.WeakReference;
import java.util.Calendar;
import java.util.Date;
import java.util.LinkedList;
import java.util.Locale;

import static de.earthlingz.oerszebra.GameSettingsConstants.*;
import static de.earthlingz.oerszebra.GlobalSettingsLoader.*;

//import android.util.Log;

public class DroidZebra extends FragmentActivity implements GameController, OnChangeListener, BoardView.OnMakeMoveListener {
    private ClipboardManager clipboard;
    private ZebraEngine mZebraThread;


    private boolean mBusyDialogUp = false;
    private boolean mHintIsUp = false;
    private boolean mIsInitCompleted = false;
    private boolean mActivityActive = false;

    private BoardView mBoardView;
    private StatusView mStatusView;

    private BoardState state = ZebraServices.getBoardState();

    private Gameparser parser;
    private WeakReference<AlertDialog> alert = null;

    public SettingsProvider settingsProvider;

    public DroidZebra() {
        super();
        this.setGameParser(ZebraServices.getGameParser());
    }


    private boolean isThinking() {
        return mZebraThread.isThinking();
    }

    private boolean isHumanToMove() {
        return mZebraThread.isHumanToMove();
    }

    private void makeMove(Move mMoveSelection) throws InvalidMove {
        mZebraThread.makeMove(mMoveSelection);
    }

    void setBoardState(@NonNull BoardState state) {
        this.state = state;
    }

    void setGameParser(Gameparser parser) {
        this.parser = parser;
    }

    private void newCompletionPort(final int zebraEngineStatus, final Runnable completion) {
        new CompletionAsyncTask(zebraEngineStatus, completion, getEngine())
                .execute();
    }

    public FieldState[][] getBoard() {
        return getState().getBoard();
    }


    public ZebraEngine getEngine() {
        return mZebraThread;
    }

    public void initBoard() {
        getState().reset();
        if (mStatusView != null)
            mStatusView.clear();
    }

    public CandidateMove[] getCandidateMoves() {
        return getState().getMoves();
    }

    public void setCandidateMoves(CandidateMove[] cmoves) {
        getState().setMoves(cmoves);
        runOnUiThread(() -> mBoardView.invalidate());
    }

    public boolean evalsDisplayEnabled() {
        return settingsProvider.isSettingPracticeMode() || mHintIsUp;
    }

    public void newGame() {
        if (mZebraThread.getEngineState() != ZebraEngine.ES_READY2PLAY) {
            mZebraThread.stopGame();
        }
        newCompletionPort(
                ZebraEngine.ES_READY2PLAY,
                () -> {
                    DroidZebra.this.initBoard();
                    DroidZebra.this.loadSettings();
                    DroidZebra.this.mZebraThread.setEngineState(ZebraEngine.ES_PLAY);
                }
        );
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
                newGame();
                return true;
            case R.id.menu_quit:
                showQuitDialog();
                return true;
            case R.id.menu_take_back:
                mZebraThread.undoMove();
                return true;
            case R.id.menu_take_redo:
                mZebraThread.redoMove();
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
                    setUpBoard(intent.getDataString()); // Handle text being sent

                    break;
                case "message/rfc822":
                    Log.i("Intent", intent.getStringExtra(Intent.EXTRA_TEXT));
                    setUpBoard(intent.getStringExtra(Intent.EXTRA_TEXT)); // Handle text being sent

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
        initBoard();

        clipboard = (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);

        setContentView(R.layout.spash_layout);
        new ActionBarHelper(this).hide();

        mZebraThread = new ZebraEngine(new AndroidContext(this));
        mZebraThread.setHandler(new DroidZebraHandler(getState(), this, mZebraThread));

        this.settingsProvider = new GlobalSettingsLoader(this);
        this.settingsProvider.setOnChangeListener(this);

        Intent intent = getIntent();
        String action = intent.getAction();
        String type = intent.getType();

        Log.i("Intent", type + " " + action);

        if (Intent.ACTION_SEND.equals(action) && type != null) {
            if ("text/plain".equals(type) || "message/rfc822".equals(type)) {
                mZebraThread.setInitialGameState(parser.makeMoveList(intent.getStringExtra(Intent.EXTRA_TEXT)));
            } else {
                Log.e("intent", "unknown intent");
            }
        } else if (savedInstanceState != null
                && savedInstanceState.containsKey("moves_played_count")
                && savedInstanceState.getInt("moves_played_count") > 0) {
            Log.i("moves_play_count", String.valueOf(savedInstanceState.getInt("moves_played_count")));
            Log.i("moves_played", String.valueOf(savedInstanceState.getInt("moves_played")));
            mZebraThread.setInitialGameState(savedInstanceState.getInt("moves_played_count"), savedInstanceState.getByteArray("moves_played"));
        }

        mZebraThread.start();

        newCompletionPort(
                ZebraEngine.ES_READY2PLAY,
                () -> {
                    DroidZebra.this.setContentView(R.layout.board_layout);
                    new ActionBarHelper(DroidZebra.this).show();
                    DroidZebra.this.mBoardView = (BoardView) DroidZebra.this.findViewById(R.id.board);
                    DroidZebra.this.mStatusView = (StatusView) DroidZebra.this.findViewById(R.id.status_panel);
                    DroidZebra.this.mBoardView.setBoardState(getState());
                    DroidZebra.this.mBoardView.setOnMakeMoveListener(DroidZebra.this);
                    DroidZebra.this.mBoardView.requestFocus();
                    DroidZebra.this.initBoard();
                    DroidZebra.this.loadSettings();
                    DroidZebra.this.mZebraThread.setEngineState(ZebraEngine.ES_PLAY);
                    DroidZebra.this.mIsInitCompleted = true;
                }
        );
    }

    private void loadSettings() {
        if (mBoardView != null) {
            mBoardView.setDisplayAnimations(settingsProvider.isSettingDisplayEnableAnimations());
            mBoardView.setAnimationDuration(settingsProvider.getSettingAnimationDuration());
            mBoardView.setDisplayLastMove(settingsProvider.isSettingDisplayLastMove());
            mBoardView.setDisplayMoves(settingsProvider.isSettingDisplayMoves());
            mBoardView.setDisplayEvals(evalsDisplayEnabled());
        }

        if (mZebraThread == null) return;
        int settingFunction = settingsProvider.getSettingFunction();
        int depth = settingsProvider.getSettingZebraDepth();
        int depthExact = settingsProvider.getSettingZebraDepthExact();
        int depthWLD = settingsProvider.getSettingZebraDepthWLD();
        try {
            mZebraThread.setAutoMakeMoves(settingsProvider.isSettingAutoMakeForcedMoves());
            mZebraThread.setForcedOpening(settingsProvider.getSettingForceOpening());
            mZebraThread.setHumanOpenings(settingsProvider.isSettingHumanOpenings());
            mZebraThread.setPracticeMode(settingsProvider.isSettingPracticeMode());
            mZebraThread.setUseBook(settingsProvider.isSettingUseBook());

            mZebraThread.setSettingFunction(settingFunction, depth, depthExact, depthWLD);

            mZebraThread.setSlack(settingsProvider.getSettingSlack());
            mZebraThread.setPerturbation(settingsProvider.getSettingPerturbation());
        } catch (EngineError e) {
            showAlertDialog(e.getError());
        }

        mStatusView.setTextForID(
                StatusView.ID_SCORE_SKILL,
                String.format(getString(R.string.display_depth), depth, depthExact, depthWLD)
        );


        if (!settingsProvider.isSettingDisplayPv()) {
            mStatusView.setTextForID(StatusView.ID_STATUS_PV, "");
            mStatusView.setTextForID(StatusView.ID_STATUS_EVAL, "");
        }

        mZebraThread.setComputerMoveDelay((settingFunction != FUNCTION_HUMAN_VS_HUMAN) ? settingsProvider.getComputerMoveDelay() : 0);
        mZebraThread.sendSettingsChanged();

    }


    private void sendMail() {
        //GetNowTime
        Calendar calendar = Calendar.getInstance();
        Date nowTime = calendar.getTime();
        StringBuilder sbBlackPlayer = new StringBuilder();
        StringBuilder sbWhitePlayer = new StringBuilder();
        ZebraBoard gs = mZebraThread.getGameState();
        SharedPreferences settings = getSharedPreferences(SHARED_PREFS_NAME, 0);
        byte[] moves = null;
        if (gs != null) {
            moves = gs.getMoveSequence();
        }

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
        StringBuilder sbMoves = new StringBuilder();
        if (moves != null) {

            for (byte move1 : moves) {
                if (move1 != 0x00) {
                    Move move = new Move(move1);
                    sbMoves.append(move.getText());
                    if (Objects.equal(getState().getLastMove(), move)) {
                        break;
                    }
                }
            }
        }
        sb.append(sbMoves);
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

        loadSettings();

        // start a new game if not playing
        if (!mZebraThread.gameInProgress())
            newGame();
    }

    private void showHint() {
        if (!settingsProvider.isSettingPracticeMode()) {
            setHintUp(true);
            mZebraThread.setPracticeMode(true);
            mZebraThread.sendSettingsChanged();
        }
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
                Log.wtf("wtf", e);
            }
        }
        mZebraThread.clean();
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

    @Override
    public boolean getSettingDisplayPV() {
        return settingsProvider.isSettingDisplayPv();
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

    public void setUpBoard(String s) {
        final LinkedList<Move> moves = parser.makeMoveList(s);
        mZebraThread.sendReplayMoves(moves);
    }

    private void showBusyDialog() {
        if (!mBusyDialogUp && mZebraThread.isThinking()) {
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

    @Override
    public boolean isHintUp() {
        return mHintIsUp;
    }

    @Override
    public void setHintUp(boolean value) {
        mHintIsUp = value;
        this.mBoardView.setDisplayEvals(evalsDisplayEnabled());
    }

    @Override
    public boolean isPracticeMode() {
        return settingsProvider.isSettingPracticeMode();
    }

    public void showAlertDialog(String msg) {
        DroidZebra.this.newGame();
        AlertDialog.Builder alertDialog = new AlertDialog.Builder(this);
        alertDialog.setTitle("Zebra Error");
        alertDialog.setMessage(msg);
        alertDialog.setPositiveButton("OK", (dialog, id) -> alert = null);
        runOnUiThread(() -> alert = new WeakReference<>(alertDialog.show()));
    }

    @Override
    public StatusView getStatusView() {
        return mStatusView;
    }

    @Override
    public BoardView getBoardView() {
        return mBoardView;
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
            mZebraThread.undoMove();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        ZebraBoard gs = mZebraThread.getGameState();
        if (gs != null) {
            outState.putByteArray("moves_played", gs.getMoveSequence());
            outState.putInt("moves_played_count", gs.getDisksPlayed());
            outState.putInt("version", 1);
        }
    }

    public BoardState getState() {
        return state;
    }

    @Override
    public void onChange() {
        loadSettings();
    }

    @Override
    public void onMakeMove(Move move) {
        if (getState().isValidMove(move)) {
            // if zebra is still thinking - no move is possible yet - throw a busy dialog
            if (isThinking() && !isHumanToMove()) {
                showBusyDialog();
            } else {
                try {
                    makeMove(move);
                } catch (InvalidMove e) {
                    Log.e("Invalid Move", e.getMessage(), e);
                }
            }
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
                    .setPositiveButton(R.string.dialog_ok, (dialog, id) -> getDroidZebra().mZebraThread.setEngineState(ZebraEngine.ES_PLAY)
                    )
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
                        getDroidZebra().newGame();
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

    public Gameparser getParser() {
        return parser;
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
                    if (getDroidZebra().mZebraThread.isThinking()) {
                        getDroidZebra().mZebraThread.stopMove();
                    }
                    getDroidZebra().mBusyDialogUp = false;
                    cancel();
                    return super.onKeyDown(keyCode, event);
                }

                @Override
                public boolean onTouchEvent(MotionEvent event) {
                    if (event.getAction() == MotionEvent.ACTION_DOWN) {
                        if (getDroidZebra().mZebraThread.isThinking()) {
                            getDroidZebra().mZebraThread.stopMove();
                        }
                        getDroidZebra().mBusyDialogUp = false;
                        cancel();
                        return true;
                    }
                    return super.onTouchEvent(event);
                }
            };
            pd.setProgressStyle(ProgressDialog.STYLE_SPINNER);
            pd.setMessage(getResources().getString(R.string.dialog_busy_message));
            return pd;
        }
    }
}
