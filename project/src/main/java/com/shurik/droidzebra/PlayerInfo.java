package com.shurik.droidzebra;

/**
 * Created by stefan on 18.03.2018.
 */ // player info
public class PlayerInfo {
    public PlayerInfo(int _player, int _skill, int _exact_skill, int _wld_skill, int _player_time, int _increment) {
        assert (_player == ZebraEngine.PLAYER_BLACK || _player == ZebraEngine.PLAYER_WHITE || _player == ZebraEngine.PLAYER_ZEBRA);
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
