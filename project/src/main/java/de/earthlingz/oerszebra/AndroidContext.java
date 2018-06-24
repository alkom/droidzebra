package de.earthlingz.oerszebra;

import android.content.Context;

import com.shurik.droidzebra.GameContext;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

public class AndroidContext implements GameContext {
    private Context droidZebra;

    public AndroidContext(Context droidZebra) {
        this.droidZebra = droidZebra;
    }


    @Override
    public InputStream open(String fromAssetPath) throws IOException {
        return droidZebra.getAssets().open(fromAssetPath);
    }

    @Override
    public File getFilesDir() {
        return droidZebra.getFilesDir();
    }
}
