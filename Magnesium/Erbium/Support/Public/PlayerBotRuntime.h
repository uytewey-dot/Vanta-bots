#pragma once

#include <string>

class AFortPlayerControllerAthena;
class UNetDriver;
class UFortItemDefinition;
class UFortWorldItem;

// Registered synthetic player controllers are driven on the server's net-driver tick.
class PlayerBotRuntime
{
public:
    // Checks reflected bot action layouts without executing game actions.
    // Supports legacy float vectors and shipping double vectors.
    static bool ValidateActions(std::string& Error);
    // Spawn loadouts start with a full magazine and up to three spare magazines.
    static UFortWorldItem* GiveSpawnItem(AFortPlayerControllerAthena* Controller,
        const UFortItemDefinition* Definition, int Count = 1);
    static void Register(AFortPlayerControllerAthena* Controller);
    static void Unregister(AFortPlayerControllerAthena* Controller);
    static void Tick(const UNetDriver* Driver);
    static bool IsManaged(const AFortPlayerControllerAthena* Controller);
    static bool HasAny();
    static void Reset();
};
