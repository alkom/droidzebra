package com.shurik.droidzebra;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

public interface GameContext {
    InputStream open(String fromAssetPath) throws IOException;

    File getFilesDir();
}
