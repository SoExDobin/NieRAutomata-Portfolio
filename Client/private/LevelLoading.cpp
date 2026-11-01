#include "pch.h"
#include "LevelLoading.h"

#include "Game.h"
#include "Loader.h"
#include <SpdLogger.h>
#include "LoadingFadeIn.h"

#include "ClientSettingManager.h"
#include "LevelGamePlay.h"
#include "LevelTitle.h"
#include "LoadingBackground.h"
#include "StaticCamera.h"

LevelLoading::LevelLoading(const ComPtr<ID3D11Device> &device,
                           const ComPtr<ID3D11DeviceContext> &context)
    : Level{device, context}, m_Loader{nullptr}, m_NextLevel{LEVEL::LEVEL_END} {
}

LevelLoading::~LevelLoading() = default;

HRESULT LevelLoading::Initialize(void *arg) {
    LEVEL_LOADING_DESC desc = *static_cast<LEVEL_LOADING_DESC*>(arg);
    m_NextLevel = desc.nextLevelID;
    m_IsLoadStatic = desc.loadStatic;

    if (FAILED(ClientSettingManager::GetInstance()->Sync_TextureJson_FromExcel())) {
        LOG_ERROR(L"Failed to synchronize Texture settings from Excel");
        return E_FAIL;
    }
    if (FAILED(ClientSettingManager::GetInstance()->Load_Textures_FromJson(LEVEL::LOADING)))
    {
        LOG_ERROR(L"Failed to load Loading Texture");
        return E_FAIL;
    }
    if (FAILED(ClientSettingManager::GetInstance()->Load_Textures_FromJson(LEVEL::STATIC))) {
        LOG_ERROR(L"Failed to Load Textures");
        return E_FAIL;
    }

    if (FAILED(Ready_LoadingUI())) return E_FAIL;

    m_Loader = Loader::Create(m_Device, m_Context, m_NextLevel, shared_from_this(), m_IsLoadStatic);
    if (nullptr == m_Loader)
        return E_FAIL;

    m_IsFinished = m_Loader->Is_Finished();

    return Level::Initialize(arg);
}

void LevelLoading::On_Destroy() {
    if (m_Loader) {
        m_Loader->On_Destroy();
        m_Loader.reset();
    }
    if (m_StaticCamera) {
        GAME_INSTANCE->Destroy(m_StaticCamera->Get_ObjectGuid());
        m_StaticCamera.reset();
    }
    Level::On_Destroy();
}

void LevelLoading::Update_Level(Float timeDelta) 
{
    m_Background->Set_LoadComplete(m_IsFinished.load());
    if (m_IsFinished && m_Background->Is_PresentationFinished())
        Transition_To_NextLevel();
}

void LevelLoading::Transition_To_NextLevel()
{
	if (!m_Covering)
	{
        m_Covering = true;
        m_FadeIn->Play(true);
        return;
	}
    if (!m_FadeIn->Is_FadeFinished()) return;
	else
	{
		if (ClientSettingManager::GetInstance()->AutoTransitionLevel(LEVEL::LOADING, m_NextLevel))
		{
			// A failed Scene must not be recreated every frame after resource loading finishes.
			m_IsFinished = false;
			const auto device = m_Device;
			const auto context = m_Context;
			const auto target = m_NextLevel;
			GAME_INSTANCE->Add_Instance_Event(ETOI(LEVEL::LOADING), L"Loading_ChangeLevel", [device, context, target]() {
				if (target == LEVEL::TITLE)
					GAME_INSTANCE->Change_Level(ETOI(target), LevelTitle::Create(device, context));
				else if (target == LEVEL::GAMEPLAY || target == LEVEL::AMUSEMENT_PARK)
					GAME_INSTANCE->Change_Level(ETOI(target), LevelGamePlay::Create(device, context, target));
			});
		}
	}
}

HRESULT LevelLoading::Render_Level()
{
    return S_OK;
}

HRESULT LevelLoading::Ready_LoadingUI()
{
    m_StaticCamera = GAME_INSTANCE->Instantiate<StaticCamera>(L"StaticCamera", ETOI(LEVEL::LOADING));
    m_Background = LoadingBackground::Load_UI(ETOI(LEVEL::LOADING));
    m_FadeIn = LoadingFadeIn::Load_UI(ETOI(LEVEL::LOADING));
    if (!m_StaticCamera || !m_Background || !m_FadeIn) return E_FAIL;
    m_Background->Set_LoadComplete(false);
    m_FadeIn->Play(false);
    return S_OK;
}

Shared<LevelLoading> LevelLoading::Create(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context, LEVEL nextLevelID, Bool loadStatic) {
    auto loadingLevel = make_shared<LevelLoading>(device, context);

    LEVEL_LOADING_DESC desc{};
    desc.nextLevelID = nextLevelID;
    desc.loadStatic = loadStatic;

    if (FAILED(loadingLevel->Initialize(&desc))) {
		LOG_ERROR(L"Failed to initialize Loading level for target {}", ETOI(nextLevelID));
		return nullptr;
    }

    return loadingLevel;
}
