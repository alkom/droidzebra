package com.shurik.droidzebra;


import android.util.Log;

import java.util.*;

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

    private final Map<Integer, Integer> rotateMap;

    @Deprecated
    public //Creates insonsistent instance
    GameState(int boardSize, List<Move> moves) {
        this.disksPlayed = moves.size();
        this.moveSequence = toBytesWithBoardSize(moves, boardSize);
        byteBoard = new ByteBoard(boardSize);
        rotateMap = initRotateMap(boardSize);
    }

    public GameState(int boardSize) {
        this.disksPlayed = 0;
        this.moveSequence = new byte[2 * boardSize * boardSize];
        byteBoard = new ByteBoard(boardSize);
        rotateMap = initRotateMap(boardSize);
    }

    @Deprecated
    public
        //Creates insonsistent instance
    GameState(int boardSize, byte[] moves, int movesPlayed) {
        this.disksPlayed = movesPlayed;
        this.moveSequence = Arrays.copyOf(moves, boardByteLength(boardSize));
        byteBoard = new ByteBoard(boardSize);
        rotateMap = initRotateMap(boardSize);
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

    /**
     * Removes all skips
     * @return the mve seqence without skips
     */
    public byte[] exportMoveSequence() {
        List<Byte> list= new ArrayList<>();
        for(int i = 0; i < disksPlayed; i++) {
            //filter passes
            if(moveSequence[i] > 0) {
                list.add(moveSequence[i]);
            }
        }
        byte[] asBytes = new byte[list.size()];
        for(int i = 0; i < list.size(); i++)  {
            asBytes[i] = list.get(i);
        }
        return asBytes;
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

    public byte[] rotate() {
        return rotateSequence(moveSequence);
    }

    private byte[] rotateSequence(byte[] moveSequence) {
        byte[] result = new byte[disksPlayed];
        for(int i = 0; i < disksPlayed; i++) {
            result[i] = rotateBoardField(moveSequence[i]);
        }
        return result;
    }

    private byte rotateBoardField(byte field) {
        if(field <= 0) { //pass and empty
            return field;
        }
        int column = field / 10;
        int row = field % 10;

        int toRow = byteBoard.size() +1 - row;
        int toColumn = byteBoard.size() +1 - column;
        return (byte)(toColumn*10 + toRow);
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

    private Map<Integer,Integer> initRotateMap(int boardSize) {
        Map<Integer, Integer> tm = new TreeMap<>();
        for(int row = 1; row <= boardSize /2; row++) {
            for(int column = 1; column <= boardSize; column++) {
                int from = (row -1) * (boardSize) + column;
                int toRow = boardSize +1 - row;
                int toColumn = boardSize +1 - column;
                int to = (toRow -1) * (boardSize) + toColumn;
                tm.put(from , to);
                tm.put(to, from);
            }
        }
        return tm;
    }

}
