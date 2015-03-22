## DroidZebra Reversi for Android ##

---


DroidZebra is a graphical front-end for well-known Zebra Othello
Engine written by Gunnar Andersson. It is one of the strongest
othello-playing programs in the world. More info on Zebra engine
can be found here: http://radagast.se/othello/



[![](http://www.android.com/images/brand/get_it_on_play_logo_large.png)](https://play.google.com/store/apps/details?id=com.shurik.droidzebra)

Version 1.5:

  * added board animations (can be disabled in settings)
  * added "hint" menu option (show evals on the board for one move)
  * updated code for newer Android versions

Version 1.4.1

  * bugfix: Randomness option in Settings was not available

Version 1.4

  * Japanese translation (by Keishiro Tsuchiya)
  * REDO function (by Keishiro Tsuchiya)
  * email function (by Keishiro Tsuchiya)
  * russian translation
  * bugfixes

Build requirements (from project/BUILD)
  * Android SDK
  * Android NDK rev 8 or above
  * Apache Ant

Create local.properties file in project folder:
```
sdk.dir=<Andorid SDK dir>
ndk.dir=<Andorid NDK dir>
```

Debug build: ant debug

Release build: ant release

More info on android command line build:
http://developer.android.com/guide/developing/building/building-cmdline.html