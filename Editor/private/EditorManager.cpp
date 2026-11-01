#include "pch.h"
#include "EditorManager.h"

#include "EditorCamera.h"
#include "Game.h"
#include "GameObject.h"
#include "PartObject.h"
#include "ScriptComponent.h"
#include "PathManager.h"

#include "EditorView.h"
#include "Inspector.h"
#include "LogConsole.h"
#include "MenuBar.h"
#include "Hierarchy.h"
#include "ModelViewer.h"
#include "AnimationPresetEditor.h"
#include "AssetBrowser.h"
#include "NavHelper.h"
#include "Transform.h"
#include "AuthoringBackup.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace
{
constexpr size_t MaxHistoryEntries = 64;

HRESULT Write_EditorProperty(Object& target, std::string_view propertyName,
	const ReflectionValue& value)
{
	// Serialization stages the snapshot until Post_Load. Live editing must apply it
	// now, including Undo/Redo, and return the actual validation/load result.
	if (auto* model = dynamic_cast<Model*>(&target); model && propertyName == "AnimationPreset")
	{
		const auto* preset = value.Try_Get<AnimationPresetSnapshot>();
		return preset ? model->Apply_AnimationPreset(*preset) : E_INVALIDARG;
	}
	return GAME_INSTANCE->Write_ReflectedProperty(target, propertyName, value);
}

Bool Belongs_To_Level(const Shared<GameObject>& object, uint32 levelIndex)
{
    return object && GAME_INSTANCE->Contains(levelIndex, object->Get_ObjectGuid());
}

Bool Is_In_Subtree(const Shared<GameObject>& object, ObjectGuid rootGuid)
{
    for (Shared<GameObject> current = object; current; current = current->Get_Parent())
        if (current->Get_ObjectGuid() == rootGuid)
            return true;

    return false;
}

void Refresh_DescendantTransforms(const Shared<GameObject>& owner)
{
	if (!owner)
		return;
	for (const Shared<GameObject>& child : owner->Get_Children()) {
		if (!child || child->Is_Destroy())
			continue;
		if (const Shared<Transform> transform = child->Get_Transform())
			transform->Update_WorldMatrix();
		Refresh_DescendantTransforms(child);
	}
}

Bool ReflectionValuesEqual(const ReflectionValue& left, const ReflectionValue& right)
{
	if (left.Get_Type() != right.Get_Type())
		return false;

	switch (left.Get_Type()) {
	case REFLECTION_VALUE_TYPE::NONE:
		return true;
	case REFLECTION_VALUE_TYPE::BOOL:
		return *left.Try_Get<Bool>() == *right.Try_Get<Bool>();
	case REFLECTION_VALUE_TYPE::INT32:
		return *left.Try_Get<int32>() == *right.Try_Get<int32>();
	case REFLECTION_VALUE_TYPE::UINT32:
		return *left.Try_Get<uint32>() == *right.Try_Get<uint32>();
	case REFLECTION_VALUE_TYPE::FLOAT:
		return *left.Try_Get<Float>() == *right.Try_Get<Float>();
	case REFLECTION_VALUE_TYPE::DOUBLE:
		return *left.Try_Get<Double>() == *right.Try_Get<Double>();
	case REFLECTION_VALUE_TYPE::STRING:
		return *left.Try_Get<string>() == *right.Try_Get<string>();
	case REFLECTION_VALUE_TYPE::WSTRING:
		return *left.Try_Get<wstring>() == *right.Try_Get<wstring>();
	case REFLECTION_VALUE_TYPE::VECTOR3:
	{
		const Vector3& a = *left.Try_Get<Vector3>();
		const Vector3& b = *right.Try_Get<Vector3>();
		return a.x == b.x && a.y == b.y && a.z == b.z;
	}
	case REFLECTION_VALUE_TYPE::FLOAT3:
	{
		const Float3& a = *left.Try_Get<Float3>();
		const Float3& b = *right.Try_Get<Float3>();
		return a.x == b.x && a.y == b.y && a.z == b.z;
	}
	case REFLECTION_VALUE_TYPE::COLOR:
	{
		const Color& a = *left.Try_Get<Color>();
		const Color& b = *right.Try_Get<Color>();
		return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
	}
	case REFLECTION_VALUE_TYPE::FLOAT4:
	{
		const Float4& a = *left.Try_Get<Float4>();
		const Float4& b = *right.Try_Get<Float4>();
		return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
	}
	case REFLECTION_VALUE_TYPE::ANIMATION_PRESET:
		return *left.Try_Get<AnimationPresetSnapshot>() ==
			*right.Try_Get<AnimationPresetSnapshot>();
	case REFLECTION_VALUE_TYPE::OBJECT_REF:
		return *left.Try_Get<ObjectGuid>() == *right.Try_Get<ObjectGuid>();
	case REFLECTION_VALUE_TYPE::TAG_COUNT_LIST:
		return *left.Try_Get<vector<TagCount>>() == *right.Try_Get<vector<TagCount>>();
	}

	return false;
}

wstring Make_UniqueName(const Shared<GameObject>& object, uint32 levelIndex)
{
    const wstring baseName = object ? object->Get_Name() : L"GameObject";
    const auto& objects = GAME_INSTANCE->Get_GameObjects(levelIndex);

    for (uint32 suffix = 0; ; ++suffix)
    {
        const wstring candidate = suffix == 0
            ? baseName
            : baseName + L"_" + to_wstring(suffix);
        const Bool overlaps = ranges::any_of(objects, [&](const auto& entry)
        {
            return entry.second && entry.second != object && entry.second->Get_Name() == candidate;
        });
        if (!overlaps)
            return candidate;
    }
}

// The registered Prefab document whose file stem is name (the tag spawners and drops use).
PrefabGuid Find_PrefabDocument(const wstring& name)
{
	for (const auto& [prefabGuid, path] : GAME_INSTANCE->Get_PrefabDocuments())
		if (filesystem::path(path).stem().wstring() == name)
			return prefabGuid;
	return {};
}

// The level a Prefab document was saved in.
optional<uint32> Read_PrefabLevel(const wstring& path)
{
	ifstream file{ filesystem::path(path) };
	const nlohmann::json document = nlohmann::json::parse(file, nullptr, false);
	if (document.is_discarded() || !document.contains("levelIndex") || !document["levelIndex"].is_number_unsigned())
		return nullopt;
	return document["levelIndex"].get<uint32>();
}
}

IMPLEMENT_SINGLETON(EditorManager)

EditorManager::EditorManager()
    : m_Inspector{nullptr}, m_EditorView{nullptr}, m_MenuBar{nullptr},
      m_AssetBrowser{nullptr}, m_LogConsole{nullptr}, m_Hierarchy{nullptr},
	  m_ModelViewer{nullptr}
{}

EditorManager::~EditorManager() {}

HRESULT EditorManager::Initialize() 
{
    EditorCamera::EDITOR_CAMERA_DESC desc;
    {
        desc.cameraSpeed = 20.f;
        desc.mouseSensitive = 5.f;
        desc.eye = Vector4{ 0.f, 10.f, -10.f, 1.f };
        desc.at = Vector4{ 0.f, 0.f, 0.f, 1.f };
        desc.up = Vector4{ 0.f, 1.f, 0.f, 1.f };
        desc.fovY = XMConvertToRadians(60.f);
        desc.aspect = static_cast<Float>(g_projectSettings.viewportWidth) / static_cast<Float>(g_projectSettings.viewportHeight);
        desc.nearPlane = 0.1f;
        desc.farPlane = 1000.f;
    }

    m_EditorCamera = EditorCamera::Create(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context(),desc);
    if (FAILED(m_EditorCamera->Bind_EditorMatrix()))
        return E_FAIL;

    /* UI 윈도우 */
    if (nullptr == (m_Inspector = Inspector::Create()))
		return E_FAIL;
    if (nullptr == (m_EditorView = EditorView::Create()))
		return E_FAIL;
    if (nullptr == (m_MenuBar = MenuBar::Create()))
		return E_FAIL;
    if (nullptr == (m_LogConsole = LogConsole::Create()))
		return E_FAIL;
    if (nullptr == (m_Hierarchy = Hierarchy::Create()))
        return E_FAIL;
    if (nullptr == (m_ModelViewer = ModelViewer::Create()))
        return E_FAIL;
	if (nullptr == (m_AnimationPresetEditor = AnimationPresetEditor::Create()))
		return E_FAIL;
    if (nullptr == (m_AssetBrowser = AssetBrowser::Create()))
        return E_FAIL;
    if (nullptr == (m_NavHelper = NavHelper::Create()))
        return E_FAIL;

	return S_OK;
}

void EditorManager::Focus_Object(const Shared<GameObject>& obj)
{
    if (!obj || obj->Is_Destroy() || !m_EditorCamera)
        return;

    BoundingBox bounds{};
    Bool hasBounds = false;
    const auto collect = [&](auto&& self, const Shared<GameObject>& current) -> void {
        if (!current || current->Is_Destroy())
            return;
        const Shared<Transform> transform = current->Get_Transform();
        BoundingBox localBounds{};
        if (const Shared<Model> model = current->Get_Component<Model>();
            transform && model && model->Compute_LocalBounds(localBounds)) {
            transform->Update_WorldMatrix();
            BoundingBox worldBounds{};
            localBounds.Transform(worldBounds, transform->Get_WorldMatrix());
            if (hasBounds)
                BoundingBox::CreateMerged(bounds, bounds, worldBounds);
            else
                bounds = worldBounds;
            hasBounds = true;
        }
        for (const Shared<GameObject>& child : current->Get_Children())
            if (child && child->Get_Parent() == current)
                self(self, child);
    };
    collect(collect, obj);

    Vector3 center{};
    Float radius = 1.f;
    if (hasBounds) {
        center = bounds.Center;
        radius = Vector3{ bounds.Extents }.Length();
    }
    else if (const Shared<Transform> transform = obj->Get_Transform()) {
        // No geometry (camera, light, empty): frame a unit sphere scaled by the object.
        transform->Update_WorldMatrix();
        center = transform->Get_WorldMatrix().Translation();
        const Vector3 scale = transform->Get_Scale();
        radius = std::max({ scale.x, scale.y, scale.z, 1.f });
    }

    m_EditorCamera->Focus(center, radius);
}

void EditorManager::Set_State(EDITOR_STATE state)
{
    if (m_State != state)
    {
        GAME_INSTANCE->Set_InputEnabled(false);
        GAME_INSTANCE->Set_MouseLock(false);
        // Play moves the editor camera along with the game camera (Render); Stop returns it to where editing left it.
        if (m_EditorCamera && m_State == EDITOR_STATE::STOP)
            m_EditorCamera->Hold_EditPose();
        else if (m_EditorCamera && state == EDITOR_STATE::STOP)
            m_EditorCamera->Restore_EditPose();
    }
    m_State = state;
    if (state != EDITOR_STATE::PAUSE)
        m_SingleStepRequested = false;
}

void EditorManager::RequestResize(Float width, Float height, uint32 screenIndex)
{
    if (!std::isfinite(width) || !std::isfinite(height) || width < 1.f || height < 1.f ||
        width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        screenIndex >= m_EngineDesc.renderTargetCount)
        return;
    m_ResizeRequests[screenIndex] = { static_cast<uint32>(width), static_cast<uint32>(height), screenIndex };
}

void EditorManager::Request_SingleStep()
{
    if (m_State == EDITOR_STATE::PAUSE)
        m_SingleStepRequested = true;
}

void EditorManager::Queue_Destroy(ObjectGuid targetGuid, uint32 levelIndex)
{
    if (targetGuid.Is_Valid())
        m_PendingMutations.push_back({ MUTATION_TYPE::DESTROY, targetGuid, {}, levelIndex, {} });
}

void EditorManager::Queue_MoveToRoot(ObjectGuid targetGuid, uint32 levelIndex)
{
    if (targetGuid.Is_Valid())
        m_PendingMutations.push_back({ MUTATION_TYPE::MOVE_TO_ROOT, targetGuid, {}, levelIndex, {} });
}

void EditorManager::Queue_Reparent(ObjectGuid targetGuid, ObjectGuid parentGuid, uint32 levelIndex,
	size_t targetIndex)
{
    if (targetGuid.Is_Valid() && parentGuid.Is_Valid())
		m_PendingMutations.push_back({ MUTATION_TYPE::REPARENT, targetGuid, parentGuid,
			levelIndex, {}, targetIndex });
}

void EditorManager::Queue_Reorder(ObjectGuid targetGuid, size_t targetIndex, uint32 levelIndex)
{
	if (targetGuid.Is_Valid())
		m_PendingMutations.push_back({ MUTATION_TYPE::REORDER, targetGuid, {},
			levelIndex, {}, targetIndex });
}

void EditorManager::Queue_Spawn(const wstring& prototypeTag, uint32 levelIndex,
	ObjectGuid parentGuid, size_t targetIndex, optional<Vector3> spawnPosition)
{
    if (!prototypeTag.empty())
		m_PendingMutations.push_back({ MUTATION_TYPE::SPAWN, {}, parentGuid,
			levelIndex, prototypeTag, targetIndex, spawnPosition });
}

void EditorManager::Queue_SpawnPrefab(const wstring& prefabName, uint32 levelIndex,
	optional<Vector3> spawnPosition, Bool openForEdit)
{
	if (prefabName.empty())
		return;

	MUTATION_COMMAND command;
	command.type = MUTATION_TYPE::SPAWN;
	command.levelIndex = levelIndex;
	command.prototypeTag = prefabName;
	command.spawnPosition = spawnPosition;
	command.isPrefab = true;
	command.openForEdit = openForEdit;
	m_PendingMutations.push_back(std::move(command));
}

void EditorManager::Queue_Duplicate(ObjectGuid targetGuid, uint32 levelIndex)
{
	if (targetGuid.Is_Valid())
		m_PendingMutations.push_back({ MUTATION_TYPE::DUPLICATE, targetGuid, {}, levelIndex, {} });
}

void EditorManager::Queue_Rename(ObjectGuid targetGuid, uint32 levelIndex, const wstring& newName)
{
	if (!targetGuid.Is_Valid() || newName.empty())
		return;

	MUTATION_COMMAND command;
	command.type = MUTATION_TYPE::RENAME;
	command.targetGuid = targetGuid;
	command.levelIndex = levelIndex;
	command.newName = newName;
	m_PendingMutations.push_back(std::move(command));
}

void EditorManager::Queue_PropertyWrite(const Shared<GameObject>& owner, Object& target,
	std::string_view propertyName, const ReflectionValue& before,
	const ReflectionValue& after, Bool beginGesture)
{
	if (!owner || owner->Is_Destroy() || propertyName.empty() ||
		!before.Is_Valid() || !after.Is_Valid() ||
		ReflectionValuesEqual(before, after))
		return;

	MUTATION_COMMAND command;
	command.type = MUTATION_TYPE::PROPERTY_WRITE;
	command.targetGuid = owner->Get_ObjectGuid();
	if (FAILED(GAME_INSTANCE->Find_Level(command.targetGuid, command.levelIndex))) {
		LOG_WARN("[EditorMutation] Dropped {} write: owner is not in any level", string(propertyName));
		return;
	}
	if (&target != owner.get()) {
		command.targetRegisteredName =
			GAME_INSTANCE->Find_RegisteredName(target.Get_RuntimeTypeId());
		if (command.targetRegisteredName.empty()) {
			LOG_WARN("[EditorMutation] Dropped {} write: target component type is not registered",
				string(propertyName));
			return;
		}
	}
	command.propertyName = propertyName;
	command.beforeValue = before;
	command.afterValue = after;
	command.beginGesture = beginGesture;
	m_PendingMutations.push_back(std::move(command));
}

void EditorManager::Queue_Undo()
{
	m_PendingMutations.push_back({ MUTATION_TYPE::UNDO });
}

void EditorManager::Queue_Redo()
{
	m_PendingMutations.push_back({ MUTATION_TYPE::REDO });
}

void EditorManager::Clear_History()
{
	m_UndoHistory.clear();
	m_RedoHistory.clear();
}

PrefabGuid EditorManager::Get_PrefabEdit(const Shared<GameObject>& root) const
{
	if (!root || root->Is_Destroy())
		return {};
	const auto it = ranges::find(m_PrefabEdits, root->Get_ObjectGuid(), &pair<ObjectGuid, PrefabGuid>::first);
	return it != m_PrefabEdits.end() ? it->second : PrefabGuid{};
}

void EditorManager::Apply_PrefabEdit(const Shared<GameObject>& root) const
{
	const PrefabGuid prefabGuid = Get_PrefabEdit(root);
	if (!prefabGuid.Is_Valid() || m_State != EDITOR_STATE::STOP)
		return;

	const wstring path = GAME_INSTANCE->Find_PrefabPath(prefabGuid);
	if (!AuthoringBackup::Capture(path))
		LOG_ERROR(L"Prefab save aborted: could not back up {}", path);
	else if (SUCCEEDED(GAME_INSTANCE->SerializePrefabDocument(prefabGuid, root)))
		LOG_INFO(L"Prefab saved: {}", path);
	else
		LOG_ERROR(L"Prefab save failed: {}", path);
}

void EditorManager::Delete_Prefab(PrefabGuid prefabGuid) const
{
	const wstring path = GAME_INSTANCE->Find_PrefabPath(prefabGuid);
	if (path.empty() || m_State != EDITOR_STATE::STOP)
		return;
	for (const auto& [rootGuid, editedPrefab] : m_PrefabEdits)
	{
		const Shared<GameObject> open = GAME_INSTANCE->Find(rootGuid);
		if (editedPrefab == prefabGuid && open && !open->Is_Destroy())
		{
			LOG_WARN(L"[PrefabEdit] Close the edit copy before deleting {}", path);
			return;
		}
	}

	error_code errorCode;
	if (!AuthoringBackup::Capture(path))
		LOG_ERROR(L"Prefab delete aborted: could not back up {}", path);
	else if (!filesystem::remove(path, errorCode) || errorCode)
		LOG_ERROR(L"Prefab delete failed: {}", path);
	else
	{
		GAME_INSTANCE->Unregister_Prefab(prefabGuid);
		LOG_INFO(L"Prefab deleted (backup in ProjectSetting/Backup): {}", path);
	}
}

Bool EditorManager::Can_SaveLevel() const
{
	if (m_State != EDITOR_STATE::STOP || !GAME_INSTANCE->LevelLoad_Finished() ||
		GAME_INSTANCE->Get_CurrentLevelIndex() == ETOI(LEVEL::LOADING))
	{
		LOG_WARN(L"[SceneSave] Stop playback and finish loading before saving an authored level");
		return false;
	}
	for (const auto& [rootGuid, prefabGuid] : m_PrefabEdits)
	{
		const Shared<GameObject> open = GAME_INSTANCE->Find(rootGuid);
		if (open && !open->Is_Destroy())
		{
			LOG_WARN(L"[PrefabEdit] Apply or Close the edit copy of '{}' before saving the level",
				filesystem::path(GAME_INSTANCE->Find_PrefabPath(prefabGuid)).stem().wstring());
			return false;
		}
	}
	return true;
}

Bool EditorManager::Can_EditHierarchy(const Shared<GameObject>& object) const
{
    // Children created by their parent's code are locked; parts only exist under their owner.
    if (!object || object->Is_Destroy() || !object->Get_StableChildKey().empty() ||
        dynamic_pointer_cast<PartObject>(object))
        return false;

    ReflectedTypeInfo typeInfo;
    return SUCCEEDED(GAME_INSTANCE->Find_ReflectedType(object->Get_RuntimeTypeId(), typeInfo)) &&
        typeInfo.objectKind == REFLECTED_OBJECT_KIND::GAMEOBJECT;
}

Bool EditorManager::Can_EditChildren(const Shared<GameObject>& object) const
{
    if (!Can_EditHierarchy(object))
        return false;

    return ranges::none_of(object->Get_Children(), [](const Shared<GameObject>& child) {
        return child && !child->Get_StableChildKey().empty();
    });
}

EditorManager::HIERARCHY_PLACEMENT EditorManager::Get_Placement(
	const Shared<GameObject>& object) const
{
	HIERARCHY_PLACEMENT placement;
	if (!object)
		return placement;

	const Shared<GameObject> parent = object->Get_Parent();
	if (!parent)
		return placement;

	placement.parentGuid = parent->Get_ObjectGuid();
	parent->Get_ChildIndex(object->Get_ObjectGuid(), placement.childIndex);
	return placement;
}

Bool EditorManager::Place_Object(const Shared<GameObject>& object,
	const HIERARCHY_PLACEMENT& placement) const
{
	if (!object)
		return false;

	if (!placement.parentGuid.Is_Valid())
		return !object->Has_Parent() || object->Remove_Parent() == S_OK;

	const Shared<GameObject> parent = GAME_INSTANCE->Find(placement.parentGuid);
	if (!parent || parent->Is_Destroy())
		return false;

	if (object->Get_Parent() == parent)
	{
		size_t currentIndex = numeric_limits<size_t>::max();
		if (FAILED(parent->Get_ChildIndex(object->Get_ObjectGuid(), currentIndex)))
			return false;
		return currentIndex == placement.childIndex ||
			parent->Reorder_Child(object, placement.childIndex) == S_OK;
	}

	return SUCCEEDED(object->Set_Parent(parent, L"", placement.childIndex));
}

Shared<Object> EditorManager::Resolve_PropertyTarget(ObjectGuid ownerGuid,
	std::string_view targetRegisteredName) const
{
	const Shared<GameObject> owner = GAME_INSTANCE->Find(ownerGuid);
	if (!owner || owner->Is_Destroy())
		return nullptr;
	if (targetRegisteredName.empty())
		return static_pointer_cast<Object>(owner);
	if (const Shared<Transform> transform = owner->Get_Transform();
		transform && GAME_INSTANCE->Find_RegisteredName(
			transform->Get_RuntimeTypeId()) == targetRegisteredName)
		return static_pointer_cast<Object>(transform);

	const auto matchesType = [targetRegisteredName](const Shared<Component>& component) {
		return component && GAME_INSTANCE->Find_RegisteredName(
			component->Get_RuntimeTypeId()) == targetRegisteredName;
	};
	for (const Shared<Component>& component : owner->Get_Components())
		if (matchesType(component))
			return static_pointer_cast<Object>(component);
	for (const Shared<ScriptComponent>& script : owner->Get_Scripts())
		if (matchesType(script))
			return static_pointer_cast<Object>(script);

	return nullptr;
}

Bool EditorManager::Capture_Subtree(const Shared<GameObject>& root,
	uint32 levelIndex, HISTORY_ENTRY& entry) const
{
	if (!root)
		return false;

	entry.targetGuid = root->Get_ObjectGuid();
	entry.levelIndex = levelIndex;
	entry.snapshotGuid = Create_PrefabGuid();
	return entry.snapshotGuid.Is_Valid() &&
		SUCCEEDED(GAME_INSTANCE->SerializeSubtreeSnapshot(
			entry.snapshotGuid, root, entry.snapshot, levelIndex));
}

Bool EditorManager::Restore_Subtree(const HISTORY_ENTRY& entry,
	Shared<GameObject>& outRoot) const
{
	outRoot.reset();
	if (!entry.snapshotGuid.Is_Valid() || entry.snapshot.empty() ||
		GAME_INSTANCE->Find(entry.targetGuid))
		return false;

	if (FAILED(GAME_INSTANCE->DeSerializeSubtreeSnapshot(
		entry.snapshotGuid, entry.snapshot, true, outRoot, entry.levelIndex)) ||
		!outRoot || outRoot->Get_ObjectGuid() != entry.targetGuid)
	{
		if (outRoot)
			GAME_INSTANCE->Destroy(outRoot->Get_ObjectGuid());
		outRoot.reset();
		return false;
	}

	if (!Place_Object(outRoot, entry.after))
	{
		GAME_INSTANCE->Destroy(outRoot->Get_ObjectGuid());
		outRoot.reset();
		return false;
	}
	return true;
}

void EditorManager::Record_History(HISTORY_ENTRY entry)
{
	if (m_UndoHistory.size() == MaxHistoryEntries)
		m_UndoHistory.erase(m_UndoHistory.begin());
	m_UndoHistory.push_back(std::move(entry));
	m_RedoHistory.clear();
}

Bool EditorManager::Apply_History(const HISTORY_ENTRY& entry, Bool undo)
{
	if (entry.levelIndex != 0 && entry.levelIndex != GAME_INSTANCE->Get_CurrentLevelIndex())
		return false;

	switch (entry.type)
	{
	case HISTORY_TYPE::ADD_SUBTREE:
		if (undo)
		{
			const Shared<GameObject> object = GAME_INSTANCE->Find(entry.targetGuid);
			if (!object)
				return false;
			if (const Shared<GameObject> selected = Get_SelectedObject();
				selected && Is_In_Subtree(selected, entry.targetGuid))
				Clear_SelectedObject();
			return SUCCEEDED(GAME_INSTANCE->Destroy(entry.targetGuid));
		}
		else
		{
			Shared<GameObject> restored;
			if (!Restore_Subtree(entry, restored))
				return false;
			Set_SelectedObject(restored);
			return true;
		}

	case HISTORY_TYPE::REMOVE_SUBTREE:
		if (undo)
		{
			Shared<GameObject> restored;
			if (!Restore_Subtree(entry, restored))
				return false;
			Set_SelectedObject(restored);
			return true;
		}
		else
		{
			const Shared<GameObject> object = GAME_INSTANCE->Find(entry.targetGuid);
			if (!object)
				return false;
			if (const Shared<GameObject> selected = Get_SelectedObject();
				selected && Is_In_Subtree(selected, entry.targetGuid))
				Clear_SelectedObject();
			return SUCCEEDED(GAME_INSTANCE->Destroy(entry.targetGuid));
		}

	case HISTORY_TYPE::MOVE:
	{
		const Shared<GameObject> object = GAME_INSTANCE->Find(entry.targetGuid);
		return object && Place_Object(object, undo ? entry.before : entry.after);
	}

	case HISTORY_TYPE::RENAME:
	{
		const Shared<GameObject> object = GAME_INSTANCE->Find(entry.targetGuid);
		if (!object || object->Get_Name() != (undo ? entry.afterName : entry.beforeName))
			return false;
		object->Set_Name(undo ? entry.beforeName : entry.afterName);
		return true;
	}

	case HISTORY_TYPE::PROPERTY:
	{
		const Shared<GameObject> owner = GAME_INSTANCE->Find(entry.targetGuid);
		const Shared<Object> target = Resolve_PropertyTarget(
			entry.targetGuid, entry.targetRegisteredName);
		if (!owner || !target)
			return false;

		ReflectionValue current;
		const ReflectionValue& expected = undo ? entry.afterValue : entry.beforeValue;
		const ReflectionValue& desired = undo ? entry.beforeValue : entry.afterValue;
		if (FAILED(GAME_INSTANCE->Read_ReflectedProperty(
			*target, entry.propertyName, current)) ||
			!ReflectionValuesEqual(current, expected) ||
			FAILED(Write_EditorProperty(
				*target, entry.propertyName, desired)))
			return false;
		if (dynamic_pointer_cast<Transform>(target))
			Refresh_DescendantTransforms(owner);
		return true;
	}
	}

	return false;
}

Bool EditorManager::Apply_Undo()
{
	if (m_UndoHistory.empty())
		return false;

	const HISTORY_ENTRY entry = m_UndoHistory.back();
	if (!Apply_History(entry, true))
		return false;
	m_UndoHistory.pop_back();
	m_RedoHistory.push_back(entry);
	return true;
}

Bool EditorManager::Apply_Redo()
{
	if (m_RedoHistory.empty())
		return false;

	const HISTORY_ENTRY entry = m_RedoHistory.back();
	if (!Apply_History(entry, false))
		return false;
	m_RedoHistory.pop_back();
	if (m_UndoHistory.size() == MaxHistoryEntries)
		m_UndoHistory.erase(m_UndoHistory.begin());
	m_UndoHistory.push_back(entry);
	return true;
}

Bool EditorManager::Apply_Mutation(const MUTATION_COMMAND& command)
{
	if (command.type == MUTATION_TYPE::UNDO)
		return Apply_Undo();
	if (command.type == MUTATION_TYPE::REDO)
		return Apply_Redo();

    const uint32 currentLevelIndex = GAME_INSTANCE->Get_CurrentLevelIndex();
    if (command.levelIndex != 0 && command.levelIndex != currentLevelIndex)
    {
        LOG_WARN(L"[EditorMutation] Rejected stale level command for level {}", command.levelIndex);
        return false;
    }

    const Shared<GameObject> target = GAME_INSTANCE->Find(command.targetGuid);

    switch (command.type)
    {
    case MUTATION_TYPE::DESTROY:
    {
        if (!target || target->Is_Destroy() ||
            !Belongs_To_Level(target, command.levelIndex) ||
            !Can_EditHierarchy(target))
            return false;

		HISTORY_ENTRY history;
		history.type = HISTORY_TYPE::REMOVE_SUBTREE;
		history.before = Get_Placement(target);
		history.after = history.before;
		if (!Capture_Subtree(target, command.levelIndex, history))
			return false;

        const Shared<GameObject> selected = Get_SelectedObject();
        if (selected && Is_In_Subtree(selected, command.targetGuid))
            Clear_SelectedObject();

		if (FAILED(GAME_INSTANCE->Destroy(command.targetGuid)))
			return false;
		Record_History(std::move(history));
		return true;
    }

    case MUTATION_TYPE::MOVE_TO_ROOT:
	{
        if (!target || target->Is_Destroy() ||
            !Belongs_To_Level(target, command.levelIndex) ||
			!Can_EditHierarchy(target) || !target->Has_Parent())
            return false;
		HISTORY_ENTRY history;
		history.type = HISTORY_TYPE::MOVE;
		history.targetGuid = target->Get_ObjectGuid();
		history.levelIndex = command.levelIndex;
		history.before = Get_Placement(target);
		if (FAILED(target->Remove_Parent()))
			return false;
		history.after = Get_Placement(target);
		Record_History(std::move(history));
		return true;
	}

    case MUTATION_TYPE::REPARENT:
    {
        const Shared<GameObject> parent = GAME_INSTANCE->Find(command.parentGuid);
        if (!target || !parent || target->Is_Destroy() || parent->Is_Destroy() ||
            target == parent || !Belongs_To_Level(target, command.levelIndex) ||
            !Belongs_To_Level(parent, command.levelIndex) ||
            !Can_EditHierarchy(target) || !Can_EditChildren(parent))
            return false;

		HISTORY_ENTRY history;
		history.type = HISTORY_TYPE::MOVE;
		history.targetGuid = target->Get_ObjectGuid();
		history.levelIndex = command.levelIndex;
		history.before = Get_Placement(target);
		if (FAILED(target->Set_Parent(parent, L"", command.targetIndex)))
			return false;
		history.after = Get_Placement(target);
		Record_History(std::move(history));
		return true;
    }

	case MUTATION_TYPE::REORDER:
	{
		const Shared<GameObject> parent = target ? target->Get_Parent() : nullptr;
		if (!target || !parent || target->Is_Destroy() ||
			!Belongs_To_Level(target, command.levelIndex) ||
			!Can_EditHierarchy(target) || !Can_EditChildren(parent))
			return false;

		HISTORY_ENTRY history;
		history.type = HISTORY_TYPE::MOVE;
		history.targetGuid = target->Get_ObjectGuid();
		history.levelIndex = command.levelIndex;
		history.before = Get_Placement(target);
		if (parent->Reorder_Child(target, command.targetIndex) != S_OK)
			return false;
		history.after = Get_Placement(target);
		Record_History(std::move(history));
		return true;
	}

    case MUTATION_TYPE::SPAWN:
    {
        Shared<GameObject> parent;
        if (command.parentGuid.Is_Valid())
        {
            parent = GAME_INSTANCE->Find(command.parentGuid);
            if (!parent || parent->Is_Destroy() ||
                !Belongs_To_Level(parent, command.levelIndex) || !Can_EditChildren(parent))
                return false;
        }

        Shared<GameObject> cloned;
		PrefabGuid prefabGuid{};
		if (command.isPrefab)
		{
			prefabGuid = Find_PrefabDocument(command.prototypeTag);
			if (!prefabGuid.Is_Valid())
			{
				LOG_WARN(L"[EditorMutation] No Prefab document named '{}'", command.prototypeTag);
				return false;
			}
			if (command.openForEdit)
			{
				// An open edit copy is selected again rather than opened twice.
				for (const auto& [rootGuid, editedPrefab] : m_PrefabEdits)
				{
					const Shared<GameObject> open = GAME_INSTANCE->Find(rootGuid);
					if (editedPrefab == prefabGuid && open && !open->Is_Destroy())
					{
						Set_SelectedObject(open);
						Focus_Object(open);
						return true;
					}
				}
				const optional<uint32> documentLevel = Read_PrefabLevel(GAME_INSTANCE->Find_PrefabPath(prefabGuid));
				if (documentLevel != command.levelIndex)
				{
					LOG_WARN(L"[PrefabEdit] '{}' was saved in level {}; open it there, this level may load stand-in models",
						command.prototypeTag, documentLevel ? to_wstring(*documentLevel) : wstring(L"?"));
					return false;
				}
			}
			if (FAILED(GAME_INSTANCE->DeSerializePrefabDocument(prefabGuid, cloned, command.levelIndex)))
				return false;
		}
		else
			cloned = GAME_INSTANCE->Instantiate<GameObject>(command.prototypeTag, command.levelIndex);
        if (!cloned)
            return false;
		if (!Can_EditHierarchy(cloned))
		{
			GAME_INSTANCE->Destroy(cloned->Get_ObjectGuid());
			return false;
		}

		// An edit copy keeps the document's root name, which Apply writes back.
		if (!command.openForEdit)
			cloned->Set_Name(Make_UniqueName(cloned, command.levelIndex));
		if (command.spawnPosition && cloned->Get_Transform())
			cloned->Get_Transform()->Set_LocalPositionByValue(*command.spawnPosition);
		if (parent && FAILED(cloned->Set_Parent(parent, L"", command.targetIndex)))
        {
            GAME_INSTANCE->Destroy(cloned->Get_ObjectGuid());
            return false;
        }

        Set_SelectedObject(cloned);
		HISTORY_ENTRY history;
		history.type = HISTORY_TYPE::ADD_SUBTREE;
		history.after = Get_Placement(cloned);
		if (!Capture_Subtree(cloned, command.levelIndex, history))
		{
			GAME_INSTANCE->Destroy(cloned->Get_ObjectGuid());
			Clear_SelectedObject();
			return false;
		}
		Record_History(std::move(history));
		if (command.openForEdit)
		{
			m_PrefabEdits.emplace_back(cloned->Get_ObjectGuid(), prefabGuid);
			Focus_Object(cloned);
			LOG_INFO(L"[PrefabEdit] Opened '{}'", command.prototypeTag);
		}
        return true;
    }

	case MUTATION_TYPE::DUPLICATE:
	{
		if (!target || target->Is_Destroy() ||
			!Belongs_To_Level(target, command.levelIndex) ||
			!Can_EditHierarchy(target))
			return false;

		const HIERARCHY_PLACEMENT sourcePlacement = Get_Placement(target);
		const Shared<GameObject> parent = target->Get_Parent();
		if (parent && !Can_EditChildren(parent))
			return false;

		HISTORY_ENTRY sourceSnapshot;
		if (!Capture_Subtree(target, command.levelIndex, sourceSnapshot))
			return false;

		Shared<GameObject> duplicate;
		if (FAILED(GAME_INSTANCE->DeSerializeSubtreeSnapshot(
			sourceSnapshot.snapshotGuid, sourceSnapshot.snapshot, false,
			duplicate, command.levelIndex)) || !duplicate)
			return false;
		if (!Can_EditHierarchy(duplicate))
		{
			GAME_INSTANCE->Destroy(duplicate->Get_ObjectGuid());
			return false;
		}

		duplicate->Set_Name(Make_UniqueName(duplicate, command.levelIndex));
		HIERARCHY_PLACEMENT duplicatePlacement = sourcePlacement;
		if (duplicatePlacement.parentGuid.Is_Valid())
			++duplicatePlacement.childIndex;
		if (!Place_Object(duplicate, duplicatePlacement))
		{
			GAME_INSTANCE->Destroy(duplicate->Get_ObjectGuid());
			return false;
		}

		HISTORY_ENTRY history;
		history.type = HISTORY_TYPE::ADD_SUBTREE;
		history.after = Get_Placement(duplicate);
		if (!Capture_Subtree(duplicate, command.levelIndex, history))
		{
			GAME_INSTANCE->Destroy(duplicate->Get_ObjectGuid());
			return false;
		}
		Record_History(std::move(history));
		Set_SelectedObject(duplicate);
		return true;
	}

	case MUTATION_TYPE::PROPERTY_WRITE:
	{
		if (!target || target->Is_Destroy() ||
			!Belongs_To_Level(target, command.levelIndex))
			return false;

		const Shared<Object> propertyTarget = Resolve_PropertyTarget(
			command.targetGuid, command.targetRegisteredName);
		ReflectionValue current;
		if (!propertyTarget || FAILED(GAME_INSTANCE->Read_ReflectedProperty(
			*propertyTarget, command.propertyName, current))) {
			LOG_WARN("[EditorMutation] {} write: property target unresolved or unreadable",
				command.propertyName);
			return false;
		}
		if (!ReflectionValuesEqual(current, command.beforeValue)) {
			LOG_WARN("[EditorMutation] {} write: live value changed since the edit was queued",
				command.propertyName);
			return false;
		}
		if (FAILED(Write_EditorProperty(
			*propertyTarget, command.propertyName, command.afterValue))) {
			LOG_WARN("[EditorMutation] {} write: reflected setter failed", command.propertyName);
			return false;
		}
		if (dynamic_pointer_cast<Transform>(propertyTarget))
			Refresh_DescendantTransforms(target);

		HISTORY_ENTRY history;
		history.type = HISTORY_TYPE::PROPERTY;
		history.targetGuid = command.targetGuid;
		history.levelIndex = command.levelIndex;
		history.targetRegisteredName = command.targetRegisteredName;
		history.propertyName = command.propertyName;
		history.beforeValue = command.beforeValue;
		history.afterValue = command.afterValue;

		if (!command.beginGesture && !m_UndoHistory.empty()) {
			HISTORY_ENTRY& previous = m_UndoHistory.back();
			if (previous.type == HISTORY_TYPE::PROPERTY &&
				previous.targetGuid == history.targetGuid &&
				previous.targetRegisteredName == history.targetRegisteredName &&
				previous.propertyName == history.propertyName) {
				previous.afterValue = history.afterValue;
				m_RedoHistory.clear();
				return true;
			}
		}

		Record_History(std::move(history));
		return true;
	}

	case MUTATION_TYPE::RENAME:
	{
		if (!target || target->Is_Destroy() ||
			!Belongs_To_Level(target, command.levelIndex) || !Can_EditHierarchy(target) ||
			target->Get_Name() == command.newName)
			return false;

		HISTORY_ENTRY history;
		history.type = HISTORY_TYPE::RENAME;
		history.targetGuid = command.targetGuid;
		history.levelIndex = command.levelIndex;
		history.beforeName = target->Get_Name();
		history.afterName = command.newName;
		target->Set_Name(command.newName);
		Record_History(std::move(history));
		return true;
	}

	case MUTATION_TYPE::UNDO:
	case MUTATION_TYPE::REDO:
		break;
    }

    return false;
}

void EditorManager::Flush_PendingMutations()
{
    if (m_PendingMutations.empty())
        return;

    vector<MUTATION_COMMAND> currentBatch;
    currentBatch.swap(m_PendingMutations);

    for (const MUTATION_COMMAND& command : currentBatch)
        if (!Apply_Mutation(command))
            LOG_WARN(L"[EditorMutation] Command rejected or failed");

    GAME_INSTANCE->Flush_DestroyedGameObjects();
}

void EditorManager::Update(Bool IsResetView) {
    const EDITOR_STATE state = Get_State();
    const Bool isLoading = GAME_INSTANCE->Get_CurrentLevelIndex() == ETOI(LEVEL::LOADING);
    const Bool singleStep = state == EDITOR_STATE::PAUSE && m_SingleStepRequested;
    m_SingleStepRequested = false;

    GAME_INSTANCE->Begin_Frame(state == EDITOR_STATE::PLAY || isLoading);
    const Float editorDelta = GAME_INSTANCE->Compute_UnscaledTimeDelta();

    Flush_PendingMutations();

    if (m_EditorCamera && state != EDITOR_STATE::PLAY)
        m_EditorCamera->Update(editorDelta);

    m_Inspector->Update(IsResetView);
	m_EditorView->Update(IsResetView);
    m_MenuBar->Update(IsResetView);
    m_LogConsole->Update(IsResetView);
    m_Hierarchy->Update(IsResetView);
    m_ModelViewer->Update(IsResetView);
	m_AnimationPresetEditor->Update(IsResetView);
    m_AssetBrowser->Update(IsResetView);
    m_NavHelper->Update(IsResetView);

    if (state == EDITOR_STATE::PLAY || isLoading || singleStep)
    {
        if (state != EDITOR_STATE::PLAY)
            GAME_INSTANCE->Set_InputEnabled(false);
        const Float runtimeDelta = singleStep
            ? GAME_INSTANCE->Get_FixedDeltaTime()
            : GAME_INSTANCE->Compute_TimeDelta();
        GAME_INSTANCE->Update_Engine(runtimeDelta, singleStep);
    }
    else
    {
        GAME_INSTANCE->Submit_RenderGroup();
    }
}

HRESULT EditorManager::Render(Bool IsResetView) {

    Shared<Camera> originalCamera = GAME_INSTANCE->Get_MainCamera();
    if (originalCamera && originalCamera != m_EditorCamera)
        m_InGameCamera = originalCamera;
    else
    {
        m_InGameCamera = nullptr;
        for (const auto& camera : GAME_INSTANCE->Get_Cameras(GAME_INSTANCE->Get_CurrentLevelIndex()))
            if (camera && camera != m_EditorCamera) { m_InGameCamera = camera; break; }
    }

    /* One view (Scene View). Play draws it through the game camera and the editor camera follows that camera; Stop and
       Pause draw it through the editor camera, so a pause looks around from where the game was. */
    const auto renderViews = [&]() -> HRESULT
    {
        if (FAILED(GAME_INSTANCE->Begin_RenderOffScreen(0))) return E_FAIL;
        if (m_InGameCamera && m_State == EDITOR_STATE::PLAY)
        {
            GAME_INSTANCE->Set_MainCamera(m_InGameCamera);
            D3D11_VIEWPORT viewport{};
            UINT count = 1;
            GAME_INSTANCE->Get_Context()->RSGetViewports(&count, &viewport);
            m_InGameCamera->Bind_Aspect(viewport.Width / viewport.Height);
            GAME_INSTANCE->Update_Pipeline();
            if (FAILED(GAME_INSTANCE->Draw())) return E_FAIL;
#ifdef _DEBUG
            GAME_INSTANCE->Render_CollisionDebug();
#endif
            if (m_EditorCamera)
                m_EditorCamera->Follow(m_InGameCamera);
            return S_OK;
        }
        if (!m_EditorCamera || FAILED(GAME_INSTANCE->Set_MainCamera(m_EditorCamera))) return E_FAIL;
        D3D11_VIEWPORT viewport{};
        UINT count = 1;
        GAME_INSTANCE->Get_Context()->RSGetViewports(&count, &viewport);
        m_EditorCamera->Set_Aspect(viewport.Width / viewport.Height);
        if (FAILED(m_EditorCamera->Bind_EditorMatrix())) return E_FAIL;
        GAME_INSTANCE->Update_Pipeline();
        m_SceneViewMatrix = GAME_INSTANCE->Get_Transform(D3DTS::VIEW);
        m_SceneProjMatrix = GAME_INSTANCE->Get_Transform(D3DTS::PROJ);
        if (FAILED(GAME_INSTANCE->Draw())) return E_FAIL;
#ifdef _DEBUG
        GAME_INSTANCE->Render_CollisionDebug();
#endif
        return S_OK;
    };
    const HRESULT viewResult = renderViews();
    GAME_INSTANCE->End_RenderOffScreen();
    GAME_INSTANCE->Set_MainCamera(originalCamera ? originalCamera : m_EditorCamera);
    if (FAILED(viewResult))
    {
        GAME_INSTANCE->Clear_RenderGroup();
        GAME_INSTANCE->Update_CameraPipeline();
        return viewResult;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGuiWindowFlags window_flags = 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | 
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground; // 배경 투명화
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Main Panel", nullptr, window_flags);
    {
        ImGui::PopStyleVar(3);
        // 7. 이 패널 공간 전체를 "도킹 가능한 구역(DockSpace)"으로 만듦
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspaceID = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        }
    }
    ImGui::End();

    m_Inspector->Render(IsResetView);
	m_EditorView->Render(IsResetView);
    m_MenuBar->Render(IsResetView);
    m_LogConsole->Render(IsResetView);
    m_Hierarchy->Render(IsResetView);
    m_ModelViewer->Render(IsResetView);
	m_AnimationPresetEditor->Render(IsResetView);
    m_AssetBrowser->Render(IsResetView);
    m_NavHelper->Render(IsResetView);

    GAME_INSTANCE->Update_CameraPipeline();
    return S_OK;
}
