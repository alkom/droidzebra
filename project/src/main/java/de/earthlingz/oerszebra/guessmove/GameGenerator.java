package de.earthlingz.oerszebra.guessmove;

import android.support.annotation.NonNull;
import com.shurik.droidzebra.*;

public class GameGenerator {
    private ZebraEngine engine;

    public GameGenerator(@NonNull ZebraEngine engine) {
        this.engine = engine;
    }

    public void generate(EngineConfig generatorConfig, EngineConfig postConfig, int movesCount, OnGenerated listener) {


        engine.newGame(generatorConfig, onGameStateReadyListener(postConfig, movesCount, listener));
    }

    private ZebraEngine.OnGameStateReadyListener onGameStateReadyListener(EngineConfig postConfig, int movesCount, OnGenerated listener) {
        return new ZebraEngine.OnGameStateReadyListener() {
            @Override
            public void onGameStateReady(GameState gameState) {
                GameStateListener waitForSettle = new GameStateListener() {
                    @Override
                    public void onBoard(GameState state) {

                        //validate if candidate moves are valid to current board
                        ByteBoard byteBoard = state.getByteBoard();
                        CandidateMove[] candidateMoves = state.getCandidateMoves();
                        for (CandidateMove candidateMove : candidateMoves) {
                            if (!byteBoard.isEmpty(candidateMove.getX(), candidateMove.getY())) {
                                return; // overlap - candidate moves are not loaded yet
                            }
                        }
                        if (candidateMoves.length <= 1) {
                            return; //this move is forced or pass - let zebra play until some unforced move
                        }

                        returnGeneratedGame();

                    }

                    private void returnGeneratedGame() {
                        engine.updateConfig(gameState, postConfig);
                        gameState.setGameStateListener(null);
                        listener.onGenerated(gameState);
                    }

                    @Override
                    public void onGameOver() {
                        returnGeneratedGame();
                    }
                };

                GameStateListener waitForMovesCount = new GameStateListener() {

                    @Override
                    public void onBoard(GameState state) {
                        if (movesCount == state.getDisksPlayed()) {
                            gameState.setGameStateListener(waitForSettle);
                        }
                    }

                };


                gameState.setGameStateListener(waitForMovesCount);

            }
        };
    }

    public interface OnGenerated {
        void onGenerated(GameState gameState);
    }
}
