package de.earthlingz.oerszebra;

import com.shurik.droidzebra.Move;

import org.junit.Test;

import java.util.LinkedList;

import de.earthlingz.oerszebra.parser.Gameparser;
import de.earthlingz.oerszebra.parser.ThorParser;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

/**
 * Created by stefan on 17.03.2018.
 */
public class GameParserTest {

    private Gameparser thorParser = ZebraServices.getGameParser();

    @Test
    public void oerszebra() {
        String game = "Generiert von Reversatile (https://play.google.com/store/apps/details?id=de.earthlingz.oerszebra) \n" +
                "Datum: Sun Jun 10 14:33:24 GMT+02:00 2018 \n" +
                "\n" +
                "Zugfolge: f5d6c3d3c4f4e6b3d2c5f3g4e3d7b4c2b5a5a4f2e2a3g3f1e1c1g1d1b1g2h4h3h2h1h5g5h6g6h7g7g8a1b6c6c7f6e7d8e8b2a2b7a6a7b8a8c8f7f8 \n" +
                "\n" +
                "Player  (B)  19:45  (W)  Player";


        assertThat(thorParser.canParse(game), is(true));

        LinkedList<Move> moves = thorParser.makeMoveList(game);
        assertThat(moves.size(), is(59));
    }

    @Test
    public void reversiwars() {
        String game = "F5D6C4D3C3F4F3E3E6C5F6G5H4E7C6F7G8F2G1E2F8H6F1D1D2D8D7C8C7B8G4C1C2H3G6H5G3H2G2B5A5H1B6H7B7A8A7A6G7E8H8A4B4E1B3A3B1";


        assertThat(thorParser.canParse(game), is(true));

        LinkedList<Move> moves = thorParser.makeMoveList(game);
        assertThat(moves.size(), is(57));
    }


}