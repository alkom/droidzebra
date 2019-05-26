package com.shurik.droidzebra;

import com.google.common.base.Objects;

/**
 * Created by stefan on 18.03.2018.
 */ // Zebra move representation
public class Move {
    public static int PASS = -1;
    private int moveInt;

    public Move(int move) {
        moveInt = move;
    }

    public Move(int x, int y) {
        moveInt = (x + 1) * 10 + y + 1;
    }

    public int getY() {
        return moveInt % 10 - 1;
    }

    public int getX() {
        return moveInt / 10 - 1;
    }

    public String getText() {
        if (moveInt == PASS) return "--";

        byte m[] = new byte[2];
        m[0] = (byte) ('a' + getX());
        m[1] = (byte) ('1' + getY());
        return new String(m);
    }

    @Override
    public String toString() {
        return "Move{" +
                "mMove=" + moveInt +
                '}';
    }

    @Override
    public boolean equals(Object o)
    {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        Move move = (Move) o;
        return moveInt == move.moveInt;
    }

    @Override
    public int hashCode() {
        return Objects.hashCode(moveInt);
    }

    public int getMoveInt() {
        return moveInt;
    }
}
