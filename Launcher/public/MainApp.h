#pragma once
#include <Engine_Define.h>


NS_BEGIN(Client)
	class ClientApp;
NS_END

NS_BEGIN(Launcher)

class MainApp final
{
public:
	explicit MainApp();
	~MainApp();

private:
	HRESULT Initialize();

public:
	void Update();
	void Render();

private:
	void Format_Stats(Float seconds, uint32 frames);

private:
	Unique<ClientApp> m_ClientApp{nullptr};

	// F1로 켜는 우상단 프레임 표시: 1초 평균 FPS/ms, draw·삼각형·묶인 인스턴스 수, 최적화별 켬/끔과 컬링·가림·LOD 타일 수.
	Float m_StatsTime{};
	uint32 m_StatsFrames{};
	wchar_t m_StatsText[256]{};
	Bool m_ShowFrameStats{ false };
	Bool m_PrevFrameStatsKey{ false };
	uint32 m_PrevToggleKeys{};	// HotKeys(MainApp.cpp) 항목마다 한 비트, 키를 누르고 있는 동안 켜진다

public:
	// Alt+Enter(WndProc): 테두리 없는 전체 화면 ↔ 창 모드. Release는 전체 화면, Debug는 창으로 시작한다.
	static void Set_Fullscreen(Bool fullscreen);
	static void Toggle_Fullscreen();
	static Unique<MainApp> Create();
};

NS_END
