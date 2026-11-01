#pragma once

#include <cstdint>
#include <string_view>
#include <string>

#include "Engine_Macro.h"

NS_BEGIN(Engine)

using RuntimeTypeId = std::uintptr_t;
using RuntimeObjectId = std::uint64_t;

template <typename Tag>
struct Guid final {
    std::uint64_t high{};
    std::uint64_t low{};

    constexpr bool Is_Valid() const noexcept { return high != 0 || low != 0; }
    constexpr explicit operator bool() const noexcept { return Is_Valid(); }
    constexpr auto operator<=>(const Guid&) const noexcept = default;
};

struct ObjectGuidTag final {};
struct AssetGuidTag final {};
struct PrefabGuidTag final {};

using ObjectGuid = Guid<ObjectGuidTag>;
using AssetGuid = Guid<AssetGuidTag>;
using PrefabGuid = Guid<PrefabGuidTag>;

struct GuidHash final {
    template <typename Tag>
    std::size_t operator()(const Guid<Tag>& guid) const noexcept {
        const std::uint64_t mixed = guid.high ^ (guid.low + 0x9e3779b97f4a7c15ull +
            (guid.high << 6) + (guid.high >> 2));
        return static_cast<std::size_t>(mixed ^ (mixed >> 32));
    }
};

ENGINE_DLL ObjectGuid Create_ObjectGuid() noexcept;
ENGINE_DLL AssetGuid Create_AssetGuid() noexcept;
ENGINE_DLL PrefabGuid Create_PrefabGuid() noexcept;
ENGINE_DLL ObjectGuid Derive_ChildObjectGuid(ObjectGuid parentObjectGuid,
    std::wstring_view stableChildKey, std::string_view registeredName) noexcept;
ENGINE_DLL RuntimeObjectId Create_RuntimeObjectId() noexcept;
ENGINE_DLL std::string To_String(ObjectGuid guid);
ENGINE_DLL std::string To_String(AssetGuid guid);
ENGINE_DLL std::string To_String(PrefabGuid guid);
ENGINE_DLL bool Try_Parse_ObjectGuid(std::string_view value, ObjectGuid& outGuid) noexcept;
ENGINE_DLL bool Try_Parse_AssetGuid(std::string_view value, AssetGuid& outGuid) noexcept;
ENGINE_DLL bool Try_Parse_PrefabGuid(std::string_view value, PrefabGuid& outGuid) noexcept;

NS_END
