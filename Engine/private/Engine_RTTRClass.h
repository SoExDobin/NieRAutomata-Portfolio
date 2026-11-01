#pragma once

#include <type_traits>
#include <typeinfo>
#include <vector>

#pragma warning(push)
#pragma warning(disable : 26495)
#pragma warning(disable : 26439)
#pragma warning(disable : 26819)
#include <rttr/registration>
#pragma warning(pop)

#include "Engine_Reflection.h"
#include "Engine_ReflectionRegistration.h"

// Per-file RTTR registration block (unique names so unity builds can merge rttr_generated files).
#define RTTR_REGISTRATION_NAMED(NAME)                                          \
static void rttr_auto_register_##NAME();                                       \
namespace {                                                                    \
    struct rttr_reg_##NAME {                                                   \
        rttr_reg_##NAME() { rttr_auto_register_##NAME(); }                     \
    };                                                                         \
}                                                                              \
static const rttr_reg_##NAME global_reg_##NAME;                                \
static void rttr_auto_register_##NAME()

/* Engine 내부 전용 RTTR 등록. public 헤더는 RTTR을 보지 않으므로 클래스 선언의 RTTR 매크로 대신
   Engine의 *_rttr.cpp가 RTTR_Class로 등록하면서 base 이름, C++ 타입, Object에서 T로 가는 경로를 함께 남긴다.
   Registry가 이 기록으로 상속 관계, 객체의 실제 타입, property를 읽고 쓸 instance를 만든다. */

namespace Engine
{
	class Object;
	class GameObject;
	class Component;

	struct RTTRClassRecord final
	{
		const Char* registeredName;
		const Char* baseRegisteredName;		// 가장 가까운 등록 조상, Object만 비어 있다.
		rttr::type reflectedType;
		const std::type_info* cppType;
		RuntimeTypeId runtimeTypeId;
		REFLECTED_OBJECT_KIND objectKind;
		Bool(*isInstance)(const Object&);
		rttr::instance(*toInstance)(Object&);
	};

	// Engine 모듈 하나에만 있는 목록(정적 초기화 순서와 무관하게 첫 사용 때 생긴다).
	inline std::vector<RTTRClassRecord>& Get_RTTRClassRecords()
	{
		static std::vector<RTTRClassRecord> records;
		return records;
	}

	template <typename T>
	rttr::registration::class_<T> RTTR_Class(const Char* registeredName, const Char* baseRegisteredName)
	{
		constexpr REFLECTED_OBJECT_KIND objectKind = std::is_base_of_v<GameObject, T>
			? REFLECTED_OBJECT_KIND::GAMEOBJECT
			: std::is_base_of_v<Component, T>
				? REFLECTED_OBJECT_KIND::COMPONENT
				: REFLECTED_OBJECT_KIND::OBJECT;

		Get_RTTRClassRecords().push_back(RTTRClassRecord{
			registeredName,
			baseRegisteredName,
			rttr::type::get<T>(),
			&typeid(T),
			Make_RuntimeTypeId<T>(),
			objectKind,
			[](const Object& object) -> Bool { return dynamic_cast<const T*>(&object) != nullptr; },
			[](Object& object) -> rttr::instance {
				T* typed = dynamic_cast<T*>(&object);
				return typed ? rttr::instance{ *typed } : rttr::instance{};
			} });
		return rttr::registration::class_<T>(registeredName);
	}

	/* RTTR는 setter의 HRESULT를 버리고 enum을 숫자로 바꾸지 못한다. 거부될 수 있는 값(ModelTag, Camera Target)과
	   enum 값(UI Anchor)은 같은 *_rttr.cpp에서 RTTR_Property로 등록한다. Client property와 같은 규칙으로 만들어
	   거부된 쓰기는 실패로 돌아가고, Registry가 C++ 타입으로 그 타입의 RTTR 기록에 붙인다. */
	struct RTTRDescriptorProperty final
	{
		const std::type_info* cppType;
		ReflectedPropertyDescriptor property;
	};

	inline std::vector<RTTRDescriptorProperty>& Get_RTTRDescriptorProperties()
	{
		static std::vector<RTTRDescriptorProperty> properties;
		return properties;
	}

	class RTTRDescriptorPropertyBuilder final
	{
	public:
		explicit RTTRDescriptorPropertyBuilder(size_t index) : m_Index{ index } {}

		template <typename Value>
		RTTRDescriptorPropertyBuilder& Meta(MetadataKeyType key, Value value)
		{
			Get_RTTRDescriptorProperties()[m_Index].property.metadata.emplace_back(key, Make_MetadataValue(value));
			return *this;
		}

	private:
		size_t m_Index{};
	};

	template <typename T, typename Getter, typename Setter>
	RTTRDescriptorPropertyBuilder RTTR_Property(const Char* name, Getter getter, Setter setter)
	{
		std::vector<RTTRDescriptorProperty>& properties = Get_RTTRDescriptorProperties();
		properties.push_back({ &typeid(T), Make_ReflectedProperty<T>(name, getter, setter) });
		return RTTRDescriptorPropertyBuilder{ properties.size() - 1 };
	}
}
