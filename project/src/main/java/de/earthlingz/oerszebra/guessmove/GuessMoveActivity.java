package de.earthlingz.oerszebra.guessmove;

import android.annotation.SuppressLint;
import android.app.ProgressDialog;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.view.menu.MenuBuilder;
import com.joanzapata.iconify.IconDrawable;
import com.joanzapata.iconify.Iconify;
import com.joanzapata.iconify.fonts.FontAwesomeIcons;
import com.joanzapata.iconify.fonts.FontAwesomeModule;
import com.shurik.droidzebra.EngineConfig;
import com.shurik.droidzebra.InvalidMove;
import com.shurik.droidzebra.ZebraEngine;
import de.earthlingz.oerszebra.AndroidContext;
import de.earthlingz.oerszebra.BoardView.BoardView;
import de.earthlingz.oerszebra.BoardView.BoardViewModel;
import de.earthlingz.oerszebra.GlobalSettingsLoader;
import de.earthlingz.oerszebra.R;
import de.earthlingz.oerszebra.SettingsPreferences;

import static com.shurik.droidzebra.ZebraEngine.PLAYER_BLACK;


public class GuessMoveActivity extends AppCompatActivity {

    private BoardView boardView;
    private BoardViewModel boardViewModel;
    private GuessMoveModeManager manager;
    private ImageView sideToMoveCircle;
    private TextView hintText;

    private EngineConfig engineConfig;
    private GlobalSettingsLoader globalSettingsLoader;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        globalSettingsLoader = new GlobalSettingsLoader(getApplicationContext());
        engineConfig = globalSettingsLoader.createEngineConfig();

        Iconify
                .with(new FontAwesomeModule());


        this.manager = new GuessMoveModeManager(ZebraEngine.get(
                new AndroidContext(getApplicationContext())),
                engineConfig);
        setContentView(R.layout.activity_guess_move);
        boardView = findViewById(R.id.guess_move_board);
        boardViewModel = manager;
        boardView.setBoardViewModel(boardViewModel);
        boardView.requestFocus();
        sideToMoveCircle = findViewById(R.id.side_to_move_circle);
        hintText = findViewById(R.id.guess_move_text);
        Button button = findViewById(R.id.guess_move_new);
        button.setOnClickListener((a) -> newGame());
        newGame();
    }

    @Override
    protected void onResume() {
        super.onResume();
        globalSettingsLoader.setOnSettingsChangedListener(() -> {
            this.engineConfig = globalSettingsLoader.createEngineConfig();
            this.manager.updateGlobalConfig(engineConfig);
        });
    }

    @Override
    protected void onPause() {
        super.onPause();
        globalSettingsLoader.setOnSettingsChangedListener(null);

    }



    private void newGame() {
        ProgressDialog progressDialog = new ProgressDialog(this);
        progressDialog.setTitle("Generating game");
        progressDialog.show();
        setBoardViewUnplayable();

        manager.generate(new GuessMoveModeManager.GuessMoveListener() {
            @Override
            public void onGenerated(int sideToMove) {

                runOnUiThread(() -> {
                    boardView.setOnMakeMoveListener(move -> manager.guess(move));
                    updateSideToMoveCircle(sideToMove);
                    setGuessText(sideToMove);
                    progressDialog.dismiss();

                });

            }

            @Override
            public void onSideToMoveChanged(int sideToMove) {
                runOnUiThread(() -> {
                    updateSideToMoveCircle(sideToMove);
                    if (!guessed) {
                        setGuessText(sideToMove);
                    }
                });
            }

            private boolean guessed = false;

            @Override
            public void onCorrectGuess() {
                guessed = true;
                setHintText(Color.CYAN, R.string.guess_move_correct);
                setBoardViewPlayable();
            }

            @Override
            public void onBadGuess() {
                guessed = true;
                setHintText(Color.RED, R.string.guess_move_incorrect);
            }

        });
    }

    private void setBoardViewUnplayable() {
        boardView.setOnMakeMoveListener(null);

        boardView.setDisplayMoves(false);
        boardView.setDisplayEvals(true);
    }

    private void setBoardViewPlayable() {
        boardView.setDisplayMoves(true);
        boardView.setDisplayEvals(true);
        boardView.setOnMakeMoveListener(move1 -> {
            try {
                manager.move(move1);
            } catch (InvalidMove ignored) {
            }
        });
    }

    private void setHintText(int cyan, int guess_move_correct) {
        hintText.setTextColor(cyan);
        hintText.setText(guess_move_correct);
    }

    private void setGuessText(int sideToMove) {
        if (sideToMove == PLAYER_BLACK) {
            hintText.setText(R.string.guess_black_move_hint);
            hintText.setTextColor(Color.BLACK);
        } else {
            hintText.setText(R.string.guess_white_move_hint);
            hintText.setTextColor(Color.WHITE);
        }

    }

    private void updateSideToMoveCircle(int sideToMove) {
        if (sideToMove == PLAYER_BLACK) {
            sideToMoveCircle.setImageResource(R.drawable.black_circle);
        } else {
            sideToMoveCircle.setImageResource(R.drawable.white_circle);
        }
    }


    /* Creates the menu items */
    @Override
    @SuppressLint("RestrictedApi")
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.guess_move_context_menu, menu);

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

        menu.findItem(R.id.menu_hint).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_info)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        menu.findItem(R.id.menu_settings).setIcon(
                new IconDrawable(this, FontAwesomeIcons.fa_cog)
                        .colorRes(R.color.white)
                        .sizeDp(12));

        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.menu_new_game:
                newGame();
                return true;
            case R.id.menu_take_back:
                manager.undoMove();
                return true;
            case R.id.menu_take_redo:
                manager.redoMove();
                return true;
            case R.id.menu_settings: {
                // Launch Preference activity
                Intent i = new Intent(this, SettingsPreferences.class);
                startActivity(i);
            }
            return true;
        }
        return false;
    }

}
