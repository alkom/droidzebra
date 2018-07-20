package de.earthlingz.oerszebra.parser;

import com.shurik.droidzebra.Move;

import java.util.LinkedList;

/**
 * Created by stefan on 18.03.2018.
 */

public class GameParser {

    private final Parser[] parser;

    public GameParser(Parser... parser) {
        this.parser = parser;
    }

    public LinkedList<Move> makeMoveList(String moves) {
        for (Parser parse : parser) {
            if (parse.canParse(moves)) {
                return parse.makeMoveList(moves);
            }
        }
        return new LinkedList<>();
    }

    public boolean canParse(String moves) {
        for (Parser parse : parser) {
            if (parse.canParse(moves)) {
                return true;
            }
        }
        return false;
    }
}
