package com.shurik.droidzebra;

public interface GameMessage {
    void putString(String key, String value);

    void putByteArray(String key, byte[] value);

    void putInt(String key, int value);

    void setCode(int msgError);

    void setObject(Object obj);

    int getCode();

    String getString(String key);

    int getInt(String key);

    byte[] getByteArray(String key);

    Object getObject();
}
