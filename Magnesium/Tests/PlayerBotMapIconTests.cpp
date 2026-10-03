#include "../Erbium/Support/Public/PlayerBotMapIconRegistry.h"

#include <array>
#include <cstdio>

namespace
{
    struct Object
    {
        int Generation = 1;
        bool Alive = true;
    };
    struct WeakObject
    {
        Object* Pointer = nullptr;
        int Generation = 0;
        explicit WeakObject(Object* Value)
            : Pointer(Value), Generation(Value ? Value->Generation : 0) {}
        Object* Get() const
        {
            return Pointer && Pointer->Alive && Pointer->Generation == Generation
                ? Pointer : nullptr;
        }
    };
}

int main()
{
    int Failures = 0;
    auto Check = [&Failures](bool Condition, const char* Message)
    {
        if (!Condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", Message);
            ++Failures;
        }
    };
    PlayerBotMapIconRegistry<WeakObject, WeakObject> Registry;
    Object Bot, Pawn, Human, HumanPawn;
    Object* Missing = nullptr;
    Check(!Registry.Contains(&Bot, &Pawn), "unregistered pawn preserves normal icons");
    Registry.Remember(&Bot, &Pawn);
    Check(Registry.Contains(&Bot, &Pawn), "synthetic pawn is suppressed before AI registration");
    Check(Registry.Contains(Missing, &Pawn), "pre-possession cosmetic callbacks remain suppressed");
    Check(!Registry.Contains(&Human, &HumanPawn), "human player icons remain available");
    Check(!Registry.Contains(&Human, &Pawn), "a human possession of the same pawn is not suppressed");
    Check(!Registry.Contains(&Bot, &HumanPawn), "controller identity cannot hide a different pawn");
    Check(!Registry.Contains(&Bot, Missing), "missing pawn cannot match");
    Registry.Remember(Missing, &Pawn);
    Registry.Remember(&Bot, Missing);
    Check(Registry.Contains(&Bot, &Pawn), "incomplete callback cannot erase live bot identity");
    // The registry has no AI/version/map preference inputs: toggling those does
    // not discard identity or let a queued cosmetic retry recreate an icon.
    for (int CosmeticRetry = 0; CosmeticRetry < 20; ++CosmeticRetry)
    {
        Registry.Remember(&Bot, &Pawn);
        Check(Registry.Contains(&Bot, &Pawn), "duplicate lifecycle callbacks preserve suppression");
    }
    Bot.Alive = false;
    Check(!Registry.Contains(&Bot, &Pawn), "destroyed controller cannot retain suppression");
    Bot.Alive = true;
    ++Bot.Generation;
    Check(!Registry.Contains(&Bot, &Pawn), "reused controller address does not inherit bot identity");
    Registry.Remember(&Bot, &Pawn);
    ++Pawn.Generation;
    Check(!Registry.Contains(&Bot, &Pawn), "reused pawn address does not inherit bot identity");
    Registry.Remember(&Bot, &Pawn);
    Pawn.Alive = false;
    Check(!Registry.Contains(&Bot, &Pawn), "destroyed pawn cannot match");
    Pawn.Alive = true;
    ++Pawn.Generation;
    Registry.Remember(&Bot, &Pawn);
    Registry.Reset();
    Check(!Registry.Contains(&Bot, &Pawn), "world transition drops all old identities");
    Registry.Remember(Missing, &Pawn);
    Registry.Remember(&Bot, Missing);
    Check(!Registry.Contains(&Bot, &Pawn), "incomplete spawn registration cannot create identity");
    std::array<Object, 600> Controllers, Pawns;
    for (std::size_t Index = 0; Index < Pawns.size(); ++Index)
        Registry.Remember(&Controllers[Index], &Pawns[Index]);
    for (std::size_t Index = 0; Index < Pawns.size(); ++Index)
        Check(Registry.Contains(&Controllers[Index], &Pawns[Index]),
            "more than the portrait retry capacity does not evict live bot identities");
    Registry.Reset();
    std::printf("Player bot map-icon lifecycle tests: %s\n", Failures ? "FAILED" : "PASS");
    return Failures ? 1 : 0;
}
