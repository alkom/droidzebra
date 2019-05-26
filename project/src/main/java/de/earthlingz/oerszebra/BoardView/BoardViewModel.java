package de.earthlingz.oerszebra.BoardView;

import android.support.annotation.Nullable;
import com.shurik.droidzebra.CandidateMove;
import com.shurik.droidzebra.Move;

public interface BoardViewModel {


    int getBoardSize();

    @Nullable
    Move getLastMove();

    //TODO encapsulation leak
    CandidateMove[] getCandidateMoves();

    boolean isValidMove(Move move);

    Move getNextMove();

    void setBoardViewModelListener(BoardViewModelListener boardViewModelListener);

    void removeBoardViewModeListener();

    boolean isFieldFlipped(int x, int y);

    boolean isFieldEmpty(int x, int y);

    boolean isFieldBlack(int x, int y);

    interface BoardViewModelListener {
        default void onBoardStateChanged() {
        }
        default void onCandidateMovesChanged() {
        }
        default void onBoardSizeChanged() {
        }
        default void onNextMoveChanged() {
        }
        default void onLastMoveChanged(){

        }
    }
}
