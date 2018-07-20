package de.earthlingz.oerszebra.BoardView;

import com.shurik.droidzebra.ZebraEngine;

/**
 * Created by stefan on 17.03.2018.
 */
class MutableFieldState implements FieldState {
    private final static byte ST_FLIPPED = 0x01;
    private byte state;
    private byte flags;

    MutableFieldState(byte state) {
        this.state = state;
        flags = 0;
    }

    void set(byte newState) {
        if (newState != ZebraEngine.PLAYER_EMPTY && state != ZebraEngine.PLAYER_EMPTY && state != newState)
            flags |= ST_FLIPPED;
        else
            flags &= ~ST_FLIPPED;
        state = newState;
    }

    @Override
    public byte getState() {
        return state;
    }

    @Override
    public boolean isEmpty() {
        return state == ZebraEngine.PLAYER_EMPTY;
    }

    @Override
    public boolean isBlack() {
        return state == ZebraEngine.PLAYER_BLACK;
    }

    @Override
    public boolean isFlipped() {
        return (flags & ST_FLIPPED) > 0;
    }
}
