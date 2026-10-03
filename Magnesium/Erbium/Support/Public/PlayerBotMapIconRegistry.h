#pragma once

#include <algorithm>
#include <vector>

// Spawn identity is independent of the AI preference. Weak handles must expose
// Get(), including a generation check, so destroyed/reused objects cannot turn a
// human pawn into a bot. Only the synthetic spawn path may call Remember().
template <typename WeakController, typename WeakPawn>
class PlayerBotMapIconRegistry
{
    struct Entry
    {
        WeakController Controller;
        WeakPawn Pawn;
    };
    std::vector<Entry> Entries;

public:
    template <typename Controller, typename Pawn>
    void Remember(Controller* ControllerObject, Pawn* PawnObject)
    {
        if (!ControllerObject || !PawnObject)
            return;
        Entries.erase(std::remove_if(Entries.begin(), Entries.end(),
            [PawnObject](const Entry& Value)
            {
                return !Value.Controller.Get() || !Value.Pawn.Get() ||
                    Value.Pawn.Get() == PawnObject;
            }), Entries.end());
        Entries.push_back({ WeakController(ControllerObject), WeakPawn(PawnObject) });
    }

    template <typename Controller, typename Pawn>
    bool Contains(Controller* ControllerObject, Pawn* PawnObject) const
    {
        if (!PawnObject)
            return false;
        for (const auto& Value : Entries)
        {
            auto RememberedController = Value.Controller.Get();
            if (RememberedController && Value.Pawn.Get() == PawnObject &&
                (!ControllerObject || ControllerObject == RememberedController))
                return true;
        }
        return false;
    }

    void Reset()
    {
        Entries.clear();
    }
};
