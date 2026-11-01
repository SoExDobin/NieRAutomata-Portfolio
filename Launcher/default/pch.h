#ifndef PCH_H
#define PCH_H

// 여기에 미리 컴파일하려는 헤더 추가
#include "framework.h"
#include <windows.h>

#include "Engine_Define.h"
#include "Client_Define.h"

using namespace Engine;
using namespace Client;

extern HWND g_hWnd;
extern HINSTANCE g_hInstance;
extern ENGINE_DESC g_EngineDesc;

namespace Launcher
{
	
}

using namespace Launcher;

#endif //PCH_H
