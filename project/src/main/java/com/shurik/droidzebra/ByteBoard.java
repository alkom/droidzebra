package com.shurik.droidzebra;

import org.json.JSONArray;
import org.json.JSONException;

import java.util.Arrays;

import static com.shurik.droidzebra.ZebraEngine.PLAYER_BLACK;
import static com.shurik.droidzebra.ZebraEngine.PLAYER_EMPTY;


public class ByteBoard {

    private final byte[] board;
    private final int boardSize;

    /*
     * This constructor should never be public - only package private
     */
    ByteBoard(JSONArray jsonByteBoard, int boardSize) throws JSONException {
        board = new byte[boardSize * boardSize];
        for (int i = 0; i < jsonByteBoard.length(); i++) {
            JSONArray row = jsonByteBoard.getJSONArray(i);
            for (int j = 0; j < row.length(); j++) {
                board[i * boardSize + j] = (byte) row.getInt(j);
            }
        }
        this.boardSize = boardSize;
    }

    public ByteBoard(int boardSize) {
        this.boardSize = boardSize;
        board = new byte[boardSize * boardSize];
        Arrays.fill(board, PLAYER_EMPTY);
    }

    public byte get(int x, int y) {
        return board[x * boardSize + y];
    }

    public boolean isEmpty(int i, int j) {
        return get(i, j) == PLAYER_EMPTY;
    }

    public boolean isBlack(int i, int j) {
        return get(i, j) == PLAYER_BLACK;
    }

    public int size() {
        return boardSize;
    }

    public byte getSequential(int xTimesBoardSizePlusY) {
        return board[xTimesBoardSizePlusY];
    }

    public boolean isSameAs(ByteBoard board) {
        if (board == this) {
            return true;
        }
        if (this.boardSize != board.size()) {
            return false;
        }

        for (int i = 0; i < this.board.length; i++) {
            if (this.getSequential(i) != board.getSequential(i)) {
                return false;
            }
        }
        return true;
    }
}
