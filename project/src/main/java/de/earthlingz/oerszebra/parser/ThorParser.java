package de.earthlingz.oerszebra.parser;

import com.shurik.droidzebra.Move;

import java.util.LinkedList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Created by stefan on 18.03.2018.
 */

public class ThorParser implements Parser {

    private final static Pattern p = Pattern.compile("[123]?[0-9]. [abcdefgh-][12345678-]([\\s][abcdefgh-][12345678-])?");

    @Override
    public LinkedList<Move> makeMoveList(String s) {
        LinkedList<Move> moves = new LinkedList<Move>();

        String sanitized = s.replace("\n", " ").replace("\r", " ");
        String input = sanitized.toLowerCase();
        Matcher matcher = p.matcher(input);
        while (matcher.find()) {
            String group = matcher.group();
            String[] split = group.split("\\s");
            if (split[1].matches("[abcdefgh][12345678]")) {
                int first = split[1].charAt(0) - 97;
                int second = Integer.valueOf("" + split[1].charAt(1)) - 1;
                moves.add(new Move(first, second));
            }

            if (split.length > 2 && split[2].matches("[abcdefgh][12345678]")) {
                int first = split[2].charAt(0) - 97;
                int second = Integer.valueOf("" + split[2].charAt(1)) - 1;
                moves.add(new Move(first, second));
            }
        }
        return moves;
    }

    @Override
    public boolean canParse(String s) {
        String sanitized = s.replace("\n", " ").replace("\r", " ");
        String input = sanitized.toLowerCase();
        Matcher matcher = p.matcher(input);
        return matcher.find();
    }
}
