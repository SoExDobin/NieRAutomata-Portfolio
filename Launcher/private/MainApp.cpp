#include "pch.h"
#include "MainApp.h"

#include <Game.h>
#include <Navigation.h>
#include <ClientApp.h>
#include "ClientSettingManager.h"
#include <sstream>

namespace
{
	/* Editor MenuBar의 DebugToggles와 같은 배치: F1 프레임 표시(따로 처리), F2~F3 디버그 그리기(Debug만), F4~F7 최적화,
	   Ctrl+F1~F8 렌더 타깃·셰이딩(Ctrl+F1 MRT 미리보기는 Debug만). 켜고 끈 FPS 차이를 Release에서도 보일 수 있게 둔다. */
	struct HOT_KEY
	{
		uByte key;
		Bool ctrl;
		Bool (*toggle)();
	};
	const HOT_KEY HotKeys[] =
	{
#ifdef _DEBUG
		{ DIK_F2, false, [] { return GAME_INSTANCE->Toggle_RenderDebug(); } },
		{ DIK_F3, false, [] { Navigation::Toggle_DebugRender(); return Navigation::Get_DebugRender(); } },
#endif
		{ DIK_F4, false, [] { return GAME_INSTANCE->Toggle_FrustumCulling(); } },
		{ DIK_F5, false, [] { return GAME_INSTANCE->Toggle_WorldLod(); } },
		{ DIK_F6, false, [] { return GAME_INSTANCE->Toggle_Occlusion(); } },
		{ DIK_F7, false, [] { return GAME_INSTANCE->Toggle_Instancing(); } },
#ifdef _DEBUG
		{ DIK_F1, true, [] { return GAME_INSTANCE->Toggle_RenderTargetDebug(); } },
#endif
		{ DIK_F2, true, [] { return GAME_INSTANCE->Toggle_Shadow(); } },
		{ DIK_F3, true, [] { return GAME_INSTANCE->Toggle_SSAO(); } },
		{ DIK_F4, true, [] { return GAME_INSTANCE->Toggle_Volumetric(); } },
		{ DIK_F5, true, [] { return GAME_INSTANCE->Toggle_Fog(); } },
		{ DIK_F6, true, [] { return GAME_INSTANCE->Toggle_Bloom(); } },
		{ DIK_F7, true, [] { return GAME_INSTANCE->Toggle_ColorGrade(); } },
		{ DIK_F8, true, [] { return GAME_INSTANCE->Toggle_CascadeDebug(); } },
	};
	static_assert(std::size(HotKeys) < 32, "m_PrevToggleKeys holds one bit per hot key");

#ifdef _DEBUG
	// Ctrl+F1 타일 이름: Renderer가 타일을 그리는 표(RenderTargetDebugTiles)로 같은 View 픽셀 자리에 쓴다(Editor는 ImGui로 붙인다).
	// 레이블은 '\n'마다 한 줄이고, 타일 폭보다 긴 줄만 단어 단위로 다시 나눈다.
	void Draw_RenderTargetLabels()
	{
		if (!GAME_INSTANCE->Get_RenderTargetDebug())
			return;
		static const wstring font = L"Nier_16";
		// Nier_16이 가장 작은 폰트라 타일을 덜 가리게 줄여 그린다.
		constexpr Float scale = 0.7f;
		// Measure_Font의 y는 대문자의 세로 중앙(가운데 맞춤용)이라 두 배가 줄 높이다.
		const Float lineHeight = GAME_INSTANCE->Measure_Font(font, L"H").y * 2.f * scale;
		const auto drawLabel = [&](const RenderTargetDebugTile& tile)
		{
			const Float maxWidth = tile.size - 8.f;
			vector<wstring> lines;
			std::istringstream rows(tile.label);
			for (string row; std::getline(rows, row);)
			{
				std::istringstream words(row);
				wstring line;
				for (string word; words >> word;)
				{
					const wstring next = line.empty() ? wstring(word.begin(), word.end()) : line + L" " + wstring(word.begin(), word.end());
					if (!line.empty() && GAME_INSTANCE->Measure_Font(font, next.c_str()).x * scale > maxWidth)
					{
						lines.push_back(line);
						line = wstring(word.begin(), word.end());
					}
					else
						line = next;
				}
				lines.push_back(line);
			}
			Vector2 position(tile.x - tile.size * 0.5f + 4.f, tile.y - tile.size * 0.5f + 2.f);
			for (const wstring& line : lines)
			{
				GAME_INSTANCE->Draw_Font(font, line.c_str(), position + Vector2(1.f, 1.f), XMVectorSet(0.f, 0.f, 0.f, 0.9f), scale);
				GAME_INSTANCE->Draw_Font(font, line.c_str(), position, XMVectorSet(1.f, 1.f, 1.f, 1.f), scale);
				position.y += lineHeight;
			}
		};
		for (const RenderTargetDebugTile& tile : RenderTargetDebugTiles)
			drawLabel(tile);
		drawLabel(ShadowMapDebugTile);
	}
#endif
}

MainApp::MainApp()
{
}

MainApp::~MainApp()
{
	m_ClientApp.reset();
	GAME_INSTANCE->DestroyInstance();
}

HRESULT MainApp::Initialize()
{
	ClientSettingManager::GetInstance()->Load_EngineDesc(g_EngineDesc);

#ifdef _DEBUG
	g_EngineDesc.winMode = WINMODE::WIN;
	if (g_EngineDesc.viewportWidth > 1600u)
		g_EngineDesc.viewportWidth = 1600u;
	if (g_EngineDesc.viewportHeight > 900u)
		g_EngineDesc.viewportHeight = 900u;

	const LONG_PTR exStyle = GetWindowLongPtr(g_hWnd, GWL_EXSTYLE) & ~WS_EX_TOPMOST;
	SetWindowLongPtr(g_hWnd, GWL_EXSTYLE, exStyle);

	RECT windowRect{0, 0, static_cast<LONG>(g_EngineDesc.viewportWidth),
	                static_cast<LONG>(g_EngineDesc.viewportHeight)};
	AdjustWindowRect(&windowRect,
	                 static_cast<DWORD>(GetWindowLongPtr(g_hWnd, GWL_STYLE)),
	                 FALSE);
	SetWindowPos(g_hWnd, HWND_NOTOPMOST, 64, 64,
	             windowRect.right - windowRect.left,
	             windowRect.bottom - windowRect.top, SWP_SHOWWINDOW);
#else
	/* 테두리 없는 전체 화면 창으로 시작한다(Alt+Enter로 창 모드와 오간다, Set_Fullscreen). 독점 전체 화면은 창이 모니터
	   크기(예: 2560x1440)로 바뀌어도 WM_SIZE → ResizeBuffers가 없어 스왑 체인이 화면에 나오지 않았다(흰 창). */
	g_EngineDesc.winMode = WINMODE::WIN;
	Set_Fullscreen(true);
#endif

	g_EngineDesc.useOffscreenRendering = false;
	g_EngineDesc.renderTargetCount = 0;

	if (FAILED(GAME_INSTANCE->Initialize_Engine(g_EngineDesc)))
	{
		MSG_BOX("Failed to initialize Engine");
		return E_FAIL;
	}

	// DXGI의 Alt+Enter는 독점 전체 화면으로 들어가 위의 흰 창이 되므로 끄고, WndProc이 Toggle_Fullscreen으로 대신한다.
	ComPtr<IDXGIFactory> factory;
	if (SUCCEEDED(GAME_INSTANCE->Get_SwapChain()->GetParent(IID_PPV_ARGS(factory.GetAddressOf()))))
		factory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);

	ClientSettingManager::GetInstance()->Set_ResourcePath(
		L"../../../Client/bin/resources/");
	ClientSettingManager::GetInstance()->Set_ShaderPath(
		L"shaders/");

	m_ClientApp = ClientApp::Create(g_EngineDesc);
	if (!m_ClientApp)
		return E_FAIL;

	return S_OK;
}

void MainApp::Update()
{
	GAME_INSTANCE->Update_Engine();

	// 누른 순간에만 바꾼다. Ctrl이 항목과 맞을 때만 세서 Ctrl+F4가 F4까지 바꾸지 않는다.
	const Bool ctrlDown = ((GAME_INSTANCE->Get_DIKeyState(DIK_LCONTROL) | GAME_INSTANCE->Get_DIKeyState(DIK_RCONTROL)) & 0x80) != 0;
	const Bool frameStatsDown = !ctrlDown && (GAME_INSTANCE->Get_DIKeyState(DIK_F1) & 0x80) != 0;
	if (frameStatsDown && !m_PrevFrameStatsKey)
		m_ShowFrameStats = !m_ShowFrameStats;
	m_PrevFrameStatsKey = frameStatsDown;
	for (uint32 i = 0; i < std::size(HotKeys); ++i)
	{
		const Bool down = HotKeys[i].ctrl == ctrlDown && (GAME_INSTANCE->Get_DIKeyState(HotKeys[i].key) & 0x80) != 0;
		const uint32 bit = 1u << i;
		if (down && !(m_PrevToggleKeys & bit))
			HotKeys[i].toggle();
		m_PrevToggleKeys = down ? (m_PrevToggleKeys | bit) : (m_PrevToggleKeys & ~bit);
	}

	m_StatsTime += GAME_INSTANCE->Compute_UnscaledTimeDelta();
	++m_StatsFrames;
	if (m_StatsTime < 1.f)
		return;

	Format_Stats(m_StatsTime, m_StatsFrames);
	m_StatsTime = 0.f;
	m_StatsFrames = 0;
}

void MainApp::Format_Stats(Float seconds, uint32 frames)
{
	// 카운터는 직전 프레임 값이다. inst는 맵 소품이 한 draw로 묶여 그려진 인스턴스 수라 F11을 끄면 0이 되고 그만큼 draws가 는다.
	swprintf_s(m_StatsText, L"%.0f FPS  %.2f ms  |  %u draws  %.2fM tris  |  cull %s %u  |  occ %s %u  |  lod %s %u  |  inst %s %u  |  shadow %s  ssao %s  vol %s",
		frames / seconds, seconds * 1000.f / frames,
		GAME_INSTANCE->Get_FrameDrawCount(),
		static_cast<double>(GAME_INSTANCE->Get_FrameTriangleCount()) / 1000000.0,
		GAME_INSTANCE->Get_FrustumCulling() ? L"on" : L"off",
		GAME_INSTANCE->Get_FrameCulledCount(),
		GAME_INSTANCE->Get_Occlusion() ? L"on" : L"off",
		GAME_INSTANCE->Get_FrameOccludedCount(),
		GAME_INSTANCE->Get_WorldLod() ? L"on" : L"off",
		GAME_INSTANCE->Get_FrameLodTileCount(),
		GAME_INSTANCE->Get_Instancing() ? L"on" : L"off",
		GAME_INSTANCE->Get_FrameInstanceCount(),
		GAME_INSTANCE->Get_Shadow() ? L"on" : L"off",
		GAME_INSTANCE->Get_SSAO() ? L"on" : L"off",
		GAME_INSTANCE->Get_Volumetric() ? L"on" : L"off");
}

void MainApp::Render()
{
	// Launcher는 오프스크린 없이 스왑 체인 백버퍼에 그린다. Present가 렌더 타깃을 풀기 때문에 매 프레임 다시 묶는다.
	static const Shared<Float4> clearColor = make_shared<Float4>(0.f, 0.f, 0.f, 1.f);
	GAME_INSTANCE->End_RenderOffScreen();
	GAME_INSTANCE->Clear_BackBufferView(clearColor);

	if (FAILED(GAME_INSTANCE->Draw()))
	{
		MSG_BOX("MainApp Rendering Failed");
	}
#ifdef _DEBUG
	GAME_INSTANCE->Render_CollisionDebug();
	Draw_RenderTargetLabels();
#endif

	// 글자 폭만큼 오른쪽에 붙여서 줄이 길어져도 화면 안에 남는다. 밝은 하늘·불꽃 위에서도 읽히게 검은 그림자를 먼저 깐다.
	if (m_ShowFrameStats)
	{
		const Float width = GAME_INSTANCE->Measure_Font(L"Nier_16", m_StatsText).x;
		const Vector2 position((std::max)(16.f, static_cast<Float>(g_EngineDesc.viewportWidth) - width - 24.f), 10.f);
		GAME_INSTANCE->Draw_Font(L"Nier_16", m_StatsText, position + Vector2(2.f, 2.f), XMVectorSet(0.f, 0.f, 0.f, 0.85f));
		GAME_INSTANCE->Draw_Font(L"Nier_16", m_StatsText, position);
	}

	GAME_INSTANCE->Present();
}

/* 전체 화면: 테두리 없는 창을 모니터에 꽉 채운다. 창 모드: 클라이언트를 EngineDesc 해상도로(작업 영역에 안 들어가면
   비율을 지켜 줄여) 작업 영역 가운데에 둔다. 스왑 체인 버퍼는 그대로라 DXGI_SCALING_STRETCH가 창 크기에 맞춰 늘린다. */
void MainApp::Set_Fullscreen(Bool fullscreen)
{
	MONITORINFO monitor{ sizeof(MONITORINFO) };
	GetMonitorInfoW(MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTOPRIMARY), &monitor);
	if (fullscreen)
	{
		SetWindowLongPtr(g_hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		SetWindowPos(g_hWnd, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
		             monitor.rcMonitor.right - monitor.rcMonitor.left,
		             monitor.rcMonitor.bottom - monitor.rcMonitor.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		return;
	}

	constexpr DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	const RECT& work = monitor.rcWork;
	RECT frame{};
	AdjustWindowRect(&frame, style, FALSE);	// 테두리·제목 줄 두께(left·top은 음수)
	const Float maxWidth = static_cast<Float>((work.right - work.left) - (frame.right - frame.left));
	const Float maxHeight = static_cast<Float>((work.bottom - work.top) - (frame.bottom - frame.top));
	const Float scale = (std::min)({ 1.f, maxWidth / g_EngineDesc.viewportWidth, maxHeight / g_EngineDesc.viewportHeight });
	const LONG width = static_cast<LONG>(g_EngineDesc.viewportWidth * scale) + (frame.right - frame.left);
	const LONG height = static_cast<LONG>(g_EngineDesc.viewportHeight * scale) + (frame.bottom - frame.top);
	SetWindowLongPtr(g_hWnd, GWL_STYLE, style);
	SetWindowPos(g_hWnd, HWND_NOTOPMOST, work.left + ((work.right - work.left) - width) / 2,
	             work.top + ((work.bottom - work.top) - height) / 2, width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

void MainApp::Toggle_Fullscreen()
{
	Set_Fullscreen(!(GetWindowLongPtr(g_hWnd, GWL_STYLE) & WS_POPUP));
}

Unique<MainApp> MainApp::Create()
{
	auto mainApp = make_unique<MainApp>();

	if FAILED((mainApp->Initialize()))
	{
		MSG_BOX("Failed to Load MainApp");
		return nullptr;
	}

	return mainApp;
}
