#pragma once
#include "EditorObject.h"
#include <filesystem>

NS_BEGIN(Engine)
class Object;
class GameObject;
class Component;
NS_END

NS_BEGIN(Editor)

class InspectorTransform;

class Inspector : public EditorObject
{
public:
	Inspector();
	~Inspector() override;

public:
	HRESULT Initialize() override;
	void Render(Bool isResize) override;

public:
	void Render_ProjectSettingsWindow();

private:
	void Render_PrefabEdit(const Shared<Engine::GameObject>& selected);
	void GameObjectGUI(const Shared<Engine::GameObject>& obj);
	void Draw_GameObjectHeader(const Shared<Engine::GameObject>& obj);
	void RenderGenericProperties(const Shared<Engine::GameObject>& owner,
		Engine::Object& object);
	Bool Render_AnimationPreset(const Char* label, Engine::AnimationPresetSnapshot& preset);
	Bool Render_TagCountList(const Char* label, std::string_view assetType, vector<Engine::TagCount>& list);

private:
    Shared<InspectorTransform> m_InspectorTransform{ nullptr };
	ObjectGuid m_PreviousSelectedGuid{};
	std::filesystem::path m_SelectedPresetPath;
	string m_PresetStatus;

private:
	char m_LayerNames[32][64] = { 0 };
	char m_TagNames[32][64] = { 0 };
	Bool m_IsFirstLoadSettings = { true };

public:
	static Shared<Inspector> Create();
};

NS_END
