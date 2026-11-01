#include "pch.h"
#include "Editor_Define.h"
#include "Inspector.h"
#include "EditorManager.h"
#include "Component.h"
#include "GameObject.h"
#include "Game.h"
#include "LayerRegistry.h"
#include "PathManager.h"
#include "TagRegistry.h"
#include "Transform.h"
#include "Engine_RTTR_Metadata.h"
#include "InspectorTransform.h"
#include "AnimationPresetEditor.h"
#include "Model.h"

using namespace Engine;
using namespace Editor;

namespace
{
	// Loaded resources a ResourceTag property can name: Static (level 0) and the current level.
	vector<wstring> Collect_AssetTags(std::string_view assetType)
	{
		vector<uint32> levels{ 0 };
		if (const uint32 currentLevel = GAME_INSTANCE->Get_CurrentLevelIndex(); currentLevel != 0)
			levels.push_back(currentLevel);

		vector<wstring> tags;
		for (const uint32 level : levels) {
			if (assetType == Asset_Type_Key::Texture) {
				for (const wstring& tag : GAME_INSTANCE->Get_TextureTags(level))
					tags.push_back(tag);
			}
			else if (assetType == Asset_Type_Key::Model) {
				for (const Shared<Model>& model : GAME_INSTANCE->Get_Models(level))
					if (model)
						tags.push_back(model->Get_ModelTag());
			}
		}
		// Prefab documents are project-wide, not per level.
		if (assetType == Asset_Type_Key::Prefab)
			for (const auto& [prefabGuid, path] : GAME_INSTANCE->Get_PrefabDocuments())
				tags.push_back(std::filesystem::path(path).stem().wstring());
		return tags;
	}

	// Drop target on the last item for an Asset Browser item ("Type|Tag") of assetType; returns its tag.
	optional<wstring> Accept_AssetDrop(std::string_view assetType)
	{
		optional<wstring> dropped;
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ITEM")) {
				const string item = static_cast<const Char*>(payload->Data);
				const size_t delimiter = item.find('|');
				if (delimiter != string::npos && std::string_view{ item }.substr(0, delimiter) == assetType)
					dropped = Helper::To_wString(item.substr(delimiter + 1));
			}
			ImGui::EndDragDropTarget();
		}
		return dropped;
	}

	// ResourceTag property: pick a loaded resource, or drop an Asset Browser item ("Type|Tag") of the property's AssetType.
	Bool Render_AssetTag(const Char* label, std::string_view assetType, wstring& tag)
	{
		Bool changed = false;
		const string current = Helper::To_String(tag);
		if (ImGui::BeginCombo(label, current.empty() ? "(None)" : current.c_str())) {
			if (ImGui::Selectable("(None)", tag.empty())) {
				tag.clear();
				changed = true;
			}
			int32 index = 0;
			for (const wstring& candidate : Collect_AssetTags(assetType)) {
				ImGui::PushID(index++);
				if (ImGui::Selectable(Helper::To_String(candidate).c_str(), candidate == tag)) {
					tag = candidate;
					changed = true;
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		if (const optional<wstring> dropped = Accept_AssetDrop(assetType)) {
			tag = *dropped;
			changed = true;
		}
		return changed;
	}
}

Inspector::Inspector() {}
Inspector::~Inspector() {}

HRESULT Inspector::Initialize() {
    auto layerRegistry = GAME_INSTANCE->Get_LayerRegister();
    auto tagRegistry = GAME_INSTANCE->Get_TagRegister();

    layerRegistry->LoadFromFile(PATH.GetLayerSettingsPath());
    tagRegistry->LoadFromFile(PATH.GetTagSettingsPath());

    m_InspectorTransform = InspectorTransform::Create();

    return EditorObject::Initialize();
}

void Inspector::Render(Bool isResize) {
    ImGui::Begin("Inspector");

    Render_ProjectSettingsWindow();

    Shared<GameObject> selected = EDITOR->Get_SelectedObject();
    if (!selected) {
		m_PreviousSelectedGuid = {};
		ImGui::TextDisabled("No object selected.");
		ImGui::End();
		return;
    }

    if (m_PreviousSelectedGuid != selected->Get_ObjectGuid()) {
        m_PreviousSelectedGuid = selected->Get_ObjectGuid();
    }

    Render_PrefabEdit(selected);
    GameObjectGUI(selected);

    ImGui::End();
}

// Banner for an Asset Browser edit copy (the selection or one of its ancestors): Apply writes it over the
// Prefab document, Close removes the copy (Undo brings it back). The level cannot be saved while it is open.
void Inspector::Render_PrefabEdit(const Shared<GameObject>& selected)
{
    Shared<GameObject> root = selected;
    while (root->Has_Parent())
        root = root->Get_Parent();
    const PrefabGuid prefabGuid = EDITOR->Get_PrefabEdit(root);
    if (!prefabGuid.Is_Valid())
        return;

    const string name = Helper::To_String(std::filesystem::path(GAME_INSTANCE->Find_PrefabPath(prefabGuid)).stem().wstring());
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.f, 1.f), "Editing Prefab: %s", name.c_str());
    ImGui::BeginDisabled(EDITOR->Get_State() != EDITOR_STATE::STOP);
    if (ImGui::Button("Apply to Prefab"))
        EDITOR->Apply_PrefabEdit(root);
    ImGui::SameLine();
    if (ImGui::Button("Close"))
        EDITOR->Queue_Destroy(root->Get_ObjectGuid(), root->Get_LevelIndex());
    ImGui::EndDisabled();
    ImGui::TextDisabled("Level save and Play wait until this copy is applied or closed.");
    ImGui::Separator();
}

void Inspector::Render_ProjectSettingsWindow()
{
    auto layerReg = GAME_INSTANCE->Get_LayerRegister();
    auto tagReg = GAME_INSTANCE->Get_TagRegister();
    // JSON 원본 문자열을 UI 버퍼에 단 한 번만 복사
    if (m_IsFirstLoadSettings)
    {
        for (const auto& pair : layerReg->Get_AllLayers()) {
            uint32 bit = static_cast<uint32>(pair.first);
            int index = 0; while ((bit >>= 1) > 0) index++;
            strcpy_s(m_LayerNames[index], 64, Helper::To_String(pair.second).c_str());
        }
        for (const auto& pair : tagReg->Get_AllTags()) {
            uint32 bit = static_cast<uint32>(pair.first);
            int index = 0; while ((bit >>= 1) > 0) index++;
            strcpy_s(m_TagNames[index], 64, Helper::To_String(pair.second).c_str());
        }
        m_IsFirstLoadSettings = false;
    }
    // ⭐️ 헤더(드롭다운)로 묶어서 공간 낭비 방지 ⭐️
    if (ImGui::CollapsingHeader("Global Layer & Tag Settings", ImGuiTreeNodeFlags_None))
    {
        // ⭐️ 스크롤을 내려도 항상 콤보박스(헤더) 바로 아래에서 세이브가 가능하도록 상단에 버튼 고정 ⭐️
        ImGui::Columns(2, "SaveButtons", false);
        if (ImGui::Button("Save Layers JSON", ImVec2(-1, 0))) layerReg->SaveToFile(PATH.GetLayerSettingsPath());
        ImGui::NextColumn();
        if (ImGui::Button("Save Tags JSON", ImVec2(-1, 0))) tagReg->SaveToFile(PATH.GetTagSettingsPath());
        ImGui::Columns(1);

        ImGui::Separator();
        // ⭐️ 오버플로우 방지 및 스크롤 영역 (높이 250) ⭐️
        ImGui::BeginChild("SettingsScrollArea", ImVec2(0, 250), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        // ⭐️ 가독성 개선: 좌(Layer) / 우(Tag) 양분 ⭐️
        ImGui::Columns(2, "LayerTagColumns", true); // 리사이즈 가능 선(true)
        // [왼쪽 컬럼: Layers]
        ImGui::TextDisabled("=== Layers ===");
        for (int i = 0; i < 32; ++i)
        {
            ImGui::Text("L%02d", i);
            ImGui::SameLine(35.f); // 텍스트박스 시작 지점을 가지런하게 맞춤
            ImGui::PushItemWidth(-1.f); // 텍스트박스를 끝까지 채움
            string label = "##Layer" + std::to_string(i);
            if (ImGui::InputText(label.c_str(), m_LayerNames[i], 64))
                layerReg->Set_LayerName(1 << i, Helper::To_wString(m_LayerNames[i]));
            ImGui::PopItemWidth();
        }
        ImGui::NextColumn(); // 다음 열로 이동
        // [오른쪽 컬럼: Tags]
        ImGui::TextDisabled("=== Tags ===");
        for (int i = 0; i < 32; ++i)
        {
            ImGui::Text("T%02d", i);
            ImGui::SameLine(35.f);
            ImGui::PushItemWidth(-1.f);
            string label = "##Tag" + std::to_string(i);
            if (ImGui::InputText(label.c_str(), m_TagNames[i], 64))
                tagReg->Set_TagName(1 << i, Helper::To_wString(m_TagNames[i]));
            ImGui::PopItemWidth();
        }
        // 범위 초과(넘치면) 자동 스크롤(슬라이더) 제공 구역 종료
        ImGui::Columns(1);
        ImGui::EndChild();
    }
    // 인스펙터 하단 오브젝트 정보와 분리선 출력
    ImGui::Separator();
}

// ====================================================
// GameObjectGUI: 선택 오브젝트의 이름・Layer/Tag・Transform・컴포넌트 표시
// ====================================================
void Inspector::GameObjectGUI(const Shared<GameObject>& obj) {
    Draw_GameObjectHeader(obj);
    ImGui::Separator();

    // Transform 렌더링
    if (m_InspectorTransform) {
        m_InspectorTransform->RenderComponent(obj->Get_Transform());
    }

    ImGui::Separator();

	if (!GAME_INSTANCE->Get_ReflectedProperties(*obj).empty()) {
		if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
			RenderGenericProperties(obj, *obj);
		ImGui::Separator();
	}

    // Transform 외 컴포넌트: 등록된 property와 메타데이터로 그린다.
    const auto& components = obj->Get_Components();
    if (!components.empty()) {
        size_t count = 0;
        for (auto& pComp : components) {
            if (pComp && pComp->Get_ComponentType() != COMPONENT_TYPE::TRANSFORM)
                ++count;
        }
        ImGui::Text("Components (%zu)", count);

        for (auto& pComp : components) {
            // Transform은 위에서 그렸으니 패스
            if (!pComp || pComp->Get_ComponentType() == COMPONENT_TYPE::TRANSFORM)
                continue;

            string typeName = GAME_INSTANCE->Find_RegisteredName(pComp->Get_RuntimeTypeId());
			if (typeName.empty())
				typeName = "<Unregistered Component>";
            if (ImGui::CollapsingHeader(typeName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
				RenderGenericProperties(obj, *pComp);
            }
        }
    }

    ImGui::Separator();

    // Scripts 섹션
    const auto& scripts = obj->Get_Scripts();
    if (!scripts.empty()) {
        ImGui::Text("Scripts (%zu)", scripts.size());
        for (auto& pScript : scripts) {
            if (!pScript) continue;
            string typeName = GAME_INSTANCE->Find_RegisteredName(pScript->Get_RuntimeTypeId());
			if (typeName.empty())
				typeName = "<Unregistered Script>";
            if (ImGui::CollapsingHeader(typeName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
				RenderGenericProperties(obj, *pScript);
            }
        }
    }
}

void Inspector::RenderGenericProperties(const Shared<GameObject>& owner, Object& object)
{
	const vector<ReflectedPropertyInfo> properties =
		GAME_INSTANCE->Get_ReflectedProperties(object);
	if (properties.empty()) {
		ImGui::TextDisabled("No reflected properties.");
		return;
	}

	// Components on one object share property names (Offset, Color...), so each object gets its own ID scope.
	ImGui::PushID(&object);
	for (const ReflectedPropertyInfo& property : properties) {
		ReflectionValue value;
		if (FAILED(GAME_INSTANCE->Read_ReflectedProperty(
			object, property.registeredName, value))) {
			ImGui::TextDisabled("%s: unavailable", property.registeredName.c_str());
			continue;
		}
		const ReflectionValue beforeValue = value;

		// DisplayName only changes the label; the ID, writes and saves keep the registered name.
		const Char* label = property.displayName.empty()
			? property.registeredName.c_str()
			: property.displayName.c_str();
		const Bool hasRange = property.minValue || property.maxValue;
		const Float minValue = property.minValue.value_or(-FLT_MAX);
		const Float maxValue = property.maxValue.value_or(FLT_MAX);
		const ImGuiSliderFlags rangeFlags = hasRange ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None;

		ImGui::PushID(property.registeredName.c_str());
		ImGui::BeginDisabled(!property.isWritable);
		Bool changed = false;
		switch (property.valueType) {
		case REFLECTION_VALUE_TYPE::BOOL:
			if (Bool* typed = value.Try_Get<Bool>())
				changed = ImGui::Checkbox(label, typed);
			break;
		case REFLECTION_VALUE_TYPE::INT32:
			if (int32* typed = value.Try_Get<int32>()) {
				if (hasRange) {
					const int32 minInt = property.minValue ? static_cast<int32>(*property.minValue) : INT32_MIN;
					const int32 maxInt = property.maxValue ? static_cast<int32>(*property.maxValue) : INT32_MAX;
					changed = ImGui::DragInt(label, typed, 1.f, minInt, maxInt, "%d", rangeFlags);
				}
				else {
					changed = ImGui::InputInt(label, typed);
				}
			}
			break;
		case REFLECTION_VALUE_TYPE::UINT32:
			if (uint32* typed = value.Try_Get<uint32>()) {
				if (property.saveDataKey == Save_Data_Key::UIAnchor) {
					static constexpr array<const Char*, 9> AnchorNames = {
						"Top Left", "Top Center", "Top Right",
						"Center Left", "Center", "Center Right",
						"Bottom Left", "Bottom Center", "Bottom Right"
					};
					const Char* preview = *typed < AnchorNames.size()
						? AnchorNames[*typed]
						: "Invalid";
					if (ImGui::BeginCombo(label, preview)) {
						for (uint32 index = 0; index < AnchorNames.size(); ++index) {
							const Bool selected = *typed == index;
							if (ImGui::Selectable(AnchorNames[index], selected)) {
								*typed = index;
								changed = true;
							}
							if (selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
				else if (hasRange) {
					const uint32 minUint = property.minValue
						? static_cast<uint32>((std::max)(*property.minValue, 0.f)) : 0u;
					const uint32 maxUint = property.maxValue
						? static_cast<uint32>((std::max)(*property.maxValue, 0.f)) : UINT32_MAX;
					changed = ImGui::DragScalar(label, ImGuiDataType_U32, typed, 1.f,
						&minUint, &maxUint, "%u", rangeFlags);
				}
				else {
					changed = ImGui::InputScalar(label, ImGuiDataType_U32, typed);
				}
			}
			break;
		case REFLECTION_VALUE_TYPE::FLOAT:
			if (Float* typed = value.Try_Get<Float>()) {
				if (property.dataTag == Data_Tag::Angle)
					// Stored in radians, edited in degrees.
					changed = ImGui::SliderAngle(label, typed,
						property.minValue ? XMConvertToDegrees(*property.minValue) : -360.f,
						property.maxValue ? XMConvertToDegrees(*property.maxValue) : 360.f,
						"%.1f deg", rangeFlags);
				else
					changed = hasRange
						? ImGui::DragFloat(label, typed, 0.01f, minValue, maxValue, "%.3f", rangeFlags)
						: ImGui::DragFloat(label, typed, 0.01f);
			}
			break;
		case REFLECTION_VALUE_TYPE::DOUBLE:
			if (Double* typed = value.Try_Get<Double>()) {
				changed = ImGui::InputDouble(label, typed);
				if (changed && hasRange)
					*typed = std::clamp(*typed, static_cast<Double>(minValue), static_cast<Double>(maxValue));
			}
			break;
		case REFLECTION_VALUE_TYPE::STRING:
			if (string* typed = value.Try_Get<string>()) {
				Char buffer[256]{};
				strncpy_s(buffer, typed->c_str(), _TRUNCATE);
				if (ImGui::InputText(label, buffer, sizeof(buffer))) {
					*typed = buffer;
					changed = true;
				}
			}
			break;
		case REFLECTION_VALUE_TYPE::WSTRING:
			if (wstring* typed = value.Try_Get<wstring>();
				typed && property.dataTag == Data_Tag::ResourceTag) {
				changed = Render_AssetTag(label, property.assetType, *typed);
			}
			else if (typed) {
				Char buffer[256]{};
				const string utf8 = Helper::To_String(*typed);
				strncpy_s(buffer, utf8.c_str(), _TRUNCATE);
				if (ImGui::InputText(label, buffer, sizeof(buffer))) {
					*typed = Helper::To_wString(buffer);
					changed = true;
				}
			}
			break;
		case REFLECTION_VALUE_TYPE::VECTOR3:
			if (Vector3* typed = value.Try_Get<Vector3>())
				changed = ImGui::DragFloat3(label, reinterpret_cast<Float*>(typed), 0.01f,
					hasRange ? minValue : 0.f, hasRange ? maxValue : 0.f, "%.3f", rangeFlags);
			break;
		case REFLECTION_VALUE_TYPE::FLOAT3:
			if (Float3* typed = value.Try_Get<Float3>())
				changed = ImGui::DragFloat3(label, reinterpret_cast<Float*>(typed), 0.01f,
					hasRange ? minValue : 0.f, hasRange ? maxValue : 0.f, "%.3f", rangeFlags);
			break;
		case REFLECTION_VALUE_TYPE::COLOR:
			if (Color* typed = value.Try_Get<Color>())
				// HDR: a light color above 1 is its intensity (a sun brighter than white).
				changed = ImGui::ColorEdit4(label,
					reinterpret_cast<Float*>(typed), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
			break;
		case REFLECTION_VALUE_TYPE::FLOAT4:
			if (Float4* typed = value.Try_Get<Float4>())
				changed = ImGui::DragFloat4(label, reinterpret_cast<Float*>(typed), 0.01f,
					hasRange ? minValue : 0.f, hasRange ? maxValue : 0.f, "%.3f", rangeFlags);
			break;
		case REFLECTION_VALUE_TYPE::ANIMATION_PRESET:
			if (AnimationPresetSnapshot* typed = value.Try_Get<AnimationPresetSnapshot>())
				changed = Render_AnimationPreset(label, *typed);
			break;
		case REFLECTION_VALUE_TYPE::TAG_COUNT_LIST:
			if (vector<TagCount>* typed = value.Try_Get<vector<TagCount>>())
				changed = Render_TagCountList(label, property.assetType, *typed);
			break;
		case REFLECTION_VALUE_TYPE::OBJECT_REF:
			if (ObjectGuid* typed = value.Try_Get<ObjectGuid>()) {
				const Shared<GameObject> target = typed->Is_Valid()
					? GAME_INSTANCE->Find(*typed)
					: nullptr;
				string preview = "None";
				if (target)
					preview = Helper::To_String(target->Get_Name());
				else if (typed->Is_Valid())
					preview = "Missing (" + To_String(*typed) + ")";

				ImGui::Text("%s", label);
				if (!property.expectedBaseRegisteredName.empty()) {
					ImGui::SameLine();
					ImGui::TextDisabled("(%s)", property.expectedBaseRegisteredName.c_str());
				}
				const string fieldId = preview + "##ObjectRef";
				ImGui::Selectable(fieldId.c_str(), false, ImGuiSelectableFlags_None,
					ImVec2(ImGui::GetContentRegionAvail().x - 35.f, 25.f));
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
						ObjectMove_PayLoadKey.c_str())) {
						if (payload->DataSize == sizeof(OBJECT_MOVE_PAYLOAD)) {
							const auto& dropped = *static_cast<const OBJECT_MOVE_PAYLOAD*>(payload->Data);
							if (GAME_INSTANCE->Find(dropped.objectGuid)) {
								*typed = dropped.objectGuid;
								changed = true;
							}
						}
					}
					ImGui::EndDragDropTarget();
				}
				ImGui::SameLine();
				if (ImGui::Button("X##ObjectRef", ImVec2(30.f, 25.f))) {
					*typed = {};
					changed = true;
				}
			}
			break;
		default:
			ImGui::TextDisabled("%s: unsupported value type", label);
			break;
		}
		ImGui::EndDisabled();

		if (changed && property.isWritable)
			EDITOR->Queue_PropertyWrite(owner, object, property.registeredName,
				beforeValue, value,
				ImGui::IsItemActivated() || !ImGui::IsItemActive());
		ImGui::PopID();
	}
	ImGui::PopID();
}

// One row per tag: pick the tag from the property's AssetType, set its count, remove it. Every edit is a single
// step (combo pick, +/- or Enter), so each one is its own Undo entry. A tag already in the list is not offered
// again, and Add takes the first unused one, so the list never holds an empty or repeated tag.
Bool Inspector::Render_TagCountList(const Char* label, std::string_view assetType, vector<TagCount>& list)
{
	Bool changed = false;
	const vector<wstring> candidates = Collect_AssetTags(assetType);
	const auto IsUsed = [&list](const wstring& tag) {
		return std::ranges::any_of(list, [&tag](const TagCount& entry) { return entry.tag == tag; });
	};

	ImGui::Text("%s", label);
	for (size_t row = 0; row < list.size(); ++row) {
		ImGui::PushID(static_cast<int32>(row));
		TagCount& entry = list[row];
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
		const string current = Helper::To_String(entry.tag);
		if (ImGui::BeginCombo("##Tag", current.c_str())) {
			for (const wstring& candidate : candidates) {
				const Bool isCurrent = candidate == entry.tag;
				ImGui::BeginDisabled(!isCurrent && IsUsed(candidate));
				if (ImGui::Selectable(Helper::To_String(candidate).c_str(), isCurrent) && !isCurrent) {
					entry.tag = candidate;
					changed = true;
				}
				ImGui::EndDisabled();
			}
			ImGui::EndCombo();
		}
		if (const optional<wstring> dropped = Accept_AssetDrop(assetType); dropped && !IsUsed(*dropped)) {
			entry.tag = *dropped;
			changed = true;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 35.f);
		const uint32 step = 1;
		changed |= ImGui::InputScalar("##Count", ImGuiDataType_U32, &entry.count, &step, nullptr, "%u",
			ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		if (ImGui::Button("X", ImVec2(30.f, 0.f))) {
			list.erase(list.begin() + static_cast<ptrdiff_t>(row));
			changed = true;
			ImGui::PopID();
			break;
		}
		ImGui::PopID();
	}

	const auto unused = std::ranges::find_if(candidates, [&IsUsed](const wstring& tag) { return !IsUsed(tag); });
	ImGui::BeginDisabled(unused == candidates.end());
	if (ImGui::Button("+ Add")) {
		list.push_back({ *unused, 1 });
		changed = true;
	}
	ImGui::EndDisabled();
	if (const optional<wstring> dropped = Accept_AssetDrop(assetType); dropped && !IsUsed(*dropped)) {
		list.push_back({ *dropped, 1 });
		changed = true;
	}
	return changed;
}

// Shows the applied snapshot and swaps in a preset document; the write applies it at once, so a rejected
// preset leaves no Undo entry. Scene/Prefab save persists the full mapping.
Bool Inspector::Render_AnimationPreset(const Char* label, AnimationPresetSnapshot& preset)
{
	ImGui::Text("%s: %s (%zu clips)", label,
		preset.animationEnum.empty() ? "<None>" : preset.animationEnum.c_str(),
		preset.animations.size());

	const string selectedPreset = m_SelectedPresetPath.empty()
		? "<Select Preset>" : Helper::To_String(m_SelectedPresetPath.stem().wstring());
	if (ImGui::BeginCombo("Preset", selectedPreset.c_str())) {
		const filesystem::path presetDirectory = PATH.GetAnimationPresetSettingsDir();
		std::error_code errorCode;
		if (filesystem::exists(presetDirectory, errorCode)) {
			for (filesystem::directory_iterator it(presetDirectory,
				filesystem::directory_options::skip_permission_denied, errorCode), end;
				it != end; it.increment(errorCode)) {
				if (errorCode) { errorCode.clear(); continue; }
				if (!it->is_regular_file() || it->path().extension() != L".json")
					continue;
				const string presetLabel = Helper::To_String(it->path().stem().wstring());
				if (ImGui::Selectable(presetLabel.c_str(), it->path() == m_SelectedPresetPath))
					m_SelectedPresetPath = it->path();
			}
		}
		ImGui::EndCombo();
	}

	Bool changed = false;
	if (ImGui::Button("Apply Preset Snapshot")) {
		AnimationPresetSnapshot loaded;
		string presetName;
		string error;
		if (AnimationPresetEditor::Load_Document(m_SelectedPresetPath, loaded, presetName, error)) {
			preset = std::move(loaded);
			changed = true;
			m_PresetStatus = "Preset snapshot queued; Scene/Prefab save will persist the full mapping.";
		}
		else {
			m_PresetStatus = "Preset apply failed: " + error;
		}
	}
	if (!m_PresetStatus.empty())
		ImGui::TextWrapped("%s", m_PresetStatus.c_str());
	return changed;
}

void Inspector::Draw_GameObjectHeader(const Shared<Engine::GameObject>& obj)
{
    // 이름
    string name = Helper::To_String(obj->Get_Name());
    ImGui::Text("Name : %s", name.c_str());
    ImGui::Text("GUID : %s", To_String(obj->Get_ObjectGuid()).c_str());
    
    ImGui::Separator();

    auto layerRegistry = GAME_INSTANCE->Get_LayerRegister();
    auto tagRegistry = GAME_INSTANCE->Get_TagRegister();

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);

    // 1. Layer (오브젝트의 현재 소속 레이어 - 보통 단일 비트)
    uint32 currentLayer = obj->Get_LayerMask().Get_Layer(); 
    string currentLayerName = Helper::To_String(layerRegistry->Get_LayerName(currentLayer));
    if (currentLayerName.empty()) currentLayerName = "Default";

    if (ImGui::BeginCombo("Layer", currentLayerName.c_str()))
    {
        for (const auto& pair : layerRegistry->Get_AllLayers())
        {
            uint32 layerBit = static_cast<uint32>(pair.first);
            if (layerBit == 0) continue; // Skip empty bit if any
            
            bool isSelected = (layerBit == currentLayer);
            string layerNameStr = Helper::To_String(pair.second);
            if (layerNameStr.empty()) continue;

            if (ImGui::Selectable(layerNameStr.c_str(), isSelected)) {
                obj->Get_LayerMask().Set_Layer(pair.first); 
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    uint32 currentMask = obj->Get_LayerMask().Get_Mask();
    string maskPreview = (currentMask == 0xFFFFFFFF) ? "Everything" : ((currentMask == 0) ? "Nothing" : "Mixed...");

    if (ImGui::BeginCombo("Layer Mask", maskPreview.c_str()))
    {
        for (const auto& pair : layerRegistry->Get_AllLayers())
        {
            uint32 layerBit = static_cast<uint32>(pair.first);
            if (layerBit == 0) continue;
            
            bool isSelected = obj->Get_LayerMask().Has(layerBit);
            string layerNameStr = Helper::To_String(pair.second);
            if (layerNameStr.empty()) continue;

            if (ImGui::Selectable(layerNameStr.c_str(), isSelected, ImGuiSelectableFlags_DontClosePopups)) {
                if (isSelected) obj->Get_LayerMask().Remove(layerBit);
                else obj->Get_LayerMask().Add(layerBit);
            }
        }
        ImGui::EndCombo();
    }


    // 3. Tag (오브젝트가 가진 태그 - 다중 비트)
    uint32 currentTag = obj->Get_TagMask().Get_Tags();
    string tagPreview = (currentTag == 0) ? "Untagged" : "Mixed...";
    if (currentTag != 0) {
        // 단일 태그일 경우 이름을 보여주는 간단한 로직
        int count = 0;
        string singleName = "";
        for (const auto& pair : tagRegistry->Get_AllTags()) {
            if (obj->Get_TagMask().Has(static_cast<uint32>(pair.first))) {
                singleName = Helper::To_String(pair.second);
                count++;
            }
        }
        if (count == 1) tagPreview = singleName;
    }

    if (ImGui::BeginCombo("Tag##GO", tagPreview.c_str()))
    {
        for (const auto& pair : tagRegistry->Get_AllTags())
        {
            uint32 tagBit = static_cast<uint32>(pair.first);
            if (tagBit == 0) continue;

            bool isSelected = obj->Get_TagMask().Has(tagBit);
            string tagStr = Helper::To_String(pair.second);
            if (tagStr.empty()) continue;

            if (ImGui::Selectable(tagStr.c_str(), isSelected, ImGuiSelectableFlags_DontClosePopups)) {
                if (isSelected) obj->Get_TagMask().Remove(tagBit);
                else obj->Get_TagMask().Add(tagBit);
            }
        }
        ImGui::EndCombo();
    }
    
    ImGui::PopItemWidth();
}

Shared<Inspector> Inspector::Create() {
  auto instance = make_shared<Inspector>();
  if (FAILED(instance->Initialize())) {
    MSG_BOX("Failed To Create Inspector");
    return nullptr;
  }
  return instance;
}
