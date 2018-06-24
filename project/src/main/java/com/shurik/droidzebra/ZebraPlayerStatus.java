package com.shurik.droidzebra;

public class ZebraPlayerStatus {
    private String time;
    private float eval;
    private int discCount;
    private byte[] moves = new byte[0];

    public void setTime(String time) {
        this.time = time;
    }

    public String getTime() {
        return time;
    }

    public void setEval(float eval) {
        this.eval = eval;
    }

    public float getEval() {
        return eval;
    }

    public void setDiscCount(int discCount) {
        this.discCount = discCount;
    }

    public int getDiscCount() {
        return discCount;
    }

    public void setMoves(byte[] moves) {
        this.moves = moves;
    }

    public byte[] getMoves() {
        return moves;
    }
}
