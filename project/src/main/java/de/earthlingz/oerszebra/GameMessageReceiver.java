package de.earthlingz.oerszebra;

import com.shurik.droidzebra.ZebraBoard;

interface GameMessageReceiver {
    void onError(String error);

    void onDebug(String debug);

    void onBoard(ZebraBoard board);

    void onPass();

    void onGameStart();

    void onGameOver();

    void onMoveStart();

    void onMoveEnd();

    void onEval(String eval);

    void onPv(byte[] moves);
}
