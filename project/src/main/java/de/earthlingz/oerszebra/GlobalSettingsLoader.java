package de.earthlingz.oerszebra;

import android.content.Context;
import android.content.SharedPreferences;

import java.util.Locale;


public class GlobalSettingsLoader implements SharedPreferences.OnSharedPreferenceChangeListener, SettingsProvider {
    public static final String SHARED_PREFS_NAME = "droidzebrasettings";

    private final String DEFAULT_SETTING_STRENGTH;
    private final boolean DEFAULT_SETTING_AUTO_MAKE_FORCED_MOVES;
    private final String DEFAULT_SETTING_FORCE_OPENING;
    private final boolean DEFAULT_SETTING_HUMAN_OPENINGS;
    private final boolean DEFAULT_SETTING_PRACTICE_MODE;
    private final boolean DEFAULT_SETTING_USE_BOOK;
    private final boolean DEFAULT_SETTING_DISPLAY_PV;
    private final boolean DEFAULT_SETTING_DISPLAY_MOVES;
    private final boolean DEFAULT_SETTING_DISPLAY_LAST_MOVE;
    private final boolean DEFAULT_SETTING_DISPLAY_ENABLE_ANIMATIONS;

    private final int DEFAULT_SETTING_RANDOMNESS;
    private final int DEFAULT_SETTING_FUNCTION;
    public static final String DEFAULT_SETTING_SENDMAIL = "";


    public static final String
            SETTINGS_KEY_FUNCTION = "settings_engine_function",
            SETTINGS_KEY_STRENGTH = "settings_engine_strength",
            SETTINGS_KEY_AUTO_MAKE_FORCED_MOVES = "settings_engine_auto_make_moves",
            SETTINGS_KEY_RANDOMNESS = "settings_engine_randomness",
            SETTINGS_KEY_FORCE_OPENING = "settings_engine_force_opening",
            SETTINGS_KEY_HUMAN_OPENINGS = "settings_engine_human_openings",
            SETTINGS_KEY_PRACTICE_MODE = "settings_engine_practice_mode",
            SETTINGS_KEY_USE_BOOK = "settings_engine_use_book",
            SETTINGS_KEY_DISPLAY_PV = "settings_ui_display_pv",
            SETTINGS_KEY_DISPLAY_MOVES = "settings_ui_display_moves",
            SETTINGS_KEY_DISPLAY_LAST_MOVE = "settings_ui_display_last_move",
            SETTINGS_KEY_SENDMAIL = "settings_sendmail",
            SETTINGS_KEY_DISPLAY_ENABLE_ANIMATIONS = "settings_ui_display_enable_animations";


    private static final int
            RANDOMNESS_NONE = 0,
            RANDOMNESS_SMALL = 1,
            RANDOMNESS_MEDIUM = 2,
            RANDOMNESS_LARGE = 3,
            RANDOMNESS_HUGE = 4;


    private int settingFunction;
    private boolean settingAutoMakeForcedMoves;
    private int settingRandomness;
    private String settingForceOpening;
    private boolean settingHumanOpenings;
    private boolean settingPracticeMode;
    private boolean settingUseBook;
    private boolean settingDisplayPv;
    private boolean settingDisplayMoves;
    private boolean settingDisplayLastMove;
    private boolean settingDisplayEnableAnimations;

    private int settingSlack;
    private int settingPerturbation;


    private int settingAnimationDelay = 1000;


    private int settingZebraDepth = 1;
    private int settingZebraDepthExact = 1;
    private int settingZebraDepthWLD = 1;

    private Context context;
    private OnChangeListener onChangeListener;

    public GlobalSettingsLoader(Context context) {

        this.context = context;

        DEFAULT_SETTING_STRENGTH = context.getString(R.string.default_search_depth);
        settingFunction = DEFAULT_SETTING_FUNCTION = Integer.parseInt(context.getString(R.string.default_engine_function));
        settingAutoMakeForcedMoves = DEFAULT_SETTING_AUTO_MAKE_FORCED_MOVES = Boolean.parseBoolean(context.getString(R.string.default_auto_make_moves));
        settingRandomness = DEFAULT_SETTING_RANDOMNESS = Integer.parseInt(context.getString(R.string.default_randomness));
        settingForceOpening = DEFAULT_SETTING_FORCE_OPENING = context.getString(R.string.default_forced_opening);
        settingHumanOpenings = DEFAULT_SETTING_HUMAN_OPENINGS = Boolean.parseBoolean(context.getString(R.string.default_human_openings));
        settingPracticeMode = DEFAULT_SETTING_PRACTICE_MODE = Boolean.parseBoolean(context.getString(R.string.default_practice_mode));
        settingUseBook = DEFAULT_SETTING_USE_BOOK = Boolean.parseBoolean(context.getString(R.string.default_use_book));
        settingDisplayPv = DEFAULT_SETTING_DISPLAY_PV = Boolean.parseBoolean(context.getString(R.string.default_display_principal_variation));
        settingDisplayMoves = DEFAULT_SETTING_DISPLAY_MOVES = Boolean.parseBoolean(context.getString(R.string.default_display_moves));
        settingDisplayLastMove = DEFAULT_SETTING_DISPLAY_LAST_MOVE = Boolean.parseBoolean(context.getString(R.string.default_display_last_move));
        settingDisplayEnableAnimations = DEFAULT_SETTING_DISPLAY_ENABLE_ANIMATIONS = Boolean.parseBoolean(context.getString(R.string.default_enable_animations));

        loadSettings();
        context.getSharedPreferences(SHARED_PREFS_NAME, 0).registerOnSharedPreferenceChangeListener(this);

    }

    private boolean loadSettings() {
        int settingsFunction, settingZebraDepth, settingZebraDepthExact, settingZebraDepthWLD;
        int settingRandomness;
        boolean settingAutoMakeForcedMoves;
        String settingZebraForceOpening;
        boolean settingZebraHumanOpenings;
        boolean settingZebraPracticeMode;
        boolean settingZebraUseBook;

        SharedPreferences settings = context.getSharedPreferences(SHARED_PREFS_NAME, 0);

        settingsFunction = Integer.parseInt(settings.getString(SETTINGS_KEY_FUNCTION, String.format(Locale.getDefault(), "%d", DEFAULT_SETTING_FUNCTION)));
        String[] strength = settings.getString(SETTINGS_KEY_STRENGTH, DEFAULT_SETTING_STRENGTH).split("\\|");
        settingZebraDepth = Integer.parseInt(strength[0]);
        settingZebraDepthExact = Integer.parseInt(strength[1]);
        settingZebraDepthWLD = Integer.parseInt(strength[2]);

        settingAutoMakeForcedMoves = settings.getBoolean(SETTINGS_KEY_AUTO_MAKE_FORCED_MOVES, DEFAULT_SETTING_AUTO_MAKE_FORCED_MOVES);
        settingRandomness = Integer.parseInt(settings.getString(SETTINGS_KEY_RANDOMNESS, String.format(Locale.getDefault(), "%d", DEFAULT_SETTING_RANDOMNESS)));
        settingZebraForceOpening = settings.getString(SETTINGS_KEY_FORCE_OPENING, DEFAULT_SETTING_FORCE_OPENING);
        settingZebraHumanOpenings = settings.getBoolean(SETTINGS_KEY_HUMAN_OPENINGS, DEFAULT_SETTING_HUMAN_OPENINGS);
        settingZebraPracticeMode = settings.getBoolean(SETTINGS_KEY_PRACTICE_MODE, DEFAULT_SETTING_PRACTICE_MODE);
        settingZebraUseBook = settings.getBoolean(SETTINGS_KEY_USE_BOOK, DEFAULT_SETTING_USE_BOOK);


        boolean bZebraSettingChanged = (
                getSettingFunction() != settingsFunction
                        || this.getSettingZebraDepth() != settingZebraDepth
                        || this.getSettingZebraDepthExact() != settingZebraDepthExact
                        || this.getSettingZebraDepthWLD() != settingZebraDepthWLD
                        || this.isSettingAutoMakeForcedMoves() != settingAutoMakeForcedMoves
                        || this.getSettingRandomness() != settingRandomness
                        || !getSettingForceOpening().equals(settingZebraForceOpening)
                        || isSettingHumanOpenings() != settingZebraHumanOpenings
                        || isSettingPracticeMode() != settingZebraPracticeMode
                        || isSettingUseBook() != settingZebraUseBook
        );

        settingFunction = settingsFunction;
        this.settingZebraDepth = settingZebraDepth;
        this.settingZebraDepthExact = settingZebraDepthExact;
        this.settingZebraDepthWLD = settingZebraDepthWLD;
        this.settingAutoMakeForcedMoves = settingAutoMakeForcedMoves;
        this.settingRandomness = settingRandomness;
        settingForceOpening = settingZebraForceOpening;
        settingHumanOpenings = settingZebraHumanOpenings;
        settingPracticeMode = settingZebraPracticeMode;
        settingUseBook = settingZebraUseBook;

        settingDisplayPv = settings.getBoolean(SETTINGS_KEY_DISPLAY_PV, DEFAULT_SETTING_DISPLAY_PV);

        settingDisplayMoves = settings.getBoolean(SETTINGS_KEY_DISPLAY_MOVES, DEFAULT_SETTING_DISPLAY_MOVES);
        settingDisplayLastMove = settings.getBoolean(SETTINGS_KEY_DISPLAY_LAST_MOVE, DEFAULT_SETTING_DISPLAY_LAST_MOVE);

        settingDisplayEnableAnimations = settings.getBoolean(SETTINGS_KEY_DISPLAY_ENABLE_ANIMATIONS, DEFAULT_SETTING_DISPLAY_ENABLE_ANIMATIONS);


        switch (settingRandomness) {
            case RANDOMNESS_SMALL:
                settingSlack = 1;
                settingPerturbation = 1;
                break;
            case RANDOMNESS_MEDIUM:
                settingSlack = 4;
                settingPerturbation = 2;
                break;
            case RANDOMNESS_LARGE:
                settingSlack = 6;
                settingPerturbation = 6;
                break;
            case RANDOMNESS_HUGE:
                settingSlack = 10;
                settingPerturbation = 16;
                break;
            case RANDOMNESS_NONE:
            default:
                settingSlack = 0;
                settingPerturbation = 0;
                break;
        }


        return bZebraSettingChanged;
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        if (loadSettings() && onChangeListener != null) {
            onChangeListener.onChange();
        }
    }

    @Override
    public void setOnChangeListener(OnChangeListener onChangeListener) {
        this.onChangeListener = onChangeListener;
    }

    @Override
    public int getSettingFunction() {
        return settingFunction;
    }

    @Override
    public boolean isSettingAutoMakeForcedMoves() {
        return settingAutoMakeForcedMoves;
    }

    @Override
    public int getSettingRandomness() {
        return settingRandomness;
    }

    @Override
    public String getSettingForceOpening() {
        return settingForceOpening;
    }

    @Override
    public boolean isSettingHumanOpenings() {
        return settingHumanOpenings;
    }

    @Override
    public boolean isSettingPracticeMode() {
        return settingPracticeMode;
    }

    @Override
    public boolean isSettingUseBook() {
        return settingUseBook;
    }

    @Override
    public boolean isSettingDisplayPv() {
        return settingDisplayPv;
    }

    @Override
    public boolean isSettingDisplayMoves() {
        return settingDisplayMoves;
    }

    @Override
    public boolean isSettingDisplayLastMove() {
        return settingDisplayLastMove;
    }

    @Override
    public boolean isSettingDisplayEnableAnimations() {
        return settingDisplayEnableAnimations;
    }

    @Override
    public int getSettingAnimationDelay() {
        return settingAnimationDelay;
    }

    @Override
    public int getSettingZebraDepth() {
        return settingZebraDepth;
    }

    @Override
    public int getSettingZebraDepthExact() {
        return settingZebraDepthExact;
    }

    @Override
    public int getSettingZebraDepthWLD() {
        return settingZebraDepthWLD;
    }

    @Override
    public int getSettingSlack() {
        return settingSlack;
    }

    @Override
    public int getSettingPerturbation() {
        return settingPerturbation;
    }

}
