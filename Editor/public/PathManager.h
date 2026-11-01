#pragma once

NS_BEGIN(Editor)

class PathManager {
  NO_COPY(PathManager)
public:
  static PathManager &GetInstance() {
    static PathManager instance;
    return instance;
  }
  void Initialize(const wstring &projectRoot = L"../../../") {
    m_ProjectRoot = projectRoot;
    m_ProjectSettingDir = m_ProjectRoot + L"ProjectSetting/";
    m_SceneSettingsDir = m_ProjectSettingDir + L"Scene/";
    m_PrefabSettingsDir = m_ProjectSettingDir + L"Prefab/";
	  m_AnimationPresetSettingsDir = m_ProjectSettingDir + L"AnimationPreset/";
	  m_MaterialSettingsDir = m_ProjectSettingDir + L"Material/";
    m_LayerSettingsPath = m_ProjectSettingDir + L"LayerSettings.json";
    m_TagSettingsPath = m_ProjectSettingDir + L"TagSettings.json";
    m_EngineDescSettingsPath = m_ProjectSettingDir + L"EngineDesc.json";
    m_NavDataDir = m_ProjectSettingDir + L"NavData/";
    m_AssetsDir = m_ProjectRoot + L"Assets/";
    m_ShadersDir = m_AssetsDir + L"Shaders/";
    m_TexturesDir = m_AssetsDir + L"Textures/";
    m_ModelsDir = m_AssetsDir + L"Models/";
	  m_ResourceDir = m_ProjectRoot + L"Client/bin/Resources/";
    m_ClientProjectDir = m_ProjectRoot + L"Client/";
    m_ClientPublicDir = m_ClientProjectDir + L"public/";
  }
  const wstring& GetProjectRoot() const { return m_ProjectRoot; }
  const wstring& GetProjectSettingDir() const { return m_ProjectSettingDir; }
  const wstring& GetSceneSettingsDir() const { return m_SceneSettingsDir; }
  const wstring& GetLayerSettingsPath() const { return m_LayerSettingsPath; }
  const wstring& GetTagSettingsPath() const { return m_TagSettingsPath; }
  const wstring& GetRenderSettingsPath() const { return m_EngineDescSettingsPath; }
  wstring GetLevelDataPath(uint32 levIndex) const {
      return m_SceneSettingsDir + L"LevelData_" + std::to_wstring(levIndex) + L".json";
  }

  const wstring& GetAssetsDir() const { return m_AssetsDir; }
  const wstring& GetShadersDir() const { return m_ShadersDir; }
  const wstring& GetTexturesDir() const { return m_TexturesDir; }
  const wstring& GetModelsDir() const { return m_ModelsDir; }
  const wstring& GetPrefabSettingsDir() const { return m_PrefabSettingsDir; }
	const wstring& GetAnimationPresetSettingsDir() const { return m_AnimationPresetSettingsDir; }
	const wstring& GetMaterialSettingsDir() const { return m_MaterialSettingsDir; }
	const wstring& GetResourceDir() const { return m_ResourceDir; }
  const wstring& GetNavDataDir() const { return m_NavDataDir; }
  const wstring& GetClientProjectDir() const { return m_ClientProjectDir; }
  const wstring& GetClientPublicDir() const { return m_ClientPublicDir; }
  void SetProjectRoot(const wstring &root) { Initialize(root); }

private:
  PathManager() = default;
  ~PathManager() = default;
  wstring m_ProjectRoot = L"../../../";
  wstring m_ProjectSettingDir = L"../../../ProjectSetting/";
  wstring m_SceneSettingsDir = L"../../../ProjectSetting/Scene/";

  wstring m_LayerSettingsPath = L"../../../ProjectSetting/LayerSettings.json";
  wstring m_TagSettingsPath = L"../../../ProjectSetting/TagSettings.json";
  wstring m_EngineDescSettingsPath = L"../../../ProjectSetting/EngineDesc.json";
  wstring m_AssetsDir = L"../../../Assets/";
  wstring m_ShadersDir = L"../../../Assets/Shaders/";
  wstring m_TexturesDir = L"../../../Assets/Textures/";
  wstring m_ModelsDir = L"../../../Assets/Models/";
  wstring m_PrefabSettingsDir = L"../../../ProjectSetting/Prefab/";
	wstring m_AnimationPresetSettingsDir = L"../../../ProjectSetting/AnimationPreset/";
	wstring m_MaterialSettingsDir = L"../../../ProjectSetting/Material/";
	wstring m_ResourceDir = L"../../../Client/bin/Resources/";
  wstring m_NavDataDir = L"../../../ProjectSetting/NavData/";
  wstring m_ClientProjectDir = L"../../../Client/";
  wstring m_ClientPublicDir = L"../../../Client/public/";
};
#define PATH Editor::PathManager::GetInstance()

NS_END
