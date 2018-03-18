package de.earthlingz.oerszebra;

import android.content.Intent;
import android.test.ActivityInstrumentationTestCase2;
import android.util.Log;

import com.shurik.droidzebra.ZebraEngine;

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
        Log.i("Board: ", asString(this.getActivity().getBoard()));
        assertSame(3, countSquares(this.getActivity().getBoard(), ZebraEngine.PLAYER_EMPTY));
        assertSame(58, countSquares(this.getActivity().getBoard(), ZebraEngine.PLAYER_WHITE));
        assertSame(3, countSquares(this.getActivity().getBoard(), ZebraEngine.PLAYER_BLACK));
    }

    public void testSkipCompleteGame() throws InterruptedException {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_SEND);
        intent.setType("message/rfc822");
        intent.putExtra(Intent.EXTRA_TEXT, "E6F4C3C4G3D6B4F5G6G5G4H5D7E3F2H4H3H2F3A4H6H7D3F6F7C5A3A2B5A5A6A7B6C6E7C7C8F1G7D8E8H8B8G8G2F8G1H1E1A8B7E2C2B1C1D1D2B3B2A1");

        this.getActivity().onNewIntent(intent);
        Thread.sleep(500);
        //this.getActivity().getEngine().waitForEngineState(ZebraEngine.ES_USER_INPUT_WAIT);
        Log.i("Board: ", asString(this.getActivity().getBoard()));

        assertSame(0, countSquares(this.getActivity().getBoard(), ZebraEngine.PLAYER_EMPTY));
        assertSame(62, countSquares(this.getActivity().getBoard(), ZebraEngine.PLAYER_WHITE));
        assertSame(2, countSquares(this.getActivity().getBoard(), ZebraEngine.PLAYER_BLACK));
    }

    private int countSquares(FieldState[][] board, byte playerEmpty) {
        int result = 0;
        for (FieldState[] row : board) {
            for (FieldState column : row) {
                if (playerEmpty == column.getState()) {
                    result++;
                }
            }
        }
        return result;
    }

    private String asString(FieldState[][] board) {
        StringBuilder builder = new StringBuilder();
        for (FieldState[] row : board) {
            for (FieldState column : row) {
                switch (column.getState()) {
                    case ZebraEngine.PLAYER_WHITE:
                        builder.append("o");
                        break;
                    case ZebraEngine.PLAYER_BLACK:
                        builder.append("x");
                        break;
                    default:
                        builder.append("-");
                        break;
                }
            }
            builder.append("\n");
        }
        return builder.toString();
    }


}
