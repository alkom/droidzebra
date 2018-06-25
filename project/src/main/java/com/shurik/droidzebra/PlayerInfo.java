package com.shurik.droidzebra;

import de.earthlingz.oerszebra.BuildConfig;

/**
 * Created by stefan on 18.03.2018.
 */ // player info
public class PlayerInfo {
    public PlayerInfo(int _player, int _skill, int _exact_skill, int _wld_skill, int _player_time, int _increment) {
        if (BuildConfig.DEBUG && !(_player == ZebraEngine.PLAYER_BLACK || _player == ZebraEngine.PLAYER_WHITE || _player == ZebraEngine.PLAYER_ZEBRA)) {
            throw new AssertionError();
        }
        playerColor = _player;
        skill = _skill;
        exactSolvingSkill = _exact_skill;
        wldSolvingSkill = _wld_skill;
        playerTime = _player_time;
        playerTimeIncrement = _increment;

    }

    public int playerColor;
    public int skill;
    public int exactSolvingSkill;
    public int wldSolvingSkill;
    public int playerTime;
    public int playerTimeIncrement;
}
