package de.earthlingz.oerszebra;

import android.content.Context;

import com.shurik.droidzebra.GameContext;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

public class AndroidContext implements GameContext {
    private Context context;

    public AndroidContext(Context context) {
        this.context = context;
    }


    @Override
    public InputStream open(String fromAssetPath) throws IOException {
        return context.getAssets().open(fromAssetPath);
    }

    @Override
    public File getFilesDir() {
        return context.getFilesDir();
    }
}
