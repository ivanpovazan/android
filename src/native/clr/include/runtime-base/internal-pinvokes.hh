#pragma once

#include <jni.h>
#include "shared/log_types.hh"
#include "timing.hh"

int _monodroid_getifaddrs (struct _monodroid_ifaddrs **ifap);
void _monodroid_freeifaddrs (struct _monodroid_ifaddrs *ifa);
void monodroid_log (xamarin::android::LogLevel level, LogCategories category, const char *message);
void monodroid_free (void *ptr);
int _monodroid_gref_get ();
void _monodroid_gref_log (const char *message);
int _monodroid_gref_log_new (jobject curHandle, char curType, jobject newHandle, char newType, const char *threadName, int threadId, const char *from, int from_writable);
void _monodroid_gref_log_delete (jobject handle, char type, const char *threadName, int threadId, const char *from, int from_writable);
int _monodroid_weak_gref_get ();
void _monodroid_weak_gref_new (jobject curHandle, char curType, jobject newHandle, char newType, const char *threadName, int threadId, const char *from, int from_writable);
void _monodroid_weak_gref_delete (jobject handle, char type, const char *threadName, int threadId, const char *from, int from_writable);
void* _monodroid_timezone_get_default_id ();
xamarin::android::managed_timing_sequence* monodroid_timing_start (const char *message);
void monodroid_timing_stop (xamarin::android::managed_timing_sequence *sequence, const char *message);
char* monodroid_TypeManager_get_java_class_name (jclass klass);
void _monodroid_gc_wait_for_bridge_processing ();
const char* _monodroid_lookup_replacement_type (const char *jniSimpleReference);

struct JniRemappingReplacementMethod;
const JniRemappingReplacementMethod* _monodroid_lookup_replacement_method_info (const char *jniSourceType, const char *jniMethodName, const char *jniMethodSignature);
void monodroid_log_traces (uint32_t kind, const char *first_line);
