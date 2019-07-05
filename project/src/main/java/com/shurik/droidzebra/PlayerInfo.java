package com.shurik.droidzebra;


/**
 * Created by stefan on 18.03.2018.
 */ // player info
public class PlayerInfo {
    PlayerInfo(int _skill, int _exact_skill, int _wld_skill) {
        skill = _skill;
        exactSolvingSkill = _exact_skill;
        wldSolvingSkill = _wld_skill;
    }

    public int skill;
    public int exactSolvingSkill;
    public int wldSolvingSkill;
}
