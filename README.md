Reversatile - Zebra Reversi for Android 
---------------------------------------------------------------

[![Build Status](https://travis-ci.org/oers/oerszebra.svg?branch=master)](https://travis-ci.org/oers/oerszebra)

This is a fork of Droidzebra: https://github.com/alkom/droidzebra

DroidZebra is a graphical front-end for well-known Zebra Othello
Engine written by Gunnar Andersson. It is one of the strongest
othello-playing programs in the world. More info on Zebra engine
can be found here: http://radagast.se/othello/

The game requires 10MB of storage (external or internal) to play.

Info about Reversi: http://en.wikipedia.org/wiki/Reversi

You will need approximately 15mb of storage space to install
and play the game.

assets/ - compressed book and coeffs2.bin
jni/ - C code Zebra + mods
src/ - Java code
res/ - resource files

Differences in this fork:
- will start in practise mode with highest difficulty
- can read games via intents/copy and paste

Current features:
- Zebra engine
- play human vs human, hunam vs computer, computer vs computer
- multiple levels of play
- practice mode
- unlimited undo


Version 1.0.0

- added functionality to enter games (via copy and paste or manually)
- added functionality to listen to intent from Reversi Wars

Version 1.3.0.
 - guess the best move mode
