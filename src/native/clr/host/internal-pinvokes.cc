#include <host/os-bridge.hh>
#include <runtime-base/internal-pinvokes.hh>
#include <host/host.hh>

using namespace xamarin::android;

int
_monodroid_gref_get ()
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_gref_get");
    return OSBridge::get_gc_gref_count ();
}

int
_monodroid_gref_log_new (jobject curHandle, char curType, jobject newHandle, char newType, const char *threadName, int threadId, const char *from, int from_writable)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_gref_log_new");
    return OSBridge::_monodroid_gref_log_new (curHandle, curType, newHandle, newType, threadName, threadId, from, from_writable);
}

void
monodroid_log (LogLevel level, LogCategories category, const char *message)
{
	switch (level) {
		case LogLevel::Verbose:
		case LogLevel::Debug:
			log_debug_nocheck (category, std::string_view { message });
			break;

		case LogLevel::Info:
			log_info_nocheck (category, std::string_view { message });
			break;

		case LogLevel::Warn:
		case LogLevel::Silent: // warn is always printed
			log_warn (category, std::string_view { message });
			break;

		case LogLevel::Error:
			log_error (category, std::string_view { message });
			break;

		case LogLevel::Fatal:
			log_fatal (category, std::string_view { message });
			break;

		default:
		case LogLevel::Unknown:
		case LogLevel::Default:
			log_info_nocheck (category, std::string_view { message });
			break;
	}
}

int
_monodroid_getifaddrs (struct _monodroid_ifaddrs **ifap)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_getifaddrs");
	*ifap = nullptr;
	return 0;
}

void
_monodroid_freeifaddrs (struct _monodroid_ifaddrs *ifa)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_freeifaddrs");
}

void
monodroid_free (void *ptr)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "monodroid_free");
    free (ptr);
}

void
_monodroid_gref_log (const char *message)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_gref_log");
    monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, message);
}

void
_monodroid_gref_log_delete (jobject handle, char type, const char *threadName, int threadId, const char *from, int from_writable)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_gref_log_delete");
}

int
_monodroid_weak_gref_get ()
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_weak_gref_get");
	return 0;
}

void
_monodroid_weak_gref_new (jobject curHandle, char curType, jobject newHandle, char newType, const char *threadName, int threadId, const char *from, int from_writable)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_weak_gref_new");
}

void
_monodroid_weak_gref_delete (jobject handle, char type, const char *threadName, int threadId, const char *from, int from_writable)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_weak_gref_delete");
}


void*
_monodroid_timezone_get_default_id ()
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_timezone_get_default_id");
	log_error (LOG_DEFAULT, "Failed to convert Java TimeZone ID to UTF8 (out of memory?)"sv);
	return nullptr;
}

xamarin::android::managed_timing_sequence*
monodroid_timing_start (const char *message)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "monodroid_timing_start");
	return nullptr;
}

void 
monodroid_timing_stop (xamarin::android::managed_timing_sequence *sequence, const char *message)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "monodroid_timing_stop");
}

char*
monodroid_TypeManager_get_java_class_name (jclass klass)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "monodroid_TypeManager_get_java_class_name");
	// TODO:
	return Host::get_java_class_name_for_TypeManager (klass);
}

void
_monodroid_gc_wait_for_bridge_processing ()
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_gc_wait_for_bridge_processing");
}

const char*
_monodroid_lookup_replacement_type (const char *jniSimpleReference)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_lookup_replacement_type");
	return nullptr;
}

struct JniRemappingReplacementMethod
{
	const char    *target_type;
	const char    *target_name;
	// const char    *target_signature;
	// const int32_t  param_count;
	const bool     is_static;
};

const
JniRemappingReplacementMethod*
_monodroid_lookup_replacement_method_info (const char *jniSourceType, const char *jniMethodName, const char *jniMethodSignature)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "_monodroid_lookup_replacement_method_info");
	return nullptr;
}

void
monodroid_log_traces (uint32_t kind, const char *first_line)
{
	monodroid_log (LogLevel::Info, LogCategories::LOG_ASSEMBLY, "monodroid_log_traces");
}
