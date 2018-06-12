package de.earthlingz.oerszebra;

import com.google.common.base.Supplier;
import com.google.common.base.Suppliers;

import de.earthlingz.oerszebra.parser.Gameparser;
import de.earthlingz.oerszebra.parser.ReversiWarsParser;
import de.earthlingz.oerszebra.parser.ThorParser;

class ZebraServices {
    private static final Supplier<BoardState> boardState = Suppliers.memoize(BoardState::new);
    private static final Supplier<Gameparser> gameParser = Suppliers.memoize(() -> new Gameparser(new ReversiWarsParser(), new ThorParser()));

    public static BoardState getBoardState() {
        return boardState.get();
    }

    public static Gameparser getGameParser() {
        return gameParser.get();
    }
}
