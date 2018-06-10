package de.earthlingz.oerszebra;

import android.support.multidex.MultiDexApplication;

import de.earthlingz.oerszebra.components.DaggerGameComponent;
import de.earthlingz.oerszebra.components.GameComponent;
import de.earthlingz.oerszebra.modules.AppModule;

public class Reversatile extends MultiDexApplication {
    private GameComponent mGameComponent;

    @Override
    public void onCreate() {
        super.onCreate();

        // Dagger%COMPONENT_NAME%
        mGameComponent = DaggerGameComponent.builder()
                // list of modules that are part of this component need to be created here too
                .appModule(new AppModule(this)) // This also corresponds to the name of your module: %component_name%Module
                .build();

        // If a Dagger 2 component does not have any constructor arguments for any of its modules,
        // then we can use .create() as a shortcut instead:
        //  mGameComponent = com.codepath.dagger.components.DaggerGameComponent.create();
    }

    public GameComponent getGameComponent() {
        return mGameComponent;
    }
}
