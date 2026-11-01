#include "pch.h"
#include "LevelGamePlay.h"

#include <SpdLogger.h>

#include "ClientSettingManager.h"
#include "ClientApp.h"
#include "UIManager.h"
#include "LoadingFadeIn.h"
#include "LevelLoading.h"
#include "Game.h"

LevelGamePlay::LevelGamePlay(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, LEVEL level)
	: Level{ device, context }, m_Level{ level }
{
}

HRESULT LevelGamePlay::Initialize(void* arg)
{

	if (FAILED(ClientSettingManager::GetInstance()->Load_LevelData(m_Level)))
	{
		LOG_CRITICAL(L"Failed To Load GamePlay Level {}", ETOI(m_Level));
		return E_FAIL;
	}

	return Level::Initialize(arg);
}

void LevelGamePlay::On_Destroy()
{
    if (const auto client = ClientApp::GetInstance())
    {
        client->End_CameraEvents(m_Level);
        client->Get_UIManager()->End_Level(m_Level);
    }
	Level::On_Destroy();
}

void LevelGamePlay::Update_Level(Float timeDelta)
{
	Level::Update_Level(timeDelta);
    // The fade is a runtime object: made on the first update (Play only in the Editor), it never reaches a saved
    // LevelData. It is drawn this same frame, so the scene does not flash before it covers.
    if (!m_Fade)
    {
        m_Fade = LoadingFadeIn::Load_UI(ETOI(m_Level));
        if (!m_Fade) return;
        m_Fade->Play(false);
        m_Fade->Submit_RenderGroup();
    }
    if (const auto client = ClientApp::GetInstance())
    {
        const Bool ready = m_NextLevel == LEVEL::LEVEL_END && m_Fade->Is_FadeFinished();
        client->Update_CameraEvents(m_Level, timeDelta, ready);
        client->Get_UIManager()->Update(m_Level, timeDelta, ready && !client->Is_CameraEventPlaying());
    }
    if (m_NextLevel == LEVEL::LEVEL_END || !m_Fade->Is_FadeFinished() || m_TransitionQueued) return;
    m_TransitionQueued = true;
    const auto device = m_Device;
    const auto context = m_Context;
    const auto target = m_NextLevel;
    GAME_INSTANCE->Add_Instance_Event(ETOI(m_Level), L"Gameplay_ChangeLevel", [device, context, target]() {
        GAME_INSTANCE->Change_Level(ETOI(LEVEL::LOADING), LevelLoading::Create(device, context, target));
    });
}

Bool LevelGamePlay::Request_LevelChange(LEVEL nextLevel)
{
    if (!m_Fade || m_NextLevel != LEVEL::LEVEL_END || (nextLevel != LEVEL::GAMEPLAY && nextLevel != LEVEL::AMUSEMENT_PARK))
        return false;
    if (const auto client = ClientApp::GetInstance())
    {
        client->Cancel_CameraEvent();
        client->Get_UIManager()->Cancel_Communication();
    }
    m_NextLevel = nextLevel;
    m_Fade->Play(true);
    return true;
}

HRESULT LevelGamePlay::Render_Level()
{
	return Level::Render_Level();
}

Shared<LevelGamePlay> LevelGamePlay::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, LEVEL level)
{
	auto levGamePlay = make_shared<LevelGamePlay>(device, context, level);

	if (FAILED(levGamePlay->Initialize(nullptr)))
	{
		LOG_ERROR(L"Failed to Create GamePlay Level");
		return nullptr;
	}

	return levGamePlay;
}
