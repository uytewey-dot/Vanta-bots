#pragma once

#include "Fortnite3141Profile.h"
#include "Fortnite3211Decode.h"

namespace SDK::FortniteProfiles
{
    // Common storage dimensions in the three shipping dumps. Field locations
    // and value encodings are selected separately below.
    using Fortnite3141Profile::NameSize;
    using Fortnite3141Profile::ObjectSize;
    using Fortnite3141Profile::ObjectArraySize;
    using Fortnite3141Profile::ObjectItemSize;
    using Fortnite3141Profile::ElementsPerChunk;
    using Fortnite3141Profile::ValidateObjectArrayCounts;

    struct FShippingProfile
    {
        double Version;
        double EngineVersion;
        std::uint64_t Changelist;
        const wchar_t* BuildString;
        const char* VersionLabel;
        std::uint32_t GObjectsRva;
        std::uint32_t AppendStringRva;
        std::uint32_t GNamesRva;
        std::uint32_t GWorldRva;
        std::uint32_t ProcessEventRva;
        std::uint32_t ProcessEventVft;
        std::uint32_t ReallocRva = 0;
        std::uint32_t ObjectNameOffset = 0x18;
        std::uint32_t ObjectFlagsOffset = 0x08;
        std::uint32_t ObjectItemObjectOffset = 0;
        std::uint32_t ClassCastFlagsOffset = 0xD8;
        std::uint32_t ClassDefaultObjectOffset = 0x110;
        std::uint32_t FieldClassOffset = 0x08;
        std::uint32_t FieldClassCastFlagsOffset = 0x10;
        std::uint32_t FieldNextOffset = 0x18;
        std::uint32_t PropertyArrayDimOffset = 0x28;
        std::uint32_t PropertyElementSizeOffset = 0x2C;
        std::uint32_t PropertyFlagsOffset = 0x30;
        std::uint32_t PropertyOffsetOffset = 0x3C;
        std::uint32_t StructChildrenOffset = 0x48;
        std::uint32_t StructChildPropertiesOffset = 0x50;
        std::uint32_t StructPropertiesSizeOffset = 0x58;
        std::uint32_t NetDriverSize = 0x890;
        std::uint32_t NetDriverReplicationDriverOffset = 0x7F8;
        std::uint32_t GameModeReplicationSystemSelectorOffset = 0x314;
        std::uint32_t IrisReplicationSystemSize = 0x58;
        std::uint32_t IrisReplicationBridgeOffset = 0x38;
        const char* IrisReplicationBridgeClassName = "EngineReplicationBridge";
        bool EncodedMetadata = false;
        bool NameFunctionAllocatesOutput = false;
    };

    // Helix-Dev-Q/FortniteDumps@a90cd2ba76e22ffe55cea3fbf0812f592ec754f7:
    // Chapter 5 Season 1/SDK/5.4.0-31511038+++Fortnite+Release-28.30-FortniteGame.7z
    inline constexpr auto Profile2830 = [] {
        FShippingProfile P{28.30, 5.4, 31511038,
            L"5.4.0-31511038+++Fortnite+Release-28.30", "28.30",
            0x1176AB00, 0x01065210, 0x119692C0, 0x11791DF8, 0x03D1D36C, 0x4C};
        P.ReallocRva = 0x043DC21C;
        P.NetDriverSize = 0x7D8;
        P.NetDriverReplicationDriverOffset = 0x740;
        P.GameModeReplicationSystemSelectorOffset = 0x31C;
        P.IrisReplicationBridgeClassName = "ActorReplicationBridge";
        return P;
    }();

    inline constexpr FShippingProfile Profile3141{31.41, 5.5, 37324991,
        Fortnite3141Profile::BuildString, "31.41", 0x121F4380, 0x015799E4,
        0x123FBFC0, 0x1221C738, 0x017CCD78, 0x4F};

    // DauntingEmperor/32.11-SDK@b91ba8a8b721450e7ff5c2ad5c6faaa1b6fea066:
    // SDK/Basic.hpp and SDK/CoreUObject_classes.hpp (shipping, not UEFN).
    inline constexpr auto Profile3211 = [] {
        FShippingProfile P{32.11, 5.5, 38202817,
            L"5.5.0-38202817+++Fortnite+Release-32.11", "32.11",
            0x12EAF270, 0x0175D6E0, 0, 0x12ED8E38, 0x01655C14, 0x46};
        P.ObjectNameOffset = 0x08;
        P.ObjectFlagsOffset = 0x18;
        P.ObjectItemObjectOffset = 0x10;
        P.ClassCastFlagsOffset = 0xB8;
        P.ClassDefaultObjectOffset = 0xF0;
        P.FieldClassOffset = 0x18;
        P.FieldClassCastFlagsOffset = 0x28;
        P.FieldNextOffset = 0x10;
        P.PropertyArrayDimOffset = 0x2C;
        P.PropertyElementSizeOffset = 0x60;
        P.PropertyFlagsOffset = 0x58;
        P.PropertyOffsetOffset = 0x64;
        P.StructChildrenOffset = 0x78;
        P.StructChildPropertiesOffset = 0xA0;
        P.StructPropertiesSizeOffset = 0x60;
        P.NetDriverSize = 0x898;
        P.GameModeReplicationSystemSelectorOffset = 0x32C;
        P.EncodedMetadata = true;
        P.NameFunctionAllocatesOutput = true;
        return P;
    }();

    inline constexpr const FShippingProfile* Profiles[] = {
        &Profile2830, &Profile3141, &Profile3211
    };

    inline const FShippingProfile* FindProfile(double Version) noexcept
    {
        if (std::isfinite(Version))
            for (const auto* P : Profiles)
                if (P->Version == Version)
                    return P;
        return nullptr;
    }

    inline bool IsProfiledVersion(double Version) noexcept
    {
        return FindProfile(Version) != nullptr;
    }

    inline bool MatchesProfileBuild(double Version, double EngineVersion,
        std::uint64_t Changelist, bool Exact) noexcept
    {
        const auto* P = FindProfile(Version);
        return P && Exact && std::isfinite(EngineVersion) &&
            P->EngineVersion == EngineVersion && P->Changelist == Changelist;
    }

    inline bool IsSupportedBuild(double Version, double EngineVersion,
        std::uint64_t Changelist, bool Exact) noexcept
    {
        if (!std::isfinite(Version) || Version < 0 ||
            !std::isfinite(EngineVersion) || EngineVersion <= 0)
            return false;
        if (FindProfile(Version))
            return MatchesProfileBuild(Version, EngineVersion, Changelist, Exact);
        return Version < 31.0;
    }

    inline void CopyName(void* Destination, const void* Source, bool Profiled) noexcept
    {
        Fortnite3141Profile::CopyName(Destination, Source, Profiled);
    }
}
