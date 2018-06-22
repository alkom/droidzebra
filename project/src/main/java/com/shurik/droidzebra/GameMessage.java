package com.shurik.droidzebra;

import android.os.Bundle;

public interface GameMessage {
    void putString(String key, String value);

    void putByteArray(String key, byte[] value);

    void putInt(String key, int value);

    void putBundle(String key, Bundle value);

    void setCode(int msgError);

    void setObject(Object obj);

    int getCode();

    String getString(String key);

    int getInt(String key);

    byte[] getByteArray(String key);

    Bundle getBundle(String key);

    Object getObject();
}
