package com.shurik.droidzebra;

import org.json.JSONArray;
import org.json.JSONException;

public class MoveList {
    private byte[] moves;

    MoveList (JSONArray moves) throws JSONException {
        this.moves = new byte[moves.length()];
        for (int i = 0; i < moves.length(); i++) {
            this.moves[i] = (byte) moves.getInt(i);
        }
    }

    MoveList() {
        moves = new byte[0];
    }

    public int length() {
        return moves.length;
    }

    public int getMoveInt(int i) {
        return moves[i];
    }

    public String getMoveText(int i) {
        return new Move(moves[i]).getText(); //TODO optimize - get rid of 'new Move'
    }

    public byte getMoveByte(int i) {
        return moves[i];
    }
}
