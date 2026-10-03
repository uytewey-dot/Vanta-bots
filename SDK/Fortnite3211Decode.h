#pragma once

#include <cstdint>

// Offline metadata transforms for the shipping 5.5.0-38202817 build of Fortnite 32.11.
// Source: DauntingEmperor/32.11-SDK, commit b91ba8a8b721450e7ff5c2ad5c6faaa1b6fea066,
// SDK/Basic.hpp:46-64. These functions operate on values; they do not access memory.
// Callers must select the exact build, validate decoded addresses/counts, and only then
// dereference them. Encoded null pointers are nonzero, so decode before null checks.
namespace SDK::Fortnite3211Decode
{
    namespace Detail
    {
        constexpr std::uint64_t Decode64(std::uint64_t Stored, std::uint64_t Mask) noexcept
        {
            const std::uint64_t Value = Stored ^ Mask;
            return ~((Value >> 16U) | (Value << 48U));
        }

        constexpr std::uint32_t Decode32(std::uint32_t Stored, std::uint32_t Mask) noexcept
        {
            return ~(Stored ^ Mask);
        }
    }

    constexpr std::uint64_t DecodeObjectsArray(std::uint64_t Stored) noexcept
    {
        return Detail::Decode64(Stored, 0xE7F18B24ULL);
    }

    constexpr std::uint64_t DecodeObjectPtr(std::uint64_t Stored) noexcept
    {
        return Detail::Decode64(Stored, 0xD8BBB184ULL);
    }

    constexpr std::uint32_t DecodeNumElements(std::uint32_t Stored) noexcept
    {
        return Detail::Decode32(Stored, 0xEE9DB424U);
    }

    constexpr std::uint32_t DecodeObjectIndex(std::uint32_t Stored) noexcept
    {
        return Detail::Decode32(Stored, 0x693FB2C4U);
    }

    constexpr std::uint64_t DecodeFFieldClass(std::uint64_t Stored) noexcept
    {
        return Detail::Decode64(Stored, 0x1B35B3A4ULL);
    }

    constexpr std::uint64_t DecodeFieldPtr(std::uint64_t Stored) noexcept
    {
        return Detail::Decode64(Stored, 0xAC0E7F64ULL);
    }

    constexpr std::uint64_t DecodeClassCastFlags(std::uint64_t Stored) noexcept
    {
        return Detail::Decode64(Stored, 0x6525D444ULL);
    }

    constexpr std::uint32_t DecodeElementSize(std::uint32_t Stored) noexcept
    {
        return Detail::Decode32(Stored, 0x7D596064U);
    }

    constexpr std::uint32_t DecodePropertyOffset(std::uint32_t Stored) noexcept
    {
        return Detail::Decode32(Stored, 0x1FD9EFC4U);
    }

    constexpr std::uint64_t DecodePropertyFlags(std::uint64_t Stored) noexcept
    {
        return Detail::Decode64(Stored, 0xBBAE5624ULL);
    }

    constexpr std::uint32_t DecodeStructSize(std::uint32_t Stored) noexcept
    {
        return Detail::Decode32(Stored, 0x5BD22E24U);
    }
}
