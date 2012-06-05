/* Copyright (C) 2012 by Alex Kompel  */

#ifndef __DROIDZEBRA_JSON__
#define __DROIDZEBRA_JSON__

#include <jni.h>

extern jobject droidzebra_json_create(JNIEnv* env, const char* str);
extern int droidzebra_json_put_int(JNIEnv* env, jobject json, const char* key, jint value);
extern int droidzebra_json_put_string(JNIEnv* env, jobject json, const char* key, const char* value);
extern jint  droidzebra_json_get_int(JNIEnv* env, jobject json, const char* key);
extern char* droidzebra_json_get_string(JNIEnv* env, jobject json, const char* key, char* buf, int bufsize);

#endif
