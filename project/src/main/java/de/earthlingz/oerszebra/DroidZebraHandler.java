package de.earthlingz.oerszebra;

import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

import com.shurik.droidzebra.CandidateMove;
import com.shurik.droidzebra.GameMessage;
import com.shurik.droidzebra.GameMessageService;
import com.shurik.droidzebra.Move;
import com.shurik.droidzebra.ZebraEngine;

import java.util.LinkedList;
import java.util.Locale;

public class DroidZebraHandler implements GameMessageHandler, GameMessageService {

    private BoardState state;
    private ZebraEngine mZebraThread;
    private GameController controller;

    private Handler handler;

    public DroidZebraHandler(BoardState state, GameController controller, ZebraEngine mZebraThread) {
        this.controller = controller;

        this.state = state;
        this.mZebraThread = mZebraThread;

        handler = new Handler(m -> handleMessage(convert(m)));
    }

    private GameMessage convert(Message m) {
        return new AndroidMessage(m);
    }

    @Override
    public boolean handleMessage(GameMessage m) {
        // block messages if waiting for something
        switch (m.getCode()) {
            case ZebraEngine.MSG_ERROR: {
                controller.showAlertDialog(m.getString("error"));
                mZebraThread.setInitialGameState(new LinkedList<>());
            }
            break;
            case ZebraEngine.MSG_MOVE_START: {
                // noop
            }
            break;
            case ZebraEngine.MSG_BOARD: {
                String score;
                int sideToMove = m.getInt("side_to_move");

                state.setBoard(m.getByteArray("board"));

                state.setmBlackScore(m.getBundle("black").getInt("disc_count"));
                state.setmWhiteScore(m.getBundle("white").getInt("disc_count"));

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
                byte[] black_moves = getByteArray(m, "black");
                byte[] white_moves = getByteArray(m, "white");

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
                controller.getBoardView().onBoardStateChanged();
            }
            break;

            case ZebraEngine.MSG_CANDIDATE_MOVES: {
                controller.setCandidateMoves((CandidateMove[]) m.getObject());
            }
            break;

            case ZebraEngine.MSG_PASS: {
                controller.showPassDialog();
            }
            break;

            case ZebraEngine.MSG_OPENING_NAME: {
                String mOpeningName = m.getString("opening");
                if (controller.getStatusView() != null) {
                    controller.getStatusView().setTextForID(
                            StatusView.ID_STATUS_OPENING,
                            mOpeningName
                    );
                }
            }
            break;

            case ZebraEngine.MSG_LAST_MOVE: {
                byte move = (byte) m.getInt("move");
                state.setmLastMove(move == Move.PASS ? null : new Move(move));
            }
            break;

            case ZebraEngine.MSG_GAME_OVER: {
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
            break;

            case ZebraEngine.MSG_EVAL_TEXT: {
                if (controller.getSettingDisplayPV()) {
                    controller.getStatusView().setTextForID(
                            StatusView.ID_STATUS_EVAL,
                            m.getString("eval")
                    );
                }
            }
            break;


            case ZebraEngine.MSG_ANALYZE_GAME: {
                Log.i("Analyze",
                        m.getString("analyze")
                );
            }
            break;

            case ZebraEngine.MSG_PV: {
                byte[] pv = m.getByteArray("pv");
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
            break;

            case ZebraEngine.MSG_MOVE_END: {
                controller.dismissBusyDialog();
                if (controller.isHintUp()) {
                    controller.setHintUp(false);
                    mZebraThread.setPracticeMode(controller.isPraticeMode());
                    mZebraThread.sendSettingsChanged();
                }
            }
            break;

            case ZebraEngine.MSG_CANDIDATE_EVALS: {
                CandidateMove[] evals = (CandidateMove[]) m.getObject();
                for (CandidateMove eval : evals) {
                    CandidateMove[] moves = state.getMoves();
                    for (int i = 0; i < moves.length; i++) {
                        if (moves[i].mMove.mMove == eval.mMove.mMove) {
                            moves[i] = eval;
                            break;
                        }
                    }
                }
                controller.getBoardView().invalidate();
            }
            break;

            case ZebraEngine.MSG_DEBUG: {
//				Log.d("DroidZebra", m.getData().getString("message"));
            }
            break;
        }
        return true;
    }

    private byte[] getByteArray(GameMessage m, String bundle) {
        Bundle bun = m.getBundle(bundle);
        if (bun == null) {
            return new byte[0];
        }
        byte[] moves = bun.getByteArray("moves");
        if (moves == null) {
            return new byte[0];
        }
        return moves;
    }

    @Override
    public GameMessage obtainMessage(int msgcode) {
        return new AndroidMessage(handler.obtainMessage(msgcode));
    }

    @Override
    public boolean sendMessage(GameMessage msg) {
        return handler.sendMessage(((AndroidMessage) msg).getMessage());
    }
}