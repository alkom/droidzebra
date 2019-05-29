package de.earthlingz.oerszebra;

import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import androidx.test.rule.ActivityTestRule;
import de.earthlingz.oerszebra.BoardView.GameStateBoardModel;
import org.junit.Before;
import org.junit.Rule;

import java.util.List;

class BasicTest {


    DroidZebra zebra;

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

    void waitForOpenendDialogs() throws InterruptedException {
        while(!hasOpenedDialogs(zebra)) {
            Thread.sleep(100);
        }
    }

   private boolean hasOpenedDialogs(FragmentActivity activity) {
        List<Fragment> fragments = activity.getSupportFragmentManager().getFragments();
        for (Fragment fragment : fragments) {
            if (fragment instanceof DialogFragment) {
                return true;
            }
        }


        return false;
    }

    int countSquares(byte color) {
        GameStateBoardModel state = this.zebra.getState();
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
