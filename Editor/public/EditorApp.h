#pragma once
#include "ClientApp.h"

NS_BEGIN(Engine)
class Game;
NS_END

NS_BEGIN(Editor)

class EditorManager;

class EditorApp final {
public:
  EditorApp();
  ~EditorApp();

public:
  HRESULT Initialize();
  void Update();
  HRESULT Render();

private:
  HRESULT Initialize_IMGUI(const ENGINE_DESC &desc);
  HRESULT Destruct_IMGUI();

private:
  HRESULT Reset_ClientApp();
  HRESULT Change_ClientLevel(uint32 levIndex);

private:
	uint32 m_StartLevel{};
	uint32 m_StartLevelIndex{};
	Bool m_IsReset = { false };
	Unique<ClientApp> m_ClientApp = {nullptr};
#ifdef _DEBUG
	// NIER_SCENE_CAPTURE=<model tag>: saves the real Scene View (deferred lighting) and quits.
	void Update_SceneCapture();
	uint32 m_SceneCaptureFrame{};
	// NIER_SCENE_CAPTURE_VIEWS: average frame time over a view's last 40 frames (after the first 20 settle), logged on save.
	std::chrono::steady_clock::time_point m_SceneCaptureViewStart{};
	Float m_SceneCaptureFrameMs{};
#endif

public:
  static Unique<EditorApp> Create();
};

NS_END
