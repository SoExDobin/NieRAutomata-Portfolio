#include "pch.h"
#include "ClientApp.h"
#include "UIManager.h"
#include "CameraEventSystem.h"
#include "Client_Function.h"

#include "ClientSettingManager.h"
#include "Game.h"
#include "LevelLoading.h"
#include "LevelGamePlay.h"
#include <mutex>

#include "LoadingFadeIn.h"
#include "LoadingFadeOut.h"
#include "StaticCamera.h"
#include "SkySphere.h"
#include "GameObject.h"
#include "Camera.h"
#include "Pl0000.h"
#include "TextUI.h"
#include "UIImage.h"

ClientApp* ClientApp::s_Instance = nullptr;

ClientApp::ClientApp() : m_UIManager{ make_unique<UIManager>() }, m_CameraEventSystem{ make_unique<CameraEventSystem>() } { s_Instance = this; }

ClientApp::~ClientApp()
{
    m_CameraEventSystem.reset();
    if (s_Instance == this) s_Instance = nullptr;
    m_UIManager.reset();
}

Bool ClientApp::Play_CameraEvent(CAMERA_EVENT event) { return m_CameraEventSystem->Play(event); }
Bool ClientApp::Is_CameraEventPlaying() const { return m_CameraEventSystem->Is_Busy(); }
void ClientApp::Update_CameraEvents(LEVEL level, Float timeDelta, Bool entryReady)
{
    m_CameraEventSystem->Update(level, timeDelta, entryReady);
}
void ClientApp::Cancel_CameraEvent() { m_CameraEventSystem->Cancel(); }
void ClientApp::End_CameraEvents(LEVEL level) { m_CameraEventSystem->End_Level(level); }

HRESULT ClientApp::Initialize(const ENGINE_DESC& desc) 
{
	if (FAILED(Client::Register_Client_Reflection()))
		return E_FAIL;
    if (FAILED(GAME_INSTANCE->Refresh_ReflectionRegistry()))
        return E_FAIL;

    ClientSettingManager::g_EngineDesc = desc;

    if (FAILED(ClientSettingManager::GetInstance()->Apply_LayerAndTagSettings())) {
      return E_FAIL;
    }
	if (FAILED(GAME_INSTANCE->Load_PrefabRepository(
		L"../../../ProjectSetting/Prefab/")))
		return E_FAIL;
    if (FAILED(ClientSettingManager::GetInstance()->Load_Shader()))
        return E_FAIL;
    
    if (FAILED(Ready_InitialObject()))
        return E_FAIL;

    if (FAILED(Ready_StartLevel(static_cast<LEVEL>(ClientSettingManager::g_EngineDesc.startLevel))))
      return E_FAIL;

    return S_OK;
}

HRESULT ClientApp::Ready_StartLevel(LEVEL startLevel) 
{
    const auto loadingLevel = LevelLoading::Create(GAME_INSTANCE->Get_Device(),
        GAME_INSTANCE->Get_Context(), startLevel, true);
    if (!loadingLevel) {
        LOG_ERROR(L"Startup blocked before level {}: Loading initialization failed; see the preceding resource error",
            ETOI(startLevel));
        return E_FAIL;
    }
    return GAME_INSTANCE->Change_Level(ETOI(LEVEL::LOADING), loadingLevel);
}

HRESULT ClientApp::Ready_InitialObject()
{
    GAME_INSTANCE->Add_Font(L"Nier_16", L"../../../Client/bin/resources/Font/NierFont_16.spritefont");
    GAME_INSTANCE->Add_Font(L"Nier_32", L"../../../Client/bin/resources/Font/NierFont_32.spritefont");
    GAME_INSTANCE->Add_Font(L"Nier_64", L"../../../Client/bin/resources/Font/NierFont_64.spritefont");
    // Thinner, with the boss names' Hangul: the WARNING band and the names.
    GAME_INSTANCE->Add_Font(L"Nier_KR_32", L"../../../Client/bin/resources/Font/NierFont_KR_32.spritefont");
    GAME_INSTANCE->Add_Font(L"Nier_KR_64", L"../../../Client/bin/resources/Font/NierFont_KR_64.spritefont");
    GAME_INSTANCE->Add_Font(L"Nier_Loading_32",
        L"../../../Client/bin/resources/Font/NierFont_Loading_32.spritefont");
    GAME_INSTANCE->Add_Font(L"Nier_Communication_32",
        L"../../../Client/bin/resources/UI/Communication/Communication_32.spritefont");

    if (FAILED(GAME_INSTANCE->Register_ReflectedPrototypes()))
        return E_FAIL;

	const Shared<TextUI> textUiPrototype = TextUI::CreatePrototype();
	if (!textUiPrototype || FAILED(GAME_INSTANCE->Add_TypePrototype(
		ETOI(LEVEL::STATIC), textUiPrototype)))
		return E_FAIL;
	// Engine GameObjects get no automatic prototype; the HUD builds its pieces from these (HpBarWorldUI, BossAlert).
	const auto& staticPrototypes = GAME_INSTANCE->Get_Prototypes(ETOI(LEVEL::STATIC));
	if (!staticPrototypes.contains(L"UIPanel"))
	{
		const Shared<UIPanel> panelPrototype = UIPanel::CreatePrototype();
		if (!panelPrototype || FAILED(GAME_INSTANCE->Add_TypePrototype(ETOI(LEVEL::STATIC), panelPrototype)))
			return E_FAIL;
	}
	if (!staticPrototypes.contains(L"UIImage"))
	{
		const Shared<UIImage> imagePrototype = UIImage::CreatePrototype();
		if (!imagePrototype || FAILED(GAME_INSTANCE->Add_TypePrototype(ETOI(LEVEL::STATIC), imagePrototype)))
			return E_FAIL;
	}

    return S_OK;
}

Unique<ClientApp> ClientApp::Create(const ENGINE_DESC &desc) {
  if (s_Instance)
  {
    LOG_ERROR(L"Only one active ClientApp may own the client UI");
    return nullptr;
  }
  Unique<ClientApp> mainApp = make_unique<ClientApp>();

  if (FAILED(mainApp->Initialize(desc))) {
    LOG_ERROR(L"ClientApp initialization failed; see the preceding startup error");
    return nullptr;
  }
  return mainApp;
}
