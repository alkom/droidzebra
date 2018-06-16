package de.earthlingz.oerszebra;

import de.earthlingz.oerszebra.parser.Gameparser;

/**
 * Created by stefan on 18.03.2018.
 */

public interface GameController {
    Gameparser getParser();

    void setUpBoard(String s);
}
