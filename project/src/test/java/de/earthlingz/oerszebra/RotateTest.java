package de.earthlingz.oerszebra;

import com.shurik.droidzebra.GameState;
import de.earthlingz.oerszebra.parser.ReversiWarsParser;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;

public class RotateTest {

    @Test
    public void testRotation() {
        GameState gameState = new GameState(8, new ReversiWarsParser().makeMoveList("F5D6C4D3C3F4F3E3E6C5F6G5H4E7C6F7G8F2G1E2F8H6F1D1D2D8D7C8C7B8G4C1C2H3G6H5G3H2G2B5A5H1B6H7B7A8A7A6G7E8H8A4B4E1B3A3B1"));
        byte[] rotate = gameState.rotate();
        gameState = new GameState(8, rotate, rotate.length);
        String result = gameState.getMoveSequenceAsString();
        assertEquals(result.trim(), "c4e3f5e6f6c5c6d6d3f4c3b4a5d2f3c2b1c7b8d7c1a3c8e8e7e1e2f1f2g1b5f8f7a6b3a4b6a7b7g4h4a8g3a2g2h1h2h3b2d1a1h5g5d8g6h6g8");

    }
}
