package de.earthlingz.oerszebra;

import android.app.ActionBar;

/**
 * Created by stefan on 17.03.2018.
 */
class ActionBarHelper {
    private DroidZebra droidZebra;

    public ActionBarHelper(DroidZebra droidZebra) {
        this.droidZebra = droidZebra;
    }

    void show() {
        ActionBar actionBar = droidZebra.getActionBar();
        if (actionBar != null) {
            actionBar.show();
        }
    }

    void hide() {
        ActionBar actionBar = droidZebra.getActionBar();
        if (actionBar != null) {
            actionBar.hide();
        }
    }
}
