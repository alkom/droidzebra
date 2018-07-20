package de.earthlingz.oerszebra.BoardView;

import android.support.annotation.Nullable;
import com.shurik.droidzebra.*;

/**
 * Created by stefan on 17.03.2018.
 */

public class BoardViewModel {
    final public static int boardSize = 8;

    private MutableFieldState board[][] = new MutableFieldState[boardSize][boardSize];
    private Move lastMove = null;
    private int whiteScore = 0;
    private int blackScore = 0;
    private final CandidateMoves possibleMoves = new CandidateMoves();
    private Move nextMove;
    private OnBoardStateChangedListener onBoardStateChangedListener = new OnBoardStateChangedListener() {
    };

    public FieldState getFieldState(int x, int y) {
        return board[x][y];
    }

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
        for (int i = 0; i < boardSize; i++)
            for (int j = 0; j < boardSize; j++)
                board[i][j] = new MutableFieldState(ZebraEngine.PLAYER_EMPTY);
    }

    public Move getNextMove() {
        return nextMove;
    }

    public void processGameOver() {
        possibleMoves.setMoves(new CandidateMove[]{});
        int max = board.length * board.length;
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
        this.onBoardStateChangedListener.onBoardStateChanged();

        return boardChanged;
    }

    private boolean updateBoard(GameState gameState) {
        ByteBoard board = gameState.getByteBoard();

        boolean changed = false;
        //only update the board if anything has changed
        for (int x = 0; !changed && x < boardSize; x++) {
            for (int y = 0; !changed && y < boardSize; y++) {
                byte newState = board.get(x, y);
                if (this.board[x][y].getState() != newState) {
                    changed = true;
                }
            }
        }

        if (changed) {
            for (int x = 0; x < boardSize; x++) {
                for (int y = 0; y < boardSize; y++) {
                    byte newState = board.get(x, y);
                    this.board[x][y].set(newState); //this also remembers if a flip has happened
                }
            }
        }
        return changed;

    }

    public int getBoardRowWidth(int x) {
        return board[x].length;
    }

    public int getBoardHeight() {
        return board.length;
    }

    public void setOnBoardStateChangedListener(BoardView onBoardStateChangedListener) {
        this.onBoardStateChangedListener = onBoardStateChangedListener;
    }

    public void removeOnBoardStateChangedListener() {
        this.onBoardStateChangedListener = new OnBoardStateChangedListener() {
        };
    }
}
