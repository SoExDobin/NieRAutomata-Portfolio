#include "Engine_ID.h"

#include <atomic>
#include <charconv>
#include <combaseapi.h>
#include <cstdio>

namespace {
    constexpr std::uint64_t FnvPrime = 1099511628211ull;

    constexpr void Hash_Byte(std::uint64_t& hash, unsigned char value) noexcept {
        hash ^= value;
        hash *= FnvPrime;
    }

    constexpr void Hash_U64(std::uint64_t& first, std::uint64_t& second, std::uint64_t value) noexcept {
        for (int shift = 56; shift >= 0; shift -= 8) {
            const auto byte = static_cast<unsigned char>((value >> shift) & 0xffull);
            Hash_Byte(first, byte);
            Hash_Byte(second, static_cast<unsigned char>(byte ^ 0xa5u));
        }
    }

    constexpr std::uint64_t Finalize_Hash(std::uint64_t value) noexcept {
        value ^= value >> 33;
        value *= 0xff51afd7ed558ccdull;
        value ^= value >> 33;
        value *= 0xc4ceb9fe1a85ec53ull;
        value ^= value >> 33;
        return value;
    }

    constexpr Engine::ObjectGuid Derive_ChildObjectGuid_Impl(
        Engine::ObjectGuid parentObjectGuid,
        std::wstring_view stableChildKey,
        std::string_view registeredName) noexcept {
        if (!parentObjectGuid.Is_Valid() || stableChildKey.empty() || registeredName.empty())
            return {};

        std::uint64_t high = 14695981039346656037ull;
        std::uint64_t low = 7809847782465536322ull;
        Hash_U64(high, low, parentObjectGuid.high);
        Hash_U64(high, low, parentObjectGuid.low);
        Hash_Byte(high, 0xffu);
        Hash_Byte(low, 0x5au);

        for (const wchar_t codeUnit : stableChildKey) {
            const auto value = static_cast<std::uint32_t>(codeUnit);
            for (int shift = 24; shift >= 0; shift -= 8) {
                const auto byte = static_cast<unsigned char>((value >> shift) & 0xffu);
                Hash_Byte(high, byte);
                Hash_Byte(low, static_cast<unsigned char>(byte ^ 0x3cu));
            }
        }

        Hash_Byte(high, 0xfeu);
        Hash_Byte(low, 0xc3u);
        for (const unsigned char byte : registeredName) {
            Hash_Byte(high, byte);
            Hash_Byte(low, static_cast<unsigned char>(byte ^ 0x96u));
        }

        const Engine::ObjectGuid derived{Finalize_Hash(high), Finalize_Hash(low)};
        return derived.Is_Valid() ? derived : Engine::ObjectGuid{};
    }

    constexpr Engine::ObjectGuid ChildGuidGoldenVector = Derive_ChildObjectGuid_Impl(
        {0x0123456789abcdefull, 0xfedcba9876543210ull}, L"Pl0000Body", "Pl0000Body");
    static_assert(ChildGuidGoldenVector.high == 0x005bedbacb415155ull &&
                  ChildGuidGoldenVector.low == 0x66558911352d1967ull);

    template <typename Tag>
    std::string Guid_ToString(Engine::Guid<Tag> guid) {
        char buffer[33]{};
        std::snprintf(buffer, sizeof(buffer), "%016llx%016llx",
                      static_cast<unsigned long long>(guid.high),
                      static_cast<unsigned long long>(guid.low));
        return buffer;
    }

    template <typename Tag>
    bool Guid_TryParse(std::string_view value, Engine::Guid<Tag>& outGuid) noexcept {
        if (value.size() != 32)
            return false;

        Engine::Guid<Tag> parsed{};
        const auto highResult = std::from_chars(value.data(), value.data() + 16, parsed.high, 16);
        const auto lowResult = std::from_chars(value.data() + 16, value.data() + 32, parsed.low, 16);
        if (highResult.ec != std::errc{} || highResult.ptr != value.data() + 16 ||
            lowResult.ec != std::errc{} || lowResult.ptr != value.data() + 32 ||
            !parsed.Is_Valid()) {
            return false;
        }

        outGuid = parsed;
        return true;
    }

	template <typename Tag>
	Engine::Guid<Tag> Create_Guid() noexcept {
		GUID nativeGuid{};
		if (FAILED(CoCreateGuid(&nativeGuid)))
			return {};

		const std::uint64_t high =
			(static_cast<std::uint64_t>(nativeGuid.Data1) << 32) |
			(static_cast<std::uint64_t>(nativeGuid.Data2) << 16) |
			static_cast<std::uint64_t>(nativeGuid.Data3);

		std::uint64_t low{};
		for (const unsigned char byte : nativeGuid.Data4)
			low = (low << 8) | byte;

		return {high, low};
	}
}

Engine::ObjectGuid Engine::Create_ObjectGuid() noexcept {
	return Create_Guid<ObjectGuidTag>();
}

Engine::AssetGuid Engine::Create_AssetGuid() noexcept {
	return Create_Guid<AssetGuidTag>();
}

Engine::PrefabGuid Engine::Create_PrefabGuid() noexcept {
	return Create_Guid<PrefabGuidTag>();
}

Engine::ObjectGuid Engine::Derive_ChildObjectGuid(
    ObjectGuid parentObjectGuid,
    std::wstring_view stableChildKey,
    std::string_view registeredName) noexcept {
    return Derive_ChildObjectGuid_Impl(parentObjectGuid, stableChildKey, registeredName);
}

Engine::RuntimeObjectId Engine::Create_RuntimeObjectId() noexcept {
    static std::atomic<RuntimeObjectId> nextId{1};
    return nextId.fetch_add(1, std::memory_order_relaxed);
}

std::string Engine::To_String(ObjectGuid guid) {
    return Guid_ToString(guid);
}

std::string Engine::To_String(AssetGuid guid) {
	return Guid_ToString(guid);
}

std::string Engine::To_String(PrefabGuid guid) {
	return Guid_ToString(guid);
}

bool Engine::Try_Parse_ObjectGuid(std::string_view value, ObjectGuid& outGuid) noexcept {
    return Guid_TryParse(value, outGuid);
}

bool Engine::Try_Parse_AssetGuid(std::string_view value, AssetGuid& outGuid) noexcept {
	return Guid_TryParse(value, outGuid);
}

bool Engine::Try_Parse_PrefabGuid(std::string_view value, PrefabGuid& outGuid) noexcept {
	return Guid_TryParse(value, outGuid);
}
