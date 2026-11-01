#include "LevelSerializer.h"
#include <rttr/registration.h>
#include <nlohmann/json.hpp>
#include "Engine_RTTR_Metadata.h"
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <limits>
#include <unordered_set>
#include "Game.h"
#include "GameObject.h"
#include "Component.h"
#include "Transform.h"
#include "String_Helper.h"
#include "SpdLogger.h"

using namespace std;
using namespace Engine;
using namespace Meta_Key_Type;
using namespace Save_Data_Key;

namespace
{
	constexpr uint32 SceneSchemaVersion = 2;

	Bool IsSupportedPropertyType(REFLECTION_VALUE_TYPE propertyType)
	{
		return propertyType != REFLECTION_VALUE_TYPE::NONE;
	}

	Bool TryParseAnimationPreset(const nlohmann::json& value,
		AnimationPresetSnapshot& outPreset)
	{
		outPreset = {};
		if (value.is_null())
			return true;
		if (!value.is_object() || value.value("schemaVersion", 0u) != 2u ||
			!value.contains("animationEnum") || !value["animationEnum"].is_string() ||
			!value.contains("animations") || !value["animations"].is_array() ||
			value["animations"].empty())
			return false;

		AnimationPresetSnapshot pending;
		pending.schemaVersion = 2;
		pending.animationEnum = value["animationEnum"].get<string>();
		if (pending.animationEnum.empty())
			return false;

		unordered_set<wstring> uniquePaths;
		for (size_t index = 0; index < value["animations"].size(); ++index)
		{
			const auto& animation = value["animations"][index];
			if (!animation.is_object() || animation.value("index", numeric_limits<size_t>::max()) != index ||
				!animation.contains("path") || !animation["path"].is_string() ||
				!animation.contains("states") || !animation["states"].is_array())
				return false;

			AnimationPresetClip clip;
			clip.relativePath = Helper::To_wString(animation["path"].get<string>());
			if (clip.relativePath.empty() || !uniquePaths.emplace(clip.relativePath).second)
				return false;
			unordered_set<string> uniqueStates;
			for (const auto& state : animation["states"])
			{
				if (!state.is_string())
					return false;
				string stateName = state.get<string>();
				if (stateName.empty() || !uniqueStates.emplace(stateName).second)
					return false;
				clip.states.push_back(std::move(stateName));
			}
			pending.animations.push_back(std::move(clip));
		}
		outPreset = std::move(pending);
		return true;
	}

	nlohmann::json SerializeAnimationPreset(const AnimationPresetSnapshot& preset)
	{
		if (preset.Is_Empty())
			return nullptr;
		nlohmann::json result = {
			{ "schemaVersion", preset.schemaVersion },
			{ "animationEnum", preset.animationEnum },
			{ "animations", nlohmann::json::array() }
		};
		for (size_t index = 0; index < preset.animations.size(); ++index)
		{
			const AnimationPresetClip& clip = preset.animations[index];
			result["animations"].push_back({
				{ "index", index },
				{ "path", Helper::To_String(clip.relativePath) },
				{ "states", clip.states }
			});
		}
		return result;
	}

	// [{ "tag": "em0010", "count": 3 }, ...]; an empty tag or a repeated tag is malformed.
	Bool TryParseTagCountList(const nlohmann::json& value, vector<TagCount>& outList)
	{
		outList.clear();
		if (!value.is_array())
			return false;
		unordered_set<wstring> uniqueTags;
		vector<TagCount> pending;
		for (const auto& row : value)
		{
			if (!row.is_object() || !row.contains("tag") || !row["tag"].is_string() ||
				!row.contains("count") || !row["count"].is_number_unsigned() ||
				row["count"].get<std::uint64_t>() > numeric_limits<uint32>::max())
				return false;
			TagCount entry{ Helper::To_wString(row["tag"].get<string>()), row["count"].get<uint32>() };
			if (entry.tag.empty() || !uniqueTags.emplace(entry.tag).second)
				return false;
			pending.push_back(std::move(entry));
		}
		outList = std::move(pending);
		return true;
	}

	nlohmann::json SerializeTagCountList(const vector<TagCount>& list)
	{
		nlohmann::json result = nlohmann::json::array();
		for (const TagCount& entry : list)
			result.push_back({ { "tag", Helper::To_String(entry.tag) }, { "count", entry.count } });
		return result;
	}

	Bool IsCompatiblePropertyValue(REFLECTION_VALUE_TYPE propertyType,
		const nlohmann::json& value)
	{
		if (!IsSupportedPropertyType(propertyType))
			return false;
		if (propertyType == REFLECTION_VALUE_TYPE::UINT32)
			return value.is_number_unsigned() && value.get<std::uint64_t>() <= numeric_limits<uint32>::max();
		// nlohmann parses a non-negative literal such as 4 as unsigned, so both kinds are accepted in range.
		if (propertyType == REFLECTION_VALUE_TYPE::INT32)
			return value.is_number_unsigned()
				? value.get<std::uint64_t>() <= static_cast<std::uint64_t>(numeric_limits<int32>::max())
				: value.is_number_integer() &&
					value.get<std::int64_t>() >= numeric_limits<int32>::min() &&
					value.get<std::int64_t>() <= numeric_limits<int32>::max();
		if (propertyType == REFLECTION_VALUE_TYPE::FLOAT ||
			propertyType == REFLECTION_VALUE_TYPE::DOUBLE)
			return value.is_number();
		if (propertyType == REFLECTION_VALUE_TYPE::BOOL)
			return value.is_boolean();
		if (propertyType == REFLECTION_VALUE_TYPE::STRING ||
			propertyType == REFLECTION_VALUE_TYPE::WSTRING)
			return value.is_string();
		if (propertyType == REFLECTION_VALUE_TYPE::ANIMATION_PRESET)
		{
			AnimationPresetSnapshot preset;
			return TryParseAnimationPreset(value, preset);
		}
		if (propertyType == REFLECTION_VALUE_TYPE::TAG_COUNT_LIST)
		{
			vector<TagCount> list;
			return TryParseTagCountList(value, list);
		}
		if (propertyType == REFLECTION_VALUE_TYPE::OBJECT_REF)
		{
			if (value.is_null())
				return true;
			ObjectGuid objectGuid{};
			return value.is_string() &&
				Try_Parse_ObjectGuid(value.get<string>(), objectGuid) &&
				objectGuid.Is_Valid();
		}
		if (propertyType == REFLECTION_VALUE_TYPE::VECTOR3 ||
			propertyType == REFLECTION_VALUE_TYPE::FLOAT3)
			return value.is_array() && value.size() == 3 &&
				all_of(value.begin(), value.end(), [](const auto& element) { return element.is_number(); });
		return value.is_array() && value.size() == 4 &&
			all_of(value.begin(), value.end(), [](const auto& element) { return element.is_number(); });
	}

	Bool IsVector3Json(const nlohmann::json& value)
	{
		return value.is_array() && value.size() == 3 &&
			all_of(value.begin(), value.end(), [](const auto& element) { return element.is_number(); });
	}

	const ReflectedPropertyInfo* FindPropertyInfo(
		const vector<ReflectedPropertyInfo>& properties, std::string_view propertyName)
	{
		const auto it = std::ranges::find(properties, propertyName,
			&ReflectedPropertyInfo::registeredName);
		return it == properties.end() ? nullptr : &*it;
	}

	Bool ReadVector3Property(Object& target, std::string_view propertyName, Vector3& outValue)
	{
		ReflectionValue reflectedValue;
		if (FAILED(GAME_INSTANCE->Read_ReflectedProperty(target, propertyName, reflectedValue)))
			return false;
		const Vector3* value = reflectedValue.Try_Get<Vector3>();
		if (!value)
			return false;
		outValue = *value;
		return true;
	}

	Bool WriteVector3Property(Object& target, std::string_view propertyName,
		const nlohmann::json& value)
	{
		if (!IsVector3Json(value))
			return false;
		ReflectionValue reflectedValue;
		reflectedValue.data = Vector3{
			value[0].get<Float>(), value[1].get<Float>(), value[2].get<Float>() };
		return SUCCEEDED(GAME_INSTANCE->Write_ReflectedProperty(
			target, propertyName, reflectedValue));
	}

	HRESULT WriteJsonAtomically(const filesystem::path& outputPath,
		const nlohmann::json& document)
	{
		std::error_code errorCode;
		if (!outputPath.parent_path().empty())
			filesystem::create_directories(outputPath.parent_path(), errorCode);
		if (errorCode)
			return E_FAIL;

		const filesystem::path temporaryPath = outputPath.wstring() + L".tmp";
		{
			ofstream output(temporaryPath, ios::binary | ios::trunc);
			if (!output.is_open())
				return E_FAIL;
			output << document.dump(2);
			output.flush();
			if (!output.good())
			{
				output.close();
				filesystem::remove(temporaryPath, errorCode);
				return E_FAIL;
			}
		}

		if (!MoveFileExW(temporaryPath.c_str(), outputPath.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			filesystem::remove(temporaryPath, errorCode);
			return HRESULT_FROM_WIN32(GetLastError());
		}
		return S_OK;
	}
}

string LevelSerializer::ToUtf8(const wstring& ws)
{
	if (ws.empty())
		return {};
	int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
	string result(len, '\0');
	WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, result.data(), len, nullptr, nullptr);
	if (!result.empty() && result.back() == '\0')
		result.pop_back();
	return result;
}

wstring LevelSerializer::FromUtf8(const string& s)
{
	if (s.empty())
		return {};
	int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
	wstring result(len, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, result.data(), len);
	if (!result.empty() && result.back() == L'\0')
		result.pop_back();
	return result;
}

LevelSerializer::LevelSerializer() : EngineManager{}
{
}

HRESULT LevelSerializer::SerializeLevel(uint32 levIndex, const wstring& filePath)
{
	return SerializeDocument(levIndex, filePath, nullptr, {});
}

HRESULT LevelSerializer::SerializePrefab(PrefabGuid prefabGuid, uint32 levIndex,
	const Shared<GameObject>& selectedRoot, const wstring& filePath)
{
	if (!prefabGuid.Is_Valid() || !selectedRoot)
		return E_INVALIDARG;
	return SerializeDocument(levIndex, filePath, selectedRoot, prefabGuid);
}

HRESULT LevelSerializer::SerializeSubtreeSnapshot(PrefabGuid snapshotGuid, uint32 levIndex,
	const Shared<GameObject>& selectedRoot, string& outSnapshot)
{
	outSnapshot.clear();
	if (!snapshotGuid.Is_Valid() || !selectedRoot)
		return E_INVALIDARG;
	return SerializeDocument(levIndex, L"", selectedRoot, snapshotGuid, &outSnapshot);
}

HRESULT LevelSerializer::DeSerializeSubtreeSnapshot(PrefabGuid snapshotGuid, uint32 levIndex,
	const string& snapshot, Bool preserveObjectGuids, Shared<GameObject>& outRoot)
{
	outRoot.reset();
	if (!snapshotGuid.Is_Valid() || levIndex == UINT_MAX || snapshot.empty())
		return E_INVALIDARG;
	return DeSerializeDocument(L"", levIndex, snapshotGuid, &outRoot,
		preserveObjectGuids, &snapshot);
}

HRESULT LevelSerializer::SerializeDocument(uint32 levIndex, const wstring& filePath,
	const Shared<GameObject>& selectedRoot, PrefabGuid prefabGuid, string* outSnapshot)
{
	nlohmann::json root;
	root["schemaVersion"] = SceneSchemaVersion;
	root["documentType"] = selectedRoot ? "Prefab" : "Scene";
	root["levelIndex"] = levIndex;
	root["roots"] = nlohmann::json::array();
	root["objects"] = nlohmann::json::object();
	if (selectedRoot)
		root["prefabGuid"] = Engine::To_String(prefabGuid);

	const auto& allObjects = GAME_INSTANCE->Get_GameObjects(levIndex);
	vector<Shared<GameObject>> objects;
	unordered_set<ObjectGuid, GuidHash> serializedGuids;
	unordered_set<const GameObject*> levelObjects;
	for (const auto& [instanceID, object] : allObjects)
		if (object && !object->Is_Destroy())
			levelObjects.emplace(object.get());

	if (selectedRoot)
	{
		if (!levelObjects.contains(selectedRoot.get()) ||
			!selectedRoot->Get_StableChildKey().empty())
		{
			LOG_ERROR(L"[PrefabSerializer] Selected root is not a standalone object in level {}", levIndex);
			return E_INVALIDARG;
		}

		const auto GatherSubtree = [&](auto&& self, const Shared<GameObject>& object) -> Bool
		{
			if (!object || object->Is_Destroy() || !levelObjects.contains(object.get()) ||
				!object->Get_ObjectGuid().Is_Valid() ||
				!serializedGuids.emplace(object->Get_ObjectGuid()).second)
				return false;
			objects.push_back(object);
			for (const Shared<GameObject>& child : object->Get_Children())
				if (child && child->Get_Parent() == object && !self(self, child))
					return false;
			return true;
		};
		if (!GatherSubtree(GatherSubtree, selectedRoot))
		{
			LOG_ERROR(L"[PrefabSerializer] Selected subtree contains an invalid hierarchy");
			return E_FAIL;
		}
	}
	else
	{
		for (const auto& [instanceID, object] : allObjects) {
			if (!object || object->Is_Destroy())
				continue;
			Bool saveToScene = true;
			for (auto owner = object; owner; owner = owner->Get_Parent())
				if (!owner->Is_SceneSerializable()) {
					saveToScene = false;
					break;
				}
			if (!saveToScene)
				continue;
			if (!object->Get_ObjectGuid().Is_Valid() ||
				!serializedGuids.emplace(object->Get_ObjectGuid()).second) {
				LOG_ERROR(L"[SceneSerializer] Invalid or duplicate ObjectGuid on {}", object->Get_Name());
				return E_FAIL;
			}
			objects.push_back(object);
		}
	}
	std::ranges::sort(objects, {}, [](const Shared<GameObject>& object) {
		return object->Get_ObjectGuid();
	});

	auto SerializeProperties = [&](Object& target, nlohmann::json& outProps) -> Bool {
		outProps = nlohmann::json::object();
		const string registeredName = GAME_INSTANCE->Find_RegisteredName(target.Get_RuntimeTypeId());
		for (const ReflectedPropertyInfo& property :
			GAME_INSTANCE->Get_ReflectedProperties(target)) {
			if (!property.isSerializable)
				continue;

			ReflectionValue value;
			if (!IsSupportedPropertyType(property.valueType) ||
				FAILED(GAME_INSTANCE->Read_ReflectedProperty(
					target, property.registeredName, value))) {
				LOG_ERROR(L"[Serializer] Unsupported saved property {} on type {}",
					FromUtf8(property.registeredName), Helper::To_wString(registeredName));
				return false;
			}

			auto& output = outProps[property.registeredName];
			switch (value.Get_Type()) {
			case REFLECTION_VALUE_TYPE::BOOL: output = *value.Try_Get<Bool>(); break;
			case REFLECTION_VALUE_TYPE::INT32: output = *value.Try_Get<int32>(); break;
			case REFLECTION_VALUE_TYPE::UINT32: output = *value.Try_Get<uint32>(); break;
			case REFLECTION_VALUE_TYPE::FLOAT: output = *value.Try_Get<Float>(); break;
			case REFLECTION_VALUE_TYPE::DOUBLE: output = *value.Try_Get<Double>(); break;
			case REFLECTION_VALUE_TYPE::STRING: output = *value.Try_Get<string>(); break;
			case REFLECTION_VALUE_TYPE::WSTRING: output = ToUtf8(*value.Try_Get<wstring>()); break;
			case REFLECTION_VALUE_TYPE::VECTOR3: {
				const Vector3& v = *value.Try_Get<Vector3>(); output = { v.x, v.y, v.z }; break;
			}
			case REFLECTION_VALUE_TYPE::FLOAT3: {
				const Float3& v = *value.Try_Get<Float3>(); output = { v.x, v.y, v.z }; break;
			}
			case REFLECTION_VALUE_TYPE::COLOR: {
				const Color& c = *value.Try_Get<Color>(); output = { c.R(), c.G(), c.B(), c.A() }; break;
			}
			case REFLECTION_VALUE_TYPE::FLOAT4: {
				const Float4& v = *value.Try_Get<Float4>(); output = { v.x, v.y, v.z, v.w }; break;
			}
			case REFLECTION_VALUE_TYPE::ANIMATION_PRESET:
				output = SerializeAnimationPreset(*value.Try_Get<AnimationPresetSnapshot>()); break;
			case REFLECTION_VALUE_TYPE::OBJECT_REF: {
				const ObjectGuid objectGuid = *value.Try_Get<ObjectGuid>();
				output = objectGuid.Is_Valid()
					? nlohmann::json(Engine::To_String(objectGuid))
					: nlohmann::json(nullptr);
				break;
			}
			case REFLECTION_VALUE_TYPE::TAG_COUNT_LIST:
				output = SerializeTagCountList(*value.Try_Get<vector<TagCount>>()); break;
			default:
				return false;
			}
		}
		return true;
	};

	for (const Shared<GameObject>& object : objects)
	{
		nlohmann::json objectJson;
		const string typeName = GAME_INSTANCE->Find_RegisteredName(object->Get_RuntimeTypeId());
		if (typeName.empty() || GAME_INSTANCE->Find_RuntimeTypeId(typeName) == 0) {
			LOG_ERROR(L"[SceneSerializer] Unregistered GameObject type {}", FromUtf8(typeName));
			return E_FAIL;
		}

		objectJson["typeName"] = typeName;
		objectJson["name"] = ToUtf8(object->Get_Name());
		objectJson["stableChildKey"] = ToUtf8(object->Get_StableChildKey());
		objectJson["children"] = nlohmann::json::array();
		for (const Shared<GameObject>& child : object->Get_Children()) {
			if (child && child->Get_Parent() == object && serializedGuids.contains(child->Get_ObjectGuid()))
				objectJson["children"].push_back(Engine::To_String(child->Get_ObjectGuid()));
		}

		const Shared<GameObject> parent = object->Get_Parent();
		if (!parent || !serializedGuids.contains(parent->Get_ObjectGuid()))
			root["roots"].push_back(Engine::To_String(object->Get_ObjectGuid()));

		if (const Shared<Transform> transform = object->Get_Transform()) {
			Vector3 position{};
			Vector3 rotation{};
			Vector3 scale{};
			if (!ReadVector3Property(*transform, "Position", position) ||
				!ReadVector3Property(*transform, "Rotation", rotation) ||
				!ReadVector3Property(*transform, "Scale", scale)) {
				LOG_ERROR(L"[SceneSerializer] Transform reflection properties are unavailable on {}",
					object->Get_Name());
				return E_FAIL;
			}
			objectJson["transform"] = {
				{ "position", { position.x, position.y, position.z } },
				{ "rotation", { rotation.x, rotation.y, rotation.z } },
				{ "scale", { scale.x, scale.y, scale.z } }
			};
		}

		if (!SerializeProperties(*object, objectJson["properties"]))
			return E_FAIL;

		objectJson["components"] = nlohmann::json::array();
		for (const Shared<Component>& component : object->Get_Components()) {
			if (!component)
				continue;
			nlohmann::json componentJson;
			const string componentTypeName =
				GAME_INSTANCE->Find_RegisteredName(component->Get_RuntimeTypeId());
			if (componentTypeName.empty() || GAME_INSTANCE->Find_RuntimeTypeId(componentTypeName) == 0) {
				LOG_ERROR(L"[SceneSerializer] Unregistered component type {}", FromUtf8(componentTypeName));
				return E_FAIL;
			}
			componentJson["typeName"] = componentTypeName;
			if (!SerializeProperties(*component, componentJson["properties"]))
				return E_FAIL;
			objectJson["components"].push_back(std::move(componentJson));
		}

		root["objects"][Engine::To_String(object->Get_ObjectGuid())] = std::move(objectJson);
	}

	try
	{
		if (outSnapshot)
			*outSnapshot = root.dump(2);
		else
		{
			const HRESULT writeResult = WriteJsonAtomically(filesystem::path(filePath), root);
			if (FAILED(writeResult))
				return writeResult;
		}
	}
	catch (const std::exception& e)
	{
		LOG_CRITICAL(L"[SceneSerializer] Serialize exception: {}", Helper::To_wString(e.what()));
		return E_FAIL;
	}

	if (selectedRoot)
		LOG_INFO(L"[PrefabSerializer] Prefab saved: {} objects", objects.size());
	else
		LOG_INFO(L"[SceneSerializer] Scene saved: {} objects", objects.size());
	return S_OK;
}

HRESULT LevelSerializer::DeSerializeLevel(const wstring& filePath)
{
	return DeSerializeDocument(filePath, UINT_MAX, {}, nullptr);
}

HRESULT LevelSerializer::DeSerializePrefab(PrefabGuid prefabGuid, uint32 levIndex,
	const wstring& filePath, Shared<GameObject>& outRoot)
{
	outRoot.reset();
	if (!prefabGuid.Is_Valid() || levIndex == UINT_MAX)
		return E_INVALIDARG;
	return DeSerializeDocument(filePath, levIndex, prefabGuid, &outRoot);
}

HRESULT LevelSerializer::DeSerializeDocument(const wstring& filePath, uint32 targetLevel,
	PrefabGuid prefabGuid, Shared<GameObject>* outRoot,
	Bool preserveObjectGuids, const string* snapshot)
{
	const Bool instantiatePrefab = prefabGuid.Is_Valid();
	if (outRoot)
		outRoot->reset();
	nlohmann::json root;
	try
	{
		if (snapshot)
			root = nlohmann::json::parse(*snapshot);
		else
		{
			std::ifstream file(filePath);
			if (!file.is_open()) return E_FAIL;
			file >> root;
			file.close();
		}
	}
	catch (const std::exception& e)
	{
		LOG_CRITICAL(L"[SceneSerializer] DeSerializeLevel exception: {}", FromUtf8(e.what()));
		return E_FAIL;
	}

	// 파일 전체를 먼저 검사한다. 이 검사가 끝나기 전에는 현재 레벨을 변경하지 않는다.
	auto ValidateProperties = [&](std::string_view registeredName,
		const nlohmann::json& properties) -> Bool {
		if (!properties.is_object())
			return false;
		const vector<ReflectedPropertyInfo> reflectedProperties =
			GAME_INSTANCE->Get_ReflectedProperties(registeredName);
		for (auto it = properties.begin(); it != properties.end(); ++it) {
			const ReflectedPropertyInfo* property =
				FindPropertyInfo(reflectedProperties, it.key());
			if (!property || !property->isSerializable || !property->isWritable ||
				!IsCompatiblePropertyValue(property->valueType, it.value())) {
				LOG_ERROR(L"[SceneSerializer] Invalid property {} on type {}",
					FromUtf8(it.key()), Helper::To_wString(registeredName));
				return false;
			}
		}
		return true;
	};

	if (!root.is_object() || !root.contains("schemaVersion") ||
		!root["schemaVersion"].is_number_unsigned() ||
		root["schemaVersion"].get<uint32>() != SceneSchemaVersion ||
		!root.contains("levelIndex") || !root["levelIndex"].is_number_unsigned() ||
		!root.contains("roots") || !root["roots"].is_array() ||
		!root.contains("objects") || !root["objects"].is_object()) {
		LOG_ERROR(L"[SceneSerializer] Unsupported or malformed scene schema");
		return E_FAIL;
	}

	const string documentType = root.value("documentType", "Scene");
	if ((!instantiatePrefab && documentType != "Scene") ||
		(instantiatePrefab && documentType != "Prefab"))
	{
		LOG_ERROR(L"[SceneSerializer] Document type does not match the requested operation");
		return E_FAIL;
	}
	if (instantiatePrefab)
	{
		PrefabGuid documentPrefabGuid{};
		if (!root.contains("prefabGuid") || !root["prefabGuid"].is_string() ||
			!Try_Parse_PrefabGuid(root["prefabGuid"].get<string>(), documentPrefabGuid) ||
			documentPrefabGuid != prefabGuid)
		{
			LOG_ERROR(L"[PrefabSerializer] PrefabGuid does not match the registered document");
			return E_FAIL;
		}
	}

	const uint32 documentLevelIndex = root["levelIndex"].get<uint32>();
	const uint32 levelIndex = instantiatePrefab ? targetLevel : documentLevelIndex;
	unordered_set<ObjectGuid, GuidHash> documentObjectGuids;
	unordered_map<ObjectGuid, string, GuidHash> objectKeys;
	unordered_map<ObjectGuid, ObjectGuid, GuidHash> parentByChild;
	unordered_map<ObjectGuid, vector<ObjectGuid>, GuidHash> childrenByParent;
	for (auto objectIt = root["objects"].begin(); objectIt != root["objects"].end(); ++objectIt)
	{
		const string objectGuidText = objectIt.key();
		const nlohmann::json& objectJson = objectIt.value();
		ObjectGuid objectGuid{};
		if (!Try_Parse_ObjectGuid(objectGuidText, objectGuid) || !objectGuid.Is_Valid() ||
			!documentObjectGuids.emplace(objectGuid).second) {
			LOG_ERROR(L"[SceneSerializer] Invalid or duplicate ObjectGuid key {}", FromUtf8(objectGuidText));
			return E_FAIL;
		}
		objectKeys.emplace(objectGuid, objectGuidText);

		if (!objectJson.is_object() || !objectJson.contains("typeName") ||
			!objectJson["typeName"].is_string() || !objectJson.contains("properties") ||
			!objectJson.contains("components") || !objectJson["components"].is_array() ||
			!objectJson.contains("children") || !objectJson["children"].is_array() ||
			(objectJson.contains("name") && !objectJson["name"].is_string()) ||
			(objectJson.contains("stableChildKey") && !objectJson["stableChildKey"].is_string())) {
			LOG_ERROR(L"[SceneSerializer] Malformed object {}", FromUtf8(objectGuidText));
			return E_FAIL;
		}

		const string typeName = objectJson["typeName"].get<string>();
		ReflectedTypeInfo objectTypeInfo;
		if (FAILED(GAME_INSTANCE->Find_ReflectedType(typeName, objectTypeInfo)) ||
			objectTypeInfo.objectKind != REFLECTED_OBJECT_KIND::GAMEOBJECT ||
			!GAME_INSTANCE->Can_InstantiateGameObject(typeName, levelIndex) ||
			!ValidateProperties(typeName, objectJson["properties"])) {
			LOG_ERROR(L"[SceneSerializer] Invalid GameObject type {}", FromUtf8(typeName));
			return E_FAIL;
		}

		if (objectJson.contains("transform")) {
			const auto& transform = objectJson["transform"];
			if (!transform.is_object() || !transform.contains("position") ||
				!transform.contains("rotation") || !transform.contains("scale") ||
				!IsVector3Json(transform["position"]) || !IsVector3Json(transform["rotation"]) ||
				!IsVector3Json(transform["scale"])) {
				LOG_ERROR(L"[SceneSerializer] Malformed transform on {}", FromUtf8(typeName));
				return E_FAIL;
			}
		}

		unordered_set<RuntimeTypeId> componentTypes;
		for (const auto& componentJson : objectJson["components"]) {
			if (!componentJson.is_object() || !componentJson.contains("typeName") ||
				!componentJson["typeName"].is_string() || !componentJson.contains("properties"))
				return E_FAIL;
			const string componentTypeName = componentJson["typeName"].get<string>();
			const RuntimeTypeId componentRuntimeTypeId = GAME_INSTANCE->Find_RuntimeTypeId(componentTypeName);
			ReflectedTypeInfo componentTypeInfo;
			if (componentRuntimeTypeId == 0 || !componentTypes.emplace(componentRuntimeTypeId).second ||
				FAILED(GAME_INSTANCE->Find_ReflectedType(componentTypeName, componentTypeInfo)) ||
				componentTypeInfo.objectKind != REFLECTED_OBJECT_KIND::COMPONENT ||
				!GAME_INSTANCE->Can_InstantiateComponent(componentTypeName, levelIndex) ||
				!ValidateProperties(componentTypeName, componentJson["properties"])) {
				LOG_ERROR(L"[SceneSerializer] Invalid or duplicate component type {}", FromUtf8(componentTypeName));
				return E_FAIL;
			}
		}
	}

	for (auto objectIt = root["objects"].begin(); objectIt != root["objects"].end(); ++objectIt) {
		ObjectGuid parentGuid{};
		Try_Parse_ObjectGuid(objectIt.key(), parentGuid);
		unordered_set<ObjectGuid, GuidHash> childGuids;
		unordered_set<wstring> stableChildKeys;
		for (const auto& childValue : objectIt.value()["children"]) {
			if (!childValue.is_string())
				return E_FAIL;
			ObjectGuid childGuid{};
			const string childGuidText = childValue.get<string>();
			if (!Try_Parse_ObjectGuid(childGuidText, childGuid) || childGuid == parentGuid ||
				!documentObjectGuids.contains(childGuid) || !childGuids.emplace(childGuid).second ||
				!parentByChild.emplace(childGuid, parentGuid).second) {
				LOG_ERROR(L"[SceneSerializer] Invalid or multiply-owned child {}", FromUtf8(childGuidText));
				return E_FAIL;
			}
			childrenByParent[parentGuid].push_back(childGuid);
			const auto& childJson = root["objects"][objectKeys.at(childGuid)];
			const wstring stableChildKey = FromUtf8(childJson.value("stableChildKey", ""));
			if (!stableChildKey.empty() && !stableChildKeys.emplace(stableChildKey).second) {
				LOG_ERROR(L"[SceneSerializer] Duplicate StableChildKey {}", stableChildKey);
				return E_FAIL;
			}
		}
	}

	unordered_set<ObjectGuid, GuidHash> rootGuids;
	for (const auto& rootValue : root["roots"]) {
		if (!rootValue.is_string())
			return E_FAIL;
		ObjectGuid rootGuid{};
		const string rootGuidText = rootValue.get<string>();
		if (!Try_Parse_ObjectGuid(rootGuidText, rootGuid) || !documentObjectGuids.contains(rootGuid) ||
			parentByChild.contains(rootGuid) || !rootGuids.emplace(rootGuid).second) {
			LOG_ERROR(L"[SceneSerializer] Invalid root {}", FromUtf8(rootGuidText));
			return E_FAIL;
		}
	}

	unordered_set<ObjectGuid, GuidHash> visitedGuids;
	const auto ValidateHierarchy = [&](auto&& self, ObjectGuid objectGuid) -> Bool {
		if (!visitedGuids.emplace(objectGuid).second)
			return false;
		const auto childIt = childrenByParent.find(objectGuid);
		if (childIt == childrenByParent.end())
			return true;
		for (const ObjectGuid childGuid : childIt->second) {
			if (!self(self, childGuid))
				return false;
		}
		return true;
	};
	for (const ObjectGuid rootGuid : rootGuids) {
		if (!ValidateHierarchy(ValidateHierarchy, rootGuid)) {
			LOG_ERROR(L"[SceneSerializer] Hierarchy cycle detected");
			return E_FAIL;
		}
	}
	if (visitedGuids.size() != documentObjectGuids.size()) {
		LOG_ERROR(L"[SceneSerializer] Orphaned object or hierarchy cycle detected");
		return E_FAIL;
	}
	if (instantiatePrefab && rootGuids.size() != 1)
	{
		LOG_ERROR(L"[PrefabSerializer] A Prefab document must contain exactly one root");
		return E_FAIL;
	}

	if (!instantiatePrefab && FAILED(GAME_INSTANCE->Clear_GameObjects(levelIndex)))
		return E_FAIL;

	unordered_map<ObjectGuid, Shared<GameObject>, GuidHash> objectGuidMap;
	vector<Shared<GameObject>> createdRoots;
	Bool deserializeFailed = false;
	struct ObjectData {
		Shared<GameObject> pObj;
		const nlohmann::json* pJson;
	};
	vector<ObjectData> loadList;
	auto RollbackLoad = [&]() {
		if (instantiatePrefab)
		{
			for (const Shared<GameObject>& createdRoot : createdRoots)
				if (createdRoot && !createdRoot->Is_Destroy())
					createdRoot->Destroy_Subtree();
			GAME_INSTANCE->Clearing_ObjectManager(levelIndex);
		}
		else
		{
			GAME_INSTANCE->Clear_GameObjects(levelIndex);
		}
		createdRoots.clear();
		loadList.clear();
		objectGuidMap.clear();
	};

	const auto RestoreObject = [&](auto&& self,
		ObjectGuid savedObjectGuid,
		const Shared<GameObject>& object) -> Bool {
		if (!object || ((!instantiatePrefab || preserveObjectGuids) &&
			object->Get_ObjectGuid() != savedObjectGuid))
			return false;

		const nlohmann::json& objectJson = root["objects"][objectKeys.at(savedObjectGuid)];
		const string savedTypeName = objectJson["typeName"].get<string>();
		const string instantiatedTypeName =
			GAME_INSTANCE->Find_RegisteredName(object->Get_RuntimeTypeId());
		if (instantiatedTypeName != savedTypeName) {
			LOG_ERROR(L"[SceneSerializer] Prototype type mismatch: saved {}, created {}",
				FromUtf8(savedTypeName), FromUtf8(instantiatedTypeName));
			return false;
		}
		if (!objectGuidMap.emplace(savedObjectGuid, object).second) {
			LOG_ERROR(L"[SceneSerializer] Runtime object matched more than once {}",
				FromUtf8(objectKeys.at(savedObjectGuid)));
			return false;
		}

		object->Set_Name(FromUtf8(objectJson.value("name", "")));
		loadList.push_back({ object, &objectJson });

		const auto savedChildrenIt = childrenByParent.find(savedObjectGuid);
		const size_t savedChildCount = savedChildrenIt == childrenByParent.end()
			? 0
			: savedChildrenIt->second.size();

		for (size_t childIndex = 0; childIndex < savedChildCount; ++childIndex) {
			const ObjectGuid savedChildGuid = savedChildrenIt->second[childIndex];
			const nlohmann::json& childJson = root["objects"][objectKeys.at(savedChildGuid)];
			const string childTypeName = childJson["typeName"].get<string>();
			const wstring stableChildKey = FromUtf8(childJson.value("stableChildKey", ""));
			Shared<GameObject> child;

			if (!stableChildKey.empty()) {
				const auto& runtimeChildren = object->Get_Children();
				child = object->Find_Child(stableChildKey);
				if (childIndex >= runtimeChildren.size() || runtimeChildren[childIndex] != child) {
					LOG_ERROR(L"[SceneSerializer] Missing factory child {}", stableChildKey);
					return false;
				}

				const ObjectGuid derivedChildGuid = Derive_ChildObjectGuid(
					object->Get_ObjectGuid(), stableChildKey, childTypeName);
				if (!child || child->Get_Parent() != object ||
					child->Get_StableChildKey() != stableChildKey ||
					!derivedChildGuid.Is_Valid() || child->Get_ObjectGuid() != derivedChildGuid ||
					((!instantiatePrefab || preserveObjectGuids) && derivedChildGuid != savedChildGuid) ||
					GAME_INSTANCE->Find_RegisteredName(child->Get_RuntimeTypeId()) != childTypeName) {
					LOG_ERROR(L"[SceneSerializer] Factory child structure mismatch {}", stableChildKey);
					return false;
				}
			}
			else {
				const auto& runtimeChildren = object->Get_Children();
				if (childIndex < runtimeChildren.size() &&
					!runtimeChildren[childIndex]->Get_StableChildKey().empty()) {
					LOG_ERROR(L"[SceneSerializer] Editor child overlaps factory child at index {}", childIndex);
					return false;
				}

				child = GAME_INSTANCE->Instantiate_GameObject(
					childTypeName, levelIndex, nullptr,
					(instantiatePrefab && !preserveObjectGuids) ? ObjectGuid{} : savedChildGuid);
				if (!child || FAILED(object->Add_Child(child, L"", childIndex))) {
					LOG_ERROR(L"[SceneSerializer] Failed to create Editor child {}", FromUtf8(childTypeName));
					return false;
				}
			}

			if (!self(self, savedChildGuid, child))
				return false;
		}

		if (object->Get_Children().size() != savedChildCount) {
			LOG_ERROR(L"[SceneSerializer] Unexpected factory child count on {}", object->Get_Name());
			return false;
		}
		return true;
	};

	// Root만 생성한다. CodeDefined Child는 Root factory가 만든 객체를 문서와 매칭한다.
	for (const auto& rootValue : root["roots"]) {
		ObjectGuid rootGuid{};
		Try_Parse_ObjectGuid(rootValue.get<string>(), rootGuid);
		const nlohmann::json& rootJson = root["objects"][objectKeys.at(rootGuid)];
		const string rootTypeName = rootJson["typeName"].get<string>();
		const Shared<GameObject> rootObject = GAME_INSTANCE->Instantiate_GameObject(
			rootTypeName, levelIndex, nullptr,
			(instantiatePrefab && !preserveObjectGuids) ? ObjectGuid{} : rootGuid);
		if (rootObject)
			createdRoots.push_back(rootObject);
		if (!rootObject || !RestoreObject(RestoreObject, rootGuid, rootObject)) {
			LOG_ERROR(L"[SceneSerializer] Failed to restore root type {}", FromUtf8(rootTypeName));
			deserializeFailed = true;
			break;
		}
	}

	if (objectGuidMap.size() != documentObjectGuids.size() ||
		(!instantiatePrefab &&
			GAME_INSTANCE->Get_GameObjects(levelIndex).size() != documentObjectGuids.size())) {
		LOG_ERROR(L"[SceneSerializer] Runtime and document hierarchy counts differ");
		deserializeFailed = true;
	}
	if (deserializeFailed) {
		RollbackLoad();
		return E_FAIL;
	}

	// 공통 속성 역직렬화 헬퍼 람다
	auto DeserializeProperties = [&](Object& target, const nlohmann::json& propsJson) -> Bool {
		const string registeredName = GAME_INSTANCE->Find_RegisteredName(target.Get_RuntimeTypeId());
		const vector<ReflectedPropertyInfo> reflectedProperties =
			GAME_INSTANCE->Get_ReflectedProperties(target);
		Bool succeeded = true;
		// Resource setters replace component-owned state. Apply them first so a
		// subsequently restored snapshot cannot be overwritten by ModelTag/TextureTag.
		for (uint32 phase = 0; phase < 2; ++phase)
		for (auto it = propsJson.begin(); it != propsJson.end(); ++it)
		{
			const ReflectedPropertyInfo* property = FindPropertyInfo(reflectedProperties, it.key());
			if (!property || !property->isSerializable || !property->isWritable) {
				LOG_ERROR(L"[Serializer] Unknown property {} on type {}",
					FromUtf8(it.key()), Helper::To_wString(registeredName));
				succeeded = false;
				continue;
			}
			const Bool isResourceTag = property->saveDataKey == Save_Data_Key::TextureTag ||
				property->saveDataKey == Save_Data_Key::ModelTag;
			if ((phase == 0) != isResourceTag)
				continue;

			ReflectionValue reflectedValue;
			const auto& value = it.value();
			switch (property->valueType) {
			case REFLECTION_VALUE_TYPE::BOOL: reflectedValue.data = value.get<Bool>(); break;
			case REFLECTION_VALUE_TYPE::INT32: reflectedValue.data = value.get<int32>(); break;
			case REFLECTION_VALUE_TYPE::UINT32: reflectedValue.data = value.get<uint32>(); break;
			case REFLECTION_VALUE_TYPE::FLOAT: reflectedValue.data = value.get<Float>(); break;
			case REFLECTION_VALUE_TYPE::DOUBLE: reflectedValue.data = value.get<Double>(); break;
			case REFLECTION_VALUE_TYPE::STRING: reflectedValue.data = value.get<string>(); break;
			case REFLECTION_VALUE_TYPE::WSTRING: reflectedValue.data = FromUtf8(value.get<string>()); break;
			case REFLECTION_VALUE_TYPE::VECTOR3:
				reflectedValue.data = Vector3{ value[0].get<Float>(), value[1].get<Float>(), value[2].get<Float>() }; break;
			case REFLECTION_VALUE_TYPE::FLOAT3:
				reflectedValue.data = Float3{ value[0].get<Float>(), value[1].get<Float>(), value[2].get<Float>() }; break;
			case REFLECTION_VALUE_TYPE::COLOR:
				reflectedValue.data = Color{ value[0].get<Float>(), value[1].get<Float>(), value[2].get<Float>(), value[3].get<Float>() }; break;
			case REFLECTION_VALUE_TYPE::FLOAT4:
				reflectedValue.data = Float4{ value[0].get<Float>(), value[1].get<Float>(), value[2].get<Float>(), value[3].get<Float>() }; break;
			case REFLECTION_VALUE_TYPE::ANIMATION_PRESET: {
				AnimationPresetSnapshot preset;
				if (TryParseAnimationPreset(value, preset))
					reflectedValue.data = std::move(preset);
				break;
			}
			case REFLECTION_VALUE_TYPE::OBJECT_REF: {
				ObjectGuid objectGuid{};
				if (!value.is_null())
					Try_Parse_ObjectGuid(value.get<string>(), objectGuid);
				if (const auto objectIt = objectGuidMap.find(objectGuid);
					objectIt != objectGuidMap.end() && objectIt->second)
					objectGuid = objectIt->second->Get_ObjectGuid();
				else if (objectGuid.Is_Valid() && !GAME_INSTANCE->Find(objectGuid))
					LOG_WARN(L"[Serializer] {}.{} references missing object {}",
						Helper::To_wString(registeredName), FromUtf8(it.key()), FromUtf8(value.get<string>()));
				reflectedValue.data = objectGuid;
				break;
			}
			case REFLECTION_VALUE_TYPE::TAG_COUNT_LIST: {
				vector<TagCount> list;
				if (TryParseTagCountList(value, list))
					reflectedValue.data = std::move(list);
				break;
			}
			default:
				break;
			}

			const Bool valueApplied = reflectedValue.Is_Valid() &&
				SUCCEEDED(GAME_INSTANCE->Write_ReflectedProperty(
					target, property->registeredName, reflectedValue));

			if (!valueApplied) {
				LOG_ERROR(L"[Serializer] Failed to set property {} on type {}",
					FromUtf8(it.key()), Helper::To_wString(registeredName));
				succeeded = false;
			}

			if (valueApplied && value.is_string() &&
				(property->saveDataKey == Save_Data_Key::TextureTag ||
					property->saveDataKey == Save_Data_Key::ModelTag)) {
				LOG_INFO(L"[Serializer] ResourceTag injected into {} : {}",
					Helper::To_wString(registeredName), FromUtf8(value.get<string>()));
			}
		}
		return succeeded;
	};


	// ── Pass 2: 데이터 주입 (Properties & Components) ─────────────────────
	for (auto& data : loadList)
	{
		auto& pObj = data.pObj;
		auto& objJson = *data.pJson;

		// 2. RTTR 프로퍼티 주입 (GameObject)
		if (objJson.contains("properties"))
		{
			if (!DeserializeProperties(*pObj, objJson["properties"]))
				deserializeFailed = true;
		}

		// 3. 컴포넌트 데이터 주입
		if (objJson.contains("components"))
		{
			unordered_map<string, Shared<Component>> runtimeComponents;
			for (const Shared<Component>& component : pObj->Get_Components()) {
				if (!component)
					continue;
				const string typeName =
					GAME_INSTANCE->Find_RegisteredName(component->Get_RuntimeTypeId());
				if (typeName.empty() || !runtimeComponents.emplace(typeName, component).second) {
					LOG_ERROR(L"[SceneSerializer] Invalid runtime component structure on {}", pObj->Get_Name());
					deserializeFailed = true;
				}
			}

			// 코드가 컴포넌트를 새로 추가한 상황이다. 실패가 아니라 아직 저장되지 않은 상태이므로
			// 기본값으로 로드되고 다음 저장 때 문서에 기록된다. 반대로 코드에서 빠진 컴포넌트는 아래에서 건너뛴다.
			if (runtimeComponents.size() > objJson["components"].size()) {
				LOG_WARN(L"[SceneSerializer] {} has {} runtime components but the document stores {}; "
					L"the extra ones load with defaults and will be written on the next save",
					pObj->Get_Name(), runtimeComponents.size(), objJson["components"].size());
			}

			for (auto& compJson : objJson["components"])
			{
				string typeName = compJson.value("typeName", "");
				if (typeName.empty() || GAME_INSTANCE->Find_RuntimeTypeId(typeName) == 0) {
					LOG_ERROR(L"[SceneSerializer] Unknown component registered name {}", FromUtf8(typeName));
					deserializeFailed = true;
					continue;
				}
				auto& compPropsJson = compJson["properties"];

				const auto componentIt = runtimeComponents.find(typeName);
				const Shared<Component> targetComp = componentIt == runtimeComponents.end()
					? nullptr
					: componentIt->second;
				if (targetComp)
				{
					if (!DeserializeProperties(*targetComp, compPropsJson))
						deserializeFailed = true;
				}
				else {
					// 코드가 더 이상 붙이지 않는 컴포넌트다. 객체는 그대로 쓸 수 있으니 건너뛰고,
					// 다음 저장 때 문서에서 빠진다.
					LOG_WARN(L"[SceneSerializer] {} no longer has component {}; its stored data is skipped "
						L"and will be dropped on the next save", pObj->Get_Name(), FromUtf8(typeName));
				}
			}
		}
		
		// 4. (무조건 등록되었던) Transform 주입
		if (objJson.contains("transform"))
		{
			auto& t = objJson["transform"];
			auto transform = pObj->Get_Transform();
			if (transform)
			{
				if (!WriteVector3Property(*transform, "Position", t["position"]) ||
					!WriteVector3Property(*transform, "Rotation", t["rotation"]) ||
					!WriteVector3Property(*transform, "Scale", t["scale"]))
					deserializeFailed = true;
			}
		}
	}

	if (deserializeFailed) {
		RollbackLoad();
		return E_FAIL;
	}

	// ── Pass 4: 로드 후 처리 ─────────────────────────────────────────────
	for (auto& data : loadList)
	{
		if (data.pObj)
		{
			if (FAILED(data.pObj->Post_Load()))
			{
				LOG_ERROR(L"[SceneSerializer] Component post-load failed on {}", data.pObj->Get_Name());
				deserializeFailed = true;
				continue;
			}
			if (data.pObj->Get_Transform()) {
				data.pObj->Get_Transform()->Update_WorldMatrix();
			}
		}
	}
	if (deserializeFailed)
	{
		RollbackLoad();
		return E_FAIL;
	}
	// Older Scene saves may contain runtime transition overlays. Their Prefab assets
	// remain editable/loadable, but stale Scene copies must never cover the next session.
	if (!instantiatePrefab)
		for (auto& data : loadList)
			if (data.pObj && !data.pObj->Is_Destroy() && !data.pObj->Is_SceneSerializable()) {
				LOG_WARN(L"[SceneSerializer] Ignoring runtime-owned Scene object {}; it will be dropped on the next save",
					data.pObj->Get_Name());
				data.pObj->Destroy_Subtree();
			}
	if (instantiatePrefab && outRoot)
		*outRoot = createdRoots.front();
	if (instantiatePrefab)
		LOG_INFO(L"[PrefabSerializer] Prefab instantiated: {} objects", loadList.size());
	else
		LOG_INFO(L"[SceneSerializer] Scene loaded: {} objects", loadList.size());
	return S_OK;
}

Unique<LevelSerializer> LevelSerializer::Create()
{
	auto levelSerializer = make_unique<LevelSerializer>();

	if (FAILED(levelSerializer->Initialize_Prototype()) ||
		FAILED(levelSerializer->Initialize(nullptr)))
	{
		LOG_ERROR(L"Failed to Create LevelSerializer");
	}

	return levelSerializer;
}
