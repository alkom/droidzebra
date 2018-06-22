package com.shurik.droidzebra;

public interface GameMessageService {
    GameMessage obtainMessage(int msgcode);

    boolean sendMessage(GameMessage msg);
}
