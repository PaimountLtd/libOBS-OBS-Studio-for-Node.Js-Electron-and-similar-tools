#include "nodeobs_api.h"
#include "osn-source.hpp"
#include "util/lexer.h"

#ifdef _WIN32

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif

#define _WIN32_WINNT 0x0502

#include <ShlObj.h>
#include <codecvt>
#include <locale>
#include <mutex>
#include <string>
#include "nodeobs_content.h"
#endif

#ifdef _MSC_VER
#include <direct.h>
#define getcwd _getcwd
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <audiopolicy.h>
#include <mmdeviceapi.h>

#include <util/windows/ComPtr.hpp>
#include <util/windows/HRError.hpp>
#include <util/windows/WinHandle.hpp>

#include "error.hpp"
#include "shared.hpp"

#define BUFFSIZE 512
#define CONNECTING_STATE 0
#define READING_STATE 1

std::string                                            g_moduleDirectory = "";
os_cpu_usage_info_t*                                   cpuUsageInfo      = nullptr;
uint64_t                                               lastBytesSent     = 0;
uint64_t                                               lastBytesSentTime = 0;
std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
std::string                                            slobs_plugin;
std::vector<std::pair<std::string, obs_module_t*>>     obsModules;

void OBS_API::Register(ipc::server& srv)
{
	std::shared_ptr<ipc::collection> cls = std::make_shared<ipc::collection>("API");

	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_API_initAPI", std::vector<ipc::type>{ipc::type::String, ipc::type::String}, OBS_API_initAPI));
	cls->register_function(
	    std::make_shared<ipc::function>("OBS_API_destroyOBS_API", std::vector<ipc::type>{}, OBS_API_destroyOBS_API));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_API_getPerformanceStatistics", std::vector<ipc::type>{}, OBS_API_getPerformanceStatistics));
	cls->register_function(std::make_shared<ipc::function>(
	    "SetWorkingDirectory", std::vector<ipc::type>{ipc::type::String}, SetWorkingDirectory));
	cls->register_function(
	    std::make_shared<ipc::function>("StopCrashHandler", std::vector<ipc::type>{}, StopCrashHandler));
	cls->register_function(std::make_shared<ipc::function>("OBS_API_QueryHotkeys", std::vector<ipc::type>{}, QueryHotkeys));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_API_ProcessHotkeyStatus",
	    std::vector<ipc::type>{ipc::type::UInt64, ipc::type::Int32},
	    ProcessHotkeyStatus));

	srv.register_collection(cls);
}

void replaceAll(std::string& str, const std::string& from, const std::string& to)
{
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
	}
};

void OBS_API::SetWorkingDirectory(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	g_moduleDirectory = args[0].value_str;
	replaceAll(g_moduleDirectory, "\\", "/");
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	rval.push_back(ipc::value(g_moduleDirectory));
	AUTO_DEBUG;
}

static string GenerateTimeDateFilename(const char* extension)
{
	time_t     now       = time(0);
	char       file[256] = {};
	struct tm* cur_time;

	cur_time = localtime(&now);
	snprintf(
	    file,
	    sizeof(file),
	    "%d-%02d-%02d %02d-%02d-%02d.%s",
	    cur_time->tm_year + 1900,
	    cur_time->tm_mon + 1,
	    cur_time->tm_mday,
	    cur_time->tm_hour,
	    cur_time->tm_min,
	    cur_time->tm_sec,
	    extension);

	return string(file);
}

static bool GetToken(lexer* lex, string& str, base_token_type type)
{
	base_token token;
	if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;
	if (token.type != type)
		return false;

	str.assign(token.text.array, token.text.len);
	return true;
}

static bool ExpectToken(lexer* lex, const char* str, base_token_type type)
{
	base_token token;
	if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;
	if (token.type != type)
		return false;

	return strref_cmp(&token.text, str) == 0;
}

/* os_dirent mimics POSIX dirent structure.
* Perhaps a better cross-platform solution can take
* place but this is as cross-platform as it gets
* for right now.  */
static uint64_t ConvertLogName(const char* name)
{
	lexer  lex;
	string year, month, day, hour, minute, second;

	lexer_init(&lex);
	lexer_start(&lex, name);

	if (!GetToken(&lex, year, BASETOKEN_DIGIT))
		return 0;
	if (!ExpectToken(&lex, "-", BASETOKEN_OTHER))
		return 0;
	if (!GetToken(&lex, month, BASETOKEN_DIGIT))
		return 0;
	if (!ExpectToken(&lex, "-", BASETOKEN_OTHER))
		return 0;
	if (!GetToken(&lex, day, BASETOKEN_DIGIT))
		return 0;
	if (!GetToken(&lex, hour, BASETOKEN_DIGIT))
		return 0;
	if (!ExpectToken(&lex, "-", BASETOKEN_OTHER))
		return 0;
	if (!GetToken(&lex, minute, BASETOKEN_DIGIT))
		return 0;
	if (!ExpectToken(&lex, "-", BASETOKEN_OTHER))
		return 0;
	if (!GetToken(&lex, second, BASETOKEN_DIGIT))
		return 0;

	std::string timestring(year);
	timestring += month + day + hour + minute + second;
	lexer_free(&lex);
	return std::stoull(timestring);
}

static void DeleteOldestFile(const char* location, unsigned maxLogs)
{
	string            oldestLog;
	uint64_t          oldest_ts = (uint64_t)-1;
	struct os_dirent* entry;

	os_dir_t* dir = os_opendir(location);

	if (!dir) {
		std::cout << "Failed to open log directory." << std::endl;
	}

	unsigned count = 0;

	while ((entry = os_readdir(dir)) != NULL) {
		if (entry->directory || *entry->d_name == '.')
			continue;

		uint64_t ts = ConvertLogName(entry->d_name);

		if (ts) {
			if (ts < oldest_ts) {
				oldestLog = entry->d_name;
				oldest_ts = ts;
			}

			count++;
		}
	}

	os_closedir(dir);

	if (count > maxLogs) {
		string delPath;

		delPath = delPath + location + "/" + oldestLog;
		os_unlink(delPath.c_str());
	}
}

#include <chrono>
#include <cstdarg>
#include <varargs.h>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#else
#include <unistd.h>
#endif

inline std::string nodeobs_log_formatted_message(const char* format, va_list& args)
{
	size_t            length  = _vscprintf(format, args);
	std::vector<char> buf     = std::vector<char>(length + 1, '\0');
	size_t            written = vsprintf_s(buf.data(), buf.size(), format, args);
	return std::string(buf.begin(), buf.begin() + length);
}

std::chrono::high_resolution_clock             hrc;
std::chrono::high_resolution_clock::time_point tp = std::chrono::high_resolution_clock::now();
static void                                    node_obs_log(int log_level, const char* msg, va_list args, void* param)
{
	// Calculate log time.
	auto timeSinceStart = (std::chrono::high_resolution_clock::now() - tp);
	auto days           = std::chrono::duration_cast<std::chrono::duration<int, ratio<86400>>>(timeSinceStart);
	timeSinceStart -= days;
	auto hours = std::chrono::duration_cast<std::chrono::hours>(timeSinceStart);
	timeSinceStart -= hours;
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(timeSinceStart);
	timeSinceStart -= minutes;
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timeSinceStart);
	timeSinceStart -= seconds;
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceStart);
	timeSinceStart -= milliseconds;
	auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(timeSinceStart);
	timeSinceStart -= microseconds;
	auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(timeSinceStart);

	// Generate timestamp and log_level part.
	/// Convert level int to human readable name
	std::string levelname = "";
	switch (log_level) {
	case LOG_INFO:
		levelname = "Info";
		break;
	case LOG_WARNING:
		levelname = "Warning";
		break;
	case LOG_ERROR:
		levelname = "Error";
		break;
	case LOG_DEBUG:
		levelname = "Debug";
		break;
	default:
		if (log_level <= 50) {
			levelname = "Critical";
		} else if (log_level > 50 && log_level < LOG_ERROR) {
			levelname = "Error";
		} else if (log_level > LOG_ERROR && log_level < LOG_WARNING) {
			levelname = "Alert";
		} else if (log_level > LOG_WARNING && log_level < LOG_INFO) {
			levelname = "Hint";
		} else if (log_level > LOG_INFO) {
			levelname = "Notice";
		}
		break;
	}

	std::vector<char> timebuf(128, '\0');
	std::string       timeformat = "[%.3d:%.2d:%.2d:%.2d.%.3d.%.3d.%.3d][%*s]"; // "%*s";
	int               length     = sprintf_s(
        timebuf.data(),
        timebuf.size(),
        timeformat.c_str(),
        days.count(),
        hours.count(),
        minutes.count(),
        seconds.count(),
        milliseconds.count(),
        microseconds.count(),
        nanoseconds.count(),
        levelname.length(),
        levelname.c_str());
	std::string time_and_level = std::string(timebuf.data(), length);

	// Format incoming text
	std::string text = nodeobs_log_formatted_message(msg, args);

	std::fstream* logStream = reinterpret_cast<std::fstream*>(param);

	// Split by \n (new-line)
	size_t last_valid_idx = 0;
	for (size_t idx = 0; idx <= text.length(); idx++) {
		char& ch = text[idx];
		if ((ch == '\n') || (idx == text.length())) {
			std::string newmsg = time_and_level + " " + std::string(&text[last_valid_idx], idx - last_valid_idx) + '\n';
			last_valid_idx     = idx + 1;

			// File Log
			*logStream << newmsg << std::flush;

			// Std Out / Std Err
			/// Why fwrite and not std::cout and std::cerr?
			/// Well, it seems that std::cout and std::cerr break if you click in the console window and paste.
			/// Which is really bad, as nothing gets logged into the console anymore.
			if (log_level <= LOG_WARNING) {
				fwrite(newmsg.data(), sizeof(char), newmsg.length(), stderr);
			}
			fwrite(newmsg.data(), sizeof(char), newmsg.length(), stdout);

			// Debugger
#ifdef _WIN32
			if (IsDebuggerPresent()) {
				int wNum = MultiByteToWideChar(CP_UTF8, 0, newmsg.c_str(), -1, NULL, 0);
				if (wNum > 1) {
					std::wstring wide_buf;
					std::mutex   wide_mutex;

					lock_guard<mutex> lock(wide_mutex);
					wide_buf.reserve(wNum + 1);
					wide_buf.resize(wNum - 1);
					MultiByteToWideChar(CP_UTF8, 0, newmsg.c_str(), -1, &wide_buf[0], wNum);

					OutputDebugStringW(wide_buf.c_str());
				}
			}
#endif
		}
	}
	*logStream << std::flush;

#if defined(_WIN32) && defined(OBS_DEBUGBREAK_ON_ERROR)
	if (log_level <= LOG_ERROR && IsDebuggerPresent())
		__debugbreak();
#endif
}

uint32_t pid = GetCurrentProcessId();

std::vector<char> registerProcess(void)
{
	std::vector<char> buffer;
	buffer.resize(sizeof(uint8_t) + sizeof(bool) + sizeof(uint32_t));
	uint8_t action     = 0;
	bool    isCritical = true;

	uint32_t offset = 0;

	memcpy(buffer.data(), &action, sizeof(action));
	offset++;
	memcpy(buffer.data() + offset, &isCritical, sizeof(isCritical));
	offset++;
	memcpy(buffer.data() + offset, &pid, sizeof(pid));

	return buffer;
}

std::vector<char> unregisterProcess(void)
{
	std::vector<char> buffer;
	buffer.resize(sizeof(uint8_t) + sizeof(uint32_t));
	uint8_t action = 1;

	uint32_t offset = 0;

	memcpy(buffer.data(), &action, sizeof(action));
	offset++;
	memcpy(buffer.data() + offset, &pid, sizeof(pid));

	return buffer;
}

std::vector<char> terminateCrashHandler(void)
{
	std::vector<char> buffer;
	buffer.resize(sizeof(uint8_t) + sizeof(uint32_t));
	uint8_t action = 2;

	uint32_t offset = 0;

	memcpy(buffer.data(), &action, sizeof(action));
	offset++;
	memcpy(buffer.data() + offset, &pid, sizeof(pid));

	return buffer;
}

void writeCrashHandler(std::vector<char> buffer)
{
	HANDLE hPipe = CreateFile( TEXT("\\\\.\\pipe\\slobs-crash-handler"), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (hPipe == INVALID_HANDLE_VALUE)
		return;

	DWORD bytesWritten;

	WriteFile(hPipe, buffer.data(), buffer.size(), &bytesWritten, NULL);

	CloseHandle(hPipe);
}

void OBS_API::OBS_API_initAPI(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	writeCrashHandler(registerProcess());
	/* Map base DLLs as soon as possible into the current process space.
	* In particular, we need to load obs.dll into memory before we call
	* any functions from obs else if we delay-loaded the dll, it will
	* fail miserably. */

	/* FIXME These should be configurable */
	/* FIXME g_moduleDirectory really needs to be a wstring */
	std::string appdata = args[0].value_str;
	std::string locale  = args[1].value_str;

	/* libobs will use three methods of finding data files:
	* 1. ${CWD}/data/libobs <- This doesn't work for us
	* 2. ${OBS_DATA_PATH}/libobs <- This works but is inflexible
	* 3. getenv(OBS_DATA_PATH) + /libobs <- Can be set anywhere
	*    on the cli, in the frontend, or the backend. */
	obs_add_data_path((g_moduleDirectory + "/libobs/data/libobs/").c_str());
	slobs_plugin = appdata.substr(0, appdata.size() - strlen("/slobs-client"));
	slobs_plugin.append("/slobs-plugins");
	obs_add_data_path((slobs_plugin + "/data/").c_str());

	std::vector<char> userData = std::vector<char>(1024);
	os_get_config_path(userData.data(), userData.capacity() - 1, "slobs-client/plugin_config");
	obs_startup(locale.c_str(), userData.data(), NULL);

	/* Logging */
	string filename = GenerateTimeDateFilename("txt");
	string log_path = appdata;
	log_path.append("/node-obs/logs/");

	/* Make sure the path is created
	before attempting to make a file there. */
	if (os_mkdirs(log_path.c_str()) == MKDIR_ERROR) {
		cerr << "Failed to open log file" << endl;
	}

	DeleteOldestFile(log_path.c_str(), 3);
	log_path.append(filename);

#if defined(_WIN32) && defined(UNICODE)
	fstream* logfile = new fstream(converter.from_bytes(log_path.c_str()).c_str(), ios_base::out | ios_base::trunc);
#else
	fstream* logfile = new fstream(log_path, ios_base::out | ios_base::trunc);
#endif

	if (!logfile) {
		cerr << "Failed to open log file" << endl;
	}

	/* Delete oldest file in the folder to imitate rotating */
	base_set_log_handler(node_obs_log, logfile);

	/* INJECT osn::Source::Manager */
	// Alright, you're probably wondering: Why is osn code here?
	// Well, simply because the hooks need to run as soon as possible. We don't
	//  want to miss a single create or destroy signal OBS gives us for the
	//  osn::Source::Manager.
	osn::Source::initialize_global_signals();
	/* END INJECT osn::Source::Manager */

	cpuUsageInfo = os_cpu_usage_info_start();

	ConfigManager::getInstance().setAppdataPath(appdata);

	/* Set global private settings for whomever it concerns */
	obs_data_t* private_settings = obs_data_create();
	obs_data_set_bool(private_settings, "BrowserHWAccel", true);
	obs_apply_private_data(private_settings);
	obs_data_release(private_settings);

	int videoError;
	if (!openAllModules(videoError)) {
		rval.push_back(ipc::value((uint64_t)ErrorCode::Error));
		rval.push_back(ipc::value(videoError));
		AUTO_DEBUG;
		return;
	}

	OBS_service::createService();

	OBS_service::createStreamingOutput();
	OBS_service::createRecordingOutput();

	OBS_service::createVideoStreamingEncoder();
	OBS_service::createVideoRecordingEncoder();

	obs_encoder_t* audioStreamingEncoder = OBS_service::getAudioStreamingEncoder();
	obs_encoder_t* audioRecordingEncoder = OBS_service::getAudioRecordingEncoder();

	OBS_service::createAudioEncoder(&audioStreamingEncoder);
	OBS_service::createAudioEncoder(&audioRecordingEncoder);

	OBS_service::resetAudioContext();
	OBS_service::resetVideoContext();

	OBS_service::associateAudioAndVideoToTheCurrentStreamingContext();
	OBS_service::associateAudioAndVideoToTheCurrentRecordingContext();

	OBS_service::associateAudioAndVideoEncodersToTheCurrentStreamingOutput();
	OBS_service::associateAudioAndVideoEncodersToTheCurrentRecordingOutput(false);

	OBS_service::setServiceToTheStreamingOutput();

	setAudioDeviceMonitoring();

	// Enable the hotkey callback rerouting that will be used when manually handling hotkeys on the frontend
	obs_hotkey_enable_callback_rerouting(true);

	// We are returning a video result here because the frontend needs to know if we sucessfully
	// initialized the Dx11 API
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	rval.push_back(ipc::value(OBS_VIDEO_SUCCESS));

	AUTO_DEBUG;
}

void OBS_API::OBS_API_destroyOBS_API(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	/* INJECT osn::Source::Manager */
	// Alright, you're probably wondering: Why is osn code here?
	// Well, simply because the hooks need to run as soon as possible. We don't
	//  want to miss a single create or destroy signal OBS gives us for the
	//  osn::Source::Manager.
	osn::Source::finalize_global_signals();
	/* END INJECT osn::Source::Manager */
	destroyOBS_API();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_API::OBS_API_getPerformanceStatistics(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));

	rval.push_back(ipc::value(getCPU_Percentage()));
	rval.push_back(ipc::value(getNumberOfDroppedFrames()));
	rval.push_back(ipc::value(getDroppedFramesPercentage()));
	rval.push_back(ipc::value(getCurrentBandwidth()));
	rval.push_back(ipc::value(getCurrentFrameRate()));
	AUTO_DEBUG;
}

void OBS_API::QueryHotkeys(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	struct HotkeyInfo
	{
		std::string                objectName;
		obs_hotkey_registerer_type objectType;
		std::string                hotkeyName;
		std::string                hotkeyDesc;
		obs_hotkey_id              hotkeyId;
	};

	// For each registered hotkey
	std::vector<HotkeyInfo> hotkeyInfos;
	obs_enum_hotkeys(
	    [](void* data, obs_hotkey_id id, obs_hotkey_t* key) {
		    // Make sure every word has an initial capital letter
		    auto ToTitle = [](std::string s) {
			    bool last = true;
			    for (char& c : s) {
				    c    = last ? ::toupper(c) : ::tolower(c);
				    last = ::isspace(c);
			    }
			    return s;
		    };
		    std::vector<HotkeyInfo>& hotkeyInfos     = *static_cast<std::vector<HotkeyInfo>*>(data);
		    auto                     registerer_type = obs_hotkey_get_registerer_type(key);
		    void*                    registerer      = obs_hotkey_get_registerer(key);
		    HotkeyInfo               currentHotkeyInfo;

		    // Discover the type of object registered with this hotkey
		    switch (registerer_type) {
		    case OBS_HOTKEY_REGISTERER_FRONTEND: {
			    // Ignore any frontend hotkey
			    return true;
			    break;
		    }
		    case OBS_HOTKEY_REGISTERER_SOURCE: {
			    auto* weak_source            = static_cast<obs_weak_source_t*>(registerer);
			    auto  key_source             = OBSGetStrongRef(weak_source);
			    currentHotkeyInfo.objectName = obs_source_get_name(key_source);
			    currentHotkeyInfo.objectType = OBS_HOTKEY_REGISTERER_SOURCE;
			    break;
		    }
		    case OBS_HOTKEY_REGISTERER_OUTPUT: {
			    auto* weak_output            = static_cast<obs_weak_output_t*>(registerer);
			    auto  key_output             = OBSGetStrongRef(weak_output);
			    currentHotkeyInfo.objectName = obs_output_get_name(key_output);
			    currentHotkeyInfo.objectType = OBS_HOTKEY_REGISTERER_OUTPUT;
			    break;
		    }
		    case OBS_HOTKEY_REGISTERER_ENCODER: {
			    auto* weak_encoder           = static_cast<obs_weak_encoder_t*>(registerer);
			    auto  key_encoder            = OBSGetStrongRef(weak_encoder);
			    currentHotkeyInfo.objectName = obs_encoder_get_name(key_encoder);
			    currentHotkeyInfo.objectType = OBS_HOTKEY_REGISTERER_ENCODER;
			    break;
		    }
		    case OBS_HOTKEY_REGISTERER_SERVICE: {
			    auto* weak_service           = static_cast<obs_weak_service_t*>(registerer);
			    auto  key_service            = OBSGetStrongRef(weak_service);
			    currentHotkeyInfo.objectName = obs_service_get_name(key_service);
			    currentHotkeyInfo.objectType = OBS_HOTKEY_REGISTERER_SERVICE;
			    break;
		    }
		    }

		    // Key defs
		    auto       key_name = std::string(obs_hotkey_get_name(key));
		    auto       desc     = std::string(obs_hotkey_get_description(key));
		    const auto hotkeyId = obs_hotkey_get_id(key);

		    // Parse the key name and the description
		    key_name = key_name.substr(key_name.find_first_of(".") + 1);
		    std::replace(key_name.begin(), key_name.end(), '-', '_');
		    std::transform(key_name.begin(), key_name.end(), key_name.begin(), ::toupper);
		    std::replace(desc.begin(), desc.end(), '-', ' ');
		    desc = ToTitle(desc);

		    currentHotkeyInfo.hotkeyName = key_name;
		    currentHotkeyInfo.hotkeyDesc = desc;
		    currentHotkeyInfo.hotkeyId   = hotkeyId;
		    hotkeyInfos.push_back(currentHotkeyInfo);

		    return true;
	    },
	    &hotkeyInfos);

	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));

	// For each hotkey that we've found
	for (auto& hotkeyInfo : hotkeyInfos) {
		rval.push_back(ipc::value(hotkeyInfo.objectName));
		rval.push_back(ipc::value(uint32_t(hotkeyInfo.objectType)));
		rval.push_back(ipc::value(hotkeyInfo.hotkeyName));
		rval.push_back(ipc::value(hotkeyInfo.hotkeyDesc));
		rval.push_back(ipc::value(uint64_t(hotkeyInfo.hotkeyId)));
	}

	AUTO_DEBUG;
}

void OBS_API::ProcessHotkeyStatus(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	obs_hotkey_id hotkeyId = args[0].value_union.ui64;
	uint64_t      press    = args[1].value_union.i32;

	// TODO: Check if the hotkey ID is valid
	obs_hotkey_trigger_routed_callback(hotkeyId, (bool)press);

	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));

	AUTO_DEBUG;
}

void OBS_API::SetProcessPriority(const char* priority)
{
	if (!priority)
		return;

	if (strcmp(priority, "High") == 0)
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	else if (strcmp(priority, "AboveNormal") == 0)
		SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
	else if (strcmp(priority, "Normal") == 0)
		SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
	else if (strcmp(priority, "BelowNormal") == 0)
		SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
	else if (strcmp(priority, "Idle") == 0)
		SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
}

void OBS_API::UpdateProcessPriority()
{
	const char* priority = config_get_string(ConfigManager::getInstance().getGlobal(), "General", "ProcessPriority");
	if (priority && strcmp(priority, "Normal") != 0)
		SetProcessPriority(priority);
}

bool DisableAudioDucking(bool disable)
{
	ComPtr<IMMDeviceEnumerator>   devEmum;
	ComPtr<IMMDevice>             device;
	ComPtr<IAudioSessionManager2> sessionManager2;
	ComPtr<IAudioSessionControl>  sessionControl;
	ComPtr<IAudioSessionControl2> sessionControl2;

	HRESULT result = CoCreateInstance(
	    __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (void**)&devEmum);
	if (FAILED(result))
		return false;

	result = devEmum->GetDefaultAudioEndpoint(eRender, eConsole, &device);
	if (FAILED(result))
		return false;

	result = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_INPROC_SERVER, nullptr, (void**)&sessionManager2);
	if (FAILED(result))
		return false;

	result = sessionManager2->GetAudioSessionControl(nullptr, 0, &sessionControl);
	if (FAILED(result))
		return false;

	result = sessionControl->QueryInterface(&sessionControl2);
	if (FAILED(result))
		return false;

	result = sessionControl2->SetDuckingPreference(disable);
	return SUCCEEDED(result);
}

void OBS_API::setAudioDeviceMonitoring(void)
{
/* load audio monitoring */
#if defined(_WIN32) || defined(__APPLE__)
	const char* device_name =
	    config_get_string(ConfigManager::getInstance().getBasic(), "Audio", "MonitoringDeviceName");
	const char* device_id = config_get_string(ConfigManager::getInstance().getBasic(), "Audio", "MonitoringDeviceId");

	obs_set_audio_monitoring_device(device_name, device_id);

	blog(LOG_INFO, "Audio monitoring device:\n\tname: %s\n\tid: %s", device_name, device_id);

	bool disableAudioDucking = config_get_bool(ConfigManager::getInstance().getBasic(), "Audio", "DisableAudioDucking");
	if (disableAudioDucking)
		DisableAudioDucking(true);
#endif
}

typedef struct
{
	OVERLAPPED        oOverlap;
	HANDLE            hPipeInst;
	std::vector<char> chRequest;
	DWORD             cbRead;
	TCHAR             chReply[BUFFSIZE];
	DWORD             cbToWrite;
	DWORD             dwState;
	BOOL              fPendingIO;
} PIPEINST, *LPPIPEINST;

PIPEINST Pipe;
HANDLE   hEvents;

BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
	BOOL fConnected, fPendingIO = FALSE;
	fConnected = ConnectNamedPipe(hPipe, lpo);

	if (fConnected) {
		return 0;
	}

	switch (GetLastError()) {
	case ERROR_IO_PENDING:
		fPendingIO = TRUE;
		break;
	case ERROR_PIPE_CONNECTED:
		if (SetEvent(lpo->hEvent))
			break;
	default: {
		return 0;
	}
	}

	return fPendingIO;
}

VOID DisconnectAndReconnect(void)
{
	if (!DisconnectNamedPipe(Pipe.hPipeInst)) {
		return;
	}

	Pipe.fPendingIO = ConnectToNewClient(Pipe.hPipeInst, &(Pipe.oOverlap));

	Pipe.dwState = Pipe.fPendingIO ? CONNECTING_STATE : READING_STATE;
}

void acknowledgeTerminate(void)
{
	BOOL   fSuccess;
	LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\exit-slobs-crash-handler");

	hEvents = CreateEvent(NULL, TRUE, TRUE, NULL);

	if (hEvents == NULL) {
		return;
	}

	Pipe.oOverlap.hEvent = hEvents;

	Pipe.hPipeInst = CreateNamedPipe(
	    lpszPipename,
	    PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
	    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
	    1,
	    BUFFSIZE * sizeof(TCHAR),
	    BUFFSIZE * sizeof(TCHAR),
	    5000,
	    NULL);

	if (Pipe.hPipeInst == INVALID_HANDLE_VALUE) {
		return;
	}

	Pipe.fPendingIO = ConnectToNewClient(Pipe.hPipeInst, &(Pipe.oOverlap));

	Pipe.dwState = Pipe.fPendingIO ? CONNECTING_STATE : READING_STATE;

	bool exit = false;
	auto timeNow = std::chrono::high_resolution_clock::now();
	DWORD i, dwWait, cbRet, dwErr;

	while (!exit) {
		auto tp    = std::chrono::high_resolution_clock::now();
		auto delta = tp - timeNow;

		if (std::chrono::duration_cast<std::chrono::milliseconds>(delta).count() > 5000) {
			// We timeout, crash handler failed to send the shutdown acknowledge,
			// we move forward with the shutdown procedure
			exit = true;
			CloseHandle(Pipe.hPipeInst);
			break;
		}

		dwWait = WaitForSingleObject(hEvents, 500);

		if (dwWait == WAIT_OBJECT_0) {
			if (Pipe.fPendingIO) {
				fSuccess = GetOverlappedResult(Pipe.hPipeInst, &(Pipe.oOverlap), &cbRet, FALSE);

				switch (Pipe.dwState) {
				case CONNECTING_STATE: {
					if (!fSuccess) {
						break;
					}
					Pipe.dwState = READING_STATE;
					break;
				}
				case READING_STATE: {
					if (!fSuccess || cbRet == 0) {
						exit = true;
						DisconnectAndReconnect();
						break;
					}
					Pipe.cbRead  = cbRet;
					Pipe.dwState = READING_STATE;
					break;
				}
				default: {
					break;
				}
				}
			}

			switch (Pipe.dwState) {
			case READING_STATE: {
				Pipe.chRequest.resize(BUFFSIZE);
				fSuccess = ReadFile(
				    Pipe.hPipeInst,
				    Pipe.chRequest.data(),
				    BUFFSIZE * sizeof(TCHAR),
				    &Pipe.cbRead,
				    &Pipe.oOverlap);

				GetOverlappedResult(Pipe.hPipeInst, &Pipe.oOverlap, &Pipe.cbRead, true);

				// The read operation completed successfully.
				if (Pipe.cbRead > 0) {
					Pipe.fPendingIO = FALSE;
				}
				dwErr = GetLastError();
				if (!fSuccess && (dwErr == ERROR_IO_PENDING)) {
					Pipe.fPendingIO = TRUE;
					break;
				}
				exit = true;
				DisconnectAndReconnect();
				break;
			}
			}
		}
	}
}

void OBS_API::StopCrashHandler(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	std::thread worker(acknowledgeTerminate);
	writeCrashHandler(unregisterProcess());

	if (worker.joinable())
		worker.join();

	writeCrashHandler(terminateCrashHandler());

	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_API::destroyOBS_API(void)
{
	os_cpu_usage_info_destroy(cpuUsageInfo);

#ifdef _WIN32
	bool disableAudioDucking = config_get_bool(ConfigManager::getInstance().getBasic(), "Audio", "DisableAudioDucking");
	if (disableAudioDucking)
		DisableAudioDucking(false);
#endif

	obs_encoder_t* streamingEncoder = OBS_service::getStreamingEncoder();
	if (streamingEncoder != NULL)
		obs_encoder_release(streamingEncoder);

	obs_encoder_t* recordingEncoder = OBS_service::getRecordingEncoder();
	if (recordingEncoder != NULL)
		obs_encoder_release(recordingEncoder);

	obs_encoder_t* audioStreamingEncoder = OBS_service::getAudioStreamingEncoder();
	if (audioStreamingEncoder != NULL)
		obs_encoder_release(audioStreamingEncoder);

	obs_encoder_t* audioRecordingEncoder = OBS_service::getAudioRecordingEncoder();
	if (audioRecordingEncoder != NULL)
		obs_encoder_release(audioRecordingEncoder);

	obs_output_t* streamingOutput = OBS_service::getStreamingOutput();
	if (streamingOutput != NULL)
		obs_output_release(streamingOutput);

	obs_output_t* recordingOutput = OBS_service::getRecordingOutput();
	if (recordingOutput != NULL)
		obs_output_release(recordingOutput);

	obs_service_t* service = OBS_service::getService();
	if (service != NULL)
		obs_service_release(service);

	obs_shutdown();

	// Release each obs module (dlls for windows)
	// TODO: We should release these modules (dlls) manually and not let the garbage
	// collector do this for us on shutdown
	for (auto& moduleInfo : obsModules) {
	}

	// The goal is to reduce this number to zero and add a throw here, so if in the future
	// a leak is detected, any developer will know for sure what is causing it
	int totalLeaks = bnum_allocs();
	std::cout << "Total leaks: " << totalLeaks << std::endl;
	if (totalLeaks) {
		// throw "OBS has memory leaks";
	}
}

struct ci_char_traits : public char_traits<char>
{
	static bool eq(char c1, char c2)
	{
		return toupper(c1) == toupper(c2);
	}
	static bool ne(char c1, char c2)
	{
		return toupper(c1) != toupper(c2);
	}
	static bool lt(char c1, char c2)
	{
		return toupper(c1) < toupper(c2);
	}
	static int compare(const char* s1, const char* s2, size_t n)
	{
		while (n-- != 0) {
			if (toupper(*s1) < toupper(*s2))
				return -1;
			if (toupper(*s1) > toupper(*s2))
				return 1;
			++s1;
			++s2;
		}
		return 0;
	}
	static const char* find(const char* s, int n, char a)
	{
		while (n-- > 0 && toupper(*s) != toupper(a)) {
			++s;
		}
		return s;
	}
};

typedef std::basic_string<char, ci_char_traits> istring;

/* This should be reusable outside of node-obs, especially
* if we go a server/client route. */
bool OBS_API::openAllModules(int& video_err)
{
	video_err = OBS_service::resetVideoContext();
	if (video_err != OBS_VIDEO_SUCCESS) {
		return false;
	}

	std::string plugins_paths[] = {g_moduleDirectory + "/obs-plugins/64bit",
	                               g_moduleDirectory + "/obs-plugins",
	                               slobs_plugin + "/obs-plugins/64bit"};

	std::string plugins_data_paths[] = {
	    g_moduleDirectory + "/data/obs-plugins", plugins_data_paths[0], slobs_plugin + "/data/obs-plugins"};

	size_t num_paths = sizeof(plugins_paths) / sizeof(plugins_paths[0]);

	for (int i = 0; i < num_paths; ++i) {
		std::string& plugins_path      = plugins_paths[i];
		std::string& plugins_data_path = plugins_data_paths[i];

		/* FIXME Plugins could be in individual folders, maybe
		* with some metainfo so we don't attempt just any
		* shared library. */
		if (!os_file_exists(plugins_path.c_str())) {
			std::cerr << "Plugin Path provided is invalid: " << plugins_path << std::endl;
			return false;
		}

		os_dir_t* plugin_dir = os_opendir(plugins_path.c_str());
		if (!plugin_dir) {
			std::cerr << "Failed to open plugin diretory: " << plugins_path << std::endl;
			return false;
		}

		for (os_dirent* ent = os_readdir(plugin_dir); ent != nullptr; ent = os_readdir(plugin_dir)) {
			std::string fullname = ent->d_name;
			std::string basename = fullname.substr(0, fullname.find_last_of('.'));

			std::string plugin_path      = plugins_path + "/" + fullname;
			std::string plugin_data_path = plugins_data_path + "/" + basename;
			if (ent->directory) {
				continue;
			}

#ifdef _WIN32
			if (fullname.substr(fullname.find_last_of(".") + 1) != "dll") {
				continue;
			}
#endif

			obs_module_t* module;
			int           result = obs_open_module(&module, plugin_path.c_str(), plugin_data_path.c_str());

			switch (result) {
			case MODULE_SUCCESS:
				obsModules.push_back(std::make_pair(fullname, module));
				break;
			case MODULE_FILE_NOT_FOUND:
				std::cerr << "Unable to load '" << plugin_path << "', could not find file." << std::endl;
				continue;
			case MODULE_MISSING_EXPORTS:
				std::cerr << "Unable to load '" << plugin_path << "', missing exports." << std::endl;
				continue;
			case MODULE_INCOMPATIBLE_VER:
				std::cerr << "Unable to load '" << plugin_path << "', incompatible version." << std::endl;
				continue;
			case MODULE_ERROR:
				std::cerr << "Unable to load '" << plugin_path << "', generic error." << std::endl;
				continue;
			default:
				continue;
			}

			bool success = obs_init_module(module);

			if (!success) {
				std::cerr << "Failed to initialize module " << plugin_path << std::endl;
				/* Just continue to next one */
			}
		}

		os_closedir(plugin_dir);
	}

	return true;
}

double OBS_API::getCPU_Percentage(void)
{
	double cpuPercentage = os_cpu_usage_info_query(cpuUsageInfo);

	cpuPercentage *= 10;
	cpuPercentage = trunc(cpuPercentage);
	cpuPercentage /= 10;

	return cpuPercentage;
}

int OBS_API::getNumberOfDroppedFrames(void)
{
	obs_output_t* streamOutput = OBS_service::getStreamingOutput();

	int totalDropped = 0;

	if (obs_output_active(streamOutput)) {
		totalDropped = obs_output_get_frames_dropped(streamOutput);
	}

	return totalDropped;
}

double OBS_API::getDroppedFramesPercentage(void)
{
	obs_output_t* streamOutput = OBS_service::getStreamingOutput();

	double percent = 0;

	if (obs_output_active(streamOutput)) {
		int totalDropped = obs_output_get_frames_dropped(streamOutput);
		int totalFrames  = obs_output_get_total_frames(streamOutput);
		if (totalFrames == 0) {
			percent = 0.0;
		} else {
			percent = (double)totalDropped / (double)totalFrames * 100.0;
		}
	}

	return percent;
}

double OBS_API::getCurrentBandwidth(void)
{
	obs_output_t* streamOutput = OBS_service::getStreamingOutput();

	double kbitsPerSec = 0;

	if (obs_output_active(streamOutput)) {
		uint64_t bytesSent     = obs_output_get_total_bytes(streamOutput);
		uint64_t bytesSentTime = os_gettime_ns();

		if (bytesSent < lastBytesSent)
			bytesSent = 0;
		if (bytesSent == 0)
			lastBytesSent = 0;

		uint64_t bitsBetween = (bytesSent - lastBytesSent) * 8;

		double timePassed = double(bytesSentTime - lastBytesSentTime) / 1000000000.0;
		if (timePassed < std::numeric_limits<double>::epsilon()
		    && timePassed > -std::numeric_limits<double>::epsilon()) {
			kbitsPerSec = 0.0;
		} else {
			kbitsPerSec = double(bitsBetween) / timePassed / 1000.0;
		}

		lastBytesSent     = bytesSent;
		lastBytesSentTime = bytesSentTime;
	}

	return kbitsPerSec;
}

double OBS_API::getCurrentFrameRate(void)
{
	return obs_get_active_fps();
}

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	MONITORINFO info;
	info.cbSize = sizeof(info);
	if (GetMonitorInfo(hMonitor, &info)) {
		std::vector<Screen>* resolutions = reinterpret_cast<std::vector<Screen>*>(dwData);

		Screen screen;

		screen.width  = std::abs(info.rcMonitor.left - info.rcMonitor.right);
		screen.height = std::abs(info.rcMonitor.top - info.rcMonitor.bottom);

		resolutions->push_back(screen);
	}
	return true;
}

std::vector<Screen> OBS_API::availableResolutions(void)
{
	std::vector<Screen> resolutions;
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&resolutions));

	return resolutions;
}
