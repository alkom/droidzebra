package com.shurik.droidzebra;

public class ZebraPlayerStatus {
    private final String time;
    private final float eval;
    private final int discCount;
    private final MoveList moveList;

    ZebraPlayerStatus() {
        this.time = "";
        this.eval = 0;
        this.discCount = 0;
        moveList = new MoveList();
    }

    ZebraPlayerStatus(String time, float eval, int discCount, MoveList moveList) {
        this.time = time;
        this.eval = eval;
        this.discCount = discCount;
        this.moveList = moveList;
    }

    public String getTime() {
        return time;
    }

    public float getEval() {
        return eval;
    }

    public int getDiscCount() {
        return discCount;
    }

    public MoveList getMoveList() {
        return moveList;
    }
}
