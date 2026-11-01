#include "TypeCatalog.h"
#include <optional>
#include "Engine_RTTR_Metadata.h"
#include "Component.h"
#include "GameObject.h"
#include "Object.h"
#include "SpdLogger.h"
#include "String_Helper.h"

namespace
{
	Engine::REFLECTION_VALUE_TYPE Get_ReflectionValueType(const rttr::type& type)
	{
		using namespace Engine;
		if (type == rttr::type::get<Bool>()) return REFLECTION_VALUE_TYPE::BOOL;
		if (type == rttr::type::get<int32>()) return REFLECTION_VALUE_TYPE::INT32;
		if (type == rttr::type::get<uint32>()) return REFLECTION_VALUE_TYPE::UINT32;
		if (type == rttr::type::get<Float>()) return REFLECTION_VALUE_TYPE::FLOAT;
		if (type == rttr::type::get<Double>()) return REFLECTION_VALUE_TYPE::DOUBLE;
		if (type == rttr::type::get<std::string>()) return REFLECTION_VALUE_TYPE::STRING;
		if (type == rttr::type::get<std::wstring>()) return REFLECTION_VALUE_TYPE::WSTRING;
		if (type == rttr::type::get<Vector3>()) return REFLECTION_VALUE_TYPE::VECTOR3;
		if (type == rttr::type::get<Float3>()) return REFLECTION_VALUE_TYPE::FLOAT3;
		if (type == rttr::type::get<Color>()) return REFLECTION_VALUE_TYPE::COLOR;
		if (type == rttr::type::get<Float4>()) return REFLECTION_VALUE_TYPE::FLOAT4;
		if (type == rttr::type::get<AnimationPresetSnapshot>())
			return REFLECTION_VALUE_TYPE::ANIMATION_PRESET;
		if (type == rttr::type::get<ObjectGuid>())
			return REFLECTION_VALUE_TYPE::OBJECT_REF;
		if (type == rttr::type::get<std::vector<TagCount>>())
			return REFLECTION_VALUE_TYPE::TAG_COUNT_LIST;
		return REFLECTION_VALUE_TYPE::NONE;
	}

	// RTTR metadata value -> the shapes descriptor metadata uses (text as std::string, numbers as Float).
	Engine::Bool To_MetadataValue(const rttr::variant& metadata, Engine::ReflectionValue& outValue)
	{
		using namespace Engine;
		if (metadata.is_type<const Char*>()) {
			const Char* text = metadata.get_value<const Char*>();
			outValue.data = text ? std::string{ text } : std::string{};
		}
		else if (metadata.is_type<std::string>()) outValue.data = metadata.get_value<std::string>();
		else if (metadata.is_type<Float>()) outValue.data = metadata.get_value<Float>();
		else if (metadata.is_type<Double>()) outValue.data = static_cast<Float>(metadata.get_value<Double>());
		else if (metadata.is_type<int32>()) outValue.data = static_cast<Float>(metadata.get_value<int32>());
		else return false;
		return true;
	}

	// One rule set for Engine RTTR metadata and descriptor metadata (Engine_RTTR_Metadata.h keys).
	Engine::Bool Apply_PropertyMetadata(Engine::ReflectedPropertyInfo& info, std::string_view key,
		const Engine::ReflectionValue& value)
	{
		using namespace Engine;
		const std::string* text = value.Try_Get<std::string>();
		const Float* number = value.Try_Get<Float>();
		if (key == Meta_Key_Type::DisplayName && text && !text->empty()) info.displayName = *text;
		else if (key == Meta_Key_Type::Min && number) info.minValue = *number;
		else if (key == Meta_Key_Type::Max && number) info.maxValue = *number;
		else if (key == Meta_Key_Type::TypeTag && text) info.dataTag = *text;
		else if (key == Meta_Key_Type::AssetType && text) info.assetType = *text;
		else if (key == Meta_Key_Type::ReferenceBase && text && !text->empty() &&
			info.valueType == REFLECTION_VALUE_TYPE::OBJECT_REF) info.expectedBaseRegisteredName = *text;
		else if (key == Meta_Key_Type::SaveData && text && !text->empty()) {
			info.saveDataKey = *text;
			info.isSerializable = true;
		}
		else return false;
		return true;
	}

	// Min/Max only describe numbers, and must not cross.
	Engine::Bool Is_ValidPropertyRange(const Engine::ReflectedPropertyInfo& info)
	{
		using namespace Engine;
		if (!info.minValue && !info.maxValue)
			return true;
		// Vectors clamp every component to the same range.
		const Bool isNumeric = info.valueType == REFLECTION_VALUE_TYPE::INT32 ||
			info.valueType == REFLECTION_VALUE_TYPE::UINT32 ||
			info.valueType == REFLECTION_VALUE_TYPE::FLOAT ||
			info.valueType == REFLECTION_VALUE_TYPE::DOUBLE ||
			info.valueType == REFLECTION_VALUE_TYPE::VECTOR3 ||
			info.valueType == REFLECTION_VALUE_TYPE::FLOAT3 ||
			info.valueType == REFLECTION_VALUE_TYPE::FLOAT4;
		return isNumeric && (!info.minValue || !info.maxValue || *info.minValue <= *info.maxValue);
	}

	// Descriptor property (Client builder or Engine RTTR_Property): applies its metadata, then checks its shape.
	Engine::Bool Normalize_DescriptorProperty(const std::string& typeName,
		Engine::ReflectedPropertyDescriptor& property)
	{
		using namespace Engine;
		for (const auto& [key, value] : property.metadata) {
			if (!Apply_PropertyMetadata(property.info, key, value)) {
				LOG_ERROR(L"Registry rejected metadata {} on {}.{}: unknown key or wrong value type",
					Helper::To_wString(key), Helper::To_wString(typeName),
					Helper::To_wString(property.info.registeredName));
				return false;
			}
		}
		property.metadata.clear();
		if (!Is_ValidPropertyRange(property.info)) {
			LOG_ERROR(L"Registry rejected Min/Max on {}.{}: non-numeric property or Min above Max",
				Helper::To_wString(typeName), Helper::To_wString(property.info.registeredName));
			return false;
		}

		const Bool isObjectReference = property.info.valueType == REFLECTION_VALUE_TYPE::OBJECT_REF;
		const Bool hasExpectedBase = !property.info.expectedBaseRegisteredName.empty();
		if (property.info.registeredName.empty() ||
			property.info.valueType == REFLECTION_VALUE_TYPE::NONE ||
			isObjectReference != hasExpectedBase ||
			!property.read ||
			(property.info.isWritable && !property.write)) {
			LOG_ERROR(L"Registry rejected property {}.{}: missing name, value type, accessor or ReferenceBase",
				Helper::To_wString(typeName), Helper::To_wString(property.info.registeredName));
			return false;
		}
		return true;
	}

	Engine::Bool To_ReflectionValue(const rttr::variant& value, Engine::ReflectionValue& outValue)
	{
		using namespace Engine;
		if (value.is_type<Bool>()) outValue.data = value.get_value<Bool>();
		else if (value.is_type<int32>()) outValue.data = value.get_value<int32>();
		else if (value.is_type<uint32>()) outValue.data = value.get_value<uint32>();
		else if (value.is_type<Float>()) outValue.data = value.get_value<Float>();
		else if (value.is_type<Double>()) outValue.data = value.get_value<Double>();
		else if (value.is_type<std::string>()) outValue.data = value.get_value<std::string>();
		else if (value.is_type<std::wstring>()) outValue.data = value.get_value<std::wstring>();
		else if (value.is_type<Vector3>()) outValue.data = value.get_value<Vector3>();
		else if (value.is_type<Float3>()) outValue.data = value.get_value<Float3>();
		else if (value.is_type<Color>()) outValue.data = value.get_value<Color>();
		else if (value.is_type<Float4>()) outValue.data = value.get_value<Float4>();
		else if (value.is_type<AnimationPresetSnapshot>())
			outValue.data = value.get_value<AnimationPresetSnapshot>();
		else if (value.is_type<ObjectGuid>()) outValue.data = value.get_value<ObjectGuid>();
		else if (value.is_type<std::vector<TagCount>>()) outValue.data = value.get_value<std::vector<TagCount>>();
		else return false;
		return true;
	}

	Engine::Bool Set_ReflectedValue(const rttr::property& property, rttr::instance target,
		const Engine::ReflectionValue& value)
	{
		return std::visit([&](const auto& typedValue) -> Engine::Bool {
			using ValueType = std::decay_t<decltype(typedValue)>;
			if constexpr (std::is_same_v<ValueType, std::monostate>) {
				return false;
			}
			else {
				return property.get_type() == rttr::type::get<ValueType>() &&
					property.set_value(target, typedValue);
			}
		}, value.data);
	}
}

HRESULT Engine::Registry::Initialize(void* arg) {
    return Refresh();
}

void Engine::Registry::On_Destroy() {
    Clear();
}

HRESULT Engine::Registry::Register_Descriptors(const ReflectionDescriptorBatch& descriptors)
{
	std::vector<ReflectedTypeDescriptor> types = descriptors.types;
	std::unordered_set<std::string> typeNames;
	std::unordered_set<RuntimeTypeId> typeIds;
	for (ReflectedTypeDescriptor& descriptor : types) {
		const ReflectedTypeInfo& info = descriptor.info;
		if (info.registeredName.empty() || info.runtimeTypeId == 0 ||
			(info.objectKind != REFLECTED_OBJECT_KIND::OBJECT &&
			 info.objectKind != REFLECTED_OBJECT_KIND::GAMEOBJECT &&
			 info.objectKind != REFLECTED_OBJECT_KIND::COMPONENT) ||
			!descriptor.cppType || !descriptor.isInstance ||
			!typeNames.emplace(info.registeredName).second ||
			!typeIds.emplace(info.runtimeTypeId).second) {
			return E_INVALIDARG;
		}
		if (!descriptor.metadata.empty()) {
			LOG_ERROR(L"Registry rejected type metadata {} on {}: no type metadata key is supported",
				Helper::To_wString(descriptor.metadata.front().first),
				Helper::To_wString(info.registeredName));
			return E_INVALIDARG;
		}

		std::unordered_set<std::string> propertyNames;
		for (ReflectedPropertyDescriptor& property : descriptor.properties) {
			if (!Normalize_DescriptorProperty(info.registeredName, property) ||
				!propertyNames.emplace(property.info.registeredName).second)
				return E_INVALIDARG;
		}
	}

	std::unordered_set<std::string> enumNames;
	for (const ReflectedEnumInfo& enumInfo : descriptors.enums) {
		if (enumInfo.registeredName.empty() || enumInfo.values.empty() ||
			!enumNames.emplace(enumInfo.registeredName).second) {
			return E_INVALIDARG;
		}
		std::unordered_set<std::string> valueNames;
		for (const ReflectedEnumValue& value : enumInfo.values) {
			if (value.name.empty() || !valueNames.emplace(value.name).second)
				return E_INVALIDARG;
		}
	}

	std::lock_guard lock(m_Mutex);
	for (const ReflectedTypeDescriptor& descriptor : types) {
		if (m_ExternalTypes.contains(descriptor.info.registeredName) ||
			m_ByRegisteredName.contains(descriptor.info.registeredName) ||
			m_RegisteredNameByRuntimeTypeId.contains(descriptor.info.runtimeTypeId)) {
			return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
		}
	}
	for (const ReflectedEnumInfo& enumInfo : descriptors.enums) {
		if (m_ExternalEnums.contains(enumInfo.registeredName) ||
			m_EnumsByRegisteredName.contains(enumInfo.registeredName)) {
			return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
		}
	}

	for (ReflectedTypeDescriptor& descriptor : types)
		m_ExternalTypes.emplace(descriptor.info.registeredName, std::move(descriptor));
	for (const ReflectedEnumInfo& enumInfo : descriptors.enums)
		m_ExternalEnums.emplace(enumInfo.registeredName, enumInfo);
	return S_OK;
}

HRESULT Engine::Registry::Refresh() {
    std::unordered_map<std::string, Entry> byRegisteredName;
    std::unordered_map<RuntimeTypeId, std::string> registeredNameByRuntimeTypeId;
	std::unordered_map<std::type_index, std::string> registeredNameByCppType;
    std::unordered_map<std::string, ReflectedEnumInfo> enumsByRegisteredName;

	// Engine types: RTTR_Class records (rttr_generated). Public headers carry no RTTR, so the base,
	// C++ type and kind come from the record, and RTTR only supplies factories and properties.
	for (const RTTRClassRecord& record : Get_RTTRClassRecords()) {
		const std::string registeredName = record.registeredName ? record.registeredName : "";
		if (registeredName.empty() || record.runtimeTypeId == 0 || !record.cppType ||
			record.reflectedType.get_name().to_string() != registeredName) {
			LOG_ERROR(L"Registry rejected Engine RTTR record {}", Helper::To_wString(registeredName));
			return E_FAIL;
		}

		Entry entry{ registeredName, record.runtimeTypeId, record.reflectedType };
		entry.typeInfo.registeredName = registeredName;
		entry.typeInfo.runtimeTypeId = record.runtimeTypeId;
		entry.typeInfo.objectKind = record.objectKind;
		if (record.baseRegisteredName && *record.baseRegisteredName)
			entry.typeInfo.baseRegisteredNames.emplace_back(record.baseRegisteredName);
		entry.cppType = record.cppType;
		entry.isInstance = record.isInstance;

        std::unordered_set<std::string> propertyNames;
        for (const rttr::property property : record.reflectedType.get_properties()) {
            const std::string propertyName = property.get_name().to_string();
            if (propertyName.empty() || !propertyNames.emplace(propertyName).second) {
                LOG_ERROR(L"Registry rejected duplicate or empty property name on {}",
                          Helper::To_wString(registeredName));
                return E_FAIL;
            }

			ReflectedPropertyInfo propertyInfo;
			propertyInfo.registeredName = propertyName;
			propertyInfo.valueType = Get_ReflectionValueType(property.get_type());
			propertyInfo.isWritable = !property.is_readonly();
			// DataType is only an annotation; every other key follows the descriptor rules.
			for (const MetadataKeyType key : { Meta_Key_Type::TypeTag, Meta_Key_Type::AssetType,
				Meta_Key_Type::SaveData, Meta_Key_Type::DisplayName, Meta_Key_Type::Min,
				Meta_Key_Type::Max, Meta_Key_Type::ReferenceBase }) {
				const rttr::variant metadata = property.get_metadata(key);
				if (!metadata.is_valid())
					continue;
				ReflectionValue metadataValue;
				if (!To_MetadataValue(metadata, metadataValue) ||
					!Apply_PropertyMetadata(propertyInfo, key, metadataValue)) {
					LOG_ERROR(L"Registry rejected metadata {} on {}.{}: wrong value type",
						Helper::To_wString(key), Helper::To_wString(registeredName),
						Helper::To_wString(propertyName));
					return E_FAIL;
				}
			}
			if (!Is_ValidPropertyRange(propertyInfo)) {
				LOG_ERROR(L"Registry rejected Min/Max on {}.{}: non-numeric property or Min above Max",
					Helper::To_wString(registeredName), Helper::To_wString(propertyName));
				return E_FAIL;
			}
			if (propertyInfo.isSerializable &&
				(propertyInfo.valueType == REFLECTION_VALUE_TYPE::NONE || !propertyInfo.isWritable)) {
				LOG_ERROR(L"Registry rejected unsupported serializable property {} on {}",
					Helper::To_wString(propertyName), Helper::To_wString(registeredName));
				return E_FAIL;
			}
			entry.properties.push_back(std::move(propertyInfo));
			entry.reflectedProperties.emplace(propertyName, RTTRProperty{ property, record.toInstance });
        }

		const Bool isConcrete = registeredName != "Object" &&
			record.reflectedType.get_constructor().is_valid();
		if (isConcrete && !record.reflectedType.get_method("Create").is_valid() &&
			!record.reflectedType.get_method("CreatePrototype").is_valid()) {
			LOG_ERROR(L"Registry rejected concrete Object type without a factory: {}",
				Helper::To_wString(registeredName));
			return E_FAIL;
		}

		if (!registeredNameByRuntimeTypeId.emplace(record.runtimeTypeId, registeredName).second ||
			!registeredNameByCppType.emplace(std::type_index{ *record.cppType }, registeredName).second ||
			!byRegisteredName.emplace(registeredName, std::move(entry)).second) {
			LOG_ERROR(L"Registry rejected duplicate Engine RTTR type {}", Helper::To_wString(registeredName));
			return E_FAIL;
		}
	}

	// RTTR_Property descriptors (HRESULT setters, enum values) join the RTTR record of their C++ type.
	for (RTTRDescriptorProperty descriptorProperty : Get_RTTRDescriptorProperties()) {
		ReflectedPropertyDescriptor& property = descriptorProperty.property;
		const auto nameIt = registeredNameByCppType.find(std::type_index{ *descriptorProperty.cppType });
		if (nameIt == registeredNameByCppType.end()) {
			LOG_ERROR(L"Registry rejected RTTR_Property {}: its type has no RTTR_Class",
				Helper::To_wString(property.info.registeredName));
			return E_FAIL;
		}
		Entry& entry = byRegisteredName.at(nameIt->second);
		if (!Normalize_DescriptorProperty(entry.registeredName, property))
			return E_FAIL;
		if (entry.reflectedProperties.contains(property.info.registeredName) ||
			entry.externalProperties.contains(property.info.registeredName)) {
			LOG_ERROR(L"Registry rejected duplicate property {}.{}",
				Helper::To_wString(entry.registeredName), Helper::To_wString(property.info.registeredName));
			return E_FAIL;
		}
		entry.properties.push_back(property.info);
		entry.externalProperties.emplace(property.info.registeredName, std::move(property));
	}

	for (const rttr::type reflectedType : rttr::type::get_types()) {
		if (!reflectedType.is_enumeration())
			continue;
		const std::string registeredName = reflectedType.get_name().to_string();
		const rttr::enumeration reflectedEnum = reflectedType.get_enumeration();
		ReflectedEnumInfo enumInfo;
		enumInfo.registeredName = registeredName;
		if (reflectedEnum.get_metadata("AnimationPreset").to_bool())
			enumInfo.category = "AnimationState";

		const auto names = reflectedEnum.get_names();
		const auto values = reflectedEnum.get_values();
		auto nameIt = names.begin();
		auto valueIt = values.begin();
		for (; nameIt != names.end() && valueIt != values.end(); ++nameIt, ++valueIt) {
			Bool converted = false;
			const std::int64_t value = valueIt->to_int64(&converted);
			if (!converted) {
				LOG_ERROR(L"Registry rejected non-integral enum value on {}",
					Helper::To_wString(registeredName));
				return E_FAIL;
			}
			enumInfo.values.push_back({ nameIt->to_string(), value });
		}
		if (registeredName.empty() || nameIt != names.end() || valueIt != values.end() ||
			enumInfo.values.empty() ||
			!enumsByRegisteredName.emplace(registeredName, std::move(enumInfo)).second) {
			LOG_ERROR(L"Registry rejected invalid enum descriptor {}",
				Helper::To_wString(registeredName));
			return E_FAIL;
		}
	}

	std::unordered_map<std::string, ReflectedTypeDescriptor> externalTypes;
	std::unordered_map<std::string, ReflectedEnumInfo> externalEnums;
	{
		std::lock_guard lock(m_Mutex);
		externalTypes = m_ExternalTypes;
		externalEnums = m_ExternalEnums;
	}

	for (const auto& [registeredName, descriptor] : externalTypes) {
		const ReflectedTypeInfo& info = descriptor.info;
		if (byRegisteredName.contains(registeredName) ||
			registeredNameByRuntimeTypeId.contains(info.runtimeTypeId) ||
			registeredNameByCppType.contains(std::type_index{ *descriptor.cppType })) {
			LOG_ERROR(L"Registry rejected conflicting external type {}",
				Helper::To_wString(registeredName));
			return E_FAIL;
		}

		Entry entry{ registeredName, info.runtimeTypeId, rttr::type::get_by_name(""), info };
		entry.externalFactory = descriptor.createPrototype;
		entry.cppType = descriptor.cppType;
		entry.isInstance = descriptor.isInstance;
		std::unordered_set<std::string> propertyNames;
		for (const ReflectedPropertyDescriptor& property : descriptor.properties) {
			if (!propertyNames.emplace(property.info.registeredName).second)
				return E_FAIL;
			entry.properties.push_back(property.info);
			entry.externalProperties.emplace(property.info.registeredName, property);
		}
		std::ranges::sort(entry.properties, {}, &ReflectedPropertyInfo::registeredName);
		byRegisteredName.emplace(registeredName, std::move(entry));
		registeredNameByRuntimeTypeId.emplace(info.runtimeTypeId, registeredName);
		registeredNameByCppType.emplace(std::type_index{ *descriptor.cppType }, registeredName);
	}

	for (const auto& [registeredName, enumInfo] : externalEnums) {
		if (!enumsByRegisteredName.emplace(registeredName, enumInfo).second) {
			LOG_ERROR(L"Registry rejected conflicting external enum {}",
				Helper::To_wString(registeredName));
			return E_FAIL;
		}
	}

	// Every type inherits its base's properties (own ones win) and sits one level below its base.
	std::unordered_set<std::string> composedTypes;
	std::unordered_set<std::string> composingTypes;
	std::function<Bool(const std::string&)> composeProperties = [&](const std::string& registeredName) -> Bool {
		if (composedTypes.contains(registeredName))
			return true;
		if (!composingTypes.emplace(registeredName).second) {
			LOG_ERROR(L"Registry detected a base cycle at {}",
				Helper::To_wString(registeredName));
			return false;
		}
		auto entryIt = byRegisteredName.find(registeredName);
		if (entryIt == byRegisteredName.end()) {
			LOG_ERROR(L"Registry could not resolve base {}",
				Helper::To_wString(registeredName));
			return false;
		}
		Entry& entry = entryIt->second;

		const auto ownProperties = entry.properties;
		const auto ownReflectedProperties = entry.reflectedProperties;
		const auto ownExternalProperties = entry.externalProperties;
		entry.properties.clear();
		entry.reflectedProperties.clear();
		entry.externalProperties.clear();
		entry.depth = 0;
		for (const std::string& baseName : entry.typeInfo.baseRegisteredNames) {
			if (!composeProperties(baseName))
				return false;
			const Entry& base = byRegisteredName.at(baseName);
			for (const ReflectedPropertyInfo& property : base.properties) {
				if (std::ranges::none_of(entry.properties, [&](const ReflectedPropertyInfo& current) {
					return current.registeredName == property.registeredName;
				})) entry.properties.push_back(property);
			}
			entry.reflectedProperties.insert(base.reflectedProperties.begin(), base.reflectedProperties.end());
			entry.externalProperties.insert(base.externalProperties.begin(), base.externalProperties.end());
			entry.depth = (std::max)(entry.depth, base.depth + 1);
		}
		auto ancestors = std::make_shared<std::vector<RuntimeTypeId>>(1, entry.runtimeTypeId);
		for (const std::string& baseName : entry.typeInfo.baseRegisteredNames) {
			for (const RuntimeTypeId ancestor : *byRegisteredName.at(baseName).ancestors) {
				if (std::ranges::find(*ancestors, ancestor) == ancestors->end())
					ancestors->push_back(ancestor);
			}
		}
		entry.ancestors = std::move(ancestors);
		for (const ReflectedPropertyInfo& property : ownProperties) {
			std::erase_if(entry.properties, [&](const ReflectedPropertyInfo& current) {
				return current.registeredName == property.registeredName;
			});
			entry.properties.push_back(property);
		}
		for (const auto& [propertyName, property] : ownReflectedProperties) {
			entry.externalProperties.erase(propertyName);
			entry.reflectedProperties.insert_or_assign(propertyName, property);
		}
		for (const auto& [propertyName, property] : ownExternalProperties) {
			entry.reflectedProperties.erase(propertyName);
			entry.externalProperties[propertyName] = property;
		}
		std::ranges::sort(entry.properties, {}, &ReflectedPropertyInfo::registeredName);
		composingTypes.erase(registeredName);
		composedTypes.emplace(registeredName);
		return true;
	};

	for (const auto& [registeredName, entry] : byRegisteredName) {
		if (!composeProperties(registeredName)) {
			LOG_ERROR(L"Registry rejected invalid base graph on {}",
				Helper::To_wString(registeredName));
			return E_FAIL;
		}
	}
	// Write checks a reference target against ReferenceBase through the base graph, so it must be a registered type.
	for (const auto& [registeredName, entry] : byRegisteredName) {
		for (const ReflectedPropertyInfo& property : entry.properties) {
			if (property.valueType == REFLECTION_VALUE_TYPE::OBJECT_REF &&
				!byRegisteredName.contains(property.expectedBaseRegisteredName)) {
				LOG_ERROR(L"Registry rejected {}.{}: ReferenceBase {} is not a registered type",
					Helper::To_wString(registeredName), Helper::To_wString(property.registeredName),
					Helper::To_wString(property.expectedBaseRegisteredName));
				return E_FAIL;
			}
		}
	}

    std::lock_guard lock(m_Mutex);
    m_ByRegisteredName = std::move(byRegisteredName);
    m_RegisteredNameByRuntimeTypeId = std::move(registeredNameByRuntimeTypeId);
	m_RegisteredNameByCppType = std::move(registeredNameByCppType);
	m_EnumsByRegisteredName = std::move(enumsByRegisteredName);
    return S_OK;
}

HRESULT Engine::Registry::Find_Identity(const Object& object, RuntimeTypeId& outRuntimeTypeId,
	std::string& outRegisteredName, Shared<const std::vector<RuntimeTypeId>>& outTypeAncestors) const
{
	outRuntimeTypeId = {};
	outRegisteredName.clear();
	outTypeAncestors.reset();
	std::lock_guard lock(m_Mutex);
	const Entry* found = nullptr;
	if (const auto nameIt = m_RegisteredNameByCppType.find(std::type_index{ typeid(object) });
		nameIt != m_RegisteredNameByCppType.end()) {
		found = &m_ByRegisteredName.at(nameIt->second);
	}
	else {
		// Unregistered subclass: the deepest registered type it is-a, as RTTR's nearest enabled base did.
		for (const auto& [registeredName, entry] : m_ByRegisteredName) {
			if (entry.isInstance && entry.isInstance(object) && (!found || entry.depth > found->depth))
				found = &entry;
		}
	}
	if (!found)
		return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
#ifdef _DEBUG
	// Cast<T> trusts the registered bases, so they must name exactly the registered types the object is in C++.
	for (const auto& [registeredName, entry] : m_ByRegisteredName) {
		if (entry.isInstance && entry.isInstance(object) !=
			(std::ranges::find(*found->ancestors, entry.runtimeTypeId) != found->ancestors->end()))
			LOG_ERROR(L"Registry: {} registered bases disagree with C++ about {}",
				Helper::To_wString(found->registeredName), Helper::To_wString(registeredName));
	}
#endif
	outRuntimeTypeId = found->runtimeTypeId;
	outRegisteredName = found->registeredName;
	outTypeAncestors = found->ancestors;
	return S_OK;
}
rttr::type Engine::Registry::Find_Type(std::string_view registeredName) const {
    std::lock_guard lock(m_Mutex);
    const auto it = m_ByRegisteredName.find(std::string{registeredName});
    return it == m_ByRegisteredName.end()
        ? rttr::type::get_by_name("")
        : it->second.reflectedType;
}

Engine::RuntimeTypeId Engine::Registry::Find_RuntimeTypeId(std::string_view registeredName) const {
    std::lock_guard lock(m_Mutex);
    const auto it = m_ByRegisteredName.find(std::string{registeredName});
    return it == m_ByRegisteredName.end() ? RuntimeTypeId{} : it->second.runtimeTypeId;
}

std::string Engine::Registry::Find_RegisteredName(RuntimeTypeId runtimeTypeId) const {
    std::lock_guard lock(m_Mutex);
    const auto it = m_RegisteredNameByRuntimeTypeId.find(runtimeTypeId);
    return it == m_RegisteredNameByRuntimeTypeId.end() ? std::string{} : it->second;
}

Bool Engine::Registry::Is_A(RuntimeTypeId runtimeTypeId, std::string_view baseRegisteredName) const
{
	std::lock_guard lock(m_Mutex);
	const auto nameIt = m_RegisteredNameByRuntimeTypeId.find(runtimeTypeId);
	if (nameIt == m_RegisteredNameByRuntimeTypeId.end())
		return false;
	// Refresh rejected base cycles, so the walk ends.
	std::vector<const std::string*> pending{ &nameIt->second };
	while (!pending.empty()) {
		const std::string& registeredName = *pending.back();
		pending.pop_back();
		if (registeredName == baseRegisteredName)
			return true;
		if (const auto typeIt = m_ByRegisteredName.find(registeredName); typeIt != m_ByRegisteredName.end())
			for (const std::string& baseName : typeIt->second.typeInfo.baseRegisteredNames)
				pending.push_back(&baseName);
	}
	return false;
}

HRESULT Engine::Registry::Find_TypeInfo(std::string_view registeredName,
	ReflectedTypeInfo& outInfo) const
{
	outInfo = {};
	std::lock_guard lock(m_Mutex);
	const auto it = m_ByRegisteredName.find(std::string{ registeredName });
	if (it == m_ByRegisteredName.end())
		return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	outInfo = it->second.typeInfo;
	return S_OK;
}

HRESULT Engine::Registry::Find_TypeInfo(RuntimeTypeId runtimeTypeId,
	ReflectedTypeInfo& outInfo) const
{
	outInfo = {};
	std::lock_guard lock(m_Mutex);
	const auto nameIt = m_RegisteredNameByRuntimeTypeId.find(runtimeTypeId);
	if (nameIt == m_RegisteredNameByRuntimeTypeId.end())
		return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	const auto typeIt = m_ByRegisteredName.find(nameIt->second);
	if (typeIt == m_ByRegisteredName.end())
		return E_FAIL;
	outInfo = typeIt->second.typeInfo;
	return S_OK;
}

std::vector<Engine::ReflectedTypeInfo> Engine::Registry::Get_Types() const
{
	std::lock_guard lock(m_Mutex);
	std::vector<ReflectedTypeInfo> result;
	result.reserve(m_ByRegisteredName.size());
	for (const auto& [registeredName, entry] : m_ByRegisteredName)
		result.push_back(entry.typeInfo);
	std::ranges::sort(result, {}, &ReflectedTypeInfo::registeredName);
	return result;
}

std::vector<Engine::ReflectedPropertyInfo> Engine::Registry::Get_Properties(
	std::string_view registeredName) const
{
	std::lock_guard lock(m_Mutex);
	const auto it = m_ByRegisteredName.find(std::string{ registeredName });
	return it == m_ByRegisteredName.end()
		? std::vector<ReflectedPropertyInfo>{}
		: it->second.properties;
}

HRESULT Engine::Registry::Find_Enum(std::string_view registeredName,
	ReflectedEnumInfo& outInfo) const
{
	outInfo = {};
	std::lock_guard lock(m_Mutex);
	const auto it = m_EnumsByRegisteredName.find(std::string{ registeredName });
	if (it == m_EnumsByRegisteredName.end())
		return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	outInfo = it->second;
	return S_OK;
}

std::vector<Engine::ReflectedEnumInfo> Engine::Registry::Get_Enums(
	std::string_view category) const
{
	std::lock_guard lock(m_Mutex);
	std::vector<ReflectedEnumInfo> result;
	for (const auto& [registeredName, enumInfo] : m_EnumsByRegisteredName) {
		if (category.empty() || enumInfo.category == category)
			result.push_back(enumInfo);
	}
	std::ranges::sort(result, {}, &ReflectedEnumInfo::registeredName);
	return result;
}

std::vector<Engine::ReflectedPropertyInfo> Engine::Registry::Get_Properties(
	RuntimeTypeId runtimeTypeId) const
{
	std::lock_guard lock(m_Mutex);
	const auto nameIt = m_RegisteredNameByRuntimeTypeId.find(runtimeTypeId);
	if (nameIt == m_RegisteredNameByRuntimeTypeId.end())
		return {};
	const auto typeIt = m_ByRegisteredName.find(nameIt->second);
	return typeIt == m_ByRegisteredName.end()
		? std::vector<ReflectedPropertyInfo>{}
		: typeIt->second.properties;
}

HRESULT Engine::Registry::Read_Property(Object& target, std::string_view propertyName,
	ReflectionValue& outValue) const
{
	outValue = {};
	if (target.Get_RuntimeTypeId() == 0 || propertyName.empty())
		return E_INVALIDARG;

	std::optional<RTTRProperty> reflectedProperty;
	std::function<HRESULT(Object&, ReflectionValue&)> externalRead;
	{
		std::lock_guard lock(m_Mutex);
		const auto nameIt = m_RegisteredNameByRuntimeTypeId.find(target.Get_RuntimeTypeId());
		if (nameIt == m_RegisteredNameByRuntimeTypeId.end())
			return E_FAIL;
		const auto typeIt = m_ByRegisteredName.find(nameIt->second);
		if (typeIt == m_ByRegisteredName.end())
			return E_FAIL;
		const std::string key{ propertyName };
		const auto propertyIt = typeIt->second.reflectedProperties.find(key);
		if (propertyIt != typeIt->second.reflectedProperties.end())
			reflectedProperty = propertyIt->second;
		else if (const auto externalIt = typeIt->second.externalProperties.find(key);
			externalIt != typeIt->second.externalProperties.end())
			externalRead = externalIt->second.read;
		else return E_FAIL;
	}
	if (externalRead)
		return externalRead(target, outValue);

	const rttr::variant reflectedValue =
		reflectedProperty->property.get_value(reflectedProperty->toInstance(target));
	return reflectedValue.is_valid() && To_ReflectionValue(reflectedValue, outValue)
		? S_OK
		: E_NOINTERFACE;
}

HRESULT Engine::Registry::Write_Property(Object& target, std::string_view propertyName,
	const ReflectionValue& value) const
{
	if (target.Get_RuntimeTypeId() == 0 || propertyName.empty() || !value.Is_Valid())
		return E_INVALIDARG;

	std::optional<RTTRProperty> reflectedProperty;
	std::function<HRESULT(Object&, const ReflectionValue&)> externalWrite;
	{
		std::lock_guard lock(m_Mutex);
		const auto nameIt = m_RegisteredNameByRuntimeTypeId.find(target.Get_RuntimeTypeId());
		if (nameIt == m_RegisteredNameByRuntimeTypeId.end())
			return E_FAIL;
		const auto typeIt = m_ByRegisteredName.find(nameIt->second);
		if (typeIt == m_ByRegisteredName.end())
			return E_FAIL;
		const std::string key{ propertyName };
		const auto propertyIt = typeIt->second.reflectedProperties.find(key);
		if (propertyIt != typeIt->second.reflectedProperties.end()) {
			if (propertyIt->second.property.is_readonly())
				return E_ACCESSDENIED;
			reflectedProperty = propertyIt->second;
		}
		else if (const auto externalIt = typeIt->second.externalProperties.find(key);
			externalIt != typeIt->second.externalProperties.end()) {
			if (!externalIt->second.info.isWritable || !externalIt->second.write)
				return E_ACCESSDENIED;
			externalWrite = externalIt->second.write;
		}
		else return E_FAIL;
	}
	if (externalWrite)
		return externalWrite(target, value);

	return Set_ReflectedValue(reflectedProperty->property, reflectedProperty->toInstance(target), value)
		? S_OK
		: E_INVALIDARG;
}

std::vector<Engine::Registry::Entry> Engine::Registry::Get_ObjectTypes() const {
    std::lock_guard lock(m_Mutex);
    std::vector<Entry> result;
    for (const auto& [registeredName, entry] : m_ByRegisteredName) {
		if (entry.typeInfo.objectKind == REFLECTED_OBJECT_KIND::GAMEOBJECT ||
			entry.typeInfo.objectKind == REFLECTED_OBJECT_KIND::COMPONENT)
            result.push_back(entry);
    }
    std::ranges::sort(result, {}, &Entry::registeredName);
    return result;
}

HRESULT Engine::Registry::Create_Prototype(std::string_view registeredName,
	Shared<Object>& outPrototype) const
{
	outPrototype.reset();
	std::function<Shared<Object>()> factory;
	RuntimeTypeId runtimeTypeId{};
	std::string canonicalName;
	Shared<const std::vector<RuntimeTypeId>> ancestors;
	{
		std::lock_guard lock(m_Mutex);
		const auto it = m_ByRegisteredName.find(std::string{ registeredName });
		if (it == m_ByRegisteredName.end())
			return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		if (!it->second.externalFactory)
			return S_FALSE;
		factory = it->second.externalFactory;
		runtimeTypeId = it->second.runtimeTypeId;
		canonicalName = it->second.registeredName;
		ancestors = it->second.ancestors;
	}

	outPrototype = factory();
	if (!outPrototype)
		return E_FAIL;
	outPrototype->Assign_ReflectedIdentity(runtimeTypeId, canonicalName, ancestors);
	return S_OK;
}

HRESULT Engine::Registry::Create_EngineComponentPrototype(std::string_view registeredName,
	Shared<Object>& outPrototype) const
{
	outPrototype.reset();
	std::optional<rttr::method> createMethod;
	{
		std::lock_guard lock(m_Mutex);
		const auto it = m_ByRegisteredName.find(std::string{ registeredName });
		if (it == m_ByRegisteredName.end())
			return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		if (it->second.typeInfo.objectKind != REFLECTED_OBJECT_KIND::COMPONENT ||
			it->second.externalFactory)
			return E_INVALIDARG;
		createMethod = it->second.reflectedType.get_method("CreatePrototype");
	}
	if (!createMethod->is_valid())
		return S_FALSE;

	const rttr::variant result = createMethod->invoke({});
	if (!result.is_valid())
		return E_FAIL;
	// CreatePrototype returns Shared<T>; every T here is a Component.
	outPrototype = result.get_value<Shared<Component>>();
	return outPrototype ? S_OK : E_FAIL;
}

void Engine::Registry::Clear() {
    std::lock_guard lock(m_Mutex);
	m_ExternalEnums.clear();
	m_ExternalTypes.clear();
	m_EnumsByRegisteredName.clear();
    m_RegisteredNameByRuntimeTypeId.clear();
    m_ByRegisteredName.clear();
}
