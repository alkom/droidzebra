package de.earthlingz.oerszebra;

import android.content.DialogInterface;
import android.content.Intent;
import android.support.v7.app.AlertDialog;
import androidx.test.rule.ActivityTestRule;
import com.shurik.droidzebra.ZebraEngine;
import de.earthlingz.oerszebra.BoardView.GameStateBoardModel;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import java.lang.ref.WeakReference;

import static org.junit.Assert.assertSame;


public class InvalidmoveTest {

    private DroidZebra zebra;

    @Rule
    public ActivityTestRule<DroidZebra> activityRule
            = new ActivityTestRule<>(DroidZebra.class);

    @Before
    public void init() throws InterruptedException {
        zebra = activityRule.getActivity();
        while (!zebra.initialized()) {
            Thread.sleep(100);
        }
    }

    @Test
    public void testIssue24() throws InterruptedException {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_SEND);
        intent.setType("message/rfc822");
        intent.putExtra(Intent.EXTRA_TEXT, "D3D4D5D6");

        zebra.onNewIntent(intent);
        Thread.sleep(1000);
        //zebra.getEngine().waitForEngineState(ZebraEngine.ES_USER_INPUT_WAIT);

        int countWait = 0;
        while (zebra.getAlert() == null && countWait < 100) {
            Thread.sleep(100);
            countWait++;
        }
        WeakReference<AlertDialog> alert = zebra.getAlert();
        AlertDialog diag = alert.get();

        zebra.runOnUiThread(() -> diag.getButton(DialogInterface.BUTTON_POSITIVE).performClick());

        Thread.sleep(1000);
        assertSame(60, countSquares(ZebraEngine.PLAYER_EMPTY));
        assertSame(2, countSquares(ZebraEngine.PLAYER_WHITE));
        assertSame(2, countSquares(ZebraEngine.PLAYER_BLACK));

    }

    private int countSquares(byte color) {
        GameStateBoardModel state = zebra.getState();
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
