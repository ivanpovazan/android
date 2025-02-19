#include <coreclrhost.h>

#include <xamarin-app.hh>
#include <host/assembly-store.hh>
#include <host/host.hh>
#include <host/host-jni.hh>
#include <host/host-util.hh>
#include <host/os-bridge.hh>
#include <runtime-base/android-system.hh>
#include <runtime-base/jni-wrappers.hh>
#include <runtime-base/logger.hh>
#include <runtime-base/timing-internal.hh>
#include <shared/log_types.hh>
#include <startup/zip.hh>
#include <vector>
#include <dirent.h>

using namespace xamarin::android;

void Host::register_jni_natives(const jchar *typeName_ptr, int32_t typeName_len, jclass jniClass, const jchar *methods_ptr, int32_t methods_len)
{
	void *delegate = nullptr;
	int hr = coreclr_create_delegate (
		clr_host,
		domain_id,
		Constants::MONO_ANDROID_ASSEMBLY_NAME.data (),
		Constants::JNIENVINIT_FULL_TYPE_NAME.data (),
		"RegisterJniNatives",
		&delegate
	);
	log_debug (LOG_ASSEMBLY, "Delegate creation result == {:x}; delegate == {:p}", static_cast<unsigned int>(hr), delegate);

	jnienv_register_jni_natives = reinterpret_cast<jnienv_register_jni_natives_fn>(delegate);

	log_debug (LOG_ASSEMBLY, "Calling RegisterJniNatives");
	jnienv_register_jni_natives(typeName_ptr, typeName_len, jniClass, methods_ptr, methods_len);

	log_debug (LOG_ASSEMBLY, "Calling returned");
}

void Host::clr_error_writer (const char *message) noexcept
{
	log_error (LOG_DEFAULT, "CLR error: {}", optional_string (message));
}

size_t Host::clr_get_runtime_property (const char *key, char *value_buffer, size_t value_buffer_size, void *contract_context) noexcept
{
	log_info (LOG_DEFAULT, "clr_get_runtime_property (\"{}\"...)", key);
	return 0;
}

bool Host::clr_bundle_probe (const char *path, void **data_start, int64_t *size) noexcept
{
	log_info (LOG_DEFAULT, "clr_bundle_probe (\"{}\"...)", path);
	if (data_start == nullptr || size == nullptr) {
		return false; // TODO: abort instead?
	}

	*data_start = AssemblyStore::open_assembly (path, *size);
	log_debug (
		LOG_ASSEMBLY,
		"Assembly data {}mapped ({:p}, {} bytes)",
		*data_start == nullptr ? "not "sv : ""sv,
		*data_start,
		*size
	);

	return *data_start != nullptr && *size > 0;
}

auto Host::zip_scan_callback (std::string_view const& apk_path, int apk_fd, dynamic_local_string<SENSIBLE_PATH_MAX> const& entry_name, uint32_t offset, uint32_t size) -> bool
{
	log_debug (LOG_ASSEMBLY, "zip entry: {}", entry_name.get ());
	if (!found_assembly_store) {
		found_assembly_store = Zip::assembly_store_file_path.compare (0, entry_name.length (), entry_name.get ()) == 0;
		if (found_assembly_store) {
			log_debug (LOG_ASSEMBLY, "Found assembly store in '{}': {}", apk_path, Zip::assembly_store_file_path);
			AssemblyStore::map (apk_fd, apk_path, Zip::assembly_store_file_path, offset, size);
			return false; // This will make the scanner keep the APK open
		}
	}
	return false;
}

void Host::gather_assemblies_and_libraries (jstring_array_wrapper& runtimeApks, bool have_split_apks)
{
	if (!AndroidSystem::is_embedded_dso_mode_enabled ()) {
		Helpers::abort_application ("Filesystem mode not supported yet.");
	}

	int64_t apk_count = static_cast<int64_t>(runtimeApks.get_length ());
	bool got_split_config_abi_apk = false;

	for (int64_t i = 0; i < apk_count; i++) {
		std::string_view apk_file = runtimeApks [static_cast<size_t>(i)].get_string_view ();

		if (have_split_apks) {
			bool scan_apk = false;

			// With split configs we need to scan only the abi apk, because both the assembly stores and the runtime
			// configuration blob are in `lib/{ARCH}`, which in turn lives in the split config APK
			if (!got_split_config_abi_apk && apk_file.ends_with (Constants::split_config_abi_apk_name.data ())) {
				got_split_config_abi_apk = scan_apk = true;
			}

			if (!scan_apk) {
				continue;
			}
		}

		Zip::scan_archive (apk_file, zip_scan_callback);
	}
}

void Host::create_xdg_directory (jstring_wrapper& home, size_t home_len, std::string_view const& relative_path, std::string_view const& environment_variable_name) noexcept
{
	static_local_string<SENSIBLE_PATH_MAX> dir (home_len + relative_path.length ());
	Util::path_combine (dir, home.get_string_view (), relative_path);

	log_debug (LOG_DEFAULT, "Creating XDG directory: {}", optional_string (dir.get ()));
	int rv = Util::create_directory (dir.get (), Constants::DEFAULT_DIRECTORY_MODE);
	if (rv < 0 && errno != EEXIST) {
		log_warn (LOG_DEFAULT, "Failed to create XDG directory {}. {}", optional_string (dir.get ()), strerror (errno));
	}

	if (!environment_variable_name.empty ()) {
		setenv (environment_variable_name.data (), dir.get (), 1);
	}
}

void Host::create_xdg_directories_and_environment (jstring_wrapper &homeDir) noexcept
{
	size_t home_len = strlen (homeDir.get_cstr ());

	constexpr auto XDG_DATA_HOME = "XDG_DATA_HOME"sv;
	constexpr auto HOME_PATH = ".local/share"sv;
	create_xdg_directory (homeDir, home_len, HOME_PATH, XDG_DATA_HOME);

	constexpr auto XDG_CONFIG_HOME = "XDG_CONFIG_HOME"sv;
	constexpr auto CONFIG_PATH = ".config"sv;
	create_xdg_directory (homeDir, home_len, CONFIG_PATH, XDG_CONFIG_HOME);
}

void Host::Java_mono_android_Runtime_initInternal (JNIEnv *env, jclass runtimeClass, jstring lang, jobjectArray runtimeApksJava,
	jstring runtimeNativeLibDir, jobjectArray appDirs, jint localDateTimeOffset, jobject loader,
	jobjectArray assembliesJava, jboolean isEmulator, jboolean haveSplitApks)
{
	privateEnv = env;
	Logger::init_logging_categories ();

	// If fast logging is disabled, log messages immediately
	FastTiming::initialize ((Logger::log_timing_categories() & LogTimingCategories::FastBare) != LogTimingCategories::FastBare);

	size_t total_time_index;
	if (FastTiming::enabled ()) [[unlikely]] {
		_timing = std::make_unique<Timing> ();
		total_time_index = internal_timing->start_event (TimingEventKind::TotalRuntimeInit);
	}

	jstring_array_wrapper applicationDirs (env, appDirs);

	jstring_wrapper jstr (env, lang);
	Util::set_environment_variable ("LANG", jstr);

	jstring_wrapper &home = applicationDirs[Constants::APP_DIRS_FILES_DIR_INDEX];
	Util::set_environment_variable_for_directory ("TMPDIR", applicationDirs[Constants::APP_DIRS_CACHE_DIR_INDEX]);
	Util::set_environment_variable_for_directory ("HOME", home);
	create_xdg_directories_and_environment (home);

	AndroidSystem::detect_embedded_dso_mode (applicationDirs);
	AndroidSystem::set_running_in_emulator (isEmulator);
	AndroidSystem::set_primary_override_dir (home);
	AndroidSystem::create_update_dir (AndroidSystem::get_primary_override_dir ());
	AndroidSystem::setup_environment ();

	jstring_array_wrapper runtimeApks (env, runtimeApksJava);
	AndroidSystem::setup_app_library_directories (runtimeApks, applicationDirs, haveSplitApks);

	// gather_assemblies_and_libraries (runtimeApks, haveSplitApks);
	// TODO: clean this up:

	log_write (LOG_DEFAULT, LogLevel::Info, "Calling CoreCLR initialization routine");
	coreclr_set_error_writer (clr_error_writer);


	const char* path = env->GetStringUTFChars(runtimeNativeLibDir, nullptr);

	std::vector<std::string> files;
	DIR* dir = opendir(path);
	if (dir == nullptr) {
			log_write (LOG_DEFAULT, LogLevel::Info, "dir is null");
	}

	struct dirent* entry;
	while ((entry = readdir(dir)) != nullptr) {
		if (entry->d_type == DT_REG) {
			std::string file_name = entry->d_name;
			if (file_name.size() >= 4 && file_name.substr(file_name.size() - 4) == ".dll") {
				files.emplace_back(file_name);
			}
		}
	}

	for (const auto& file : files) {
		log_write (LOG_DEFAULT, LogLevel::Info, "Found file: " + file);
	}

	std::string result;
	for (const auto& file : files) {
		if (!result.empty()) {
			result += ":";
		}
		result += std::string(path) + "/" + file;
	}
	log_write(LOG_DEFAULT, LogLevel::Info, "Resulting string: " + result);

	std::string executable_path_str = std::string(path) + "/" + Constants::MONO_ANDROID_ASSEMBLY_NAME.data () + ".dll";
	char* executable_path = new char[executable_path_str.size() + 1];
	std::strcpy(executable_path, executable_path_str.c_str());

	const char* appctx_keys[4];
    appctx_keys[0] = "RUNTIME_IDENTIFIER";
    appctx_keys[1] = "APP_CONTEXT_BASE_DIRECTORY";
    appctx_keys[2] = "TRUSTED_PLATFORM_ASSEMBLIES";
	appctx_keys[3] = "PINVOKE_OVERRIDE";

	const char* appctx_values[4];
    appctx_values[0] = "android-arm64";
	appctx_values[1] = path;


	char* tpa_list = new char[result.size() + 1];
	std::strcpy(tpa_list, result.c_str());
	appctx_values[2] = tpa_list;

	char pinvoke_override_addr [16];
    sprintf (pinvoke_override_addr, "%p", &clr_pinvoke_override);
	appctx_values[3] = pinvoke_override_addr;

	unsigned int coreclr_domainId = 0;
    void *coreclr_handle = NULL;

	int hr = coreclr_initialize (
		executable_path,
		Constants::MONO_ANDROID_ASSEMBLY_NAME.data (),
		4,
		appctx_keys,
		appctx_values,
		&clr_host,
		&domain_id
		);

	log_debug (LOG_ASSEMBLY, "CoreCLR init result == {:x}; clr_host == {:p}; domain ID == {}", static_cast<unsigned int>(hr), clr_host, domain_id);

	log_write (LOG_DEFAULT, LogLevel::Info, "CoreCLR initialization routine returned");

	abort_unless (
		clr_host != nullptr,
		[&hr] {
			return detail::_format_message ("Failure to initialize CoreCLR host instance. Returned result 0x%x", static_cast<unsigned int>(hr));
		}
	);

	struct JnienvInitializeArgs init = {};
	init.javaVm                                         = jvm;
	init.env                                            = env;
	init.logCategories                                  = log_categories;
	init.version                                        = env->GetVersion ();
	init.isRunningOnDesktop                             = false;
	init.brokenExceptionTransitions                     = 0;
	init.packageNamingPolicy                            = static_cast<int>(application_config.package_naming_policy);
	init.boundExceptionType                             = 0; // System
	init.jniAddNativeMethodRegistrationAttributePresent = application_config.jni_add_native_method_registration_attribute_present ? 1 : 0;
	init.jniRemappingInUse                              = application_config.jni_remapping_replacement_type_count > 0 || application_config.jni_remapping_replacement_method_index_entry_count > 0;
	init.marshalMethodsEnabled                          = application_config.marshal_methods_enabled;

	// GC threshold is 90% of the max GREF count
	init.grefGcThreshold                                = static_cast<int>(AndroidSystem::get_gref_gc_threshold ());
	init.grefClass                                      = HostUtil::get_class_from_runtime_field (env, runtimeClass, "java_lang_Class", true);
	Class_getName                                       = env->GetMethodID (init.grefClass, "getName", "()Ljava/lang/String;");
	init.Class_forName                                  = env->GetStaticMethodID (init.grefClass, "forName", "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");

	jclass lrefLoaderClass                              = env->GetObjectClass (loader);
    init.Loader_loadClass                               = env->GetMethodID (lrefLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    env->DeleteLocalRef (lrefLoaderClass);

	init.grefLoader                                     = env->NewGlobalRef (loader);
	init.grefIGCUserPeer                                = HostUtil::get_class_from_runtime_field (env, runtimeClass, "mono_android_IGCUserPeer", true);
	init.grefGCUserPeerable                             = HostUtil::get_class_from_runtime_field (env, runtimeClass, "net_dot_jni_GCUserPeerable", true);

	log_info (LOG_GC, "GREF GC Threshold: {}", init.grefGcThreshold);

	// TODO: GC bridge to initialize here

	OSBridge::initialize_on_runtime_init (env, runtimeClass);

	log_debug (LOG_DEFAULT, "Calling into managed runtime init"sv);

	size_t native_to_managed_index;
	if (FastTiming::enabled ()) [[unlikely]] {
		native_to_managed_index = internal_timing->start_event (TimingEventKind::NativeToManagedTransition);
	}

	log_debug (LOG_ASSEMBLY, "Creating UCO delegate to {}.Initialize", Constants::JNIENVINIT_FULL_TYPE_NAME);
	void *delegate = nullptr;
	hr = coreclr_create_delegate (
		clr_host,
		domain_id,
		Constants::MONO_ANDROID_ASSEMBLY_NAME.data (),
		Constants::JNIENVINIT_FULL_TYPE_NAME.data (),
		"Initialize",
		&delegate
	);
	log_debug (LOG_ASSEMBLY, "Delegate creation result == {:x}; delegate == {:p}", static_cast<unsigned int>(hr), delegate);
	// TODO: make S_OK & friends known to us
	// if (hr != S_OK) {
	// }

	auto initialize = reinterpret_cast<jnienv_initialize_fn> (delegate);
	abort_unless (
		initialize != nullptr,
		"Failed to obtain unmanaged-callers-only pointer to the Android.Runtime.JNIEnvInit.Initialize method."
	);
	initialize (&init);

	if (FastTiming::enabled ()) [[unlikely]] {
		internal_timing->end_event (native_to_managed_index);
	}

	if (FastTiming::enabled ()) [[unlikely]] {
		internal_timing->end_event (total_time_index);
	}
}

auto Host::Java_JNI_OnLoad (JavaVM *vm, [[maybe_unused]] void *reserved) noexcept -> jint
{
	log_write (LOG_DEFAULT, LogLevel::Info, "Host OnLoad");
	jvm = vm;
	AndroidSystem::init_max_gref_count ();
	return JNI_VERSION_1_6;
}

char*
Host::get_java_class_name_for_TypeManager (jclass klass) noexcept
{
	log_debug (LOG_ASSEMBLY, "get_java_class_name_for_TypeManager klass: {:p}", reinterpret_cast<void*>(klass));
	if (klass == nullptr || Class_getName == nullptr)
		return nullptr;

	JNIEnv *env = privateEnv; //OSBridge.ensure_jnienv ();
	jstring name = reinterpret_cast<jstring> (env->CallObjectMethod (klass, Class_getName));
	if (name == nullptr) {
		log_error (LOG_DEFAULT, "Failed to obtain Java class name for object at {:p}", reinterpret_cast<void*>(klass));
		return nullptr;
	}

	const char *mutf8 = env->GetStringUTFChars (name, nullptr);
	if (mutf8 == nullptr) {
		log_error (LOG_DEFAULT, "Failed to convert Java class name to UTF8 (out of memory?)"sv);
		env->DeleteLocalRef (name);
		return nullptr;
	}
	char *ret = strdup (mutf8);

	env->ReleaseStringUTFChars (name, mutf8);
	env->DeleteLocalRef (name);

	char *dot = strchr (ret, '.');
	while (dot != nullptr) {
		*dot = '/';
		dot = strchr (dot + 1, '.');
	}

	return ret;
}