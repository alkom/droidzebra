package de.earthlingz.oerszebra;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import com.crashlytics.android.Crashlytics;
import com.google.firebase.analytics.FirebaseAnalytics;
import com.shurik.droidzebra.GameState;
import io.fabric.sdk.android.Fabric;

import javax.annotation.Nullable;
import java.util.concurrent.atomic.AtomicReference;

import static android.content.Context.MODE_PRIVATE;
import static de.earthlingz.oerszebra.GlobalSettingsLoader.SHARED_PREFS_NAME;

public class Analytics {

    static final String ANALYTICS_SETTING = "analytics_setting";
    private static AtomicReference<DroidZebra> app = new AtomicReference<>();

    public static void setApp(DroidZebra zebra) {
        app.set(zebra);
    }

    public static void ask(DroidZebra app) {
        final SharedPreferences settings =
                app.getSharedPreferences(SHARED_PREFS_NAME, MODE_PRIVATE);
        if (settings.getBoolean("isFirstRun", true)) {
            settings.edit().putBoolean("isFirstRun", false).apply();
            new AlertDialog.Builder(app)
                    .setTitle(R.string.ask_analytics)
                    .setMessage(R.string.ask_analytics_help)
                    .setPositiveButton(R.string.ask_analytics_accept, new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            Analytics.initSettings(app, true);
                        }
                    })
                    .setNeutralButton(R.string.ask_analytics_deny, new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            Analytics.initSettings(app, false);
                        }
                    }).show();
        }
    }

    private static void initSettings(DroidZebra app, boolean consent) {
        final SharedPreferences settings =
                app.getSharedPreferences(SHARED_PREFS_NAME, MODE_PRIVATE);

        settings.edit().putBoolean("analytics_setting", consent).apply();

        handleConsent(app, consent);
    }

    public static void settingsChanged() {
        final SharedPreferences settings =
                app.get().getSharedPreferences(SHARED_PREFS_NAME, MODE_PRIVATE);

        boolean consent = settings.getBoolean(ANALYTICS_SETTING, true);

        handleConsent(app.get(), consent);
    }

    public static void log(String id, String message) {

        if(app.get() == null) {
            return;
        }

        Fabric.getLogger().log(Log.INFO, id, message);
    }

    public static void build() {

        if(app.get() == null) {
            return;
        }

        DroidZebra droidZebra = app.get();
        final SharedPreferences settings =
                droidZebra.getSharedPreferences(SHARED_PREFS_NAME, MODE_PRIVATE);
        boolean consent = settings.getBoolean("analytics_setting", false);

        if (consent) { //one time only, needs reboot of the app to work
            Fabric.with(droidZebra, new Crashlytics());
        }

        handleConsent(droidZebra, consent);

        return;

    }

    private static void handleConsent(Context app, boolean consent) {

        FirebaseAnalytics instance = FirebaseAnalytics.getInstance(app);
        instance.setAnalyticsCollectionEnabled(consent);

        if (!consent) {
            instance.resetAnalyticsData();
        }
    }

    public static void converse(String converse, @Nullable Bundle bundle) {
        if(app.get() != null) {
            FirebaseAnalytics fb = FirebaseAnalytics.getInstance(app.get());
            fb.logEvent(converse, bundle);
        }

    }

    public static void error(String msg, GameState state) {

        if(app.get() == null) {
            return;
        }
        Fabric.getLogger().log(Log.ERROR, "alert", msg + (state!=null? " -" + state.getMoveSequenceAsString():""));
    }
}
