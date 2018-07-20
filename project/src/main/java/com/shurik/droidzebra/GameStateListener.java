package com.shurik.droidzebra;

public interface GameStateListener {
    default void onBoard(GameState board) {
    }

    default void onPass() {
    }

    default void onGameStart() {
    }

    default void onGameOver() {
    }

    default void onMoveStart() {
    }

    default void onMoveEnd() {
    }

    default void onEval(String eval) {
    }

    default void onPv(byte[] moves) {
    }
}
