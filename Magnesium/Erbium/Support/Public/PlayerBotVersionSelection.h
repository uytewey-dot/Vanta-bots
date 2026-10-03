#pragma once

#include <cmath>
#include <string_view>

namespace PlayerBotVersionSelection
{
    // These values are stable configuration/UI identifiers, not game versions.
    enum ESelection : int
    {
        Automatic = 0,
        Fortnite2830 = 1,
        Fortnite3141 = 2,
        Fortnite3211 = 3,
        Fortnite1910 = 4,
        Fortnite2420 = 5,
        Fortnite2630 = 6,
        Chapter2Season2 = 7,
        Chapter2Season4 = 8,
        Fortnite1040 = 9,
        Fortnite1131 = 10,
        Fortnite1241 = 11,
        Fortnite1261 = 12,
        Fortnite1550 = 13,
        Fortnite1730 = 14,
        Count = 15
    };

    struct FOption
    {
        const char* Label;
        const char* PreferenceValue;
        double Version;
        // Zero selects one exact release; season choices use [Version, End).
        double EndVersionExclusive = 0.0;
    };

    inline constexpr FOption Options[Count] = {
        { "Automatic (current game)", "auto", 0.0 },
        { "Fortnite 28.30", "28.30", 28.30 },
        { "Fortnite 31.41", "31.41", 31.41 },
        { "Fortnite 32.11", "32.11", 32.11 },
        { "Fortnite 19.10", "19.10", 19.10 },
        { "Fortnite 24.20", "24.20", 24.20 },
        { "Fortnite 26.30", "26.30", 26.30 },
        { "Chapter 2 / Season 2 (12.xx)", "12.xx", 12.0, 13.0 },
        { "Chapter 2 / Season 4 (14.xx)", "14.xx", 14.0, 15.0 },
        { "Fortnite 10.40", "10.40", 10.40 },
        { "Fortnite 11.31", "11.31", 11.31 },
        { "Fortnite 12.41", "12.41", 12.41 },
        { "Fortnite 12.61", "12.61", 12.61 },
        { "Fortnite 15.50", "15.50", 15.50 },
        { "Fortnite 17.30", "17.30", 17.30 }
    };

    // Keep stored IDs stable while presenting releases in chronological order.
    inline constexpr int DisplayOrder[Count] = {
        Automatic, Fortnite1040, Fortnite1131, Chapter2Season2, Fortnite1241, Fortnite1261,
        Chapter2Season4, Fortnite1550, Fortnite1730, Fortnite1910, Fortnite2420, Fortnite2630,
        Fortnite2830, Fortnite3141, Fortnite3211
    };

    inline constexpr int Normalize(int Selection) noexcept
    {
        return Selection >= Automatic && Selection < Count
            ? Selection : Automatic;
    }

    inline constexpr int ParsePreference(std::string_view Value) noexcept
    {
        for (int Selection = Automatic; Selection < Count; ++Selection)
            if (Value == Options[Selection].PreferenceValue)
                return Selection;

        // Old or unknown persisted preferences retain automatic behavior.
        return Automatic;
    }

    inline constexpr const char* PreferenceValue(int Selection) noexcept
    {
        return Options[Normalize(Selection)].PreferenceValue;
    }

    // These exact releases have supplied SDK references for action validation.
    // This classifies reference evidence, not successful in-game verification.
    inline bool HasLegacyReference(double CurrentVersion) noexcept
    {
        if (!std::isfinite(CurrentVersion))
            return false;
        for (int Selection = Fortnite1040; Selection <= Fortnite1730; ++Selection)
            if (CurrentVersion == Options[Selection].Version)
                return true;
        return false;
    }

    inline bool Allows(int Selection, double CurrentVersion) noexcept
    {
        // Validate raw selections before normalization so a corrupted live
        // setting cannot silently remove an explicit version restriction.
        if (Selection < Automatic || Selection >= Count ||
            !std::isfinite(CurrentVersion) || CurrentVersion < 0.0)
            return false;

        if (Selection == Automatic)
            return true;
        const auto& Target = Options[Selection];
        return Target.EndVersionExclusive > Target.Version
            ? CurrentVersion >= Target.Version && CurrentVersion < Target.EndVersionExclusive
            : Target.Version == CurrentVersion;
    }
}
