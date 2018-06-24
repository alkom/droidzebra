package com.shurik.droidzebra;

import com.google.common.base.Objects;

/**
 * Created by stefan on 18.03.2018.
 */ // Zebra move representation
public class Move {
    public static int PASS = -1;
    public int mMove;

    public Move(int move) {
        set(move);
    }

    public Move(int x, int y) {
        set(x, y);
    }

    public void set(int move) {
        mMove = move;
    }

    public void set(int x, int y) {
        mMove = (x + 1) * 10 + y + 1;
    }

    public int getY() {
        return mMove % 10 - 1;
    }

    public int getX() {
        return mMove / 10 - 1;
    }

    public String getText() {
        if (mMove == PASS) return "--";

        byte m[] = new byte[2];
        m[0] = (byte) ('a' + getX());
        m[1] = (byte) ('1' + getY());
        return new String(m);
    }

    @Override
    public String toString() {
        return "Move{" +
                "mMove=" + mMove +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        Move move = (Move) o;
        return mMove == move.mMove;
    }

    @Override
    public int hashCode() {
        return Objects.hashCode(mMove);
    }
}
