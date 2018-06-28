package com.shurik.droidzebra;

public class ZebraBoard {
    private byte[] board;
    private int sideToMove;
    private ZebraPlayerStatus blackPlayer = new ZebraPlayerStatus();
    private ZebraPlayerStatus whitePlayer = new ZebraPlayerStatus();
    private int disksPlayed;
    private byte[] moveSequence = new byte[0];
    private CandidateMove[] candidateMoves = new CandidateMove[0];
    private String opening;
    private int lastMove;
    private int nextMove;

    public void setBoard(byte[] board) {
        this.board = board;
    }

    public byte[] getBoard() {
        return board;
    }

    public void setSideToMove(int sideToMove) {
        this.sideToMove = sideToMove;
    }

    public int getSideToMove() {
        return sideToMove;
    }

    public void setBlackPlayer(ZebraPlayerStatus blackPlayer) {
        this.blackPlayer = blackPlayer;
    }

    public ZebraPlayerStatus getBlackPlayer() {
        return blackPlayer;
    }

    public void setWhitePlayer(ZebraPlayerStatus whitePlayer) {
        this.whitePlayer = whitePlayer;
    }

    public ZebraPlayerStatus getWhitePlayer() {
        return whitePlayer;
    }

    public void setDisksPlayed(int disksPlayed) {
        this.disksPlayed = disksPlayed;
    }

    public int getDisksPlayed() {
        return disksPlayed;
    }

    public void setMoveSequence(byte[] moveSequence) {
        this.moveSequence = moveSequence;
    }

    public byte[] getMoveSequence() {
        return moveSequence;
    }

    public void setCandidateMoves(CandidateMove[] candidateMoves) {
        this.candidateMoves = candidateMoves;
    }

    public CandidateMove[] getCandidateMoves() {
        return candidateMoves;
    }

    public void setOpening(String opening) {
        this.opening = opening;
    }

    public String getOpening() {
        return opening;
    }

    public void setLastMove(int lastMove) {
        this.lastMove = lastMove;
    }

    public int getLastMove() {
        return lastMove;
    }

    public void setNextMove(int nextMove) {
        this.nextMove = nextMove;
    }

    public int getNextMove() {
        return nextMove;
    }

    public void addCandidateMoveEvals(CandidateMove[] cmoves) {
        for (CandidateMove candidateMoveWithEval : cmoves) {
            for (int i = 0, candidateMovesLength = candidateMoves.length; i < candidateMovesLength; i++) {
                CandidateMove candidateMove = candidateMoves[i];
                if (candidateMove.mMove.mMove == candidateMoveWithEval.mMove.mMove) {
                    candidateMoves[i] = candidateMoveWithEval;
                }
            }
        }
    }
}
