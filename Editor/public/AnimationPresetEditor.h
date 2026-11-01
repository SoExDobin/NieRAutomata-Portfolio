#pragma once

#include "EditorObject.h"
#include "Engine_Reflection.h"
#include <filesystem>

NS_BEGIN(Engine)
class Object;
NS_END

NS_BEGIN(Editor)

class AnimationPresetEditor final : public EditorObject
{
public:
	AnimationPresetEditor() = default;
	~AnimationPresetEditor() override = default;

	HRESULT Initialize() override;
	void Update(Bool isResize) override;
	void Render(Bool isResize) override;

	static Bool Load_Document(const std::filesystem::path& presetPath,
		Engine::AnimationPresetSnapshot& outPreset, string& outName, string& outError);
	static Bool Build_Snapshot(const std::filesystem::path& manifestPath,
		const string& enumName, Engine::AnimationPresetSnapshot& outPreset, string& outError);
	static Bool Save_Document(const std::filesystem::path& targetPath, AssetGuid assetGuid,
		const string& name, const Engine::AnimationPresetSnapshot& preset, string& outError);
	static HRESULT Apply_PresetFile(Engine::Object& model,
		const std::filesystem::path& presetPath, string& outError);

private:
	struct ENUM_SLOT final
	{
		int32 index{};
		vector<string> names;
	};

	void Refresh_Sources();
	void Load_Manifest(const std::filesystem::path& manifestPath);
	void Select_Enum(const string& enumName);
	void New_Document();
	void Save_Document();
	void Load_Document_ToEditor(const std::filesystem::path& presetPath);
	void Delete_Document();
	void Apply_ToPreviewModel();
	Bool Build_Snapshot(Engine::AnimationPresetSnapshot& outPreset, string& outError) const;

private:
	Char m_Name[128]{};
	Char m_AssetGuid[33]{};
	string m_AnimationEnum;
	vector<ENUM_SLOT> m_EnumSlots;
	vector<wstring> m_AnimationPaths;
	vector<std::filesystem::path> m_ManifestPaths;
	vector<std::filesystem::path> m_PresetPaths;
	std::filesystem::path m_LoadedPath;
	string m_Status;

public:
	static Shared<AnimationPresetEditor> Create();
};

NS_END
