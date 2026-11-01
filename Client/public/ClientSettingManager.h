#pragma once

#include "Engine_Define.h"
#include "Engine_Reflection.h"

NS_BEGIN(Client)

class CLIENT_DLL ClientSettingManager
{
	DECLARE_SINGLETON(ClientSettingManager)
	ClientSettingManager() = default;
	~ClientSettingManager() = default;

public:
	uint32 Get_LevelCount() const { return ETOI(LEVEL::LEVEL_END); }

public: /* Project Path */
	void Set_ResourcePath(const wstring& path) { m_ResourcePath = path; }
	void Set_ShaderPath(const wstring& path) { m_ShaderPath = path; }

public: /* Texture */
	HRESULT Load_Textures_FromJson(LEVEL level) const;
	HRESULT Sync_TextureJson_FromExcel() const;

public: /* Model */
	HRESULT Load_Model_FromJson(LEVEL level) const;
	HRESULT Load_AnimationPreset(const wstring& modelTag, Engine::AnimationPresetSnapshot& outPreset) const;
	HRESULT Sync_ModelJson_FromExcel() const;

public: /* Shader */
	HRESULT Load_Shader() const;

public: /* Sound */
	HRESULT Load_Sound_FromJson() const;
	HRESULT Sync_SoundJson_FromCSV() const;

public: /* Navigation */
	HRESULT Load_Navigation_FromBinary() const;

public:
	HRESULT Load_EngineDesc(ENGINE_DESC& outDesc) const;
	HRESULT Apply_LayerAndTagSettings() const;
	HRESULT Load_LevelData(LEVEL level) const;
	
public:
	Bool AutoTransitionLevel(LEVEL curLevel, LEVEL nextLev);
	
private:
	wstring m_ResourcePath = L"../../../Client/bin/resources/";
	wstring m_ShaderPath = L"shaders/";
	wstring m_ProjectSettingPath = L"../../../ProjectSetting/";

public:
	static ENGINE_DESC g_EngineDesc;
};

NS_END
