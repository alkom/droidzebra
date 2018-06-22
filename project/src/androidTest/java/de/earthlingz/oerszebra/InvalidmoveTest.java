package de.earthlingz.oerszebra;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.test.ActivityInstrumentationTestCase2;
import android.util.Log;

import com.shurik.droidzebra.ZebraEngine;

import java.lang.ref.WeakReference;

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
public class InvalidmoveTest extends ActivityInstrumentationTestCase2<DroidZebra> {

    public InvalidmoveTest() {
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

    public void testIssue24() throws InterruptedException {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_SEND);
        intent.setType("message/rfc822");
        intent.putExtra(Intent.EXTRA_TEXT, "D3D4D5D6");

        this.getActivity().onNewIntent(intent);
        Thread.sleep(1000);
        //this.getActivity().getEngine().waitForEngineState(ZebraEngine.ES_USER_INPUT_WAIT);
        Log.i("Board: ", asString(this.getActivity().getBoard()));

        int countWait = 0;
        while (getActivity().getAlert() == null && countWait < 100) {
            Thread.sleep(100);
            countWait++;
        }
        WeakReference<AlertDialog> alert = getActivity().getAlert();
        AlertDialog diag = alert.get();

        getActivity().runOnUiThread(() -> diag.getButton(DialogInterface.BUTTON_POSITIVE).performClick());

        Thread.sleep(1000);
        assertSame(60, countSquares(this.getActivity().getBoard(), ZebraEngine.PLAYER_EMPTY));
        assertSame(2, countSquares(this.getActivity().getBoard(), ZebraEngine.PLAYER_WHITE));
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
