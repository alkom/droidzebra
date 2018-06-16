package com.shurik.droidzebra;

/**
 * Created by stefan on 18.03.2018.
 */ // candidate move with evals
public class CandidateMove {
    public Move mMove;
    public boolean mHasEval;
    public String mEvalShort;
    public String mEvalLong;
    public boolean mBest;

    public CandidateMove(Move move) {
        mMove = move;
        mHasEval = false;
    }

    public CandidateMove(Move move, String evalShort, String evalLong, boolean best) {
        mMove = move;
        mEvalShort = evalShort;
        mEvalLong = evalLong;
        mBest = best;
        mHasEval = true;
    }
}
