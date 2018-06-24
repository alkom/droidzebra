package com.shurik.droidzebra;

public interface GameMessageService {
    void sendError(String error);

    void sendDebug(String debug);

    void sendBoard(ZebraBoard board);

    void sendPass();

    void sendGameStart();

    void sendGameOver();

    void sendMoveStart();

    void sendMoveEnd();

    void sendEval(String eval);

    void sendPv(byte[] moves);
}
