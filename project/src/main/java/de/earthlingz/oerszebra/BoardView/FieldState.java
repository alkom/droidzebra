package de.earthlingz.oerszebra.BoardView;

public interface FieldState {
    byte getState();

    boolean isEmpty();

    boolean isBlack();

    boolean isFlipped();
}
