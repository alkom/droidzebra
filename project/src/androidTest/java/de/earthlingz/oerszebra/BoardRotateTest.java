package de.earthlingz.oerszebra;

import android.content.Intent;
import com.shurik.droidzebra.ZebraEngine;
import org.junit.Test;

import static org.junit.Assert.assertSame;


public class BoardRotateTest extends BasicTest {

    @Test
    public void testRotate() throws InterruptedException {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_SEND);
        intent.setType("message/rfc822");
        intent.putExtra(Intent.EXTRA_TEXT, "F5D6C4D3C3F4F3E3E6C5F6G5H4E7C6F7G8F2G1E2F8H6F1D1D2D8D7C8C7B8G4C1C2H3G6H5G3H2G2B5A5H1B6H7B7A8A7A6G7E8H8A4B4E1B3A3B1");

        zebra.runOnUiThread(() -> zebra.onNewIntent(intent));
        //zebra.getEngine().waitForEngineState(ZebraEngine.ES_USER_INPUT_WAIT);

        waitForOpenendDialogs();

        assertSame(3, countSquares(ZebraEngine.PLAYER_EMPTY));
        assertSame(58, countSquares(ZebraEngine.PLAYER_WHITE));
        assertSame(3, countSquares(ZebraEngine.PLAYER_BLACK));
        assertSame(zebra.getState().getBlackScore(), 3);
        assertSame(zebra.getState().getWhiteScore(), 61);

        zebra.runOnUiThread(() -> zebra.rotate());

        Thread.sleep(3000);

        assertSame(3, countSquares(ZebraEngine.PLAYER_EMPTY));
        assertSame(58, countSquares(ZebraEngine.PLAYER_WHITE));
        assertSame(3, countSquares(ZebraEngine.PLAYER_BLACK));
        assertSame(zebra.getState().getBlackScore(), 3);
        assertSame(zebra.getState().getWhiteScore(), 61);

    }
}
