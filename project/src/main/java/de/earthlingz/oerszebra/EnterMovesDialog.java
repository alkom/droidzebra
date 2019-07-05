package de.earthlingz.oerszebra;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.os.Bundle;
import android.text.InputType;
import android.widget.EditText;
import androidx.fragment.app.DialogFragment;

import javax.annotation.Nonnull;

/**
 * Created by stefan on 18.03.2018.
 */ //-------------------------------------------------------------------------
// Moves as Text
public class EnterMovesDialog extends DialogFragment {

    private ClipboardManager clipBoard;

    public static EnterMovesDialog newInstance(ClipboardManager clipBoard) {
        EnterMovesDialog moves = new EnterMovesDialog();
        moves.setClipBoard(clipBoard);
        return moves;
    }

    public MoveStringConsumer getController() {
        return (MoveStringConsumer) getActivity();
    }

    @Nonnull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
        builder.setTitle(R.string.menu_enter_moves);

        // Set up the input
        final EditText input = new EditText(getActivity());
        input.setInputType(InputType.TYPE_CLASS_TEXT);
        input.setLines(8);
        input.setMinLines(3);
        builder.setView(input);

        if (clipBoard != null && clipBoard.getPrimaryClip() != null) {
            String possibleMatch = null;
            ClipData primaryClip = clipBoard.getPrimaryClip();
            for (int i = 0; i < primaryClip.getItemCount(); i++) {
                ClipData.Item clip = primaryClip.getItemAt(i);
                CharSequence charSequence = clip.coerceToText(getActivity().getBaseContext());
                if (charSequence != null) {
                    possibleMatch = charSequence.toString();
                    break;
                }
            }

            if (possibleMatch != null && ZebraServices.getGameParser().canParse(possibleMatch)) {
                input.setText(possibleMatch);
            }
        }


        // Set up the buttons
        builder.setPositiveButton(R.string.dialog_ok, (dialog, which) -> {
            Analytics.converse("enter_moves", null);
            Analytics.log("enter_moves", input.getText().toString());
            getController().consumeMovesString(input.getText().toString());
        });
        builder.setNegativeButton(R.string.dialog_cancel, (dialog, which) -> dialog.cancel());

        return builder.create();
    }

    public void setClipBoard(ClipboardManager clipBoard) {
        this.clipBoard = clipBoard;
    }
}
