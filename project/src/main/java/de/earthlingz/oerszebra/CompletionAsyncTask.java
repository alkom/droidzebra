package de.earthlingz.oerszebra;

import android.os.AsyncTask;

import com.shurik.droidzebra.ZebraEngine;

public class CompletionAsyncTask extends AsyncTask<Void, Void, Void> {
    private int zebraEngineStatus;
    private Runnable completion;
    private ZebraEngine engine;

    public CompletionAsyncTask(int zebraEngineStatus, final Runnable completion, ZebraEngine engine) {

        this.zebraEngineStatus = zebraEngineStatus;
        this.completion = completion;
        this.engine = engine;
    }

    @Override
    protected Void doInBackground(Void... p) {
        engine.waitForEngineState(zebraEngineStatus);
        return null;
    }

    @Override
    protected void onPostExecute(Void result) {
        completion.run();
    }
}
