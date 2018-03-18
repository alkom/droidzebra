package de.earthlingz.oerszebra.components;

import javax.inject.Singleton;

import dagger.Component;
import de.earthlingz.oerszebra.DroidZebra;
import de.earthlingz.oerszebra.modules.AppModule;

@Singleton
@Component(modules = {AppModule.class})
public interface GameComponent {
    // void inject(MainActivity activity);
    void inject(DroidZebra fragment);
    // void inject(MyService service);
}