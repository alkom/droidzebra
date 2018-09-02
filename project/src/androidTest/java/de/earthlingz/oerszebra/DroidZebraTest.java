package de.earthlingz.oerszebra;

import android.content.Intent;
import android.test.ActivityInstrumentationTestCase2;
import com.shurik.droidzebra.ZebraEngine;
import de.earthlingz.oerszebra.BoardView.GameStateBoardModel;

/**
 * This is a simple framework for a test of an Application.  See
 * {@link android.test.ApplicationTestCase ApplicationTestCase} for more information on
 * how to write and extend Application tests.
 * <p/>
 * To run this test, you can type:
 * adb shell am instrument -w \
 * -e class de.earthlingz.oerszebra.DroidZebraTest \
 * de.earthlingz.oerszebra.tests/android.test.InstrumentationTestRunner
 */
public class DroidZebraTest extends ActivityInstrumentationTestCase2<DroidZebra> {

    public DroidZebraTest() {
        super("de.earthlingz.oerszebra", DroidZebra.class);
    }

    
    @Override
    protected void setUp() throws Exception {
    	super.setUp();
        while (!getActivity().initialized()) {
            Thread.sleep(100);
        }
    }


    @Override
    protected void tearDown() throws Exception {
    	super.tearDown();
    }

//    public TestView_Functional_Test() {
//    	super("project.base", TestView.class);
//    }

    public void testSkipWithFinalEmptySquares() throws InterruptedException {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_SEND);
        intent.setType("message/rfc822");
        intent.putExtra(Intent.EXTRA_TEXT, "F5D6C4D3C3F4F3E3E6C5F6G5H4E7C6F7G8F2G1E2F8H6F1D1D2D8D7C8C7B8G4C1C2H3G6H5G3H2G2B5A5H1B6H7B7A8A7A6G7E8H8A4B4E1B3A3B1");

        this.getActivity().onNewIntent(intent);

        Thread.sleep(500);
        assertSame(3, countSquares(ZebraEngine.PLAYER_EMPTY));
        assertSame(58, countSquares(ZebraEngine.PLAYER_WHITE));
        assertSame(3, countSquares(ZebraEngine.PLAYER_BLACK));
        assertSame(this.getActivity().getState().getBlackScore(), 3);
        assertSame(this.getActivity().getState().getWhiteScore(), 61);
    }

    public void testSkipCompleteGame() throws InterruptedException {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_SEND);
        intent.setType("message/rfc822");
        intent.putExtra(Intent.EXTRA_TEXT, "E6F4C3C4G3D6B4F5G6G5G4H5D7E3F2H4H3H2F3A4H6H7D3F6F7C5A3A2B5A5A6A7B6C6E7C7C8F1G7D8E8H8B8G8G2F8G1H1E1A8B7E2C2B1C1D1D2B3B2A1");

        this.getActivity().onNewIntent(intent);
        Thread.sleep(500);
        //this.getActivity().getEngine().waitForEngineState(ZebraEngine.ES_USER_INPUT_WAIT);

        assertSame(0, countSquares(ZebraEngine.PLAYER_EMPTY));
        assertSame(62, countSquares(ZebraEngine.PLAYER_WHITE));
        assertSame(2, countSquares(ZebraEngine.PLAYER_BLACK));
    }

    public void testIssue22() throws InterruptedException {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_SEND);
        intent.setType("message/rfc822");
        intent.putExtra(Intent.EXTRA_TEXT, "D3C5F6F5F4C3C4D2E2B4D1F3B5E3F2F1A4D6E6E7F7B6E8C6B3A5D7A3E1A6G1A2C2C7B8D8C8G8G6H6G5H5G4H4H3G7H8F8H7A8A7B7A1B2G3G2H2H1C1B1");

        this.getActivity().onNewIntent(intent);
        Thread.sleep(500);
        //this.getActivity().getEngine().waitForEngineState(ZebraEngine.ES_USER_INPUT_WAIT);
        assertSame(0, countSquares(ZebraEngine.PLAYER_EMPTY));
        assertSame(32, countSquares(ZebraEngine.PLAYER_WHITE));
        assertSame(32, countSquares(ZebraEngine.PLAYER_BLACK));
    }

    private int countSquares(byte color) {
        GameStateBoardModel state = this.getActivity().getState();
        int result = 0;
        for (int y = 0, boardLength = state.getBoardHeight(); y < boardLength; y++) {
            for (int x = 0, rowLength = state.getBoardRowWidth(); x < rowLength; x++) {
                if (color == state.getFieldByte(x,y)) {
                    result++;
                }
            }
        }
        return result;
    }


}
