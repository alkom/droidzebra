package de.earthlingz.oerszebra.parser;

import com.shurik.droidzebra.Move;

import java.util.LinkedList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Created by stefan on 18.03.2018.
 */

public class ReversiWarsParser implements Parser {

    private final static Pattern p = Pattern.compile("(D1|C4|E6|F5)([ABCDEFGH][12345678])+");

    @Override
    public LinkedList<Move> makeMoveList(String s) {
        LinkedList<Move> moves = new LinkedList<Move>();

        Matcher matcher = p.matcher(sanitize(s.toUpperCase()));
        if (!matcher.find()) {
            return new LinkedList<Move>();
        }
        String group = matcher.group();
        System.out.println("Match: " + group);
        for (int i = 0; i < group.length(); i += 2) {
            int first = group.charAt(i) - 65;
            int second = Integer.valueOf("" + group.charAt(i + 1)) - 1;
            moves.add(new Move(first, second));
            System.out.println(first + "/" + second);
        }
        return moves;
    }

    @Override
    public boolean canParse(String s) {
        CharSequence sanitize = sanitize(s.toUpperCase());
        Matcher matcher = p.matcher(sanitize);
        return matcher.find();
    }

    private CharSequence sanitize(String s) {
        String s1 = Pattern.quote(s);
        return s1;
    }
}
