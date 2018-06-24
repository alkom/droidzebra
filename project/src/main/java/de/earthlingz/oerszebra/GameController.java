package de.earthlingz.oerszebra;

import com.shurik.droidzebra.CandidateMove;

import de.earthlingz.oerszebra.parser.Gameparser;

/**
 * Created by stefan on 18.03.2018.
 */

public interface GameController {
    Gameparser getParser();

    void setUpBoard(String s);

    void showAlertDialog(String msg);

    StatusView getStatusView();

    BoardView getBoardView();

    void setCandidateMoves(CandidateMove[] candidateMoves);

    void showGameOverDialog();

    void showPassDialog();

    boolean getSettingDisplayPV();

    void dismissBusyDialog();

    boolean isHintUp();

    void setHintUp(boolean value);

    boolean isPraticeMode();
}
