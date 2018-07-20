package com.shurik.droidzebra;

import android.support.annotation.NonNull;

import java.util.Observable;

/**
 * Created by stefan on 27.11.2016.
 */

public class CandidateMoves extends Observable {

    private CandidateMove[] moves = new CandidateMove[]{};

    public CandidateMoves() {
    }

    public CandidateMove[] getMoves() {
        return moves;
    }

    public void setMoves(@NonNull CandidateMove[] moves) {
        this.moves = moves;
        super.setChanged();
        super.notifyObservers(moves);
    }
}
