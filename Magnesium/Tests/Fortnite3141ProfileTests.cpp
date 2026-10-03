#include "../../SDK/Fortnite3141Profile.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace Profile = SDK::Fortnite3141Profile;

static_assert(Profile::GObjectsRva == 0x121F4380);
static_assert(Profile::AppendStringRva == 0x015799E4);
static_assert(Profile::ProcessEventRva == 0x017CCD78);
static_assert(Profile::ProcessEventVft == 0x4F);
static_assert(Profile::NameSize == 4);
static_assert(Profile::ObjectSize == 0x28);
static_assert(Profile::PropertyOffsetOffset == 0x3C);
static_assert(Profile::FunctionExecOffset == 0xD8);

int main()
{
    int Failures = 0;
    auto Expect = [&Failures](bool Condition, const char* Description)
    {
        if (!Condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", Description);
            ++Failures;
        }
    };

    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double Infinity = std::numeric_limits<double>::infinity();
    constexpr std::uint64_t Changelist = Profile::TargetChangelist;

    Expect(Profile::IsTargetVersion(31.41), "recognise the exact target version");
    for (double Version : { -31.41, 0.0, 30.0, 31.0, 31.4, 31.42, 32.0,
            NaN, Infinity, -Infinity })
        Expect(!Profile::IsTargetVersion(Version), "reject a different or invalid target version");
    Expect(!Profile::IsTargetVersion(std::nextafter(31.41, Infinity)),
        "do not round another version into the target");

    Expect(Profile::MatchesTargetBuild(31.41, 5.5, Changelist, true),
        "accept the exact shipping build identity");
    Expect(!Profile::MatchesTargetBuild(31.41, 5.5, Changelist, false),
        "reject an unverified build identity");
    Expect(!Profile::MatchesTargetBuild(31.41, 5.5, 99999999, false),
        "reject the metadata fallback changelist");
    Expect(!Profile::MatchesTargetBuild(31.41, 5.5, 99999999, true),
        "a synthetic changelist cannot become exact");
    Expect(!Profile::MatchesTargetBuild(31.41, 5.5, Changelist + 1, true),
        "reject a different changelist");
    Expect(!Profile::MatchesTargetBuild(31.41, 5.4, Changelist, true),
        "reject a different engine ABI");
    Expect(!Profile::MatchesTargetBuild(31.40, 5.5, Changelist, true),
        "reject an adjacent release");

    for (double Version : { 0.0, 1.72, 2.5, 10.0, 27.11, 30.0, 30.99 })
        Expect(Profile::IsSupportedBuild(Version, 4.27, 0, false),
            "preserve legacy releases without an exact modern identity");
    Expect(Profile::IsSupportedBuild(30.0, 5.4, 99999999, false),
        "legacy metadata fallback remains supported");
    Expect(Profile::IsSupportedBuild(31.41, 5.5, Changelist, true),
        "support the exact 31.41 profile");
    Expect(!Profile::IsSupportedBuild(31.41, 5.5, Changelist, false),
        "fail closed for an incomplete 31.41 identity");
    Expect(!Profile::IsSupportedBuild(31.41, 5.5, 99999999, false),
        "metadata alone cannot enable 31.41");
    Expect(!Profile::IsSupportedBuild(31.41, 5.4, Changelist, true),
        "fail closed for a target release on another engine");

    for (double Version : { -1.0, 31.0, 31.40, 31.42, 31.99, 32.0,
            NaN, Infinity, -Infinity })
        Expect(!Profile::IsSupportedBuild(Version, 5.5, Changelist, true),
            "reject unsupported releases and malformed version values");
    for (double Engine : { -5.5, 0.0, NaN, Infinity, -Infinity })
    {
        Expect(!Profile::IsSupportedBuild(30.0, Engine, 0, false),
            "reject an invalid engine version for legacy builds");
        Expect(!Profile::MatchesTargetBuild(31.41, Engine, Changelist, true),
            "reject an invalid engine version for the target build");
    }

    Expect(Profile::ValidateObjectArrayCounts(1, 1, 1, 1),
        "accept a populated first object chunk");
    Expect(Profile::ValidateObjectArrayCounts(65536, 65536, 1, 1),
        "accept the exact first-chunk boundary");
    Expect(Profile::ValidateObjectArrayCounts(65537, 131072, 2, 2),
        "accept a populated second chunk");
    Expect(Profile::ValidateObjectArrayCounts(1, 131072, 2, 2),
        "accept a preallocated pool with unused chunks");
    Expect(Profile::ValidateObjectArrayCounts(0x1000000, 0x1000000, 256, 256),
        "accept the bounded maximum object pool");
    Expect(!Profile::ValidateObjectArrayCounts(0, 65536, 1, 1),
        "an empty pool cannot establish startup readiness");
    Expect(!Profile::ValidateObjectArrayCounts(-1, 65536, 1, 1),
        "reject negative object counts");
    Expect(!Profile::ValidateObjectArrayCounts(2, 1, 1, 1),
        "reject live objects beyond the pool capacity");
    Expect(!Profile::ValidateObjectArrayCounts(1, -1, 1, 1),
        "reject negative pool capacity");
    Expect(!Profile::ValidateObjectArrayCounts(65537, 131072, 1, 2),
        "reject missing live-object chunks");
    Expect(!Profile::ValidateObjectArrayCounts(1, 65537, 1, 1),
        "reject insufficient reserved chunk slots");
    Expect(!Profile::ValidateObjectArrayCounts(1, 65536, 0, 1),
        "reject an unallocated live-object chunk");
    Expect(!Profile::ValidateObjectArrayCounts(1, 65536, -1, 1),
        "reject negative allocated chunk counts");
    Expect(!Profile::ValidateObjectArrayCounts(1, 65536, 2, 1),
        "reject allocated chunks beyond the pointer-array capacity");
    Expect(!Profile::ValidateObjectArrayCounts(1, 65536, 1, -1),
        "reject negative chunk capacity");
    Expect(!Profile::ValidateObjectArrayCounts(1, 0x1000001, 1, 256),
        "reject an oversized object pool");
    Expect(!Profile::ValidateObjectArrayCounts(1, 65536, 1, 257),
        "reject an oversized chunk pointer array");
    Expect(!Profile::ValidateObjectArrayCounts(std::numeric_limits<std::int32_t>::max(),
            std::numeric_limits<std::int32_t>::max(), 256, 256),
        "reject integer-overflow inputs before computing chunk ceilings");

    // Shipping IrisCore.ObjectReplicationBridgeFilterConfig stores adjacent
    // four-byte ClassName, DynamicFilterName and FilterProfile fields.
    struct FilterConfig
    {
        std::uint32_t ClassName;
        std::uint32_t DynamicFilterName;
        std::uint32_t FilterProfile;
        std::uint8_t ForceEnable;
        std::uint8_t Padding[3];
    };
    static_assert(sizeof(FilterConfig) == 0x10);
    static_assert(offsetof(FilterConfig, DynamicFilterName) == 0x04);
    static_assert(offsetof(FilterConfig, FilterProfile) == 0x08);

    FilterConfig Filters{ 0x12345678, 0x23456789, 0x34567890,
        1, { 0xA1, 0xB2, 0xC3 } };
    const std::array<std::uint32_t, 2> LocalNone{ 0, 0xDEADBEEF };
    Profile::CopyName(&Filters.DynamicFilterName, LocalNone.data(), true);
    Expect(Filters.DynamicFilterName == 0,
        "clear a reflected target name with the comparison index only");
    Expect(Filters.ClassName == 0x12345678 && Filters.FilterProfile == 0x34567890,
        "target name assignment preserves both adjacent reflected names");
    Expect(Filters.ForceEnable == 1 && Filters.Padding[0] == 0xA1 &&
            Filters.Padding[1] == 0xB2 && Filters.Padding[2] == 0xC3,
        "target name assignment preserves the following flag and padding");

    // Exactly four-byte objects exercise the helper's source and destination
    // bounds under AddressSanitizer as well as checking the resulting value.
    const std::uint32_t ReflectedName = 0x45678901;
    std::uint32_t ReflectedDestination = 0;
    Profile::CopyName(&ReflectedDestination, &ReflectedName, true);
    Expect(ReflectedDestination == ReflectedName,
        "target copy accepts source and destination storage of exactly four bytes");
    std::array<std::uint32_t, 2> LocalName{ 0, 0xABADBABE };
    Profile::CopyName(LocalName.data(), &ReflectedName, true);
    Expect(LocalName[0] == ReflectedName && LocalName[1] == 0xABADBABE,
        "copying a target name into local storage leaves the dummy number alone");

    const std::array<std::uint32_t, 2> LegacyName{ 0x56789012, 23 };
    std::array<std::uint32_t, 4> LegacyDestination{
        0xAABBCCDD, 0, 0, 0xEEFF0011 };
    Profile::CopyName(&LegacyDestination[1], LegacyName.data(), false);
    Expect(LegacyDestination[1] == LegacyName[0] &&
            LegacyDestination[2] == LegacyName[1],
        "legacy name copy retains both comparison index and number");
    Expect(LegacyDestination[0] == 0xAABBCCDD &&
            LegacyDestination[3] == 0xEEFF0011,
        "legacy name copy preserves storage before and after its eight bytes");

    Profile::CopyName(&Filters.DynamicFilterName, &Filters.DynamicFilterName, true);
    Expect(Filters.DynamicFilterName == 0 && Filters.FilterProfile == 0x34567890,
        "target self-assignment preserves the name and adjacent field");
    Profile::CopyName(&LegacyDestination[1], &LegacyDestination[1], false);
    Expect(LegacyDestination[1] == LegacyName[0] &&
            LegacyDestination[2] == LegacyName[1],
        "legacy self-assignment preserves both name words");

    return Failures == 0 ? 0 : 1;
}
