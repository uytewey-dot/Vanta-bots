#include "../../SDK/FortniteProfiles.h"

#include <array>
#include <cstdio>
#include <limits>

namespace P = SDK::FortniteProfiles;
static_assert(P::Profile2830.ProcessEventVft == 0x4C);
static_assert(P::Profile3141.ProcessEventVft == 0x4F);
static_assert(P::Profile3211.ProcessEventVft == 0x46);
static_assert(P::Profile2830.NetDriverSize == 0x7D8);
static_assert(P::Profile3211.NetDriverSize == 0x898);
static_assert(P::Profile3211.ObjectNameOffset == 8);
static_assert(P::Profile3211.ObjectFlagsOffset == 0x18);
static_assert(P::Profile3211.ObjectItemObjectOffset == 0x10);
static_assert(P::Profile3211.StructChildPropertiesOffset == 0xA0);
static_assert(P::Profile3211.PropertyOffsetOffset == 0x64);
static_assert(P::Profile3211.PropertyArrayDimOffset == 0x2C);
static_assert(P::Profile3211.NameFunctionAllocatesOutput);

int main()
{
    int Failures = 0;
    auto Check = [&](bool Value, const char* What) {
        if (!Value) { std::fprintf(stderr, "FAILED: %s\n", What); ++Failures; }
    };
    const auto Infinity = std::numeric_limits<double>::infinity();
    for (const auto* Profile : P::Profiles)
    {
        Check(P::FindProfile(Profile->Version) == Profile, "select exact profile");
        Check(P::IsSupportedBuild(Profile->Version, Profile->EngineVersion,
            Profile->Changelist, true), "accept pinned build");
        Check(!P::IsSupportedBuild(Profile->Version, Profile->EngineVersion,
            Profile->Changelist, false), "reject incomplete identity, including 28.30");
        Check(!P::IsSupportedBuild(Profile->Version, Profile->EngineVersion,
            Profile->Changelist + 1, true), "reject adjacent changelist");
        Check(!P::IsSupportedBuild(Profile->Version, Profile->EngineVersion + .1,
            Profile->Changelist, true), "reject engine mismatch");
        Check(!P::IsSupportedBuild(Profile->Version, Profile->EngineVersion,
            99999999, false), "reject synthetic metadata");
        Check(P::FindProfile(std::nextafter(Profile->Version, Infinity)) == nullptr,
            "do not round adjacent versions into a profile");

        // The 32.11 name precedes the encoded index; older names precede padding.
        // Copy into the exact live field and preserve every neighbouring byte.
        std::array<std::uint8_t, 0x28> Object;
        Object.fill(0xA5);
        const std::uint32_t Name = 0x12345678;
        P::CopyName(Object.data() + Profile->ObjectNameOffset, &Name, true);
        std::uint32_t Copied = 0;
        std::memcpy(&Copied, Object.data() + Profile->ObjectNameOffset, 4);
        Check(Copied == Name, "copy reflected name");
        for (std::size_t I = 0; I < Object.size(); ++I)
            if (I < Profile->ObjectNameOffset || I >= Profile->ObjectNameOffset + 4)
                Check(Object[I] == 0xA5, "preserve adjacent object/index/flags storage");
    }
    Check(P::IsSupportedBuild(30.0, 5.4, 0, false), "preserve unprofiled legacy path");
    // These releases use the existing signature/reflection path, not the newer
    // fixed-RVA shipping profiles. Selecting bots must not route them to 28.30.
    for (double Version : {12.00, 12.10, 12.41, 12.61, 14.00, 14.60, 19.10, 24.20, 26.30})
    {
        Check(!P::IsProfiledVersion(Version), "legacy bot releases keep dynamic SDK discovery");
        Check(P::IsSupportedBuild(Version, 5.0, 0, false), "legacy bot releases pass the SDK guard");
    }
    for (double Version : {31.40, 31.42, 32.10, 32.12, 33.0, -1.0, Infinity,
            std::numeric_limits<double>::quiet_NaN()})
        Check(!P::IsSupportedBuild(Version, 5.5, 38202817, true), "reject unsupported identity");
    Check(std::strcmp(P::Profile2830.IrisReplicationBridgeClassName,
        "ActorReplicationBridge") == 0, "28.30 selects its actual bridge class");
    Check(!P::Profile2830.EncodedMetadata && !P::Profile3141.EncodedMetadata &&
        P::Profile3211.EncodedMetadata, "decoding is restricted to the exact 32.11 profile");
    Check(P::ValidateObjectArrayCounts(65537, 131072, 2, 2), "accept second chunk");
    Check(!P::ValidateObjectArrayCounts(65537, 131072, 1, 2), "reject missing chunk");
    return Failures ? 1 : 0;
}
