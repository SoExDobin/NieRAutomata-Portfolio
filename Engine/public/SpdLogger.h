#pragma once
#include "Engine_Define.h"
#include <functional>
#pragma warning(push)
#pragma warning(disable: 26498)
#include <spdlog/common.h> // source_loc를 위해 필요
#pragma warning(pop)

NS_BEGIN(Engine)

enum class LogLevel : int { Trace = 0, Debug, Info, Warn, Error, Critical };
using LogCallback = std::function<void(LogLevel, const std::string&)>;

class ENGINE_DLL SpdLogger final {
    NO_COPY(SpdLogger)
public:
    static void Initialize();
    static void Shutdown();

    // spdlog::source_loc를 인자로 받는 형태로 변경
    static void Trace(spdlog::source_loc loc, const std::string& message);
    static void Debug(spdlog::source_loc loc, const std::string& message);
    static void Info(spdlog::source_loc loc, const std::string& message);
    static void Warn(spdlog::source_loc loc, const std::string& message);
    static void Error(spdlog::source_loc loc, const std::string& message);
    static void Critical(spdlog::source_loc loc, const std::string& message);

    static void Trace(spdlog::source_loc loc, const std::wstring& message);
    static void Debug(spdlog::source_loc loc, const std::wstring& message);
    static void Info(spdlog::source_loc loc, const std::wstring& message);
    static void Warn(spdlog::source_loc loc, const std::wstring& message);
    static void Error(spdlog::source_loc loc, const std::wstring& message);
    static void Critical(spdlog::source_loc loc, const std::wstring& message);

    static void SetLogCallback(LogCallback callback);
    /* Release Launcher never initializes the logger, so LOG_* stops here before formatting. */
    static bool Is_Enabled();
};
NS_END

#define LOG_INIT() Engine::SpdLogger::Initialize()
#define LOG_SHUTDOWN() Engine::SpdLogger::Shutdown()

// 호출 시점의 파일, 라인, 함수명을 source_loc로 묶어서 전달합니다. 로거가 꺼져 있으면 포맷하지 않습니다.
#define LOG_TRACE(fmt, ...)    do { if (Engine::SpdLogger::Is_Enabled()) Engine::SpdLogger::Trace({__FILE__, __LINE__, __FUNCTION__}, std::format(fmt, __VA_ARGS__)); } while (0)
#define LOG_DEBUG(fmt, ...)    do { if (Engine::SpdLogger::Is_Enabled()) Engine::SpdLogger::Debug({__FILE__, __LINE__, __FUNCTION__}, std::format(fmt, __VA_ARGS__)); } while (0)
#define LOG_INFO(fmt, ...)     do { if (Engine::SpdLogger::Is_Enabled()) Engine::SpdLogger::Info({__FILE__, __LINE__, __FUNCTION__}, std::format(fmt, __VA_ARGS__)); } while (0)
#define LOG_WARN(fmt, ...)     do { if (Engine::SpdLogger::Is_Enabled()) Engine::SpdLogger::Warn({__FILE__, __LINE__, __FUNCTION__}, std::format(fmt, __VA_ARGS__)); } while (0)
#define LOG_ERROR(fmt, ...)    do { if (Engine::SpdLogger::Is_Enabled()) Engine::SpdLogger::Error({__FILE__, __LINE__, __FUNCTION__}, std::format(fmt, __VA_ARGS__)); } while (0)
#define LOG_CRITICAL(fmt, ...) do { if (Engine::SpdLogger::Is_Enabled()) Engine::SpdLogger::Critical({__FILE__, __LINE__, __FUNCTION__}, std::format(fmt, __VA_ARGS__)); } while (0)