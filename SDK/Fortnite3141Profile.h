#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

// Shipping Fortnite SDK dump, not the UEFN editor SDK:
// https://github.com/Ducki67/Fn-SDK/blob/f195a7101664deda221194ab0f826865f9d7e05b/31.41%20SDK.rar
// Archive added in commit 891729d9e9cbca6dc7315e164cc25b9dfe997cee.
// Archive blob: 78527b51768a0882f57799d5fb93d6199261ce6b.
// CppSDK/SDK.hpp identifies 5.5.0-37324991+++Fortnite+Release-31.41.
// RVAs come from SDK/Basic.hpp; layouts from Basic.hpp and CoreUObject_classes.hpp.
// These constants require runtime image and layout validation before engine calls.
namespace SDK::Fortnite3141Profile
{
    inline constexpr double TargetFortniteVersion = 31.41;
    inline constexpr double TargetEngineVersion = 5.5;
    inline constexpr std::uint64_t TargetChangelist = 37324991;
    inline constexpr wchar_t BuildString[] =
        L"5.5.0-37324991+++Fortnite+Release-31.41";

    inline constexpr std::uint32_t GObjectsRva = 0x121F4380;
    inline constexpr std::uint32_t AppendStringRva = 0x015799E4;
    inline constexpr std::uint32_t GNamesRva = 0x123FBFC0;
    inline constexpr std::uint32_t GWorldRva = 0x1221C738;
    inline constexpr std::uint32_t ProcessEventRva = 0x017CCD78;
    inline constexpr std::uint32_t ProcessEventVft = 0x4F;

    inline constexpr std::uint32_t ObjectArraySize = 0x20;
    inline constexpr std::uint32_t ObjectArrayObjectsOffset = 0x00;
    inline constexpr std::uint32_t ObjectArrayMaxElementsOffset = 0x10;
    inline constexpr std::uint32_t ObjectArrayNumElementsOffset = 0x14;
    inline constexpr std::uint32_t ObjectArrayMaxChunksOffset = 0x18;
    inline constexpr std::uint32_t ObjectArrayNumChunksOffset = 0x1C;
    inline constexpr std::uint32_t ObjectItemSize = 0x18;
    inline constexpr std::uint32_t ObjectItemObjectOffset = 0x00;
    inline constexpr std::uint32_t ElementsPerChunk = 0x10000;
    inline constexpr std::int32_t MaximumObjectCount = 0x1000000;
    inline constexpr std::int32_t MaximumChunkCount =
        MaximumObjectCount / static_cast<std::int32_t>(ElementsPerChunk);

    inline constexpr std::uint32_t ObjectSize = 0x28;
    inline constexpr std::uint32_t ObjectFlagsOffset = 0x08;
    inline constexpr std::uint32_t ObjectIndexOffset = 0x0C;
    inline constexpr std::uint32_t ObjectClassOffset = 0x10;
    inline constexpr std::uint32_t ObjectNameOffset = 0x18;
    inline constexpr std::uint32_t ObjectOuterOffset = 0x20;
    inline constexpr std::uint32_t NameSize = 0x04;

    inline constexpr std::uint32_t FieldSize = 0x28;
    inline constexpr std::uint32_t FieldClassOffset = 0x08;
    inline constexpr std::uint32_t FieldOwnerOffset = 0x10;
    inline constexpr std::uint32_t FieldNextOffset = 0x18;
    inline constexpr std::uint32_t FieldNameOffset = 0x20;
    inline constexpr std::uint32_t FieldFlagsOffset = 0x24;
    inline constexpr std::uint32_t PropertySize = 0x68;
    inline constexpr std::uint32_t PropertyArrayDimOffset = 0x28;
    inline constexpr std::uint32_t PropertyElementSizeOffset = 0x2C;
    inline constexpr std::uint32_t PropertyFlagsOffset = 0x30;
    inline constexpr std::uint32_t PropertyOffsetOffset = 0x3C;
    inline constexpr std::uint32_t BoolByteOffsetOffset = 0x69;
    inline constexpr std::uint32_t BoolByteMaskOffset = 0x6A;
    inline constexpr std::uint32_t BoolFieldMaskOffset = 0x6B;

    inline constexpr std::uint32_t StructSize = 0xB0;
    inline constexpr std::uint32_t StructBaseChainOffset = 0x30;
    inline constexpr std::uint32_t StructSuperOffset = 0x40;
    inline constexpr std::uint32_t StructChildrenOffset = 0x48;
    inline constexpr std::uint32_t StructChildPropertiesOffset = 0x50;
    inline constexpr std::uint32_t StructPropertiesSizeOffset = 0x58;
    inline constexpr std::uint32_t ClassCastFlagsOffset = 0xD8;
    inline constexpr std::uint32_t ClassDefaultObjectOffset = 0x110;
    inline constexpr std::uint32_t FunctionSize = 0xE0;
    inline constexpr std::uint32_t FunctionFlagsOffset = 0xB0;
    inline constexpr std::uint32_t FunctionExecOffset = 0xD8;

    // Engine_classes.hpp exposes ReplicationDriver but does not expose the
    // native ReplicationSystem pointer hidden in the following padding.
    inline constexpr std::uint32_t NetDriverSize = 0x890;
    inline constexpr std::uint32_t NetDriverReplicationDriverOffset = 0x7F8;
    inline constexpr std::uint32_t GameModeReplicationSystemSelectorOffset = 0x314;
    // IrisCore_classes.hpp: UReplicationSystem's reflected ReplicationBridge.
    inline constexpr std::uint32_t IrisReplicationSystemSize = 0x58;
    inline constexpr std::uint32_t IrisReplicationBridgeOffset = 0x38;

    inline bool IsTargetVersion(double FortniteVersion) noexcept
    {
        return std::isfinite(FortniteVersion) &&
            FortniteVersion == TargetFortniteVersion;
    }

    inline void CopyName(void* Destination, const void* Source,
        bool Target3141) noexcept
    {
        // The SDK wrapper retains two words for legacy ABI, but a reflected
        // shipping 31.41 FName occupies only one. Its following word can be
        // another reflected property and must not be read or overwritten.
        if (Destination != Source)
            std::memcpy(Destination, Source, Target3141 ? NameSize : 8);
    }

    inline bool ValidateObjectArrayCounts(std::int32_t NumElements,
        std::int32_t MaxElements, std::int32_t NumChunks,
        std::int32_t MaxChunks) noexcept
    {
        // SDK startup needs live objects; an empty or uninitialised array cannot
        // prove that the release layout is ready. Bounds also keep subsequent
        // pointer-array validation finite before any pointer is dereferenced.
        if (NumElements <= 0 || MaxElements < NumElements ||
            MaxElements > MaximumObjectCount || NumChunks <= 0 ||
            MaxChunks < NumChunks || MaxChunks > MaximumChunkCount)
            return false;

        const auto RequiredChunks = [](std::int32_t Count) noexcept
        {
            return (static_cast<std::uint32_t>(Count) + ElementsPerChunk - 1) /
                ElementsPerChunk;
        };
        // Preallocated pools may contain more chunks than the live count needs.
        return RequiredChunks(NumElements) <= static_cast<std::uint32_t>(NumChunks) &&
            RequiredChunks(MaxElements) <= static_cast<std::uint32_t>(MaxChunks);
    }

    inline bool MatchesTargetBuild(double FortniteVersion, double EngineVersion,
        std::uint64_t Changelist, bool ExactBuildIdentity) noexcept
    {
        return ExactBuildIdentity && IsTargetVersion(FortniteVersion) &&
            std::isfinite(EngineVersion) && EngineVersion == TargetEngineVersion &&
            Changelist == TargetChangelist;
    }

    inline bool IsSupportedBuild(double FortniteVersion, double EngineVersion,
        std::uint64_t Changelist, bool ExactBuildIdentity) noexcept
    {
        if (!std::isfinite(FortniteVersion) || FortniteVersion < 0.0 ||
            !std::isfinite(EngineVersion) || EngineVersion <= 0.0)
            return false;

        // Preserve the legacy range, including the pre-release 0.00 identity.
        // A metadata-only synthetic changelist cannot enable the 31.41 profile.
        return FortniteVersion < 31.0 || MatchesTargetBuild(FortniteVersion,
            EngineVersion, Changelist, ExactBuildIdentity);
    }
}
