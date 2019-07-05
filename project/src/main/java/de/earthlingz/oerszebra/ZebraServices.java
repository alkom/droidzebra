package de.earthlingz.oerszebra;

import com.google.common.base.Supplier;
import com.google.common.base.Suppliers;

import de.earthlingz.oerszebra.BoardView.BoardViewModel;
import de.earthlingz.oerszebra.BoardView.GameStateBoardModel;
import de.earthlingz.oerszebra.parser.GameParser;
import de.earthlingz.oerszebra.parser.ReversiWarsParser;
import de.earthlingz.oerszebra.parser.ThorParser;

class ZebraServices {
    private static final Supplier<GameStateBoardModel> boardState = Suppliers.memoize(GameStateBoardModel::new);
    private static final Supplier<GameParser> gameParser = Suppliers.memoize(() -> new GameParser(new ReversiWarsParser(), new ThorParser()));

    public static GameStateBoardModel getBoardState() {
        return boardState.get();
    }

    public static GameParser getGameParser() {
        return gameParser.get();
    }
}
