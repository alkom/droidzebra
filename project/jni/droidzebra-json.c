/* Copyright (C) 2012 by Alex Kompel  */
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


#include <assert.h>
#include <stdlib.h>
#include "droidzebra-json.h"

jobject droidzebra_json_create(JNIEnv* env, const char* str)
{
	jclass cls;
	jmethodID constructor;
	jobject object;

	cls = (*env)->FindClass(env, "org/json/JSONObject");
	if ((*env)->ExceptionCheck(env)) return NULL;
	assert(cls!=NULL);

	if( !str ) {
		constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
		object = (*env)->NewObject(env, cls, constructor);
		if ((*env)->ExceptionCheck(env)) return NULL;
	} else {
		jobject strobj = (*env)->NewStringUTF(env, str);
		constructor = (*env)->GetMethodID(env, cls, "<init>", "(Ljava/lang/String;)V");
		object = (*env)->NewObject(env, cls, constructor, strobj);
		if ((*env)->ExceptionCheck(env)) return NULL;
		(*env)->DeleteLocalRef(env, strobj);
	}
	assert(object!=NULL);
	(*env)->DeleteLocalRef(env, cls);
	return object;
}

int droidzebra_json_put_int(JNIEnv* env, jobject json, const char* key, jint value)
{
	jobject obj;
	jclass cls = (*env)->GetObjectClass(env, json);
	jmethodID mid = (*env)->GetMethodID(env, cls, "put", "(Ljava/lang/String;I)Lorg/json/JSONObject;");
	assert(mid!=0);
	if (mid != 0) {
		jobject keyobj = (*env)->NewStringUTF(env, key);
		obj = (*env)->CallObjectMethod(env, json, mid, keyobj, value);
		if ((*env)->ExceptionCheck(env)) return -1;
		(*env)->DeleteLocalRef(env, obj);
		(*env)->DeleteLocalRef(env, keyobj);
	}
	(*env)->DeleteLocalRef(env, cls);
	return 0;
}

int droidzebra_json_put_string(JNIEnv* env, jobject json, const char* key, const char* value)
{
	jobject obj;
	jclass cls = (*env)->GetObjectClass(env, json);
	jmethodID mid = (*env)->GetMethodID(env, cls, "put", "(Ljava/lang/String;Ljava/lang/Object;)Lorg/json/JSONObject;");
	assert(mid!=0);
	if (mid != 0) {
		jobject keyobj = (*env)->NewStringUTF(env, key);
		jobject valueobj = (*env)->NewStringUTF(env, value);
		obj = (*env)->CallObjectMethod(env, json, mid, keyobj, valueobj);
		if ((*env)->ExceptionCheck(env)) return -1;
		(*env)->DeleteLocalRef(env, obj);
		(*env)->DeleteLocalRef(env, keyobj);
		(*env)->DeleteLocalRef(env, valueobj);
	}
	(*env)->DeleteLocalRef(env, cls);
	return 0;
}

jint droidzebra_json_get_int(JNIEnv* env, jobject json, const char* key)
{
	jint value = 0;
	jclass cls = (*env)->GetObjectClass(env, json);
	jmethodID mid = (*env)->GetMethodID(env, cls, "getInt", "(Ljava/lang/String;)I");
	assert(mid!=0);
	if (mid != 0) {
		jobject keyobj = (*env)->NewStringUTF(env, key);
		value = (*env)->CallIntMethod(env, json, mid, keyobj);
		if ((*env)->ExceptionCheck(env)) return -1;
		(*env)->DeleteLocalRef(env, keyobj);
	}
	(*env)->DeleteLocalRef(env, cls);
	return value;
}

char* droidzebra_json_get_string(JNIEnv* env, jobject json, const char* key, char* buf, int bufsize)
		{
	char* retval = buf;
	const char* str = NULL;
	jclass cls;
	jmethodID mid;

	cls = (*env)->GetObjectClass(env, json);
	mid = (*env)->GetMethodID(env, cls, "getString", "(Ljava/lang/String;)Ljava/lang/String;");
	assert(mid!=0);
	if (mid != 0) {
		jobject obj_str;
		jobject keyobj = (*env)->NewStringUTF(env, key);

		obj_str = (*env)->CallObjectMethod(env, json, mid, (*env)->NewStringUTF(env, keyobj));
		if ((*env)->ExceptionCheck(env)) return NULL;

		str = (*env)->GetStringUTFChars(env, obj_str, NULL);
		if( !str ) return NULL;

		strncpy(buf, str, bufsize);
		if( strlen(str)>=bufsize ) retval = NULL;

		(*env)->ReleaseStringUTFChars(env, obj_str, str);
		(*env)->DeleteLocalRef(env, obj_str);
		(*env)->DeleteLocalRef(env, keyobj);
	}
	(*env)->DeleteLocalRef(env, cls);
	return retval;
		}
