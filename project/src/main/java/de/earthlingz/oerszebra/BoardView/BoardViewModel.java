package de.earthlingz.oerszebra.BoardView;

import android.support.annotation.Nullable;
import com.shurik.droidzebra.*;

import static com.shurik.droidzebra.ZebraEngine.PLAYER_EMPTY;

/**
 * Created by stefan on 17.03.2018.
 */

public class BoardViewModel {
    final public static int boardSize = 8;

    private Move lastMove = null;
    private int whiteScore = 0;
    private int blackScore = 0;
    private final CandidateMoves possibleMoves = new CandidateMoves();
    private Move nextMove;
    private OnBoardStateChangedListener onBoardStateChangedListener = new OnBoardStateChangedListener() {
    };
    private ByteBoard currentBoard = new ByteBoard(8);
    private ByteBoard previousBoard = currentBoard;

    @Nullable
    public Move getLastMove() {
        return lastMove;
    }

    public int getWhiteScore() {
        return whiteScore;
    }

    public int getBlackScore() {
        return blackScore;
    }

    //TODO encapsulation leak
    public CandidateMove[] getMoves() {
        return possibleMoves.getMoves();
    }

    public boolean isValidMove(Move move) {
        for (CandidateMove m : possibleMoves.getMoves()) {
            if (m.getX() == move.getX() && m.getY() == move.getY()) {
                return true;
            }
        }
        return false;
    }

    public void reset() {
        lastMove = null;
        whiteScore = blackScore = 0;
        previousBoard = currentBoard = new ByteBoard(8);
    }

    public Move getNextMove() {
        return nextMove;
    }

    public void processGameOver() {
        possibleMoves.setMoves(new CandidateMove[]{});
        int max = currentBoard.size() * currentBoard.size();
        if (getBlackScore() + getWhiteScore() < max) {
            //adjust result
            if (getBlackScore() > getWhiteScore()) {
                this.blackScore = max - getWhiteScore();
            } else {
                this.whiteScore = max - getBlackScore();
            }
        }
    }

    public boolean update(GameState gameState) {
        boolean boardChanged = updateBoard(gameState);

        this.blackScore = gameState.getBlackPlayer().getDiscCount();
        this.whiteScore = gameState.getWhitePlayer().getDiscCount();

        byte lastMove = (byte) gameState.getLastMove();
        this.lastMove = lastMove == Move.PASS ? null : new Move(lastMove);

        byte moveNext = (byte) gameState.getNextMove();
        this.nextMove = moveNext == Move.PASS ? null : new Move(moveNext);


        possibleMoves.setMoves(gameState.getCandidateMoves());
        if(boardChanged) {
            this.onBoardStateChangedListener.onBoardStateChanged();
        }

        return boardChanged;
    }

    private boolean updateBoard(GameState gameState) {
        ByteBoard board = gameState.getByteBoard();
        this.previousBoard = currentBoard;
        this.currentBoard = board;

        return !previousBoard.isSameAs(currentBoard);

    }

    public int getBoardRowWidth() {
        return currentBoard.size();
    }

    public int getBoardHeight() {
        return currentBoard.size();
    }

    public void setOnBoardStateChangedListener(BoardView onBoardStateChangedListener) {
        this.onBoardStateChangedListener = onBoardStateChangedListener;
    }

    public void removeOnBoardStateChangedListener() {
        this.onBoardStateChangedListener = new OnBoardStateChangedListener() {
        };
    }

    public boolean isFieldFlipped(int x, int y) {
        byte field = currentBoard.get(x, y);
        return field != PLAYER_EMPTY && field != previousBoard.get(x, y);
    }

    public boolean isFieldEmpty(int i, int j) {
        return currentBoard.isEmpty(i, j);
    }

    public boolean isFieldBlack(int i, int j) {
        return currentBoard.isBlack(i, j);
    }

    public byte getStateByte(int x, int y) {
        return currentBoard.get(x, y);
    }
}
