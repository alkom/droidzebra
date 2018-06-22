package de.earthlingz.oerszebra;

import android.os.Bundle;
import android.os.Message;
import android.support.annotation.Nullable;

import com.shurik.droidzebra.GameMessage;

class AndroidMessage implements GameMessage {
    private Message message;
    private Bundle bundle;

    public AndroidMessage(Message m) {
        this.message = m;
        bundle = m.getData();
    }

    public Message getMessage() {
        return message;
    }

    @Override
    public void putString(String key, String value) {
        bundle.putString(key, value);
    }

    @Override
    public void putByteArray(String key, byte[] value) {
        bundle.putByteArray(key, value);
    }

    @Override
    public void putInt(String key, int value) {
        bundle.putInt(key, value);
    }

    @Override
    public void putBundle(String key, Bundle value) {
        bundle.putBundle(key, value);
    }

    @Override
    public void setCode(int code) {
        message.what = code;
    }

    @Override
    public void setObject(Object obj) {
        message.obj = obj;
    }

    @Override
    public int getCode() {
        return message.what;
    }

    @Override
    @Nullable
    public String getString(String key) {
        return bundle.getString(key);
    }

    @Override
    @Nullable
    public int getInt(String key) {
        return bundle.getInt(key);
    }

    @Override
    @Nullable
    public byte[] getByteArray(String key) {
        return bundle.getByteArray(key);
    }

    @Override
    @Nullable
    public Bundle getBundle(String key) {
        return bundle.getBundle(key);
    }

    @Override
    @Nullable
    public Object getObject() {
        return message.obj;
    }
}
