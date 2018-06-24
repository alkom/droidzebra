package com.shurik.droidzebra;

public class ZebraBoard {
    private byte[] board;
    private int sideToMove;
    private ZebraPlayerStatus blackPlayer;
    private ZebraPlayerStatus whitePlayer;
    private int disksPlayed;
    private byte[] moveSequence;

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
}
