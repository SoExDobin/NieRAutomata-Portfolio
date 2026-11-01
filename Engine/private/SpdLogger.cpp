#include "SpdLogger.h"
#include "String_Helper.h"
#pragma warning(push)
#pragma warning(disable: 26498)
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#pragma warning(pop)
#include <Windows.h>
#include <format>

NS_BEGIN(Engine)

static Shared<spdlog::logger> g_Logger = { nullptr };
static LogCallback g_LogCallback = nullptr;

void SpdLogger::Initialize() {
    try {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        // 요청하신 패턴 적용
        console_sink->set_pattern("[%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%!:%#] %v");

        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/engine.log", 1024 * 1024 * 10, 3);
        file_sink->set_level(spdlog::level::trace);
        file_sink->set_pattern("[%m-%d %H:%M:%S.%e] [%l] [%t] [%!:%#] %v");

        spdlog::sinks_init_list sink_list = { console_sink, file_sink };
        g_Logger = std::make_shared<spdlog::logger>("EngineLogger", sink_list.begin(), sink_list.end());
        g_Logger->set_level(spdlog::level::trace);
        g_Logger->flush_on(spdlog::level::warn);

        spdlog::set_default_logger(g_Logger);
    }
    catch (const spdlog::spdlog_ex& ex) {
        std::string errorMsg = "SpdLogger Init Failed: " + std::string(ex.what());
        MessageBoxA(nullptr, errorMsg.c_str(), "Error", MB_OK | MB_ICONERROR);
    }
}

void SpdLogger::Shutdown() {
    if (g_Logger) {
        spdlog::drop_all();
        spdlog::shutdown();
    }
    g_LogCallback = nullptr;
}

void SpdLogger::SetLogCallback(LogCallback callback) { g_LogCallback = callback; }

bool SpdLogger::Is_Enabled() { return g_Logger || g_LogCallback; }

/* ===== 구현부: 핵심은 g_Logger->log(loc, level, message) 호출 ===== */

void SpdLogger::Trace(spdlog::source_loc loc, const std::string& message) {
    if (g_Logger) g_Logger->log(loc, spdlog::level::trace, message);
    if (g_LogCallback) {
        // UI 콜백용 메시지에 위치 정보([함수:라인])를 수동으로 합쳐줍니다.
        std::string uiMsg = std::format("[{}:{}]\n{}", loc.funcname, loc.line, message);
        g_LogCallback(LogLevel::Trace, uiMsg);
    }
}

void SpdLogger::Debug(spdlog::source_loc loc, const std::string& message) {
    if (g_Logger) g_Logger->log(loc, spdlog::level::debug, message);
    if (g_LogCallback) {
        std::string uiMsg = std::format("[{}:{}]\n{}", loc.funcname, loc.line, message);
        g_LogCallback(LogLevel::Debug, uiMsg);
    }
}

void SpdLogger::Info(spdlog::source_loc loc, const std::string& message) {
    if (g_Logger) g_Logger->log(loc, spdlog::level::info, message);
    if (g_LogCallback) {
        std::string uiMsg = std::format("[{}:{}]\n{}", loc.funcname, loc.line, message);
        g_LogCallback(LogLevel::Info, uiMsg);
    }
}

void SpdLogger::Warn(spdlog::source_loc loc, const std::string& message) {
    if (g_Logger) g_Logger->log(loc, spdlog::level::warn, message);
    if (g_LogCallback) {
        std::string uiMsg = std::format("[{}:{}]\n{}", loc.funcname, loc.line, message);
        g_LogCallback(LogLevel::Warn, uiMsg);
    }
}

void SpdLogger::Error(spdlog::source_loc loc, const std::string& message) {
    if (g_Logger) g_Logger->log(loc, spdlog::level::err, message);
    if (g_LogCallback) {
        std::string uiMsg = std::format("[{}:{}]\n{}", loc.funcname, loc.line, message);
        g_LogCallback(LogLevel::Error, uiMsg);
    }
}

void SpdLogger::Critical(spdlog::source_loc loc, const std::string& message) {
    if (g_Logger) g_Logger->log(loc, spdlog::level::critical, message);
    if (g_LogCallback) {
        std::string uiMsg = std::format("[{}:{}]\n{}", loc.funcname, loc.line, message);
        g_LogCallback(LogLevel::Critical, uiMsg);
    }
}

/* ===== wstring 버전 (loc를 그대로 전달) ===== */
void SpdLogger::Trace(spdlog::source_loc loc, const std::wstring& message) { Trace(loc, Helper::To_String(message)); }
void SpdLogger::Debug(spdlog::source_loc loc, const std::wstring& message) { Debug(loc, Helper::To_String(message)); }
void SpdLogger::Info(spdlog::source_loc loc, const std::wstring& message) { Info(loc, Helper::To_String(message)); }
void SpdLogger::Warn(spdlog::source_loc loc, const std::wstring& message) { Warn(loc, Helper::To_String(message)); }
void SpdLogger::Error(spdlog::source_loc loc, const std::wstring& message) { Error(loc, Helper::To_String(message)); }
void SpdLogger::Critical(spdlog::source_loc loc, const std::wstring& message) { Critical(loc, Helper::To_String(message)); }

NS_END