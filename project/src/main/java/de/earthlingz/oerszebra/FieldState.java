package de.earthlingz.oerszebra;

import com.shurik.droidzebra.ZebraEngine;

/**
 * Created by stefan on 17.03.2018.
 */
public class FieldState {
    public final static byte ST_FLIPPED = 0x01;
    public byte mState;
    public byte mFlags;

    public FieldState(byte state) {
        mState = state;
        mFlags = 0;
    }

    public void set(byte newState) {
        if (newState != ZebraEngine.PLAYER_EMPTY && mState != ZebraEngine.PLAYER_EMPTY && mState != newState)
            mFlags |= ST_FLIPPED;
        else
            mFlags &= ~ST_FLIPPED;
        mState = newState;
    }

    public byte getState() {
        return mState;
    }

    public boolean isFlipped() {
        return (mFlags & ST_FLIPPED) > 0;
    }
}
