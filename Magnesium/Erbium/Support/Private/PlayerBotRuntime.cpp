#include "pch.h"
#include "../Public/PlayerBotRuntime.h"
#include "../Public/VersionFeatureAdapter.h"
#include "../Public/FaultGuard.h"
#include "../Public/AIDebugLogger.h"
#include "../../Public/Configuration.h"
#include "../../../Engine/Public/NetDriver.h"
#include "../../../FortniteGame/Public/FortWeapon.h"
#include <cmath>
#include <initializer_list>
#include <vector>

namespace
{
    constexpr uint64 ParameterFlag = 0x80;
    constexpr uint64 ReturnFlag = 0x400;
    constexpr float PerceptionInterval = 0.25f;
    constexpr float ActionInterval = 0.1f;
    constexpr int ParticipantScanLimit = 512;

    bool IsAIEnabledForCurrentGame() noexcept
    {
        return FConfiguration::bBotAIEnabled.load(std::memory_order_relaxed) &&
            FConfiguration::IsBotVersionSelectedForCurrentGame();
    }

    struct FBot
    {
        TWeakObjectPtr<AFortPlayerControllerAthena> Controller;
        TWeakObjectPtr<AFortPlayerPawnAthena> FiringPawn;
        TWeakObjectPtr<AFortPlayerPawnAthena> Target;
        bool Firing = false;
        bool Removed = false;
        bool Chase = false;
        float NextPerception = 0.f;
        float NextAction = 0.f;
        float NextReload = 0.f;
        float BurstEnd = 0.f;
        float NextTrigger = 0.f;
    };

    std::vector<FBot> Bots;
    std::vector<TWeakObjectPtr<AFortPlayerControllerAthena>> PendingRegistrations;
    TWeakObjectPtr<UWorld> BotWorld;
    DWORD ServerThread = 0;
    bool InsideTick = false;
    bool PendingReset = false;
    int LifecycleDepth = 0;

    bool Dispatch(const UObject* Object, UFunction* Function, void* Buffer)
    {
        bool Succeeded = false;
        ++GGuardedNativeCallDepth;
        __try
        {
            Object->ProcessEvent(Function, Buffer);
            Succeeded = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Succeeded = false;
        }
        --GGuardedNativeCallDepth;
        return Succeeded;
    }

    // Only recognised parameters with the reflected size are accepted. Unexpected
    // inputs disable that action instead of guessing a positional ABI.
    class FNativeCall
    {
        const UObject* Object;
        UFunction* Function = nullptr;
        UFunction::ParamsNamed Parameters{};
        std::vector<uint8> Buffer;
        const char* ActionName;

        void Reject()
        {
            Function = nullptr;
            AIDebugLogger::MissingFeature(ActionName,
                "the native bot action or its reflected parameter layout is unavailable");
        }

    public:
        FNativeCall(const UObject* InObject, const char* Name,
            std::initializer_list<const char*> Allowed)
            : Object(InObject), ActionName(Name)
        {
            if (!VersionFeatureAdapter::IsLiveObject(Object))
                return;
            Function = Object->GetFunction(Name);
            if (!VersionFeatureAdapter::IsLiveObject(Function))
            {
                Reject();
                return;
            }
            Parameters = Function->GetParamsNamed();
            if (Parameters.Size > 0x1000)
            {
                Reject();
                return;
            }
            for (const auto& Parameter : Parameters.NameOffsetMap)
            {
                if (!(Parameter.PropertyFlags & ParameterFlag))
                    continue;
                bool Recognised = false;
                for (const char* AllowedName : Allowed)
                    Recognised |= Parameter.Name == AllowedName;
                if (!Recognised || Parameter.Offset > Parameters.Size ||
                    Parameter.ElementSize > Parameters.Size - Parameter.Offset)
                {
                    Reject();
                    return;
                }
            }
            Buffer.resize(Parameters.Size ? Parameters.Size : 1, 0);
        }

        bool Field(const char* Name, const void* Value, size_t Size, bool Return = false)
        {
            if (!Function)
                return false;
            for (const auto& Parameter : Parameters.NameOffsetMap)
            {
                if (Parameter.Name != Name)
                    continue;
                if (!(Parameter.PropertyFlags & ParameterFlag) ||
                    ((Parameter.PropertyFlags & ReturnFlag) != 0) != Return ||
                    Parameter.ElementSize != Size || Parameter.Offset > Buffer.size() ||
                    Size > Buffer.size() - Parameter.Offset)
                {
                    Reject();
                    return false;
                }
                if (Value)
                    memcpy(Buffer.data() + Parameter.Offset, Value, Size);
                return true;
            }
            Reject();
            return false;
        }

        bool Invoke()
        {
            return Function && Dispatch(Object, Function, Buffer.data());
        }

        bool ReadReturn(void* Value, size_t Size) const
        {
            if (!Function)
                return false;
            for (const auto& Parameter : Parameters.NameOffsetMap)
                if (Parameter.Name == "ReturnValue" &&
                    (Parameter.PropertyFlags & ReturnFlag) && Parameter.ElementSize == Size &&
                    Parameter.Offset <= Buffer.size() && Size <= Buffer.size() - Parameter.Offset)
                {
                    memcpy(Value, Buffer.data() + Parameter.Offset, Size);
                    return true;
                }
            return false;
        }
    };

    struct FActionField
    {
        const char* Name;
        size_t Size;
        bool Return = false;
    };

    const UObject* ActionDefault(const UClass* Class, const char* Name, std::string& Error)
    {
        Error = std::string(Name) + ": class/default reflection is unavailable";
        if (!VersionFeatureAdapter::IsLiveObject(Class))
            return nullptr;
        auto Default = Class->GetDefaultObj();
        if (!VersionFeatureAdapter::IsLiveObject(Default) || !Default->IsDefaultObject() ||
            Default->Class != Class)
            return nullptr;
        return Default;
    }

    bool ActionProperty(const UObject* Object, const char* Owner, const char* Name,
        size_t Size, uint64 CastFlags, std::string& Error)
    {
        const std::string Label = std::string(Owner) + "." + Name;
        Error = Label + ": required reflected property is unavailable";
        auto Property = Object->GetProperty(Name, CastFlags);
        const size_t MetadataSize = static_cast<size_t>((std::max)({ Offsets::Offset_Internal,
            Offsets::ElementSize, Offsets::ArrayDim })) + sizeof(uint32);
        if (!Property || !Offsets::ArrayDim ||
            !SDK::MemReadable(Property, MetadataSize))
            return false;
        const auto Offset = SDK::ReadPropertyOffset(GetFromOffset<uint32>(Property,
            Offsets::Offset_Internal));
        const auto ElementSize = SDK::ReadPropertyElementSize(GetFromOffset<uint32>(Property,
            Offsets::ElementSize));
        const auto ArrayDimension = GetFromOffset<int32>(Property, Offsets::ArrayDim);
        const int32 OwnerSize = Object->Class->GetPropertiesSize();
        if (ElementSize != Size || ArrayDimension != 1 || OwnerSize < 0 ||
            Offset > static_cast<uint32>(OwnerSize) ||
            Size > static_cast<size_t>(OwnerSize) - Offset ||
            !SDK::MemReadable(reinterpret_cast<const uint8*>(Object) + Offset, Size))
        {
            Error = Label + ": expected a bounded scalar property of " +
                std::to_string(Size) + " bytes";
            return false;
        }
        if (CastFlags == 0x20000 && (!Offsets::FieldMask ||
            !SDK::MemReadable(Property, static_cast<size_t>(Offsets::FieldMask) + sizeof(uint8)) ||
            !Property->GetFieldMask()))
        {
            Error = Label + ": boolean field mask is unavailable";
            return false;
        }
        return true;
    }

    bool ActionSchema(const UObject* Object, const char* Owner, const char* Name,
        std::initializer_list<const char*> Allowed, std::initializer_list<FActionField> Fields,
        std::string& Error)
    {
        const std::string Label = std::string(Owner) + "." + Name;
        Error = Label + ": reflected action layout is unavailable";
        FNativeCall Call(Object, Name, Allowed);
        for (const auto& Field : Fields)
        {
            if (!Call.Field(Field.Name, nullptr, Field.Size, Field.Return))
            {
                Error = Label + ": unavailable or incompatible parameter " + Field.Name +
                    " (expected " + std::to_string(Field.Size) + "-byte " +
                    (Field.Return ? "return" : "input") + ")";
                return false;
            }
        }
        return true;
    }

    bool ValidateActionsInternal(std::string& Error)
    {
        // Shipping 28.30, 31.41 and 32.11 expose the same bot action parameter sizes.
        Error = "bot actions: expected shipping vector/rotation, pointer and ability-handle sizes";
        if (FVector::Size() != 24 || FRotator::Size() != 24 || sizeof(AActor*) != 8 ||
            sizeof(FGameplayAbilitySpecHandle) != 4)
            return false;
        auto Pawn = ActionDefault(SDK::FindClass("FortPlayerPawnAthena"),
            "FortPlayerPawnAthena", Error);
        if (!Pawn)
            return false;
        auto Controller = ActionDefault(SDK::FindClass("FortPlayerControllerAthena"),
            "FortPlayerControllerAthena", Error);
        if (!Controller)
            return false;
        auto PlayerState = ActionDefault(SDK::FindClass("FortPlayerStateAthena"),
            "FortPlayerStateAthena", Error);
        if (!PlayerState)
            return false;
        auto Weapon = ActionDefault(SDK::FindClass("FortWeapon"), "FortWeapon", Error);
        if (!Weapon)
            return false;
        auto AbilitySystem = ActionDefault(SDK::FindClass("AbilitySystemComponent"),
            "AbilitySystemComponent", Error);
        if (!AbilitySystem)
            return false;

        if (!ActionProperty(Pawn, "FortPlayerPawnAthena", "Controller", 8, 0x10000, Error) ||
            !ActionProperty(Pawn, "FortPlayerPawnAthena", "PlayerState", 8, 0x10000, Error) ||
            !ActionProperty(Pawn, "FortPlayerPawnAthena", "CurrentWeapon", 8, 0x10000, Error) ||
            !ActionProperty(Controller, "FortPlayerControllerAthena", "Pawn", 8, 0x10000, Error) ||
            !ActionProperty(Controller, "FortPlayerControllerAthena", "PlayerState", 8, 0x10000, Error) ||
            !ActionProperty(PlayerState, "FortPlayerStateAthena", "TeamIndex", 1,
                GUESS_PROP_FLAGS(uint8), Error) ||
            !ActionProperty(PlayerState, "FortPlayerStateAthena", "bIsABot", 1, 0x20000, Error) ||
            !ActionProperty(PlayerState, "FortPlayerStateAthena", "AbilitySystemComponent", 8,
                0x10000, Error) ||
            !ActionProperty(Weapon, "FortWeapon", "AmmoCount", 4, 0x80, Error) ||
            !ActionProperty(Weapon, "FortWeapon", "ReloadAbilitySpecHandle", 4, 0, Error) ||
            !ActionProperty(AbilitySystem, "AbilitySystemComponent", "OwnerActor", 8,
                0x10000, Error) ||
            !ActionProperty(AbilitySystem, "AbilitySystemComponent", "AvatarActor", 8,
                0x10000, Error))
            return false;

        if (!ActionSchema(Pawn, "FortPlayerPawnAthena", "K2_GetActorLocation",
                { "ReturnValue" }, { { "ReturnValue", 24, true } }, Error) ||
            !ActionSchema(Pawn, "FortPlayerPawnAthena", "AddMovementInput",
                { "WorldDirection", "ScaleValue", "bForce" },
                { { "WorldDirection", 24 }, { "ScaleValue", 4 }, { "bForce", 1 } }, Error) ||
            !ActionSchema(Controller, "FortPlayerControllerAthena", "SetControlRotation",
                { "NewRotation" }, { { "NewRotation", 24 } }, Error) ||
            !ActionSchema(Controller, "FortPlayerControllerAthena", "LineOfSightTo",
                { "Other", "ViewPoint", "bAlternateChecks", "ReturnValue" },
                { { "Other", 8 }, { "ViewPoint", 24 }, { "bAlternateChecks", 1 },
                    { "ReturnValue", 1, true } }, Error) ||
            !ActionSchema(Pawn, "FortPlayerPawnAthena", "PawnStartFire",
                { "FireModeNum" }, { { "FireModeNum", 1 } }, Error) ||
            !ActionSchema(Pawn, "FortPlayerPawnAthena", "PawnStopFire",
                { "FireModeNum" }, { { "FireModeNum", 1 } }, Error) ||
            !ActionSchema(Pawn, "FortPlayerPawnAthena", "GetHealth",
                { "ReturnValue" }, { { "ReturnValue", 4, true } }, Error) ||
            !ActionSchema(AbilitySystem, "AbilitySystemComponent", "TryActivateAbility",
                { "AbilityToActivate", "bAllowRemoteActivation", "ReturnValue" },
                { { "AbilityToActivate", 4 }, { "bAllowRemoteActivation", 1 },
                    { "ReturnValue", 1, true } }, Error))
            return false;
        Error.clear();
        return true;
    }

    bool IsAlive(AFortPlayerPawnAthena* Pawn)
    {
        if (!VersionFeatureAdapter::IsLiveActor(Pawn) ||
            (Pawn->HasbIsDying() && Pawn->bIsDying) ||
            (Pawn->HasbIsHiddenForDeath() && Pawn->bIsHiddenForDeath))
            return false;
        float Health = 0.f;
        FNativeCall Call(Pawn, "GetHealth", { "ReturnValue" });
        if (Call.Field("ReturnValue", nullptr, sizeof(Health), true) &&
            Call.Invoke() && Call.ReadReturn(&Health, sizeof(Health)) &&
            std::isfinite(Health))
            return Health > 0.f;
        auto HealthSet = Pawn->HasHealthSet() ? Pawn->HealthSet : nullptr;
        if (!VersionFeatureAdapter::IsLiveObject(HealthSet) || !HealthSet->HasHealth())
            return false;
        auto& Attribute = HealthSet->Health;
        if (!Attribute.HasCurrentValue())
            return false;
        Health = Attribute.CurrentValue;
        return std::isfinite(Health) && Health > 0.f;
    }

    bool IsDown(AFortPlayerPawnAthena* Pawn)
    {
        return Pawn->HasbIsDBNO() && Pawn->bIsDBNO;
    }

    bool Location(const AActor* Actor, FVector& Result)
    {
        FNativeCall Call(Actor, "K2_GetActorLocation", { "ReturnValue" });
        return Call.Field("ReturnValue", nullptr, FVector::Size(), true) && Call.Invoke() &&
            Call.ReadReturn(&Result, FVector::Size()) && std::isfinite(Result.X) &&
            std::isfinite(Result.Y) && std::isfinite(Result.Z);
    }

    bool Fire(AFortPlayerPawnAthena* Pawn, bool Start)
    {
        if (Start)
        {
            FNativeCall StopCall(Pawn, "PawnStopFire", { "FireModeNum" });
            if (!StopCall.Field("FireModeNum", nullptr, sizeof(uint8)))
                return false;
        }
        FNativeCall Call(Pawn, Start ? "PawnStartFire" : "PawnStopFire", { "FireModeNum" });
        const uint8 Mode = 0;
        return Call.Field("FireModeNum", &Mode, sizeof(Mode)) && Call.Invoke();
    }

    void Stop(FBot& Bot)
    {
        if (Bot.Firing)
        {
            auto Pawn = Bot.FiringPawn.Get();
            if (VersionFeatureAdapter::IsLiveActor(Pawn) && Pawn->HasController() &&
                Pawn->Controller == Bot.Controller.Get() && !Fire(Pawn, false))
                return; // Keep retrying a failed native stop while this pawn exists.
        }
        Bot.Firing = false;
        Bot.FiringPawn = TWeakObjectPtr<AFortPlayerPawnAthena>();
    }

    bool Move(AFortPlayerPawnAthena* Pawn, const FVector& From, const FVector& To)
    {
        const double X = To.X - From.X;
        const double Y = To.Y - From.Y;
        const double Length = std::sqrt(X * X + Y * Y);
        if (!std::isfinite(Length) || Length < 150.)
            return false;
        const FVector Direction(X / Length, Y / Length, 0.);
        const float Scale = 1.f;
        const uint8 Force = 1;
        FNativeCall Call(Pawn, "AddMovementInput", { "WorldDirection", "ScaleValue", "bForce" });
        return Call.Field("WorldDirection", &Direction, FVector::Size()) &&
            Call.Field("ScaleValue", &Scale, sizeof(Scale)) &&
            Call.Field("bForce", &Force, sizeof(Force)) && Call.Invoke();
    }

    bool Aim(AFortPlayerControllerAthena* Controller, const FVector& From, const FVector& To)
    {
        constexpr double Degrees = 57.29577951308232;
        const double X = To.X - From.X;
        const double Y = To.Y - From.Y;
        const double Z = To.Z - From.Z;
        const FRotator Rotation(std::atan2(Z, std::sqrt(X * X + Y * Y)) * Degrees,
            std::atan2(Y, X) * Degrees, 0.);
        FNativeCall Call(Controller, "SetControlRotation", { "NewRotation" });
        return Call.Field("NewRotation", &Rotation, FRotator::Size()) && Call.Invoke();
    }

    bool Visible(AFortPlayerControllerAthena* Controller, AFortPlayerPawnAthena* Other,
        const FVector& ViewPoint)
    {
        FNativeCall Call(Controller, "LineOfSightTo",
            { "Other", "ViewPoint", "bAlternateChecks", "ReturnValue" });
        const uint8 Alternate = 0;
        uint8 Result = 0;
        return Call.Field("Other", &Other, sizeof(Other)) &&
            Call.Field("ViewPoint", &ViewPoint, FVector::Size()) &&
            Call.Field("bAlternateChecks", &Alternate, sizeof(Alternate)) &&
            Call.Field("ReturnValue", nullptr, sizeof(Result), true) &&
            Call.Invoke() && Call.ReadReturn(&Result, sizeof(Result)) && Result != 0;
    }

    AFortPlayerStateAthena* State(AFortPlayerPawnAthena* Pawn)
    {
        auto Value = Pawn && Pawn->HasPlayerState() ? Pawn->PlayerState : nullptr;
        return VersionFeatureAdapter::IsLiveActor(Value) &&
            Value->IsA(AFortPlayerStateAthena::StaticClass())
            ? static_cast<AFortPlayerStateAthena*>(Value) : nullptr;
    }

    bool Hostile(AFortPlayerPawnAthena* Pawn, AFortPlayerPawnAthena* Other)
    {
        if (Other == Pawn || !IsAlive(Other) || IsDown(Other))
            return false;
        auto OwnState = State(Pawn);
        auto OtherState = State(Other);
        if (!OwnState || !OtherState || !OwnState->HasTeamIndex() || !OtherState->HasTeamIndex() ||
            (OtherState->HasbIsSpectator() && OtherState->bIsSpectator) ||
            (OtherState->HasbOnlySpectator() && OtherState->bOnlySpectator))
            return false;
        // Team 0/1 are unassigned/spectator teams; never infer hostility without teams.
        return OwnState->TeamIndex >= 2 && OtherState->TeamIndex >= 2 &&
            OwnState->TeamIndex != OtherState->TeamIndex;
    }

    AFortPlayerPawnAthena* FindTarget(AFortGameMode* GameMode,
        AFortPlayerPawnAthena* Pawn, const FVector& Position, double DetectionRange)
    {
        AFortPlayerPawnAthena* Best = nullptr;
        double BestDistance = DetectionRange * DetectionRange;
        auto Scan = [&](const TArray<AActor*>& Participants)
        {
            const int Count = (std::min)(Participants.Num(), ParticipantScanLimit);
            for (int Index = 0; Index < Count; ++Index)
            {
                auto Controller = Participants[Index];
                if (!VersionFeatureAdapter::IsLiveActor(Controller))
                    continue;
                // Native AI controller classes need not inherit FortPlayerControllerAthena.
                auto Property = Controller->GetProperty("Pawn", 0x10000);
                if (!Property || !SDK::MemReadable(Property, (std::max)({ Offsets::Offset_Internal,
                        Offsets::ElementSize, Offsets::ArrayDim }) + sizeof(uint32)) ||
                    SDK::ReadPropertyElementSize(GetFromOffset<uint32>(Property,
                        Offsets::ElementSize)) != sizeof(AActor*) ||
                    GetFromOffset<int32>(Property, Offsets::ArrayDim) != 1)
                    continue;
                const auto Offset = Controller->GetOffset("Pawn", 0x10000);
                const int ClassSize = Controller->Class->GetPropertiesSize();
                if (Offset == uint32(-1) || ClassSize < int(sizeof(AActor*)) ||
                    Offset > uint32(ClassSize - sizeof(AActor*)) ||
                    !SDK::MemReadable((const uint8*)Controller + Offset, sizeof(AActor*)))
                    continue;
                auto OtherActor = GetFromOffset<AActor*>(Controller, Offset);
                if (!VersionFeatureAdapter::IsLiveActor(OtherActor) ||
                    !OtherActor->IsA(AFortPlayerPawnAthena::StaticClass()))
                    continue;
                auto Other = static_cast<AFortPlayerPawnAthena*>(OtherActor);
                if (!Hostile(Pawn, Other))
                    continue;
                FVector OtherPosition;
                if (!Location(Other, OtherPosition))
                    continue;
                const double Distance = (OtherPosition - Position).SizeSquared();
                if (Distance < BestDistance)
                {
                    Best = Other;
                    BestDistance = Distance;
                }
            }
        };
        if (GameMode->HasAlivePlayers())
            Scan(GameMode->AlivePlayers);
        if (GameMode->HasAliveBots())
            Scan(GameMode->AliveBots);
        return Best;
    }

    UClass* GrantedReloadClass(UAbilitySystemComponent* AbilitySystem,
        const FGameplayAbilitySpecHandle& Handle)
    {
        if (!AbilitySystem->HasActivatableAbilities() ||
            !FGameplayAbilitySpecContainer::HasItems() ||
            !FGameplayAbilitySpec::HasHandle() || !FGameplayAbilitySpec::HasAbility())
            return nullptr;

        const auto SpecSize = FGameplayAbilitySpec::Size();
        const auto& Items = AbilitySystem->ActivatableAbilities.Items;
        if (!SpecSize || SpecSize > 0x1000 || Items.Num() <= 0 || Items.Num() > 512 ||
            Items.Num() > Items.MaxElements ||
            FGameplayAbilitySpec::Handle__Offset < 0 ||
            FGameplayAbilitySpec::Ability__Offset < 0 ||
            size_t(FGameplayAbilitySpec::Handle__Offset) + sizeof(Handle) > SpecSize ||
            size_t(FGameplayAbilitySpec::Ability__Offset) + sizeof(UObject*) > SpecSize ||
            !SDK::MemReadable(Items.Data, size_t(Items.Num()) * SpecSize))
            return nullptr;

        UClass* ReloadClass = nullptr;
        for (int Index = 0; Index < Items.Num(); ++Index)
        {
            const auto& Spec = Items.Get(Index, SpecSize);
            if (Spec.Handle.Handle == Handle.Handle &&
                VersionFeatureAdapter::IsLiveObject(Spec.Ability) &&
                VersionFeatureAdapter::IsLiveObject(Spec.Ability->Class))
            {
                ReloadClass = Spec.Ability->Class;
                break;
            }
        }
        if (!ReloadClass)
            return nullptr;

        // By-class activation chooses a granted spec. Refuse an ambiguous class
        // rather than accidentally reloading a different weapon in the inventory.
        int MatchingSpecs = 0;
        for (int Index = 0; Index < Items.Num(); ++Index)
        {
            const auto& Spec = Items.Get(Index, SpecSize);
            if (VersionFeatureAdapter::IsLiveObject(Spec.Ability) &&
                Spec.Ability->Class == ReloadClass)
                ++MatchingSpecs;
        }
        return MatchingSpecs == 1 ? ReloadClass : nullptr;
    }

    void Reload(FBot& Bot, AFortPlayerPawnAthena* Pawn, float Now)
    {
        if (Now < Bot.NextReload)
            return;
        Bot.NextReload = Now + 1.f;

        auto Controller = Bot.Controller.Get();
        if (!VersionFeatureAdapter::IsLiveActor(Controller) ||
            !VersionFeatureAdapter::IsLiveActor(Pawn) || !Controller->HasPawn() ||
            Controller->Pawn != Pawn || !Pawn->HasController() ||
            Pawn->Controller != Controller || !Controller->HasPlayerState())
            return;
        auto PlayerState = State(Pawn);
        if (!PlayerState || Controller->PlayerState != PlayerState)
            return;

        auto WeaponActor = Pawn->HasCurrentWeapon() ? Pawn->CurrentWeapon : nullptr;
        auto Weapon = VersionFeatureAdapter::IsLiveActor(WeaponActor) &&
            WeaponActor->IsA(AFortWeapon::StaticClass())
            ? static_cast<AFortWeapon*>(WeaponActor) : nullptr;
        auto AbilitySystem = PlayerState->HasAbilitySystemComponent()
            ? PlayerState->AbilitySystemComponent : nullptr;
        if (Weapon && Weapon->HasReloadAbilitySpecHandle() &&
            VersionFeatureAdapter::IsLiveObject(AbilitySystem))
        {
            // Never activate a component still bound to a previous pawn/state.
            if (!AbilitySystem->IsA(UAbilitySystemComponent::StaticClass()) ||
                (AbilitySystem->HasOwnerActor() && AbilitySystem->OwnerActor != PlayerState) ||
                (AbilitySystem->HasAvatarActor() && AbilitySystem->AvatarActor != Pawn))
                return;
            const auto Handle = Weapon->ReloadAbilitySpecHandle;
            if (Handle.Handle != -1)
            {
                const uint8 AllowRemoteActivation = 0;
                uint8 Activated = 0;
                FNativeCall AbilityCall(AbilitySystem, "TryActivateAbility",
                    { "AbilityToActivate", "bAllowRemoteActivation", "ReturnValue" });
                if (AbilityCall.Field("AbilityToActivate", &Handle, sizeof(Handle)) &&
                    AbilityCall.Field("bAllowRemoteActivation", &AllowRemoteActivation,
                        sizeof(AllowRemoteActivation)) &&
                    AbilityCall.Field("ReturnValue", nullptr, sizeof(Activated), true) &&
                    AbilityCall.Invoke() && AbilityCall.ReadReturn(&Activated, sizeof(Activated)))
                    return; // Honor native rejection as well as successful activation.

                // 12.41 and 14.60 expose TryActivateAbilityByClass, not the
                // handle overload. Resolve the equipped weapon's granted spec.
                auto ReloadClass = GrantedReloadClass(AbilitySystem, Handle);
                if (ReloadClass)
                {
                    FNativeCall ClassCall(AbilitySystem, "TryActivateAbilityByClass",
                        { "InAbilityToActivate", "bAllowRemoteActivation", "ReturnValue" });
                    if (ClassCall.Field("InAbilityToActivate", &ReloadClass, sizeof(ReloadClass)) &&
                        ClassCall.Field("bAllowRemoteActivation", &AllowRemoteActivation,
                            sizeof(AllowRemoteActivation)) &&
                        ClassCall.Field("ReturnValue", nullptr, sizeof(Activated), true) &&
                        ClassCall.Invoke() && ClassCall.ReadReturn(&Activated, sizeof(Activated)))
                        return; // Native cooldowns, ammo checks and rejection still apply.
                }
            }
        }

        // Older builds can instead expose a no-arg native reload lifecycle.
        FNativeCall PawnCall(Pawn, "ReloadWeapon", {});
        if (PawnCall.Invoke())
            return;
        if (Pawn->HasCurrentWeapon() && VersionFeatureAdapter::IsLiveActor(Pawn->CurrentWeapon))
        {
            FNativeCall WeaponCall(Pawn->CurrentWeapon, "Reload", {});
            WeaponCall.Invoke();
        }
    }

    void TickBot(FBot& Bot, AFortPlayerControllerAthena* Controller,
        AFortPlayerPawnAthena* Pawn, AFortGameMode* GameMode, float Now)
    {
        if (!IsAIEnabledForCurrentGame() || IsDown(Pawn) ||
            VersionFeatureAdapter::GetMatchPhase() != EPlayerAIMatchPhase::InProgress ||
            VersionFeatureAdapter::IsInAircraft(Controller) ||
            VersionFeatureAdapter::IsSkinCommitPending(Pawn) ||
            (Pawn->HasbIsSkydiving() && Pawn->bIsSkydiving))
        {
            Stop(Bot);
            Bot.Target = TWeakObjectPtr<AFortPlayerPawnAthena>();
            return;
        }
        if (Bot.Firing && Now >= Bot.BurstEnd)
        {
            Stop(Bot);
            Bot.NextTrigger = Now + 0.1f;
        }
        FVector Position;
        if (!Location(Pawn, Position))
        {
            Stop(Bot);
            return;
        }
        FVector ZoneCenter;
        float ZoneRadius = 0.f;
        if (VersionFeatureAdapter::TryGetSafeZone(ZoneCenter, ZoneRadius) &&
            std::isfinite(ZoneRadius) && ZoneRadius > 0.f)
        {
            const double X = Position.X - ZoneCenter.X;
            const double Y = Position.Y - ZoneCenter.Y;
            if (X * X + Y * Y > double(ZoneRadius) * ZoneRadius)
            {
                Stop(Bot);
                Move(Pawn, Position, ZoneCenter);
                return;
            }
        }
        const float RawDetection = FConfiguration::BotAIDetectionRange.load(std::memory_order_relaxed);
        const float RawEngage = FConfiguration::BotAIEngageRange.load(std::memory_order_relaxed);
        if (!std::isfinite(RawDetection) || !std::isfinite(RawEngage))
        {
            Stop(Bot);
            return;
        }
        const float Detection = (std::clamp)(RawDetection, 1000.f, 30000.f);
        const float Engage = (std::clamp)(RawEngage, 500.f, Detection);
        auto Target = Bot.Target.Get();
        if (Now >= Bot.NextPerception || !Hostile(Pawn, Target))
        {
            Target = FindTarget(GameMode, Pawn, Position, Detection);
            Bot.Target = TWeakObjectPtr<AFortPlayerPawnAthena>(Target);
            Bot.NextPerception = Now + PerceptionInterval;
        }
        FVector TargetPosition;
        if (!Target || !Hostile(Pawn, Target) || !Location(Target, TargetPosition) ||
            (TargetPosition - Position).SizeSquared() > double(Detection) * Detection)
        {
            Stop(Bot);
            return;
        }
        const double DistanceSquared = (TargetPosition - Position).SizeSquared();
        if (Now < Bot.NextAction)
        {
            if (Bot.Chase)
                Move(Pawn, Position, TargetPosition);
            return;
        }
        Bot.NextAction = Now + ActionInterval;
        // Native LOS uses an eye-height view point and handles blocking geometry.
        FVector ViewPoint(Position.X, Position.Y, Position.Z +
            (Pawn->HasBaseEyeHeight() ? Pawn->BaseEyeHeight : 64.f));
        FVector AimPoint(TargetPosition.X, TargetPosition.Y, TargetPosition.Z + 45.);
        const bool CanFire = DistanceSquared <= double(Engage) * Engage &&
            Visible(Controller, Target, ViewPoint) && Aim(Controller, ViewPoint, AimPoint);
        if (!CanFire)
        {
            Bot.Chase = true;
            Stop(Bot);
            Move(Pawn, Position, TargetPosition);
            return;
        }
        Bot.Chase = false;
        auto Weapon = Pawn->HasCurrentWeapon() ? Pawn->CurrentWeapon : nullptr;
        if (!VersionFeatureAdapter::IsLiveActor(Weapon) ||
            !Weapon->IsA(AFortWeaponRanged::StaticClass()))
        {
            Stop(Bot);
            return;
        }
        auto FortWeapon = static_cast<AFortWeapon*>(Weapon);
        if (!FortWeapon->HasAmmoCount() || FortWeapon->AmmoCount <= 0)
        {
            Stop(Bot);
            Reload(Bot, Pawn, Now);
            return;
        }
        if (!Bot.Firing && Now >= Bot.NextTrigger && Fire(Pawn, true))
        {
            Bot.Firing = true;
            Bot.FiringPawn = TWeakObjectPtr<AFortPlayerPawnAthena>(Pawn);
            Bot.BurstEnd = Now + 0.5f;
        }
    }

    void ResetInternal()
    {
        if (InsideTick || LifecycleDepth > 1)
        {
            PendingReset = true;
            for (auto& Bot : Bots)
                Bot.Removed = true;
            return;
        }
        for (auto& Bot : Bots)
            Stop(Bot);
        Bots.clear();
        PendingRegistrations.clear();
        BotWorld = TWeakObjectPtr<UWorld>();
        ServerThread = 0;
        PendingReset = false;
    }

    void RegisterInternal(AFortPlayerControllerAthena* Controller)
    {
        auto World = UWorld::GetWorld();
        if (!VersionFeatureAdapter::IsLiveActor(Controller) || !World)
            return;
        if (InsideTick || LifecycleDepth > 1)
        {
            for (const auto& Pending : PendingRegistrations)
                if (Pending.Get() == Controller)
                    return;
            PendingRegistrations.emplace_back(Controller);
            return;
        }
        if (BotWorld.Get() != World)
        {
            ResetInternal();
            BotWorld = TWeakObjectPtr<UWorld>(World);
        }
        for (const auto& Bot : Bots)
            if (!Bot.Removed && Bot.Controller.Get() == Controller)
                return;
        FBot Bot{};
        Bot.Controller = TWeakObjectPtr<AFortPlayerControllerAthena>(Controller);
        Bots.push_back(Bot);
        VersionFeatureAdapter::SetManagedAIControllerHooks(&PlayerBotRuntime::IsManaged,
            &PlayerBotRuntime::HasAny);
    }

    void TickInternal(const UNetDriver* Driver)
    {
        auto World = UWorld::GetWorld();
        if (!World || !Driver || Driver != World->NetDriver || InsideTick)
            return;
        auto GameMode = VersionFeatureAdapter::GetGameMode();
        if (!VersionFeatureAdapter::IsLiveActor(GameMode))
            return; // Client worlds have no authoritative GameMode.
        if (BotWorld.Get() != World)
        {
            ResetInternal();
            return;
        }
        const DWORD Thread = GetCurrentThreadId();
        if (ServerThread && ServerThread != Thread)
            return;
        ServerThread = Thread;
        const float Now = VersionFeatureAdapter::GetTimeSeconds();
        if (!std::isfinite(Now))
            return;
        InsideTick = true;
        const bool Enabled = IsAIEnabledForCurrentGame();
        for (auto Iterator = Bots.begin(); Iterator != Bots.end();)
        {
            auto Controller = Iterator->Controller.Get();
            if (!Enabled)
            {
                Stop(*Iterator);
                Iterator->Target = TWeakObjectPtr<AFortPlayerPawnAthena>();
                Iterator->NextPerception = 0.f;
                Iterator->NextAction = 0.f;
                Iterator->Chase = false;
                if (Iterator->Removed || !VersionFeatureAdapter::IsLiveActor(Controller))
                    Iterator = Bots.erase(Iterator);
                else
                    ++Iterator;
                continue;
            }
            auto Pawn = VersionFeatureAdapter::IsLiveActor(Controller) && Controller->HasPawn()
                ? Controller->Pawn : nullptr;
            auto PlayerState = VersionFeatureAdapter::IsLiveActor(Pawn) ? State(Pawn) : nullptr;
            const bool OwnsSyntheticPawn = PlayerState && Pawn->HasController() &&
                Pawn->Controller == Controller && PlayerState && PlayerState->HasbIsABot() &&
                PlayerState->bIsABot;
            if (Iterator->Removed || !Controller || !OwnsSyntheticPawn || !IsAlive(Pawn))
            {
                Stop(*Iterator);
                Iterator = Bots.erase(Iterator);
                continue;
            }
            if (Iterator->Firing && Iterator->FiringPawn.Get() != Pawn)
                Stop(*Iterator);
            TickBot(*Iterator, Controller, Pawn, GameMode, Now);
            ++Iterator;
        }
        InsideTick = false;
        if (PendingReset)
        {
            ResetInternal();
            return;
        }
        auto Registrations = std::move(PendingRegistrations);
        PendingRegistrations.clear();
        for (const auto& Registration : Registrations)
            RegisterInternal(Registration.Get());
    }

    void UnregisterInternal(AFortPlayerControllerAthena* Controller)
    {
        PendingRegistrations.erase(std::remove_if(PendingRegistrations.begin(),
            PendingRegistrations.end(), [Controller](const auto& Pending)
            { return Pending.Get() == Controller; }), PendingRegistrations.end());
        for (auto& Bot : Bots)
            if (Bot.Controller.Get() == Controller)
            {
                Bot.Removed = true;
                if (!InsideTick && LifecycleDepth <= 1)
                    Stop(Bot);
            }
    }

    bool IsManagedInternal(const AFortPlayerControllerAthena* Controller)
    {
        if (!Controller || !IsAIEnabledForCurrentGame())
            return false;
        for (const auto& Bot : Bots)
            if (!Bot.Removed && Bot.Controller.Get() == Controller)
                return true;
        return false;
    }

    bool HasAnyInternal()
    {
        if (!IsAIEnabledForCurrentGame())
            return false;
        for (const auto& Bot : Bots)
            if (!Bot.Removed && VersionFeatureAdapter::IsLiveActor(Bot.Controller.Get()))
                return true;
        return false;
    }

    void DrainLifecycle()
    {
        if (InsideTick || LifecycleDepth)
            return;
        ++LifecycleDepth;
        if (PendingReset)
            ResetInternal();
        auto Registrations = std::move(PendingRegistrations);
        PendingRegistrations.clear();
        for (const auto& Registration : Registrations)
            RegisterInternal(Registration.Get());
        --LifecycleDepth;
    }

    void GuardedDrainLifecycle()
    {
        if (InsideTick || LifecycleDepth)
            return;
        __try { DrainLifecycle(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { LifecycleDepth = 0; }
    }

    void GuardedStop(FBot* Bot)
    {
        __try { Stop(*Bot); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void StopFaultedBots()
    {
        for (auto& Bot : Bots)
        {
            Bot.Removed = true;
            GuardedStop(&Bot);
        }
    }
}

bool PlayerBotRuntime::ValidateActions(std::string& Error)
{
    bool Result = false;
    ++GGuardedNativeCallDepth;
    __try { Result = ValidateActionsInternal(Error); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    --GGuardedNativeCallDepth;
    return Result;
}

UFortWorldItem* PlayerBotRuntime::GiveSpawnItem(AFortPlayerControllerAthena* Controller,
    const UFortItemDefinition* Definition, int Count)
{
    if (Count <= 0 || !VersionFeatureAdapter::IsLiveActor(Controller) ||
        !Controller->HasWorldInventory() ||
        !VersionFeatureAdapter::IsLiveActor(Controller->WorldInventory) ||
        !VersionFeatureAdapter::IsLiveObject(Definition))
        return nullptr;

    auto Inventory = Controller->WorldInventory;
    int LoadedAmmo = 0;
    const bool Ranged = Definition->IsA(UFortWeaponRangedItemDefinition::StaticClass());
    if (Ranged)
    {
        auto Weapon = static_cast<const UFortWeaponItemDefinition*>(Definition);
        auto Stats = Weapon->HasWeaponStatHandle() ? AFortInventory::GetStats(Weapon) : nullptr;
        if (Stats && FFortRangedWeaponStats::HasClipSize())
            LoadedAmmo = (std::max)(Stats->ClipSize, 0);
    }

    auto Item = Inventory->GiveItem(Definition, Count, LoadedAmmo);
    if (!Item || !Ranged || LoadedAmmo <= 0)
        return Item;

    UFortWorldItemDefinition* Ammo = nullptr;
    FNativeCall AmmoCall(Definition, "GetAmmoWorldItemDefinition_BP", { "ReturnValue" });
    if (AmmoCall.Field("ReturnValue", nullptr, sizeof(Ammo), true) &&
        AmmoCall.Invoke() && AmmoCall.ReadReturn(&Ammo, sizeof(Ammo)) &&
        VersionFeatureAdapter::IsLiveObject(Ammo) &&
        Ammo->IsA(UFortAmmoItemDefinition::StaticClass()))
    {
        const int MaxStack = Ammo->GetMaxStackSize();
        // Widen before multiplying and let the inventory cap an existing stack.
        const int Reserve = int((std::min)(int64(LoadedAmmo) * 3,
            int64((std::max)(MaxStack, 0))));
        if (Reserve > 0)
            Inventory->GiveItemToSingleStack(Ammo, Reserve);
    }
    return Item;
}

void PlayerBotRuntime::Register(AFortPlayerControllerAthena* Controller)
{
    ++GGuardedNativeCallDepth;
    ++LifecycleDepth;
    __try { RegisterInternal(Controller); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    --LifecycleDepth;
    GuardedDrainLifecycle();
    --GGuardedNativeCallDepth;
}

void PlayerBotRuntime::Unregister(AFortPlayerControllerAthena* Controller)
{
    ++GGuardedNativeCallDepth;
    ++LifecycleDepth;
    __try { UnregisterInternal(Controller); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    --LifecycleDepth;
    GuardedDrainLifecycle();
    --GGuardedNativeCallDepth;
}

void PlayerBotRuntime::Tick(const UNetDriver* Driver)
{
    ++GGuardedNativeCallDepth;
    ++LifecycleDepth;
    __try { TickInternal(Driver); }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        InsideTick = false;
        StopFaultedBots();
        FConfiguration::bBotAIEnabled.store(false, std::memory_order_relaxed);
        AIDebugLogger::MissingFeature("PlayerBotRuntime",
            "a bot runtime fault stopped bot AI; inspect the game build before spawning again");
    }
    --LifecycleDepth;
    GuardedDrainLifecycle();
    --GGuardedNativeCallDepth;
}

bool PlayerBotRuntime::IsManaged(const AFortPlayerControllerAthena* Controller)
{
    bool Result = false;
    __try { Result = IsManagedInternal(Controller); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return Result;
}

bool PlayerBotRuntime::HasAny()
{
    bool Result = false;
    __try { Result = HasAnyInternal(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return Result;
}

void PlayerBotRuntime::Reset()
{
    ++GGuardedNativeCallDepth;
    ++LifecycleDepth;
    __try { ResetInternal(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    --LifecycleDepth;
    GuardedDrainLifecycle();
    --GGuardedNativeCallDepth;
}
