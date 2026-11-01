#pragma once
#include "EditorObject.h"

NS_BEGIN(Editor)

class MenuBar final : public EditorObject
{
public:
	MenuBar();
	~MenuBar() override = default;

public:
	HRESULT Initialize() override;
	void Update(Bool isResize) override;
	void Render(Bool isResize) override;

private:
	void Update_HotKey();
	void Render_Debug();
	void Render_HotKeyHelp() const;
	void Render_Prefab();
	void Spawn_LayoutLights(Bool spawnMissing);

private:
	char m_Title[MAX_PATH] = "client";

	Bool m_ShowProjectSettings = false;
	char m_PrefabName[MAX_PATH]{};
	Bool m_LayoutLightsApplyPending = false;	// lights spawned this frame get their Color/Range next frame

	uint32 m_PrevToggleKeys = 0;	// one bit per DebugToggles entry (MenuBar.cpp), set while its key is held
	Bool m_PrevFrameStatsKey = false;
	Bool m_ShowFrameStats = true;	// Editor starts with frame stats shown (F1)

public:
	static Shared<MenuBar> Create();
};

NS_END
