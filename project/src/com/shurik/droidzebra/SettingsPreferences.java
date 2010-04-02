package com.shurik.droidzebra;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceActivity;


public class SettingsPreferences extends PreferenceActivity 
	implements SharedPreferences.OnSharedPreferenceChangeListener {
	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
	    super.onCreate(savedInstanceState);
        getPreferenceManager().setSharedPreferencesName(DroidZebra.SHARED_PREFS_NAME);
	    addPreferencesFromResource(R.xml.settings);
        getPreferenceManager().getSharedPreferences().registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
    }
    
    @Override
    protected void onDestroy() {
        getPreferenceManager().getSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
        super.onDestroy();
    }

	@Override
	public void onSharedPreferenceChanged(SharedPreferences sharedPreferences,
			String key) {
		// TODO Auto-generated method stub
		
	}
}
