package com.shurik.droidzebra;


import java.util.Arrays;
import java.util.List;

/**
 * Holds Game State, This state should only be produced by engine as that's the source of truth
 */
public class GameState {
    private int sideToMove;
    private ZebraPlayerStatus blackPlayer = new ZebraPlayerStatus();
    private ZebraPlayerStatus whitePlayer = new ZebraPlayerStatus();
    private int disksPlayed;
    private byte[] moveSequence = new byte[0];
    private CandidateMove[] candidateMoves = new CandidateMove[0];
    private String opening;
    private int lastMove;
    private int nextMove;
    private ByteBoard byteBoard;
    private GameStateListener handler = new GameStateListener() {
    };

    @Deprecated
        //Creates insonsistent instance
    GameState(int boardSize, List<Move> moves) {
        this.disksPlayed = moves.size();
        this.moveSequence = toBytesWithBoardSize(moves, boardSize);
        byteBoard = new ByteBoard(boardSize);
    }

    public GameState(int boardSize) {
        this.disksPlayed = 0;
        this.moveSequence = new byte[2 * boardSize * boardSize];
        byteBoard = new ByteBoard(boardSize);
    }

    @Deprecated
        //Creates insonsistent instance
    GameState(int boardSize, byte[] moves, int movesPlayed) {
        this.disksPlayed = movesPlayed;
        this.moveSequence = Arrays.copyOf(moves, boardByteLength(boardSize));
        byteBoard = new ByteBoard(boardSize);
    }


    public void removeGameStateListener() {
        handler = new GameStateListener() {
        };
    }

    public void setGameStateListener(GameStateListener handler) {
        if (handler == null) {
            removeGameStateListener();
        } else {
            this.handler = handler;
        }
    }


    private static int boardByteLength(int boardSize) {
        return boardSize * boardSize * 2;
    }

    private static byte[] toBytesWithBoardSize(List<Move> moves, int boardSize) {
        byte[] moveBytes = new byte[boardByteLength(boardSize)];
        for (int i = 0; i < moves.size() && i < moveBytes.length; i++) {
            moveBytes[i] = (byte) moves.get(i).getMoveInt();
        }
        return moveBytes;
    }


    public ByteBoard getByteBoard() {
        return byteBoard;
    }

    public int getSideToMove() {
        return sideToMove;
    }

    public ZebraPlayerStatus getBlackPlayer() {
        return blackPlayer;
    }

    public ZebraPlayerStatus getWhitePlayer() {
        return whitePlayer;
    }

    public int getDisksPlayed() {
        return disksPlayed;
    }

    public byte[] exportMoveSequence() {
        return moveSequence.clone();
    }

    void setCandidateMoves(CandidateMove[] candidateMoves) {
        this.candidateMoves = candidateMoves;
    }

    //TODO possible encapsulation leak
    public CandidateMove[] getCandidateMoves() {
        return candidateMoves;
    }

    void setOpening(String opening) {
        this.opening = opening;
        handler.onBoard(this);
    }

    public String getOpening() {
        return opening;
    }

    void setLastMove(int lastMove) {
        this.lastMove = lastMove;
        handler.onBoard(this);
    }

    public int getLastMove() {
        return lastMove;
    }

    void setNextMove(int nextMove) {
        this.nextMove = nextMove;
        handler.onBoard(this);
    }

    public int getNextMove() {
        return nextMove;
    }

    void addCandidateMoveEvals(CandidateMove[] cmoves) {
        for (CandidateMove candidateMoveWithEval : cmoves) {
            for (int i = 0, candidateMovesLength = candidateMoves.length; i < candidateMovesLength; i++) {
                CandidateMove candidateMove = candidateMoves[i];
                if (candidateMove.getMoveInt() == candidateMoveWithEval.getMoveInt()) {
                    candidateMoves[i] = candidateMoveWithEval;
                }
            }
        }
        handler.onBoard(this);
    }


    private void updateMoveSequence(MoveList blackMoveList, MoveList whiteMoveList) {
        for (int i = 0; i < blackMoveList.length(); i++) {
            moveSequence[2 * i] = blackMoveList.getMoveByte(i);
        }

        for (int i = 0; i < whiteMoveList.length(); i++) {
            moveSequence[2 * i + 1] = whiteMoveList.getMoveByte(i);
        }
    }


    public String getMoveSequenceAsString() {
        StringBuilder sbMoves = new StringBuilder();

        if (moveSequence != null) {

            for (byte move1 : moveSequence) {
                if (move1 != 0x00) {
                    Move move = new Move(move1);
                    sbMoves.append(move.getText());
                    if (move1 == lastMove) {
                        break;
                    }
                }
            }
        }
        return sbMoves.toString();
    }

    void updateGameState(int sideToMove, int disksPlayed, String blackTime, float blackEval, int blackDiscCount, String whiteTime, float whiteEval, int whiteDiscCOunt, MoveList blackMoveList, MoveList whiteMoveList, ByteBoard byteBoard) {
        this.byteBoard = byteBoard;
        this.sideToMove = sideToMove;
        this.disksPlayed = disksPlayed;
        updateMoveSequence(blackMoveList, whiteMoveList);
        this.blackPlayer = new ZebraPlayerStatus(
                blackTime,
                blackEval,
                blackDiscCount,
                blackMoveList
        );

        this.whitePlayer = new ZebraPlayerStatus(
                whiteTime,
                whiteEval,
                whiteDiscCOunt,
                whiteMoveList
        );
        handler.onBoard(this);

    }

    public void sendPv(byte[] moves) {
        this.handler.onPv(moves);
    }

    public void sendPass() {
        handler.onPass();
    }

    public void sendGameStart() {
        handler.onGameStart();
    }

    public void sendGameOver() {
        handler.onGameOver();
    }

    public void sendMoveStart() {
        handler.onMoveStart();
    }

    public void sendEval(String eval) {
        handler.onEval(eval);
    }

    public void sendMoveEnd() {
        handler.onMoveEnd();
    }

    public CandidateMove getBestMove() { //TODO what if multiple best moves?
        for (CandidateMove candidateMove : candidateMoves) {
            if (candidateMove.isBest) {
                return candidateMove;
            }
        }
        return null;
    }
}
