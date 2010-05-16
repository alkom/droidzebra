# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
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
        
#zebra/display.c 

#DEFS =          -DINCLUDE_BOOKTOOL -DTEXT_BASED -DZLIB_STATIC -D__linux__ -D__CYGWIN__ -DANDROID
DEFS =          -DZLIB_STATIC -D__linux__ -D__CYGWIN__ -DANDROID
WARNINGS =      -Wall -Wcast-align -Wwrite-strings -Wstrict-prototypes -Winline
OPTS =          -O4 -s -fomit-frame-pointer -falign-functions=32 -finline-limit=3200
LOCAL_CFLAGS += $(OPTS) $(WARNINGS) $(DEFS)
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -lm -lz
 
include $(BUILD_SHARED_LIBRARY)

