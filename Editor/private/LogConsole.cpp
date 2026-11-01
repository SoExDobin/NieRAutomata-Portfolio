#include "pch.h"
#include "LogConsole.h"
#include <chrono>
#include <imgui.h>


LogConsole::LogConsole() {}
LogConsole::~LogConsole() {
  Engine::SpdLogger::SetLogCallback(nullptr);
}

HRESULT LogConsole::Initialize() { return EditorObject::Initialize(); }

void LogConsole::Update(Bool isResize)
{
}

void LogConsole::AddLog(Engine::LogLevel level, const string &message) 
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	struct tm tm_buf;
	localtime_s(&tm_buf, &time_t);

    Char timeBuf[32];
    snprintf(
        timeBuf, 
        sizeof(timeBuf), 
        "%02d:%02d:%02d.%03d", 
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, 
        static_cast<int32>(ms.count())
    );

    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.timestamp = timeBuf;

    std::scoped_lock lock(m_Mutex);
    m_Entries.push_back(std::move(entry));
    while (m_Entries.size() > MAX_ENTRIES)
		m_Entries.pop_front();
}

void LogConsole::Clear() {
	std::scoped_lock lock(m_Mutex);
	m_Entries.clear();
}

void LogConsole::Render(Bool isResize) 
{
    ImGui::Begin("Log Console");

    if (ImGui::Button("Clear"))
      Clear();

    ImGui::SameLine();
    ImGui::Checkbox("Auto-Scroll", &m_AutoScroll);

    ImGui::SameLine();
    ImGui::Separator();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(Engine::LogLevel::Trace));
    ImGui::Checkbox("TRC", &m_ShowTrace);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(Engine::LogLevel::Debug));
    ImGui::Checkbox("DBG", &m_ShowDebug);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(Engine::LogLevel::Info));
    ImGui::Checkbox("INF", &m_ShowInfo);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(Engine::LogLevel::Warn));
    ImGui::Checkbox("WRN", &m_ShowWarn);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(Engine::LogLevel::Error));
    ImGui::Checkbox("ERR", &m_ShowError);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(Engine::LogLevel::Critical));
    ImGui::Checkbox("CRT", &m_ShowCritical);
    ImGui::PopStyleColor();

    /* 텍스트 필터 */
    ImGui::SameLine();
    ImGui::PushItemWidth(200.f);
    ImGui::InputText("##Filter", m_FilterBuf, sizeof(m_FilterBuf));
    ImGui::PopItemWidth();

    ImGui::Separator();

    /* ===== 로그 출력 영역 ===== */
    ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false,
                    ImGuiWindowFlags_HorizontalScrollbar);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);

    for (const auto &entry : m_Entries) {
      /* 레벨 필터 */
      switch (entry.level) {
      case Engine::LogLevel::Trace:
        if (!m_ShowTrace)
          continue;
        break;
      case Engine::LogLevel::Debug:
        if (!m_ShowDebug)
          continue;
        break;
      case Engine::LogLevel::Info:
        if (!m_ShowInfo)
          continue;
        break;
      case Engine::LogLevel::Warn:
        if (!m_ShowWarn)
          continue;
        break;
      case Engine::LogLevel::Error:
        if (!m_ShowError)
          continue;
        break;
      case Engine::LogLevel::Critical:
        if (!m_ShowCritical)
          continue;
        break;
      }

      /* 텍스트 필터 */
      if (m_FilterBuf[0] != '\0') {
        if (entry.message.find(m_FilterBuf) == string::npos)
          continue;
      }

      /* 색상 적용된 출력 */
      ImVec4 color = GetLevelColor(entry.level);
      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::TextUnformatted(("[" + entry.timestamp + "] [" +
                              GetLevelLabel(entry.level) + "] " + entry.message)
                                 .c_str());
      ImGui::PopStyleColor();
    }
  }

  if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    ImGui::SetScrollHereY(1.0f);

  ImGui::EndChild();
  ImGui::End();
}

ImVec4 LogConsole::GetLevelColor(Engine::LogLevel level) 
{
	switch (level) 
	{
	case Engine::LogLevel::Trace:
		return {0.6f, 0.6f, 0.6f, 1.0f}; /* 회색 */
	case Engine::LogLevel::Debug:
		return {0.4f, 0.8f, 1.0f, 1.0f}; /* 하늘색 */
	case Engine::LogLevel::Info:
		return {0.2f, 1.0f, 0.2f, 1.0f}; /* 녹색 */
	case Engine::LogLevel::Warn:
		return {1.0f, 0.9f, 0.2f, 1.0f}; /* 노란색 */
	case Engine::LogLevel::Error:
		return {1.0f, 0.3f, 0.3f, 1.0f}; /* 빨간색 */
	case Engine::LogLevel::Critical:
		return {1.0f, 0.0f, 0.5f, 1.0f}; /* 핫핑크 */
	default:
		return {1.0f, 1.0f, 1.0f, 1.0f};
	}
}

const char *LogConsole::GetLevelLabel(Engine::LogLevel level) {
	switch (level) 
	{
	case Engine::LogLevel::Trace: return "TRACE";
	case Engine::LogLevel::Debug: return "DEBUG";
	case Engine::LogLevel::Info: return "INFO ";
	case Engine::LogLevel::Warn: return "WARN ";
	case Engine::LogLevel::Error: return "ERROR";
	case Engine::LogLevel::Critical: return "CRIT ";
	default: return "?????";
	}
}

Shared<LogConsole> LogConsole::Create() {
	auto instance = make_shared<LogConsole>();

	if (FAILED(instance->Initialize())) 
    {
		MSG_BOX("Failed To Create LogConsole");
		return nullptr;
	}

  /* SpdLogger 콜백 연결. Editor는 Release에서도 로그를 켠다(ENGINE_DESC::useLog). */
    auto *rawPtr = instance.get();
    Engine::SpdLogger::SetLogCallback(
      [rawPtr](Engine::LogLevel level, const std::string &msg) {
        rawPtr->AddLog(level, msg);
      });

	return instance;
}
