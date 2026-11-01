#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

#include "Engine_ID.h"
#include "Engine_Typedef.h"

namespace Engine
{
	class Object;

	// RuntimeTypeId of every reflected type, Engine and Client alike. Process-local, never saved.
	template <typename T>
	constexpr RuntimeTypeId Make_RuntimeTypeId() noexcept
	{
#if defined(_MSC_VER)
		constexpr std::string_view signature = __FUNCSIG__;
#else
		constexpr std::string_view signature = __PRETTY_FUNCTION__;
#endif
		RuntimeTypeId hash = sizeof(RuntimeTypeId) == 8
			? static_cast<RuntimeTypeId>(14695981039346656037ull)
			: static_cast<RuntimeTypeId>(2166136261u);
		constexpr RuntimeTypeId prime = sizeof(RuntimeTypeId) == 8
			? static_cast<RuntimeTypeId>(1099511628211ull)
			: static_cast<RuntimeTypeId>(16777619u);
		for (const char character : signature) {
			hash ^= static_cast<unsigned char>(character);
			hash *= prime;
		}
		return hash == 0 ? RuntimeTypeId{ 1 } : hash; // 0 means "not registered"
	}

	enum class REFLECTED_OBJECT_KIND : uint8
	{
		VALUE,
		OBJECT,
		GAMEOBJECT,
		COMPONENT,
	};

	enum class REFLECTION_VALUE_TYPE : uint8
	{
		NONE,
		BOOL,
		INT32,
		UINT32,
		FLOAT,
		DOUBLE,
		STRING,
		WSTRING,
		VECTOR3,
		FLOAT3,
		COLOR,
		FLOAT4,
		ANIMATION_PRESET,
		OBJECT_REF,
		TAG_COUNT_LIST,
	};

	struct AnimationPresetClip final
	{
		std::wstring relativePath;
		std::vector<std::string> states;
		Bool operator==(const AnimationPresetClip&) const = default;
	};

	struct AnimationPresetSnapshot final
	{
		uint32 schemaVersion{ 2 };
		std::string animationEnum;
		std::vector<AnimationPresetClip> animations;
		Bool operator==(const AnimationPresetSnapshot&) const = default;

		Bool Is_Empty() const noexcept
		{
			return animationEnum.empty() && animations.empty();
		}
	};

	// One row of a TAG_COUNT_LIST: an asset tag (the property's AssetType) and how many of it.
	struct TagCount final
	{
		std::wstring tag;
		uint32 count{};
		Bool operator==(const TagCount&) const = default;
	};

	struct ReflectionValue final
	{
		using Storage = std::variant<std::monostate, Bool, int32, uint32, Float, Double,
			std::string, std::wstring, Vector3, Float3, Color, Float4,
			AnimationPresetSnapshot, ObjectGuid, std::vector<TagCount>>;

		Storage data{};

		REFLECTION_VALUE_TYPE Get_Type() const noexcept
		{
			return static_cast<REFLECTION_VALUE_TYPE>(data.index());
		}

		Bool Is_Valid() const noexcept
		{
			return Get_Type() != REFLECTION_VALUE_TYPE::NONE;
		}

		template <typename T>
		const T* Try_Get() const noexcept
		{
			return std::get_if<T>(&data);
		}

		template <typename T>
		T* Try_Get() noexcept
		{
			return std::get_if<T>(&data);
		}
	};

	struct ReflectedTypeInfo final
	{
		std::string registeredName;
		RuntimeTypeId runtimeTypeId{};
		std::vector<std::string> baseRegisteredNames;
		REFLECTED_OBJECT_KIND objectKind{ REFLECTED_OBJECT_KIND::VALUE };
	};

	struct ReflectedPropertyInfo final
	{
		std::string registeredName;
		REFLECTION_VALUE_TYPE valueType{ REFLECTION_VALUE_TYPE::NONE };
		std::string dataTag;
		std::string assetType;
		std::string saveDataKey;
		std::string expectedBaseRegisteredName;
		std::string displayName;
		std::optional<Float> minValue;
		std::optional<Float> maxValue;
		Bool isReadable{ true };
		Bool isWritable{};
		Bool isSerializable{};
	};

	struct ReflectedEnumValue final
	{
		std::string name;
		std::int64_t value{};
	};

	struct ReflectedEnumInfo final
	{
		std::string registeredName;
		std::string category;
		std::vector<ReflectedEnumValue> values;
	};

	// Engine_RTTR_Metadata.h key + value. Registry normalizes these into info the same way it reads
	// Engine RTTR metadata, and rejects unknown keys or wrong value types.
	using ReflectedMetadata = std::vector<std::pair<std::string, ReflectionValue>>;

	struct ReflectedPropertyDescriptor final
	{
		ReflectedPropertyInfo info;
		ReflectedMetadata metadata;
		std::function<HRESULT(Object&, ReflectionValue&)> read;
		std::function<HRESULT(Object&, const ReflectionValue&)> write;
	};

	struct ReflectedTypeDescriptor final
	{
		ReflectedTypeInfo info;
		ReflectedMetadata metadata;
		// Lets Registry find an object's type without RTTR: exact C++ type, then is-a for unregistered subclasses.
		const std::type_info* cppType{};
		Bool(*isInstance)(const Object&){};
		std::function<Shared<Object>()> createPrototype;
		std::vector<ReflectedPropertyDescriptor> properties;
	};

	struct ReflectionDescriptorBatch final
	{
		std::vector<ReflectedTypeDescriptor> types;
		std::vector<ReflectedEnumInfo> enums;
	};
}
