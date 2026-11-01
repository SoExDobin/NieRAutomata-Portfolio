#include "pch.h"
#include "AnimationPresetEditor.h"

#include "EditorManager.h"
#include "Model.h"
#include "ModelViewer.h"
#include "PathManager.h"
#include "String_Helper.h"

using namespace Engine;

NS_BEGIN(Editor)

namespace
{
	Bool Is_SafeRelativeAnimationPath(const filesystem::path& path)
	{
		if (path.empty() || path.is_absolute() || path.extension() != L".anim")
			return false;
		return std::ranges::none_of(path, [](const filesystem::path& part) {
			return part == L".." || part == L".";
		});
	}

	Bool Is_ValidFileName(const string& name)
	{
		if (name.empty() || name == "." || name == ".." || name.back() == ' ' || name.back() == '.')
			return false;
		return std::ranges::none_of(name, [](Char ch) {
			return static_cast<unsigned char>(ch) < 0x20 || ch == '<' || ch == '>' || ch == ':' ||
				ch == '"' || ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*';
		});
	}

	Bool Find_AnimationEnum(const string& enumName, ReflectedEnumInfo& outEnum)
	{
		return SUCCEEDED(GAME_INSTANCE->Find_ReflectedEnum(enumName, outEnum)) &&
			outEnum.category == "AnimationState" && !outEnum.values.empty();
	}

	Bool Validate_Snapshot(const AnimationPresetSnapshot& preset, string& outError)
	{
		outError.clear();
		if (preset.schemaVersion != 2 || preset.animationEnum.empty() || preset.animations.empty())
		{
			outError = "Invalid AnimationPreset schema.";
			return false;
		}

		ReflectedEnumInfo enumInfo;
		if (!Find_AnimationEnum(preset.animationEnum, enumInfo))
		{
			outError = "Animation enum is not registered: " + preset.animationEnum;
			return false;
		}

		unordered_map<string, int64_t> enumValues;
		for (const ReflectedEnumValue& value : enumInfo.values)
		{
			if (value.name.empty() || value.value < 0 ||
				static_cast<size_t>(value.value) >= preset.animations.size() ||
				!enumValues.emplace(value.name, value.value).second)
			{
				outError = "Animation enum contains an invalid slot.";
				return false;
			}
		}

		unordered_set<wstring> paths;
		unordered_set<string> states;
		const filesystem::path resourceRoot = PATH.GetResourceDir();
		for (size_t index = 0; index < preset.animations.size(); ++index)
		{
			const AnimationPresetClip& clip = preset.animations[index];
			filesystem::path relativePath = filesystem::path(clip.relativePath).lexically_normal();
			wstring pathKey = relativePath.generic_wstring();
			std::ranges::transform(pathKey, pathKey.begin(), [](wchar_t ch) {
				return static_cast<wchar_t>(::towlower(ch));
			});
			if (!Is_SafeRelativeAnimationPath(relativePath) || !paths.emplace(pathKey).second ||
				!filesystem::is_regular_file(resourceRoot / relativePath))
			{
				outError = "Missing, unsafe, or duplicate animation path: " + Helper::To_String(clip.relativePath);
				return false;
			}
			for (const string& state : clip.states)
			{
				const auto stateIt = enumValues.find(state);
				if (stateIt == enumValues.end() || stateIt->second != static_cast<int64_t>(index) ||
					!states.emplace(state).second)
				{
					outError = "Animation State mapping is invalid: " + state;
					return false;
				}
			}
		}
		if (states.size() != enumValues.size())
		{
			outError = "AnimationPreset does not cover every Animation State.";
			return false;
		}
		return true;
	}

	Bool Load_AnimationPaths(const filesystem::path& manifestPath,
		vector<wstring>& outPaths, string& outError)
	{
		outPaths.clear();
		outError.clear();
		try
		{
			ifstream in(manifestPath);
			nlohmann::json manifest;
			in >> manifest;
			if (manifest.value("schemaVersion", 0) != 1 ||
				!manifest.contains("animations") || !manifest["animations"].is_array())
				throw runtime_error("Invalid animation manifest schema.");

			vector<wstring> pendingPaths;
			unordered_set<wstring> uniquePaths;
			const filesystem::path resourceRoot = PATH.GetResourceDir();
			for (size_t index = 0; index < manifest["animations"].size(); ++index)
			{
				const auto& animation = manifest["animations"][index];
				if (!animation.is_object() ||
					animation.value("index", numeric_limits<size_t>::max()) != index)
					throw runtime_error("Animation manifest index is not dense.");
				const string fileName = animation.value("file", string{});
				const filesystem::path fullPath =
					manifestPath.parent_path() / Helper::To_wString(fileName);
				std::error_code errorCode;
				const filesystem::path relativePath =
					filesystem::relative(fullPath, resourceRoot, errorCode);
				if (fileName.empty() || errorCode ||
					!Is_SafeRelativeAnimationPath(relativePath) ||
					!filesystem::is_regular_file(fullPath) ||
					!uniquePaths.emplace(relativePath.generic_wstring()).second)
					throw runtime_error("Animation manifest contains a missing or duplicate clip.");
				pendingPaths.push_back(relativePath.generic_wstring());
			}
			if (pendingPaths.empty())
				throw runtime_error("Animation manifest has no clips.");
			outPaths = std::move(pendingPaths);
			return true;
		}
		catch (const exception& exception)
		{
			outError = exception.what();
			return false;
		}
	}

	Bool Build_SnapshotFromPaths(const vector<wstring>& animationPaths,
		const string& enumName, AnimationPresetSnapshot& outPreset, string& outError)
	{
		outPreset = {};
		ReflectedEnumInfo enumInfo;
		if (!Find_AnimationEnum(enumName, enumInfo))
		{
			outError = "Animation enum is not registered: " + enumName;
			return false;
		}

		AnimationPresetSnapshot pending;
		pending.schemaVersion = 2;
		pending.animationEnum = enumName;
		pending.animations.reserve(animationPaths.size());
		for (const wstring& animationPath : animationPaths)
			pending.animations.push_back({ animationPath, {} });
		for (const ReflectedEnumValue& value : enumInfo.values)
		{
			if (value.value < 0 || static_cast<size_t>(value.value) >= pending.animations.size())
			{
				outError = "Animation enum contains an invalid slot.";
				return false;
			}
			pending.animations[static_cast<size_t>(value.value)].states.push_back(value.name);
		}
		if (!Validate_Snapshot(pending, outError))
			return false;
		outPreset = std::move(pending);
		return true;
	}
}

HRESULT AnimationPresetEditor::Initialize()
{
	m_Enable = false;
	Refresh_Sources();
	return EditorObject::Initialize();
}

void AnimationPresetEditor::Update(Bool isResize)
{
	if (m_Enable)
		EditorObject::Update(isResize);
}

void AnimationPresetEditor::Render(Bool isResize)
{
	if (!m_Enable)
		return;
	if (!ImGui::Begin("Animation Preset", &m_Enable))
	{
		ImGui::End();
		return;
	}

	ImGui::InputText("Preset Name", m_Name, sizeof(m_Name));
	ImGui::InputText("AssetGuid", m_AssetGuid, sizeof(m_AssetGuid), ImGuiInputTextFlags_ReadOnly);
	if (ImGui::Button("New")) New_Document();
	ImGui::SameLine();
	if (ImGui::Button("Save")) Save_Document();
	ImGui::SameLine();
	if (ImGui::Button("Delete")) ImGui::OpenPopup("Delete AnimationPreset?");
	ImGui::SameLine();
	if (ImGui::Button("Apply / Preview")) Apply_ToPreviewModel();
	ImGui::SameLine();
	if (ImGui::Button("Refresh")) Refresh_Sources();

	if (ImGui::BeginPopupModal("Delete AnimationPreset?", nullptr,
		ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (m_LoadedPath.empty())
			ImGui::TextWrapped("Load a preset before deleting it.");
		else
			ImGui::TextWrapped("Delete '%s'? Saved Scene/Prefab snapshots will remain unchanged.",
				Helper::To_String(m_LoadedPath.filename().wstring()).c_str());
		if (!m_LoadedPath.empty() && ImGui::Button("Delete Document"))
		{
			Delete_Document();
			ImGui::CloseCurrentPopup();
		}
		if (!m_LoadedPath.empty()) ImGui::SameLine();
		if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	const string enumPreview = m_AnimationEnum.empty() ? "<Select Animation State>" : m_AnimationEnum;
	if (ImGui::BeginCombo("Animation State", enumPreview.c_str()))
	{
		for (const ReflectedEnumInfo& enumInfo : GAME_INSTANCE->Get_ReflectedEnums("AnimationState"))
		{
			const Bool selected = enumInfo.registeredName == m_AnimationEnum;
			if (ImGui::Selectable(enumInfo.registeredName.c_str(), selected))
				Select_Enum(enumInfo.registeredName);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if (!m_Status.empty())
		ImGui::TextWrapped("%s", m_Status.c_str());

	if (ImGui::BeginTable("AnimationPresetLayout", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
		ImVec2(0.f, 430.f)))
	{
		ImGui::TableSetupColumn("Animation Sets");
		ImGui::TableSetupColumn("Animation State Slots");
		ImGui::TableSetupColumn("Saved Presets");
		ImGui::TableHeadersRow();
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		const filesystem::path resourceRoot = PATH.GetResourceDir();
		for (const filesystem::path& manifestPath : m_ManifestPaths)
		{
			std::error_code errorCode;
			const string label = Helper::To_String(
				filesystem::relative(manifestPath, resourceRoot, errorCode).generic_wstring());
			if (!errorCode && ImGui::Selectable(label.c_str(), false))
				Load_Manifest(manifestPath);
		}

		ImGui::TableSetColumnIndex(1);
		for (const ENUM_SLOT& slot : m_EnumSlots)
		{
			ImGui::PushID(slot.index);
			string stateLabel;
			for (const string& state : slot.names)
			{
				if (!stateLabel.empty()) stateLabel += " / ";
				stateLabel += state;
			}
			ImGui::Text("[%d] %s", slot.index, stateLabel.c_str());
			if (slot.index >= 0 && static_cast<size_t>(slot.index) < m_AnimationPaths.size())
			{
				const string current = Helper::To_String(
					filesystem::path(m_AnimationPaths[slot.index]).filename().wstring());
				if (ImGui::BeginCombo("##Clip", current.c_str()))
				{
					for (size_t candidate = 0; candidate < m_AnimationPaths.size(); ++candidate)
					{
						const string name = Helper::To_String(
							filesystem::path(m_AnimationPaths[candidate]).filename().wstring());
						const string candidateLabel = "[" + std::to_string(candidate) + "] " + name;
						if (ImGui::Selectable(candidateLabel.c_str(), candidate == static_cast<size_t>(slot.index)) &&
							candidate != static_cast<size_t>(slot.index))
							std::swap(m_AnimationPaths[slot.index], m_AnimationPaths[candidate]);
					}
					ImGui::EndCombo();
				}
			}
			else
				ImGui::TextColored(ImVec4(1.f, .35f, .35f, 1.f), "No clip for slot %d", slot.index);
			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::TableSetColumnIndex(2);
		for (const filesystem::path& presetPath : m_PresetPaths)
		{
			const Bool selected = presetPath == m_LoadedPath;
			const string label = Helper::To_String(presetPath.stem().wstring());
			if (ImGui::Selectable(label.c_str(), selected))
				Load_Document_ToEditor(presetPath);
		}
		ImGui::EndTable();
	}
	ImGui::End();
}

void AnimationPresetEditor::Refresh_Sources()
{
	m_ManifestPaths.clear();
	m_PresetPaths.clear();
	std::error_code errorCode;
	const filesystem::path resourceRoot = PATH.GetResourceDir();
	if (filesystem::exists(resourceRoot, errorCode))
	{
		for (filesystem::recursive_directory_iterator it(resourceRoot,
			filesystem::directory_options::skip_permission_denied, errorCode), end;
			it != end; it.increment(errorCode))
		{
			if (errorCode) { errorCode.clear(); continue; }
			if (it->is_regular_file() && it->path().filename() == L"manifest.json" &&
				it->path().parent_path().filename() == L"Animations")
				m_ManifestPaths.push_back(it->path());
		}
	}
	const filesystem::path presetRoot = PATH.GetAnimationPresetSettingsDir();
	errorCode.clear();
	if (filesystem::exists(presetRoot, errorCode))
	{
		for (filesystem::directory_iterator it(presetRoot,
			filesystem::directory_options::skip_permission_denied, errorCode), end;
			it != end; it.increment(errorCode))
		{
			if (errorCode) { errorCode.clear(); continue; }
			if (it->is_regular_file() && it->path().extension() == L".json")
				m_PresetPaths.push_back(it->path());
		}
	}
	std::ranges::sort(m_ManifestPaths);
	std::ranges::sort(m_PresetPaths);
}

void AnimationPresetEditor::Load_Manifest(const filesystem::path& manifestPath)
{
	string error;
	vector<wstring> pendingPaths;
	if (!Load_AnimationPaths(manifestPath, pendingPaths, error))
	{
		m_Status = "Failed to load animation set: " + error;
		return;
	}
	m_AnimationPaths = std::move(pendingPaths);
	m_Status = "Animation set loaded: " + manifestPath.parent_path().parent_path().filename().string();
}

void AnimationPresetEditor::Select_Enum(const string& enumName)
{
	m_AnimationEnum.clear();
	m_EnumSlots.clear();
	ReflectedEnumInfo enumInfo;
	if (!Find_AnimationEnum(enumName, enumInfo))
	{
		m_Status = "Animation enum is not registered: " + enumName;
		return;
	}
	map<int32, vector<string>> grouped;
	for (const ReflectedEnumValue& value : enumInfo.values)
	{
		if (value.value < 0 || value.value > numeric_limits<int32>::max())
		{
			m_Status = "Animation enum contains an invalid value.";
			return;
		}
		grouped[static_cast<int32>(value.value)].push_back(value.name);
	}
	for (auto& [index, names] : grouped)
		m_EnumSlots.push_back({ index, std::move(names) });
	m_AnimationEnum = enumName;
}

Bool AnimationPresetEditor::Build_Snapshot(AnimationPresetSnapshot& outPreset, string& outError) const
{
	return Build_SnapshotFromPaths(m_AnimationPaths, m_AnimationEnum, outPreset, outError);
}

Bool AnimationPresetEditor::Build_Snapshot(const filesystem::path& manifestPath,
	const string& enumName, AnimationPresetSnapshot& outPreset, string& outError)
{
	vector<wstring> animationPaths;
	return Load_AnimationPaths(manifestPath, animationPaths, outError) &&
		Build_SnapshotFromPaths(animationPaths, enumName, outPreset, outError);
}

void AnimationPresetEditor::New_Document()
{
	const AssetGuid guid = Create_AssetGuid();
	const string guidText = To_String(guid);
	strncpy_s(m_AssetGuid, guidText.c_str(), _TRUNCATE);
	m_Name[0] = '\0';
	m_AnimationEnum.clear();
	m_EnumSlots.clear();
	m_AnimationPaths.clear();
	m_LoadedPath.clear();
	m_Status = guid.Is_Valid() ? "New AnimationPreset created." : "Failed to create AssetGuid.";
}

void AnimationPresetEditor::Save_Document()
{
	const string name = m_Name;
	AssetGuid guid{};
	AnimationPresetSnapshot preset;
	string error;
	if (!Is_ValidFileName(name) || !Try_Parse_AssetGuid(m_AssetGuid, guid) ||
		!Build_Snapshot(preset, error))
	{
		m_Status = error.empty() ? "A valid name, AssetGuid, enum, and animation set are required." : error;
		return;
	}

	const filesystem::path directory = PATH.GetAnimationPresetSettingsDir();
	const filesystem::path target = directory / Helper::To_wString(name + ".json");
	std::error_code pathError;
	if (!m_LoadedPath.empty() &&
		(!filesystem::equivalent(m_LoadedPath, target, pathError) || pathError))
	{
		m_Status = "Renaming a loaded preset is not allowed; create a new preset instead.";
		return;
	}
	if (m_LoadedPath.empty() && filesystem::exists(target))
	{
		m_Status = "A preset with the same name already exists.";
		return;
	}

	for (const filesystem::path& candidate : m_PresetPaths)
	{
		if (!m_LoadedPath.empty())
		{
			pathError.clear();
			if (filesystem::equivalent(candidate, m_LoadedPath, pathError) && !pathError)
				continue;
		}
		try
		{
			ifstream in(candidate);
			nlohmann::json document;
			in >> document;
			AssetGuid other{};
			if (Try_Parse_AssetGuid(document.value("assetGuid", string{}), other) && other == guid)
			{
				m_Status = "Duplicate AnimationPreset AssetGuid.";
				return;
			}
		}
		catch (...) {}
	}

	if (!Save_Document(target, guid, name, preset, error))
	{
		m_Status = error;
		return;
	}
	m_LoadedPath = target;
	m_Status = "AnimationPreset saved atomically.";
	Refresh_Sources();
}

Bool AnimationPresetEditor::Save_Document(const filesystem::path& targetPath,
	AssetGuid assetGuid, const string& name, const AnimationPresetSnapshot& preset,
	string& outError)
{
	outError.clear();
	if (targetPath.empty() || !assetGuid.Is_Valid() || !Is_ValidFileName(name) ||
		!Validate_Snapshot(preset, outError))
	{
		if (outError.empty())
			outError = "A valid target, name, AssetGuid, enum, and animation set are required.";
		return false;
	}
	nlohmann::json document = {
		{ "schemaVersion", 2 },
		{ "assetGuid", To_String(assetGuid) },
		{ "name", name },
		{ "animationEnum", preset.animationEnum },
		{ "animations", nlohmann::json::array() }
	};
	for (size_t index = 0; index < preset.animations.size(); ++index)
	{
		document["animations"].push_back({
			{ "index", index },
			{ "path", Helper::To_String(preset.animations[index].relativePath) },
			{ "states", preset.animations[index].states }
		});
	}

	std::error_code errorCode;
	filesystem::create_directories(targetPath.parent_path(), errorCode);
	if (errorCode)
	{
		outError = "Failed to create AnimationPreset directory.";
		return false;
	}
	const filesystem::path temporary = targetPath.wstring() + L".tmp";
	{
		ofstream out(temporary, ios::binary | ios::trunc);
		if (!out.is_open())
		{
			outError = "Failed to open the temporary preset file.";
			return false;
		}
		out << document.dump(4);
		out.flush();
		if (!out.good())
		{
			outError = "Failed while writing the temporary preset file.";
			return false;
		}
	}
	if (!MoveFileExW(temporary.c_str(), targetPath.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		filesystem::remove(temporary, errorCode);
		outError = "Failed to atomically replace the AnimationPreset file.";
		return false;
	}
	return true;
}

Bool AnimationPresetEditor::Load_Document(const filesystem::path& presetPath,
	AnimationPresetSnapshot& outPreset, string& outName, string& outError)
{
	outPreset = {};
	outName.clear();
	outError.clear();
	try
	{
		ifstream in(presetPath);
		if (!in.is_open())
			throw runtime_error("Unable to open preset file.");
		nlohmann::json document;
		in >> document;
		AssetGuid guid{};
		if (document.value("schemaVersion", 0) != 2 ||
			!Try_Parse_AssetGuid(document.value("assetGuid", string{}), guid) ||
			!document.contains("animations") || !document["animations"].is_array())
			throw runtime_error("Invalid AnimationPreset schema.");

		AnimationPresetSnapshot pending;
		pending.schemaVersion = 2;
		pending.animationEnum = document.value("animationEnum", string{});
		for (size_t index = 0; index < document["animations"].size(); ++index)
		{
			const auto& animation = document["animations"][index];
			if (!animation.is_object() || animation.value("index", numeric_limits<size_t>::max()) != index ||
				!animation.contains("path") || !animation["path"].is_string() ||
				!animation.contains("states") || !animation["states"].is_array())
				throw runtime_error("AnimationPreset clip entries are invalid.");
			AnimationPresetClip clip;
			clip.relativePath = Helper::To_wString(animation["path"].get<string>());
			for (const auto& state : animation["states"])
			{
				if (!state.is_string())
					throw runtime_error("AnimationPreset state entry is invalid.");
				clip.states.push_back(state.get<string>());
			}
			pending.animations.push_back(std::move(clip));
		}
		if (!Validate_Snapshot(pending, outError))
			return false;
		outName = document.value("name", presetPath.stem().string());
		if (!Is_ValidFileName(outName))
		{
			outError = "AnimationPreset name is invalid.";
			return false;
		}
		outPreset = std::move(pending);
		return true;
	}
	catch (const exception& exception)
	{
		outError = exception.what();
		return false;
	}
}

void AnimationPresetEditor::Load_Document_ToEditor(const filesystem::path& presetPath)
{
	AnimationPresetSnapshot preset;
	string name;
	string error;
	if (!Load_Document(presetPath, preset, name, error))
	{
		m_Status = "Failed to load AnimationPreset: " + error;
		return;
	}
	try
	{
		ifstream in(presetPath);
		nlohmann::json document;
		in >> document;
		const string guid = document.value("assetGuid", string{});
		strncpy_s(m_Name, name.c_str(), _TRUNCATE);
		strncpy_s(m_AssetGuid, guid.c_str(), _TRUNCATE);
		Select_Enum(preset.animationEnum);
		m_AnimationPaths.clear();
		for (const AnimationPresetClip& clip : preset.animations)
			m_AnimationPaths.push_back(clip.relativePath);
		m_LoadedPath = presetPath;
		m_Status = "AnimationPreset loaded without partial state changes.";
	}
	catch (const exception& exception)
	{
		m_Status = string("Failed to load AnimationPreset: ") + exception.what();
	}
}

HRESULT AnimationPresetEditor::Apply_PresetFile(Object& model,
	const filesystem::path& presetPath, string& outError)
{
	AnimationPresetSnapshot preset;
	string name;
	if (!Load_Document(presetPath, preset, name, outError))
		return E_FAIL;
	Model* targetModel = dynamic_cast<Model*>(&model);
	const HRESULT result = targetModel
		? targetModel->Apply_AnimationPreset(preset)
		: E_INVALIDARG;
	if (FAILED(result) && outError.empty())
		outError = "The preset is incompatible with the selected model skeleton.";
	return result;
}

void AnimationPresetEditor::Apply_ToPreviewModel()
{
	AnimationPresetSnapshot preset;
	string error;
	if (!Build_Snapshot(preset, error))
	{
		m_Status = error;
		return;
	}
	const Shared<Model> model = EDITOR->Get_ModelViewer()->Get_SelectedModel();
	if (!model)
	{
		m_Status = "Select a model in Model Viewer first.";
		return;
	}
	m_Status = SUCCEEDED(model->Apply_AnimationPreset(preset))
		? "AnimationPreset applied to the preview model."
		: "AnimationPreset is incompatible with the preview model.";
}

void AnimationPresetEditor::Delete_Document()
{
	if (m_LoadedPath.empty())
	{
		m_Status = "Load a preset before deleting it.";
		return;
	}
	std::error_code errorCode;
	if (!filesystem::remove(m_LoadedPath, errorCode) || errorCode)
	{
		m_Status = "Failed to delete the AnimationPreset document.";
		return;
	}
	m_LoadedPath.clear();
	m_Status = "AnimationPreset authoring document deleted; saved snapshots are unchanged.";
	Refresh_Sources();
}

Shared<AnimationPresetEditor> AnimationPresetEditor::Create()
{
	auto editor = make_shared<AnimationPresetEditor>();
	return SUCCEEDED(editor->Initialize()) ? editor : nullptr;
}

NS_END
