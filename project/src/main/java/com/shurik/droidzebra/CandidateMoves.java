package com.shurik.droidzebra;

import java.util.Observable;

/**
 * Created by stefan on 27.11.2016.
 */

public class CandidateMoves extends Observable {

    ZebraEngine.CandidateMove[] moves = new ZebraEngine.CandidateMove[]{};

    public CandidateMoves() {
    }

    public ZebraEngine.CandidateMove[] getMoves() {
        return moves;
    }

    public void setMoves(ZebraEngine.CandidateMove[] moves) {
        this.moves = moves;
        super.setChanged();
        super.notifyObservers(moves);
    }
}
