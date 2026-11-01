#pragma once
#include "EditorObject.h"
#include "SpdLogger.h"
#include <deque>
#include <mutex>


NS_BEGIN(Editor)

class LogConsole final : public EditorObject {
  NO_COPY(LogConsole)
public:
  struct LogEntry {
    Engine::LogLevel level;
    string message;
    string timestamp;
  };

public:
  LogConsole();
  ~LogConsole() override;

public:
  HRESULT Initialize() override;
  void Update(Bool isResize) override;
  void Render(Bool isResize) override;

public:
  void AddLog(Engine::LogLevel level, const string &message);
  void Clear();

public:
  static Shared<LogConsole> Create();

private:
  static ImVec4 GetLevelColor(Engine::LogLevel level);
  static const char *GetLevelLabel(Engine::LogLevel level);

private:
    std::deque<LogEntry> m_Entries;
    std::mutex m_Mutex;
    bool m_AutoScroll = true;
    bool m_ShowTrace = true;
    bool m_ShowDebug = true;
    bool m_ShowInfo = true;
    bool m_ShowWarn = true;
    bool m_ShowError = true;
    bool m_ShowCritical = true;
    static const size_t MAX_ENTRIES = 2048;
    char m_FilterBuf[128] = {};
};

NS_END