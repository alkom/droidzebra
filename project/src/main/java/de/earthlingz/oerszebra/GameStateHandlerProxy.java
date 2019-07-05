package de.earthlingz.oerszebra;

import com.shurik.droidzebra.*;


/**
 * Uses Android Handler to proxy GameState changes to Thread on which is created
 */
public class GameStateHandlerProxy implements GameStateListener {

    private GameStateListener receiver;
    private android.os.Handler handler = new android.os.Handler();

    GameStateHandlerProxy(GameStateListener receiver) {
        this.receiver = receiver;
    }

    @Override
    public void onBoard(GameState board) {
        handler.post(() -> receiver.onBoard(board));
    }

    @Override
    public void onPass() {
        handler.post(receiver::onPass);
    }

    @Override
    public void onGameStart() {
        handler.post(receiver::onGameStart);
    }

    @Override
    public void onGameOver() {
        handler.post(receiver::onGameOver);
    }

    @Override
    public void onMoveStart() {
        handler.post(receiver::onMoveStart);
    }

    @Override
    public void onMoveEnd() {
        handler.post(receiver::onMoveEnd);

    }

    @Override
    public void onEval(String eval) {
        handler.post(() -> receiver.onEval(eval));
    }

    @Override
    public void onPv(byte[] moves) {
        handler.post(() -> receiver.onPv(moves));
    }
}