#ifndef __DROIDZEBRA__
#define __DROIDZEBRA__

#include <jni.h>

JNIEnv* droidzebra_jnienv(void);
jobject droidzebra_RPC_callback(jint message, jobject json);
void droidzebra_message(int category, const char* json_str);
int droidzebra_message_debug(const char* format, ...);

#endif

