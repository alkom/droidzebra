package de.earthlingz.oerszebra;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.DialogFragment;
import android.text.InputType;
import android.widget.EditText;

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

    public GameController getController() {
        return (GameController) getActivity();
    }

    @NonNull
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

            if (possibleMatch != null && getController().getParser().canParse(possibleMatch)) {
                input.setText(possibleMatch);
            }
        }


        // Set up the buttons
        builder.setPositiveButton(R.string.dialog_ok, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                getController().setUpBoard(input.getText().toString());
            }
        });
        builder.setNegativeButton(R.string.dialog_cancel, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.cancel();
            }
        });

        return builder.create();
    }

    public void setClipBoard(ClipboardManager clipBoard) {
        this.clipBoard = clipBoard;
    }
}
