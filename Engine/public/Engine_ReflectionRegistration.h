#pragma once

#include <type_traits>
#include <vector>

#include "Engine_Reflection.h"
#include "Engine_RTTR_Metadata.h"
#include "Game.h"

/* Engine 밖(Client)에서 RTTR 없이 타입·property·메타데이터를 Registry에 등록하는 경로.
 - 타입마다 ENGINE_REFLECTION_REGISTRATION(Name) { ... } 블록을 그 타입의 등록 파일에 둔다. 블록은 모듈이 로드될 때
   그 모듈의 목록에 모이고, 모듈 진입점이 Collect_ModuleReflection으로 batch에 담아 Game::Register_ReflectionDescriptors로 넘긴다.
 - 메타데이터 키는 Engine_RTTR_Metadata.h의 Meta_Key_Type을 쓴다. Meta는 바로 앞 Property에 붙고,
   Registry가 Engine RTTR 메타데이터와 같은 규칙으로 검사한다(모르는 키·잘못된 값 타입은 등록 실패).
 - ReadOnly(name, getter)는 저장하지 않는 실행 중 값이다. Inspector에 회색으로만 보인다(Engine의 RTTR property_readonly와 같음). */

namespace Engine
{
	namespace Reflection_Detail
	{
		template <typename T, typename... Ts>
		consteval REFLECTION_VALUE_TYPE Find_ValueType(std::variant<Ts...>*)
		{
			constexpr Bool matches[] = { std::is_same_v<T, Ts>... };
			for (size_t index = 0; index < sizeof...(Ts); ++index) {
				if (matches[index])
					return static_cast<REFLECTION_VALUE_TYPE>(index);
			}
			return REFLECTION_VALUE_TYPE::NONE;
		}
	}

	// C++ value type -> REFLECTION_VALUE_TYPE (ReflectionValue::Storage index), NONE when unsupported.
	template <typename T>
	inline constexpr REFLECTION_VALUE_TYPE ReflectionValueType_v =
		Reflection_Detail::Find_ValueType<T>(static_cast<ReflectionValue::Storage*>(nullptr));

	// Runtime value backed by Owner's getter only: the Inspector shows it greyed out and nothing saves it.
	template <typename Owner, typename Getter>
	ReflectedPropertyDescriptor Make_ReadOnlyProperty(const Char* name, Getter getter)
	{
		using Value = std::remove_cvref_t<std::invoke_result_t<Getter, const Owner&>>;
		constexpr REFLECTION_VALUE_TYPE valueType = ReflectionValueType_v<Value>;
		static_assert(valueType != REFLECTION_VALUE_TYPE::NONE && valueType != REFLECTION_VALUE_TYPE::OBJECT_REF,
			"Read-only property value type is not a plain ReflectionValue type");

		ReflectedPropertyDescriptor property;
		property.info.registeredName = name;
		property.info.valueType = valueType;
		property.info.dataTag = valueType == REFLECTION_VALUE_TYPE::COLOR ? Data_Tag::Color : Data_Tag::NoneTag;
		property.info.assetType = Asset_Type_Key::NoneAsset;
		property.read = [getter](Object& target, ReflectionValue& outValue) -> HRESULT {
			auto* owner = dynamic_cast<Owner*>(&target);
			if (!owner)
				return E_NOINTERFACE;
			outValue.data = (owner->*getter)();
			return S_OK;
		};
		return property;
	}

	// Saved, Inspector-editable value backed by Owner's getter/setter. The value type comes from the
	// getter; a setter returning HRESULT reports rejection, a void setter always succeeds.
	// Client의 ReflectedTypeBuilder와 Engine의 RTTR_Property가 같은 규칙으로 만든다.
	template <typename Owner, typename Getter, typename Setter>
	ReflectedPropertyDescriptor Make_ReflectedProperty(const Char* name, Getter getter, Setter setter)
	{
		using Value = std::remove_cvref_t<std::invoke_result_t<Getter, const Owner&>>;
		constexpr REFLECTION_VALUE_TYPE valueType = ReflectionValueType_v<Value>;
		static_assert(valueType != REFLECTION_VALUE_TYPE::NONE,
			"Property value type is not a ReflectionValue type");

		ReflectedPropertyDescriptor property;
		property.info.registeredName = name;
		property.info.valueType = valueType;
		if constexpr (valueType == REFLECTION_VALUE_TYPE::OBJECT_REF) {
			// GUID reference to another GameObject; Meta(ReferenceBase, ...) names the accepted base.
			property.info.dataTag = "ObjectRef";
			property.info.assetType = Asset_Type_Key::GameObject;
			property.info.saveDataKey = Save_Data_Key::ObjectReference;
		}
		else {
			property.info.dataTag = valueType == REFLECTION_VALUE_TYPE::COLOR ? Data_Tag::Color : Data_Tag::NoneTag;
			property.info.assetType = Asset_Type_Key::NoneAsset;
			property.info.saveDataKey = name;
		}
		property.info.isWritable = true;
		property.info.isSerializable = true;
		property.read = [getter](Object& target, ReflectionValue& outValue) -> HRESULT {
			auto* owner = dynamic_cast<Owner*>(&target);
			if (!owner)
				return E_NOINTERFACE;
			outValue.data = (owner->*getter)();
			return S_OK;
		};
		property.write = [setter](Object& target, const ReflectionValue& value) -> HRESULT {
			auto* owner = dynamic_cast<Owner*>(&target);
			const Value* typed = value.Try_Get<Value>();
			if (!owner || !typed)
				return E_INVALIDARG;
			if constexpr (std::is_same_v<std::invoke_result_t<Setter, Owner&, const Value&>, HRESULT>) {
				return (owner->*setter)(*typed);
			}
			else {
				(owner->*setter)(*typed);
				return S_OK;
			}
		};
		return property;
	}

	// Meta(key, value)의 값 모양: 글자는 std::string, 숫자는 Float.
	inline ReflectionValue Make_MetadataValue(const Char* text)
	{
		return ReflectionValue{ std::string{ text ? text : "" } };
	}

	template <typename Number> requires std::is_arithmetic_v<Number>
	ReflectionValue Make_MetadataValue(Number number)
	{
		return ReflectionValue{ static_cast<Float>(number) };
	}

	template <typename Owner>
	class ReflectedTypeBuilder final
	{
	public:
		ReflectedTypeBuilder(ReflectionDescriptorBatch& batch, const Char* registeredName,
			const Char* baseRegisteredName)
			: m_Batch{ batch }, m_TypeIndex{ batch.types.size() }
		{
			ReflectedTypeDescriptor descriptor;
			descriptor.info.registeredName = registeredName;
			descriptor.info.runtimeTypeId = Make_RuntimeTypeId<Owner>();
			descriptor.info.baseRegisteredNames.emplace_back(baseRegisteredName);
			descriptor.info.objectKind = std::is_base_of_v<Component, Owner>
				? REFLECTED_OBJECT_KIND::COMPONENT
				: REFLECTED_OBJECT_KIND::GAMEOBJECT;
			descriptor.cppType = &typeid(Owner);
			descriptor.isInstance = [](const Object& object) -> Bool {
				return dynamic_cast<const Owner*>(&object) != nullptr;
			};
			// An abstract type only takes a place in the base graph; it has no prototype.
			if constexpr (!std::is_abstract_v<Owner>)
				descriptor.createPrototype = []() -> Shared<Object> {
					return std::static_pointer_cast<Object>(Owner::Create(
						GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context()));
				};
			batch.types.push_back(std::move(descriptor));
		}

		template <typename Getter, typename Setter>
		ReflectedTypeBuilder& Property(const Char* name, Getter getter, Setter setter)
		{
			Descriptor().properties.push_back(Make_ReflectedProperty<Owner>(name, getter, setter));
			return *this;
		}

		template <typename Getter>
		ReflectedTypeBuilder& ReadOnly(const Char* name, Getter getter)
		{
			Descriptor().properties.push_back(Make_ReadOnlyProperty<Owner>(name, getter));
			return *this;
		}

		template <typename Value>
		ReflectedTypeBuilder& Meta(MetadataKeyType key, Value value)
		{
			return Add_Metadata(key, Make_MetadataValue(value));
		}

	private:
		ReflectedTypeDescriptor& Descriptor() { return m_Batch.types[m_TypeIndex]; }

		// Before the first Property the entry lands on the type; Registry has no type keys yet and rejects it.
		ReflectedTypeBuilder& Add_Metadata(MetadataKeyType key, ReflectionValue value)
		{
			ReflectedTypeDescriptor& descriptor = Descriptor();
			ReflectedMetadata& metadata = descriptor.properties.empty()
				? descriptor.metadata
				: descriptor.properties.back().metadata;
			metadata.emplace_back(key, std::move(value));
			return *this;
		}

		ReflectionDescriptorBatch& m_Batch;
		size_t m_TypeIndex{};
	};

	// Enum value as Registry stores it (batch.enums), e.g. AnimationState labels for AnimationPreset.
	template <typename Enum> requires std::is_enum_v<Enum>
	ReflectedEnumValue Reflect_EnumValue(const Char* name, Enum value)
	{
		return { name, static_cast<std::int64_t>(value) };
	}

	using ReflectionRegistrationFunction = void(*)(ReflectionDescriptorBatch&);

	// Not exported, so every module that includes this header keeps its own list.
	inline std::vector<ReflectionRegistrationFunction>& Get_ModuleReflectionRegistrations()
	{
		static std::vector<ReflectionRegistrationFunction> registrations;
		return registrations;
	}

	struct ModuleReflectionRegistrar final
	{
		explicit ModuleReflectionRegistrar(ReflectionRegistrationFunction registration)
		{
			Get_ModuleReflectionRegistrations().push_back(registration);
		}
	};

	inline void Collect_ModuleReflection(ReflectionDescriptorBatch& batch)
	{
		for (const ReflectionRegistrationFunction registration : Get_ModuleReflectionRegistrations())
			registration(batch);
	}
}

#define REFLECTION_ENUM_VALUE(value, name) Engine::Reflect_EnumValue(#name, value)

#define ENGINE_REFLECTION_REGISTRATION(Name) \
	static void Name##_Reflection(Engine::ReflectionDescriptorBatch& batch); \
	static const Engine::ModuleReflectionRegistrar Name##_ReflectionRegistrar{ &Name##_Reflection }; \
	static void Name##_Reflection(Engine::ReflectionDescriptorBatch& batch)
