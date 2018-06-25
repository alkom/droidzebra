package de.earthlingz.oerszebra;

interface SettingsProvider {

    void setOnChangeListener(OnChangeListener onChangeListener);

    int getSettingFunction();

    boolean isSettingAutoMakeForcedMoves();

    int getSettingRandomness();

    String getSettingForceOpening();

    boolean isSettingHumanOpenings();

    boolean isSettingPracticeMode();

    boolean isSettingUseBook();

    boolean isSettingDisplayPv();

    boolean isSettingDisplayMoves();

    boolean isSettingDisplayLastMove();

    boolean isSettingDisplayEnableAnimations();

    int getSettingAnimationDelay();

    int getSettingZebraDepth();

    int getSettingZebraDepthExact();

    int getSettingZebraDepthWLD();

    int getSettingSlack();

    int getSettingPerturbation();

    interface OnChangeListener {
        void onChange();
    }
}
