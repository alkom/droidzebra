package de.earthlingz.oerszebra.BoardView;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import com.shurik.droidzebra.*;

import static com.shurik.droidzebra.ZebraEngine.PLAYER_EMPTY;

/**
 * Created by stefan on 17.03.2018.
 */

public class GameStateBoardModel implements BoardViewModel {

    private Move lastMove = null;
    private int whiteScore = 0;
    private int blackScore = 0;
    private final CandidateMoves possibleMoves = new CandidateMoves();
    private Move nextMove;
    private BoardViewModelListener boardViewModelListener = new BoardViewModelListener() {
    };
    private ByteBoard currentBoard = new ByteBoard(8);
    private ByteBoard previousBoard = currentBoard;

    @Override
    public int getBoardSize() {
        return 8;
    }

    @Override
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
    @Override
    public CandidateMove[] getCandidateMoves() {
        return possibleMoves.getMoves();
    }

    @Override
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

    @Override
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
        if (boardChanged) {
            this.boardViewModelListener.onBoardStateChanged();
        }
        this.boardViewModelListener.onCandidateMovesChanged();


        return boardChanged;
    }

    private boolean updateBoard(GameState gameState) {
        ByteBoard board = gameState.getByteBoard();
        boolean changed = !currentBoard.isSameAs(board);


        if (changed) {
            this.previousBoard = currentBoard;
            this.currentBoard = board;
        }
        return changed;

    }

    public int getBoardRowWidth() {
        return currentBoard.size();
    }

    public int getBoardHeight() {
        return currentBoard.size();
    }

    @Override
    public void setBoardViewModelListener(@NonNull BoardViewModelListener boardViewModelListener) {
        this.boardViewModelListener = boardViewModelListener;
    }

    @Override
    public void removeBoardViewModeListener() {
        this.boardViewModelListener = new BoardViewModelListener() {
        };
    }

    @Override
    public boolean isFieldFlipped(int x, int y) {
        byte currentField = currentBoard.get(x, y);
        byte previousField = previousBoard.get(x, y);
        return currentField != PLAYER_EMPTY && previousField != PLAYER_EMPTY && currentField != previousField;
    }

    @Override
    public boolean isFieldEmpty(int x, int y) {
        return currentBoard.isEmpty(x, y);
    }

    @Override
    public boolean isFieldBlack(int x, int y) {
        return currentBoard.isBlack(x, y);
    }

    public byte getFieldByte(int x, int y) {
        return currentBoard.get(x, y);
    }
}
