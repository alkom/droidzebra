package de.earthlingz.oerszebra;

import android.util.Log;

import com.shurik.droidzebra.CandidateMove;
import com.shurik.droidzebra.GameMessageService;
import com.shurik.droidzebra.Move;
import com.shurik.droidzebra.ZebraBoard;
import com.shurik.droidzebra.ZebraEngine;

import java.util.LinkedList;
import java.util.Locale;

public class DroidZebraHandler implements GameMessageService {

    private BoardState state;
    private ZebraEngine mZebraThread;
    private GameController controller;

    DroidZebraHandler(BoardState state, GameController controller, ZebraEngine mZebraThread) {
        this.controller = controller;

        this.state = state;
        this.mZebraThread = mZebraThread;
    }

    @Override
    public void sendError(String error) {
        controller.showAlertDialog(error);
        mZebraThread.setInitialGameState(new LinkedList<>());
    }

    @Override
    public void sendDebug(String debug) {
        Log.v("OersZebra", debug);
    }

    @Override
    public void sendBoard(ZebraBoard board) {
        String score;
        int sideToMove = board.getSideToMove();

        state.setBoard(board.getBoard());

        state.setmBlackScore(board.getBlackPlayer().getDiscCount());
        state.setmWhiteScore(board.getWhitePlayer().getDiscCount());

        if (sideToMove == ZebraEngine.PLAYER_BLACK) {
            score = String.format(Locale.getDefault(), "•%d", state.getmBlackScore());
        } else {
            score = String.format(Locale.getDefault(), "%d", state.getmBlackScore());
        }
        controller.getStatusView().setTextForID(
                StatusView.ID_SCORE_BLACK,
                score
        );

        if (sideToMove == ZebraEngine.PLAYER_WHITE) {
            score = String.format(Locale.getDefault(), "%d•", state.getmWhiteScore());
        } else {
            score = String.format(Locale.getDefault(), "%d", state.getmWhiteScore());
        }
        controller.getStatusView().setTextForID(
                StatusView.ID_SCORE_WHITE,
                score
        );

        int iStart, iEnd;
        byte[] black_moves = board.getBlackPlayer().getMoves();
        byte[] white_moves = board.getWhitePlayer().getMoves();

        iEnd = black_moves.length;
        iStart = Math.max(0, iEnd - 4);
        for (int i = 0; i < 4; i++) {
            String num_text = String.format(Locale.getDefault(), "%d", i + iStart + 1);
            String move_text;
            if (i + iStart < iEnd) {
                Move move = new Move(black_moves[i + iStart]);
                move_text = move.getText();
            } else {
                move_text = "";
            }
            controller.getStatusView().setTextForID(
                    StatusView.ID_SCORELINE_NUM_1 + i,
                    num_text
            );
            controller.getStatusView().setTextForID(
                    StatusView.ID_SCORELINE_BLACK_1 + i,
                    move_text
            );
        }

        iEnd = white_moves.length;
        iStart = Math.max(0, iEnd - 4);
        for (int i = 0; i < 4; i++) {
            String move_text;
            if (i + iStart < iEnd) {
                Move move = new Move(white_moves[i + iStart]);
                move_text = move.getText();
            } else {
                move_text = "";
            }
            controller.getStatusView().setTextForID(
                    StatusView.ID_SCORELINE_WHITE_1 + i,
                    move_text
            );
        }

        byte move = (byte) board.getLastMove();
        state.setmLastMove(move == Move.PASS ? null : new Move(move));
        state.setMoves(board.getCandidateMoves());
        for (CandidateMove eval : board.getCandidateMoves()) {
            CandidateMove[] moves = state.getMoves();
            for (int i = 0; i < moves.length; i++) {
                if (moves[i].mMove.mMove == eval.mMove.mMove) {
                    moves[i] = eval;
                    break;
                }
            }
        }

        if (controller.getStatusView() != null && board.getOpening() != null) {
            controller.getStatusView().setTextForID(
                    StatusView.ID_STATUS_OPENING,
                    board.getOpening()
            );
        }

        controller.getBoardView().onBoardStateChanged();
    }

    @Override
    public void sendPass() {
        controller.showPassDialog();
    }

    @Override
    public void sendGameStart() {
        // noop
    }

    @Override
    public void sendGameOver() {
        controller.setCandidateMoves(new CandidateMove[]{});
        int max = state.getmBoard().length * state.getmBoard().length;
        if (state.getmBlackScore() + state.getmWhiteScore() < max) {
            //adjust result
            if (state.getmBlackScore() > state.getmWhiteScore()) {
                state.setmBlackScore(max - state.getmWhiteScore());
            } else {
                state.setmWhiteScore(max - state.getmBlackScore());
            }
        }
        controller.showGameOverDialog();
    }

    @Override
    public void sendMoveStart() {

    }

    @Override
    public void sendMoveEnd() {
        controller.dismissBusyDialog();
        if (controller.isHintUp()) {
            controller.setHintUp(false);
            mZebraThread.setPracticeMode(controller.isPraticeMode());
            mZebraThread.sendSettingsChanged();
        }

    }

    @Override
    public void sendEval(String eval) {
        if (controller.getSettingDisplayPV()) {
            controller.getStatusView().setTextForID(
                    StatusView.ID_STATUS_EVAL,
                    eval
            );
        }
    }

    @Override
    public void sendPv(byte[] pv) {
        if (controller.getSettingDisplayPV() && pv != null) {
            StringBuilder pvText = new StringBuilder();
            for (byte move : pv) {
                pvText.append(new Move(move).getText());
                pvText.append(" ");
            }
            controller.getStatusView().setTextForID(
                    StatusView.ID_STATUS_PV,
                    pvText.toString()
            );
        }

    }
}