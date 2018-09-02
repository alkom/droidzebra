package de.earthlingz.oerszebra;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.test.ActivityInstrumentationTestCase2;

import com.shurik.droidzebra.ZebraEngine;
import de.earthlingz.oerszebra.BoardView.GameStateBoardModel;

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

        int countWait = 0;
        while (getActivity().getAlert() == null && countWait < 100) {
            Thread.sleep(100);
            countWait++;
        }
        WeakReference<AlertDialog> alert = getActivity().getAlert();
        AlertDialog diag = alert.get();

        getActivity().runOnUiThread(() -> diag.getButton(DialogInterface.BUTTON_POSITIVE).performClick());

        Thread.sleep(1000);
        assertSame(60, countSquares(ZebraEngine.PLAYER_EMPTY));
        assertSame(2, countSquares(ZebraEngine.PLAYER_WHITE));
        assertSame(2, countSquares(ZebraEngine.PLAYER_BLACK));

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
