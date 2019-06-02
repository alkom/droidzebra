package de.earthlingz.oerszebra;

import com.crashlytics.android.Crashlytics;
import io.fabric.sdk.android.Fabric;

public class Analytics {

    private final DroidZebra app;

    public Analytics(DroidZebra app) {
        this.app = app;
    }
    public void build() {
        Fabric.with(app, new Crashlytics());
    }
}
