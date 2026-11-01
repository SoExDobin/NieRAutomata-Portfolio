#include "Timer.h"

HRESULT Timer::Initialize() {
  m_TimeScale = 1.0f;

  QueryPerformanceFrequency(&m_Frequency);
  QueryPerformanceCounter(&m_PrevTime);
  m_CurrentTime = m_PrevTime;

  m_DeltaTime = 0.0f;
  m_UnscaledDeltaTime = 0.0f;
  m_DeltaAcc = 0.0f;

  m_FixedDeltaTime = 0.016667f; // 60Hz
  m_FixedAcc = 0.0f;

  m_FPS = 0;
  m_FrameAcc = 0;
  m_TempFPS = 0;
  m_FPSTimeElapsed = 0.0f;

  return S_OK;
}

Float Timer::Update_Timer(Bool accumulateFixedTime) {
    QueryPerformanceCounter(&m_CurrentTime);
    m_UnscaledDeltaTime =
        static_cast<Float>(m_CurrentTime.QuadPart - m_PrevTime.QuadPart) /
        static_cast<Float>(m_Frequency.QuadPart);

    m_UnscaledDeltaTime = min(m_UnscaledDeltaTime, 0.1f);

    m_DeltaTime = m_UnscaledDeltaTime * m_TimeScale;
    m_DeltaAcc += m_DeltaTime;

    if (accumulateFixedTime)
      m_FixedAcc += m_DeltaTime;
    else
      m_FixedAcc = 0.f;

    ++m_FrameAcc;
    ++m_TempFPS;

    m_FPSTimeElapsed += m_DeltaTime;
    if (m_FPSTimeElapsed >= 1.f) {
      m_FPS = m_TempFPS;
      m_TempFPS = 0;
      m_FPSTimeElapsed = 0.f;
    }

    m_PrevTime = m_CurrentTime;

    return m_DeltaTime;
}

Bool Timer::IsFixedUpdate() const { return m_FixedAcc >= m_FixedDeltaTime; }

void Timer::ConsumeFixedDeltaTime() { m_FixedAcc -= m_FixedDeltaTime; }

Shared<Timer> Timer::Create() {
  auto timer = std::make_shared<Timer>();

  if (FAILED(timer->Initialize())) {
    MSG_BOX("Failed To Create Timer");
    return nullptr;
  }

  return timer;
}
