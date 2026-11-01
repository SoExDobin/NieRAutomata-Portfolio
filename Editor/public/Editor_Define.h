#pragma once

#include <windows.h>
#include "Engine_Define.h"

extern inline HWND g_hWnd = { nullptr };
extern inline HINSTANCE g_hInst = { nullptr };
extern inline ENGINE_DESC g_projectSettings = {
	nullptr, 
	nullptr,
	WINMODE::WIN, 
	L"Editor", 
	1920, 
	1080, 
	0, 
	0,
	true, 
	2
};

namespace Editor
{
	struct OBJECT_MOVE_PAYLOAD final
	{
		ObjectGuid objectGuid{};
		uint32 levelIndex{};
	};

	#define EDITOR		Editor::EditorManager::GetInstance()
	enum class EDITOR_STATE { PLAY, PAUSE, STOP };

	const string Drag_PayLoadKey = "PROTOTYPE_DRAG";
	const string ObjectMove_PayLoadKey = "OBJECT_MOVE_DRAG";
}

using namespace Editor;
