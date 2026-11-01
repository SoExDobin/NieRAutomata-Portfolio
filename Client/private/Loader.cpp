#include "pch.h"
#include "Loader.h"
#include "ClientApp.h"
#include "UIManager.h"
#include "SpdLogger.h"

#include "ClientSettingManager.h"
#include "Game.h"
#include "NavigationManager.h"


Loader::Loader(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context)
    : Level{device, context} {}
Loader::~Loader() {}

HRESULT Loader::Initialize_Prototype() { return Level::Initialize_Prototype(); }

HRESULT Loader::Render_Level() { return Level::Render_Level(); }

HRESULT Loader::Initialize(void *arg)
{
    if (nullptr == arg)
    {
        LOG_CRITICAL(L"Failed to Initialize Loader By LoaderDesc");
        return E_FAIL;
    }

    LOADER_DESC& desc = *static_cast<LOADER_DESC*>(arg);

    m_NextLevelID = desc.nextLevelID;
    m_OwnerLevel = desc.ownerLevel;
    m_LoadStatic = desc.isLoadStatic;

    // The job keeps the loader alive until Loading() returns, even if the owning level lets go of it first.
    m_LoadResult = GAME_INSTANCE->Submit_Job([self = static_pointer_cast<Loader>(shared_from_this())]
        {
            const HRESULT hr = self->Loading();
            if (FAILED(hr))
                LOG_ERROR(L"Loading level {} failed (HRESULT 0x{:08X})", ETOI(self->m_NextLevelID), static_cast<uint32>(hr));
            return hr;
        });

	return Level::Initialize(arg);
}

void Loader::On_Destroy() {
    if (m_LoadResult.valid())
        m_LoadResult.wait();

    Level::On_Destroy();
}

void Loader::Update_Level(Float timeDelta)
{
	Level::Update_Level(timeDelta);
}

HRESULT Loader::Loading() {
    // Runs on a JobManager worker, which has already initialized COM for WIC.
    HRESULT hr = S_OK;
    if (m_LoadStatic)
        hr = Loading_Global_Prototype();

    // Missing settings resources return S_FALSE; actual failures must not be overwritten.
    if (SUCCEEDED(hr))
    {
        switch (m_NextLevelID) {
        case LEVEL::TITLE:
            hr = Loading_For_TitleLevel();
            break;
        case LEVEL::GAMEPLAY:
        case LEVEL::AMUSEMENT_PARK:
            hr = Loading_For_GamePlayLevel();
            break;
        default:
            hr = E_FAIL;
        }
    }

    return hr;
}

HRESULT Loader::Loading_For_TitleLevel() {
	m_isFinished = false;

    if (FAILED(ClientSettingManager::GetInstance()->Sync_TextureJson_FromExcel())) {
        LOG_ERROR(L"Failed to synchronize TITLE Texture settings from Excel");
        return E_FAIL;
    }
    if (FAILED(ClientSettingManager::GetInstance()->Load_Textures_FromJson(LEVEL::TITLE))) {
        LOG_ERROR(L"Failed to Load TITLE Textures");
        return E_FAIL;
    }
    if (FAILED(ClientSettingManager::GetInstance()->Sync_ModelJson_FromExcel())) {
        LOG_ERROR(L"Failed to synchronize TITLE Model settings from Excel");
        return E_FAIL;
    }
    if (FAILED(ClientSettingManager::GetInstance()->Load_Model_FromJson(LEVEL::TITLE))) {
        LOG_ERROR(L"Failed to Load TITLE Model");
        return E_FAIL;
    }

    m_isFinished = true;
    if (!m_OwnerLevel.expired())
		m_OwnerLevel.lock()->Set_LoadFinishFlag(m_isFinished);
	return S_OK;
}

HRESULT Loader::Loading_For_GamePlayLevel() {
    m_isFinished = false;

    if (FAILED(ClientSettingManager::GetInstance()->Sync_TextureJson_FromExcel())) {
        LOG_ERROR(L"Failed to synchronize GAMEPLAY Texture settings from Excel");
        return E_FAIL;
    }
    if (FAILED(ClientSettingManager::GetInstance()->Load_Textures_FromJson(m_NextLevelID))) {
        LOG_ERROR(L"Failed to Load GAMEPLAY Textures");
        return E_FAIL;
    }
    if (FAILED(ClientSettingManager::GetInstance()->Sync_ModelJson_FromExcel())) {
        LOG_ERROR(L"Failed to synchronize GAMEPLAY Model settings from Excel");
        return E_FAIL;
    }
    if (FAILED(ClientSettingManager::GetInstance()->Load_Model_FromJson(m_NextLevelID))) {
        LOG_ERROR(L"Failed to Load GAMEPLAY Model");
        return E_FAIL;
    }

    m_isFinished = true;
    if (!m_OwnerLevel.expired())
        m_OwnerLevel.lock()->Set_LoadFinishFlag(m_isFinished);
	return S_OK;
}

HRESULT Loader::Loading_Global_Prototype()
{
    m_isFinished = false;

    // Shaders are loaded once into STATIC by ClientApp::Initialize before any loader starts.
    if (FAILED(ClientSettingManager::GetInstance()->Sync_TextureJson_FromExcel())) {
        LOG_ERROR(L"Failed to synchronize STATIC Texture settings from Excel");
        return E_FAIL;
    }
    if (FAILED(ClientSettingManager::GetInstance()->Load_Textures_FromJson(LEVEL::STATIC))) {
        LOG_ERROR(L"Failed to Load Textures");
        return E_FAIL;
    }
    if (FAILED(ClientSettingManager::GetInstance()->Sync_ModelJson_FromExcel())) {
        LOG_ERROR(L"Failed to synchronize STATIC Model settings from Excel");
        return E_FAIL;
    }
    if (FAILED(ClientSettingManager::GetInstance()->Load_Model_FromJson(LEVEL::STATIC))) {
        LOG_ERROR(L"Failed to Load Model");
        return E_FAIL;
    }

    if (SUCCEEDED(ClientSettingManager::GetInstance()->Sync_SoundJson_FromCSV())) {
        if (FAILED(ClientSettingManager::GetInstance()->Load_Sound_FromJson())) {
            LOG_ERROR(L"Failed to Load Sounds");
            return E_FAIL;
        }
    }
    // Communication voices, signals and textures load here with the other sounds, not on the first call's frame.
    // A failure only disables communications (UIManager logs it), as before.
    if (const auto app = ClientApp::GetInstance(); app && FAILED(app->Get_UIManager()->Ready_Resources()))
        LOG_WARN(L"[Communication] Preload failed; communications stay disabled");

    if (FAILED(ClientSettingManager::GetInstance()->Load_Navigation_FromBinary())) {
        LOG_ERROR(L"Failed to Ready Load_Navigation_FromBinary");
        return E_FAIL;
    }
    else
    {
        NavigationManager::GetInstance()->Mapping_NavigationTable();
    }
    

    return S_OK;
}

Shared<Loader> Loader::Create(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context, LEVEL nextLevelID, const Shared<Level>& ownerLevel, Bool IsLoadStatic)
{
    Shared<Loader> loader = make_shared<Loader>(device, context);

    LOADER_DESC desc{};
    desc.isLoadStatic = IsLoadStatic;
    desc.nextLevelID = nextLevelID;
    desc.ownerLevel = ownerLevel;

    if (FAILED(loader->Initialize(&desc))) {
		MSG_BOX("Failed to Created : Loader");
        return nullptr;
    }

    return loader;
}
