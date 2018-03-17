# This file is part of DroidZebra.
#
#    DroidZebra is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    DroidZebra is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with DroidZebra.  If not, see <http://www.gnu.org/licenses/>
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := droidzebra
LOCAL_SRC_FILES := droidzebra-jni.c \
		droidzebra-autop.c \
		droidzebra-display.c \
		droidzebra-json.c \
		droidzebra-msg.c \
        zebra/bitbcnt.c \
        zebra/bitbmob.c \
        zebra/bitboard.c \
        zebra/bitbtest.c \
        zebra/cntflip.c \
        zebra/counter.c \
        zebra/doflip.c \
        zebra/end.c \
        zebra/epcstat.c \
        zebra/eval.c \
        zebra/game.c \
        zebra/getcoeff.c \
        zebra/globals.c \
        zebra/hash.c \
        zebra/learn.c \
        zebra/midgame.c \
        zebra/moves.c \
        zebra/myrandom.c \
        zebra/opname.c \
        zebra/osfbook.c \
        zebra/patterns.c \
        zebra/pcstat.c \
        zebra/probcut.c \
        zebra/safemem.c \
        zebra/search.c \
        zebra/stable.c \
        zebra/thordb.c \
        zebra/timer.c \
        zebra/unflip.c
     
#DEFS =          -DINCLUDE_BOOKTOOL -DTEXT_BASED -DZLIB_STATIC -D__linux__ -D__CYGWIN__ -DANDROID
DEFS =          -DZLIB_STATIC -D__linux__ -D__CYGWIN__ -DANDROID
WARNINGS =      -Wall -Wcast-align -Wwrite-strings -Wstrict-prototypes -Winline
OPTS =          -O4 -s -fomit-frame-pointer -falign-functions=32 -finline-limit=3200
LOCAL_CFLAGS += $(OPTS) $(WARNINGS) $(DEFS)
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -lm -lz
 
include $(BUILD_SHARED_LIBRARY)

