#include "../Erbium/Support/Public/PlayerBotVersionSelection.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string_view>

namespace Selection = PlayerBotVersionSelection;

static_assert(Selection::Automatic == 0);
static_assert(Selection::Fortnite2830 == 1);
static_assert(Selection::Fortnite3141 == 2);
static_assert(Selection::Fortnite3211 == 3);
static_assert(Selection::Fortnite1910 == 4);
static_assert(Selection::Fortnite2420 == 5);
static_assert(Selection::Fortnite2630 == 6);
static_assert(Selection::Chapter2Season2 == 7);
static_assert(Selection::Chapter2Season4 == 8);
static_assert(Selection::Fortnite1040 == 9);
static_assert(Selection::Fortnite1131 == 10);
static_assert(Selection::Fortnite1241 == 11);
static_assert(Selection::Fortnite1261 == 12);
static_assert(Selection::Fortnite1550 == 13);
static_assert(Selection::Fortnite1730 == 14);
static_assert(Selection::ParsePreference("auto") == Selection::Automatic);
static_assert(Selection::ParsePreference("28.30") == Selection::Fortnite2830);
static_assert(Selection::ParsePreference("31.41") == Selection::Fortnite3141);
static_assert(Selection::ParsePreference("32.11") == Selection::Fortnite3211);
static_assert(Selection::ParsePreference("19.10") == Selection::Fortnite1910);
static_assert(Selection::ParsePreference("24.20") == Selection::Fortnite2420);
static_assert(Selection::ParsePreference("26.30") == Selection::Fortnite2630);
static_assert(Selection::ParsePreference("12.xx") == Selection::Chapter2Season2);
static_assert(Selection::ParsePreference("14.xx") == Selection::Chapter2Season4);
static_assert(Selection::ParsePreference("10.40") == Selection::Fortnite1040);
static_assert(Selection::ParsePreference("11.31") == Selection::Fortnite1131);
static_assert(Selection::ParsePreference("12.41") == Selection::Fortnite1241);
static_assert(Selection::ParsePreference("12.61") == Selection::Fortnite1261);
static_assert(Selection::ParsePreference("15.50") == Selection::Fortnite1550);
static_assert(Selection::ParsePreference("17.30") == Selection::Fortnite1730);
static_assert(Selection::Normalize(-1) == Selection::Automatic);

int main()
{
    int Failures = 0;
    auto Check = [&Failures](bool Passed, const char* What)
    {
        if (!Passed)
        {
            std::fprintf(stderr, "FAILED: %s\n", What);
            ++Failures;
        }
    };

    const double Infinity = std::numeric_limits<double>::infinity();
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const std::array<double, 29> Releases{0.0, 1.72, 10.30, 10.40, 11.30, 11.31,
        11.50, 12.00, 12.10, 12.40, 12.41, 12.60, 12.61, 13.00, 14.00, 14.20,
        14.40, 14.60, 15.00, 15.50, 17.20, 17.30, 19.10, 24.20, 26.30,
        28.30, 31.41, 32.11, 33.0};
    const std::array<const char*, Selection::Count> Tokens{
        "auto", "28.30", "31.41", "32.11", "19.10", "24.20", "26.30", "12.xx", "14.xx",
        "10.40", "11.31", "12.41", "12.61", "15.50", "17.30"};
    const std::array<double, Selection::Count> Targets{
        0.0, 28.30, 31.41, 32.11, 19.10, 24.20, 26.30, 12.0, 14.0,
        10.40, 11.31, 12.41, 12.61, 15.50, 17.30};

    Check(std::string_view(Selection::Options[Selection::Automatic].Label) ==
        "Automatic (current game)", "automatic label describes the actual loaded game");

    for (int Raw = Selection::Automatic; Raw < Selection::Count; ++Raw)
    {
        Check(Selection::Normalize(Raw) == Raw, "preserve a recognized selection");
        Check(std::string_view(Selection::PreferenceValue(Raw)) == Tokens[Raw],
            "write the stable preference token");
        Check(Selection::ParsePreference(Selection::PreferenceValue(Raw)) == Raw,
            "round trip every persisted selection");
        Check(Selection::Options[Raw].Label && Selection::Options[Raw].Label[0],
            "every selectable release has a display label");
        for (int Other = Raw + 1; Other < Selection::Count; ++Other)
            Check(Tokens[Raw] != std::string_view(Selection::Options[Other].PreferenceValue),
                "exact releases and season choices have distinct saved preferences");

        for (double Release : Releases)
        {
            const bool Expected = Raw == Selection::Automatic ||
                (Raw == Selection::Chapter2Season2 ? Release >= 12.0 && Release < 13.0 :
                 Raw == Selection::Chapter2Season4 ? Release >= 14.0 && Release < 15.0 :
                 Release == Targets[Raw]);
            Check(Selection::Allows(Raw, Release) == Expected,
                "explicit selection permits only its matching release or season");
        }
        for (double Invalid : {-1.0, -1e-300, NaN, Infinity, -Infinity})
            Check(!Selection::Allows(Raw, Invalid),
                "reject malformed or negative loaded game versions");

        if (Raw == Selection::Chapter2Season2 || Raw == Selection::Chapter2Season4)
        {
            const double First = Targets[Raw];
            const double NextSeason = First + 1.0;
            Check(Selection::Allows(Raw, First), "include the season's initial release");
            Check(Selection::Allows(Raw, std::nextafter(First, Infinity)),
                "allow patches within the selected season");
            Check(!Selection::Allows(Raw, std::nextafter(First, -Infinity)),
                "exclude the previous season at the lower boundary");
            Check(Selection::Allows(Raw, std::nextafter(NextSeason, -Infinity)),
                "include patches before the next season");
            Check(!Selection::Allows(Raw, NextSeason), "exclude the next season");
        }
        else if (Raw != Selection::Automatic)
        {
            Check(!Selection::Allows(Raw, std::nextafter(Targets[Raw], Infinity)),
                "do not round a larger nearby version into the selected release");
            Check(!Selection::Allows(Raw, std::nextafter(Targets[Raw], -Infinity)),
                "do not round a smaller nearby version into the selected release");
        }
    }

    Check(Selection::Allows(Selection::Automatic, -0.0),
        "preserve the legacy zero release in automatic mode");
    Check(Selection::Allows(Selection::Automatic, std::numeric_limits<double>::max()),
        "automatic selection leaves finite release support to the SDK guard");
    Check(Selection::Allows(Selection::Chapter2Season2, 12.41) &&
        Selection::Allows(Selection::Chapter2Season2, 12.61),
        "the season choice continues to include both newly named exact releases");
    Check(!Selection::Allows(Selection::Fortnite1241, 12.61) &&
        !Selection::Allows(Selection::Fortnite1261, 12.41),
        "an exact Chapter 2 choice cannot accept the other release from the same season");
    for (double Release : Releases)
        Check(Selection::HasLegacyReference(Release) ==
            (Release == 10.40 || Release == 11.31 || Release == 12.41 ||
             Release == 12.61 || Release == 15.50 || Release == 17.30),
            "SDK reference validation applies to the six audited releases only");
    for (double Invalid : {-1.0, NaN, Infinity, -Infinity})
        Check(!Selection::HasLegacyReference(Invalid),
            "malformed versions cannot qualify for a supplied SDK reference");
    for (double Release : {10.40, 11.31, 12.41, 12.61, 15.50, 17.30})
    {
        Check(!Selection::HasLegacyReference(std::nextafter(Release, Infinity)) &&
            !Selection::HasLegacyReference(std::nextafter(Release, -Infinity)),
            "a nearby patch cannot reuse exact reference validation evidence");
    }

    for (int Invalid : {std::numeric_limits<int>::min(), -1, int(Selection::Count),
            std::numeric_limits<int>::max()})
    {
        Check(Selection::Normalize(Invalid) == Selection::Automatic,
            "normalize malformed stored selections to automatic");
        Check(std::string_view(Selection::PreferenceValue(Invalid)) == "auto",
            "serialize malformed stored selections safely");
        for (double Release : Releases)
            Check(!Selection::Allows(Invalid, Release),
                "reject an invalid raw live selection instead of weakening its restriction");
        Check(Selection::Allows(Selection::Normalize(Invalid), 1.72),
            "explicit preference normalization preserves legacy automatic behavior");
    }

    for (std::string_view Unknown : {"", "automatic", "current", "legacy", "30.20",
            "10.4", "11.310", "12.410", "12.61suffix", "14.60", "15.5", "17.3",
            "12.XX", "14.xx suffix", "19.1", "24.2", "26.3",
            "28.3", "28.30suffix", "31.4", "32.110", "1", "AUTO", " auto", "auto "})
        Check(Selection::ParsePreference(Unknown) == Selection::Automatic,
            "old or unknown preference strings migrate to automatic");
    Check(Selection::ParsePreference(std::string_view("31.41\0obsolete", 14)) ==
        Selection::Automatic, "parse the complete preference value, including embedded nulls");

    std::array<bool, Selection::Count> Seen{};
    double PreviousVersion = -1.0;
    for (const int Id : Selection::DisplayOrder)
    {
        Check(Id >= 0 && Id < Selection::Count, "displayed release has a valid stored ID");
        if (Id < 0 || Id >= Selection::Count) continue;
        Check(!Seen[Id], "display each release exactly once");
        Seen[Id] = true;
        Check(Selection::Options[Id].Version > PreviousVersion, "display releases chronologically");
        PreviousVersion = Selection::Options[Id].Version;
    }
    for (bool Present : Seen) Check(Present, "every saved release is selectable");

    return Failures == 0 ? 0 : 1;
}
