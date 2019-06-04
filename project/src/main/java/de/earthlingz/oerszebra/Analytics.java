package de.earthlingz.oerszebra;

import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.support.v7.app.AlertDialog;
import com.crashlytics.android.Crashlytics;
import com.google.firebase.analytics.FirebaseAnalytics;
import io.fabric.sdk.android.Fabric;

import static android.content.Context.MODE_PRIVATE;
import static de.earthlingz.oerszebra.GlobalSettingsLoader.SHARED_PREFS_NAME;

public class Analytics {

    public static final String ANALYTICS_SETTING = "analytics_setting";
    private final DroidZebra app;

    Analytics(DroidZebra app) {
        this.app = app;
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

    public static void settingsChanged(Context app) {
        final SharedPreferences settings =
                app.getSharedPreferences(SHARED_PREFS_NAME, MODE_PRIVATE);

        boolean consent = settings.getBoolean(ANALYTICS_SETTING, true);

        handleConsent(app, consent);
    }

    public Analytics build() {

        final SharedPreferences settings =
                app.getSharedPreferences(SHARED_PREFS_NAME, MODE_PRIVATE);
        boolean consent = settings.getBoolean("analytics_setting", false);

        if (consent) { //one time only, needs reboot of the app to work
            Fabric.with(app, new Crashlytics());
        }

        handleConsent(app, consent);

        return this;

    }

    private static void handleConsent(Context app, boolean consent) {

        FirebaseAnalytics instance = FirebaseAnalytics.getInstance(app);
        instance.setAnalyticsCollectionEnabled(consent);

        if (!consent) {
            instance.resetAnalyticsData();
        }


    }
}
