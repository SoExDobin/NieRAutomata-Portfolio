#include "pch.h"
#include "LevelTitle.h"
#include <SpdLogger.h>
#include <Game.h>

#include "ClientSettingManager.h"
#include "LevelGamePlay.h"
#include "LevelLoading.h"
#include "Loader.h"
#include "LoadingFadeIn.h"
#include "StaticCamera.h"
#include "TitleBackground.h"
#include <TextUI.h>

namespace
{
	constexpr Float TapPromptY = 860.f;
	constexpr Float TapPromptAlpha = 0.9f;
	constexpr Float TapPromptPeriod = 2.4f;	// s per breath
}

LevelTitle::LevelTitle(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Level { device, context } {}

HRESULT LevelTitle::Initialize(void* arg)
{
	const HRESULT sceneResult = ClientSettingManager::GetInstance()->Load_LevelData(LEVEL::TITLE);
	if (FAILED(sceneResult)) return E_FAIL;
	// A fresh checkout has no authored Title scene. Keep the existing Title asset usable.
	if (sceneResult == S_FALSE)
	{
		if (!GAME_INSTANCE->Instantiate<StaticCamera>(L"StaticCamera", ETOI(LEVEL::TITLE)) ||
			!GAME_INSTANCE->Instantiate<TitleBackground>(L"TitleBackground", ETOI(LEVEL::TITLE)))
			return E_FAIL;
	}

	return Level::Initialize(arg);
}

void LevelTitle::On_Destroy()
{
	Level::On_Destroy();
}

void LevelTitle::Update_Level(Float timeDelta)
{
    // Editor STOP loads the authored scene without advancing runtime UI animations.
    if (!m_FadeIn) {
        if (FAILED(Ready_TitleUI())) return;
        m_FadeIn->Submit_RenderGroup();
        m_TapPrompt->Submit_RenderGroup();
    }
    if (!m_IsIntroPlayed) {
        GAME_INSTANCE->PlaySoundFXOnce(L"Title_Intro", SOUNDCHANNEL::CHANNEL_1);
        m_IsIntroPlayed = true;
    }
    // The prompt breathes until the screen is tapped.
    m_PromptTime += timeDelta;
    Color promptColor = m_TapPrompt->Get_Color();
    promptColor.w = m_Covering ? 0.f : TapPromptAlpha * (0.65f + 0.35f * cosf(m_PromptTime * XM_2PI / TapPromptPeriod));
    m_TapPrompt->Set_Color(promptColor);
    if (!m_FadeIn->Is_FadeFinished() || m_TransitionQueued) return;
    if (!m_Covering) {
        if (GAME_INSTANCE->Get_DIMouseState(DIMB::LBUTTON) & 0x80) {
            m_Covering = true;
            m_FadeIn->Play(true);
            GAME_INSTANCE->PlaySoundFXOnce(L"Title_Tap", SOUNDCHANNEL::CHANNEL_2);
        }
        return;
    }
    m_TransitionQueued = true;
    const auto device = m_Device;
    const auto context = m_Context;
    GAME_INSTANCE->Add_Instance_Event(ETOI(LEVEL::TITLE), L"Title_ChangeLevel", [device, context]() {
        GAME_INSTANCE->Change_Level(ETOI(LEVEL::LOADING), LevelLoading::Create(device, context, LEVEL::GAMEPLAY, true));
    });
}

HRESULT LevelTitle::Render_Level()
{
	return S_OK;
}

HRESULT LevelTitle::Ready_TitleUI()
{
    m_FadeIn = LoadingFadeIn::Load_UI(ETOI(LEVEL::TITLE));
    if (!m_FadeIn) return E_FAIL;
    m_FadeIn->Play(false);

    // "Tap to start", centred in the lower part of the screen in the HUD's ivory.
    TextUI::UI_TEXT_DESC promptDesc{};
    promptDesc.anchor = UI_ANCHOR::TOP_LEFT;
    promptDesc.x = 960.f;
    promptDesc.y = TapPromptY;
    promptDesc.fontTag = L"Nier_KR_32";
    promptDesc.text = L"탭하여 시작";
    promptDesc.color = Color{ 0.92f, 0.9f, 0.86f, TapPromptAlpha };
    promptDesc.spacing = 6.f;
    promptDesc.pivot = 0.5f;
    m_TapPrompt = GAME_INSTANCE->Instantiate<TextUI>(L"TextUI", ETOI(LEVEL::TITLE), &promptDesc);
    return m_TapPrompt ? S_OK : E_FAIL;
}

Shared<LevelTitle> LevelTitle::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto titleLevel = make_shared<LevelTitle>(device, context);

	if (FAILED(titleLevel->Initialize(nullptr))) {
		LOG_ERROR(L"Failed To Create LoadingBackground");
		return nullptr;
	}

	return titleLevel;
}
