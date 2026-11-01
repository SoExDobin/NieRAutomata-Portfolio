#pragma once

#include <typeindex>
#include "EngineManager.h"
#include "Engine_Reflection.h"
#include "Engine_RTTRClass.h"

NS_BEGIN(Engine)

class Object;

class Registry final : public EngineManager {
    NO_COPY(Registry)
public:
	// An RTTR property and how to reach its declaring type from an Object (no RTTR base info exists).
	struct RTTRProperty final {
		rttr::property property;
		rttr::instance(*toInstance)(Object&);
	};

    struct Entry final {
        std::string registeredName;
        RuntimeTypeId runtimeTypeId{};
        rttr::type reflectedType;
        ReflectedTypeInfo typeInfo;
        std::vector<ReflectedPropertyInfo> properties;
        std::unordered_map<std::string, RTTRProperty> reflectedProperties;
		std::unordered_map<std::string, ReflectedPropertyDescriptor> externalProperties;
		std::function<Shared<Object>()> externalFactory;
		const std::type_info* cppType{};
		Bool(*isInstance)(const Object&){};
		uint32 depth{};
		// This type, then every registered base; shared with each Object of the type for Is_A.
		Shared<const std::vector<RuntimeTypeId>> ancestors;
    };

    Registry() = default;
    ~Registry() override = default;

    HRESULT Initialize(void* arg) override;
    void On_Destroy() override;
	HRESULT Register_Descriptors(const ReflectionDescriptorBatch& descriptors);
    HRESULT Refresh();
    rttr::type Find_Type(std::string_view registeredName) const;
    RuntimeTypeId Find_RuntimeTypeId(std::string_view registeredName) const;
    std::string Find_RegisteredName(RuntimeTypeId runtimeTypeId) const;
	Bool Is_A(RuntimeTypeId runtimeTypeId, std::string_view baseRegisteredName) const;
	HRESULT Find_Identity(const Object& object, RuntimeTypeId& outRuntimeTypeId,
		std::string& outRegisteredName, Shared<const std::vector<RuntimeTypeId>>& outTypeAncestors) const;
    HRESULT Find_TypeInfo(std::string_view registeredName, ReflectedTypeInfo& outInfo) const;
    HRESULT Find_TypeInfo(RuntimeTypeId runtimeTypeId, ReflectedTypeInfo& outInfo) const;
    std::vector<ReflectedTypeInfo> Get_Types() const;
    std::vector<ReflectedPropertyInfo> Get_Properties(std::string_view registeredName) const;
    std::vector<ReflectedPropertyInfo> Get_Properties(RuntimeTypeId runtimeTypeId) const;
    HRESULT Find_Enum(std::string_view registeredName, ReflectedEnumInfo& outInfo) const;
    std::vector<ReflectedEnumInfo> Get_Enums(std::string_view category = {}) const;
    HRESULT Read_Property(Object& target, std::string_view propertyName,
        ReflectionValue& outValue) const;
    HRESULT Write_Property(Object& target, std::string_view propertyName,
        const ReflectionValue& value) const;
	HRESULT Create_Prototype(std::string_view registeredName, Shared<Object>& outPrototype) const;
	// Engine RTTR component's static CreatePrototype; S_FALSE when the type registers none.
	HRESULT Create_EngineComponentPrototype(std::string_view registeredName,
		Shared<Object>& outPrototype) const;
    std::vector<Entry> Get_ObjectTypes() const;
    void Clear();

private:
    mutable std::mutex m_Mutex;
    std::unordered_map<std::string, Entry> m_ByRegisteredName;
    std::unordered_map<RuntimeTypeId, std::string> m_RegisteredNameByRuntimeTypeId;
	std::unordered_map<std::type_index, std::string> m_RegisteredNameByCppType;
    std::unordered_map<std::string, ReflectedEnumInfo> m_EnumsByRegisteredName;
	std::unordered_map<std::string, ReflectedTypeDescriptor> m_ExternalTypes;
	std::unordered_map<std::string, ReflectedEnumInfo> m_ExternalEnums;
};

NS_END
