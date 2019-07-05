package com.shurik.droidzebra;

import android.os.AsyncTask;


public class CompletionAsyncTask extends AsyncTask<Void, Void, Void> {
    private Runnable completion;
    private ZebraEngine engine;

    CompletionAsyncTask(final Runnable completion, ZebraEngine engine) {

        this.completion = completion;
        this.engine = engine;
    }

    @Override
    protected Void doInBackground(Void... p) {
        engine.waitForReadyToPlay();
        return null;
    }

    @Override
    protected void onPostExecute(Void result) {
        completion.run();
    }
}
