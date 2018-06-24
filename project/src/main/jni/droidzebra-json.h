/* Copyright (C) 2010 by Alex Kompel  */
/* This file is part of DroidZebra.

    DroidZebra is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    DroidZebra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DroidZebra.  If not, see <http://www.gnu.org/licenses/>
*/


#ifndef __DROIDZEBRA_JSON__
#define __DROIDZEBRA_JSON__

#include <jni.h>

extern jobject droidzebra_json_create(JNIEnv* env, const char* str);
extern int droidzebra_json_put_int(JNIEnv* env, jobject json, const char* key, jint value);
extern int droidzebra_json_put_string(JNIEnv* env, jobject json, const char* key, const char* value);
extern jint  droidzebra_json_get_int(JNIEnv* env, jobject json, const char* key);
extern char* droidzebra_json_get_string(JNIEnv* env, jobject json, const char* key, char* buf, int bufsize);

#endif
