package de.earthlingz.oerszebra;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.shurik.droidzebra.CandidateMove;
import com.shurik.droidzebra.CandidateMoves;
import com.shurik.droidzebra.Move;
import com.shurik.droidzebra.ZebraEngine;

/**
 * Created by stefan on 17.03.2018.
 */

public class BoardState {
    final public static int boardSize = 8;

    private FieldState mBoard[][] = new FieldState[boardSize][boardSize];
    private Move mLastMove = null;
    private int mWhiteScore = 0;
    private int mBlackScore = 0;
    private final CandidateMoves mCandidateMoves = new CandidateMoves();

    public BoardState() {
        super();
    }

    public FieldState[][] getmBoard() {
        return mBoard;
    }

    public void setmBoard(FieldState[][] mBoard) {
        this.mBoard = mBoard;
    }

    @Nullable
    public Move getmLastMove() {
        return mLastMove;
    }

    public void setmLastMove(Move mLastMove) {
        this.mLastMove = mLastMove;
    }

    public int getmWhiteScore() {
        return mWhiteScore;
    }

    public void setmWhiteScore(int mWhiteScore) {
        this.mWhiteScore = mWhiteScore;
    }

    public int getmBlackScore() {
        return mBlackScore;
    }

    public void setmBlackScore(int mBlackScore) {
        this.mBlackScore = mBlackScore;
    }

    public CandidateMove[] getMoves() {
        return mCandidateMoves.getMoves();
    }

    public void setMoves(@NonNull CandidateMove[] moves) {
        mCandidateMoves.setMoves(moves);
    }

    public boolean isValidMove(Move move) {
        for (CandidateMove m : mCandidateMoves.getMoves()) {
            if (m.mMove.getX() == move.getX() && m.mMove.getY() == move.getY()) {
                return true;
            }
        }
        return false;
    }

    public void setBoard(byte[] board) {
        for (int i = 0; i < boardSize; i++)
            for (int j = 0; j < boardSize; j++) {
                mBoard[i][j].set(board[i * boardSize + j]);
            }
    }

    public void reset() {
        mLastMove = null;
        mWhiteScore = mBlackScore = 0;
        for (int i = 0; i < boardSize; i++)
            for (int j = 0; j < boardSize; j++)
                mBoard[i][j] = new FieldState(ZebraEngine.PLAYER_EMPTY);
    }
}
