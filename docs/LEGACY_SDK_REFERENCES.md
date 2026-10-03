# Legacy SDK reference audit

The seven user-supplied archives below were inspected as reference material for
Vanta Bots version selection and reflected bot actions. This records the exact
inputs and the API contracts observed in their SDKs. The archives, their generated
SDKs, and their bot implementations are not included in this repository.

**SDK inspection and a successful build do not establish live gameplay
compatibility.** Startup, spawning, replication, movement, aiming, firing, and
reload behavior still require a server session on each corresponding game build.
A version label in an archive is not an independently verified shipping build
identity. These releases retain the existing dynamic SDK discovery path; this
audit does not introduce fixed-address shipping profiles.

## Input archives

SHA-256 identifies the uploaded ZIP bytes, including their directory layout.
It does not identify an upstream Git commit or imply an upstream endorsement.

| Release | Uploaded archive | SHA-256 |
| --- | --- | --- |
| 10.40 | `Spectra-10.40-main.zip` | `90a494d5a3962ae96736d52bebb8b502edb4657e7c33bf62a3cc8d8246d16af0` |
| 11.31 | `MagmaGS-11.31-main.zip` | `849f59f18e8319c7ae62ab8d532bf3a32887ea99e8e21abe74051cdc15ce2054` |
| 12.41 | `OGS-12.41-main.zip` | `ef86b2f6c7fb4832a818cae9a80359b325a15126186861192d5657bfe0bd5e59` |
| 12.61 | `Asteria-12.61-main.zip` | `72673f832f0a6fb0e864ebadf50412e3fc39d397b3e9ebcf279d1c66c1123f70` |
| 15.50 | `OGS-15.50-main.zip` | `a943cbbfd87b03bf3043e3989d571ebc8ef542a4281db9b5e77b8655a2f6c05b` |
| 17.30 | `17.30-Gameserver-main.zip` | `94f2ff4178c475b809199b961e7e7748ac69a29d84d3f06ce0eded87caed380e` |
| 19.01 | `19.01.zip` | `ab1d4f5a3f63b1a868d2b7977e9c6937ebce48748d7a95f11ad6bb8388f11ecc` |

No standalone `LICENSE` or `COPYING` file was found in these extracted archives.
Their availability does not establish permission to redistribute their source.
This integration records API facts and uses Vanta's existing reflection layer;
it does not copy third-party implementations or relicense the uploaded projects.
The repository's existing [LICENSE](../LICENSE) remains unchanged.

## Reflected API contracts

All seven reference SDKs use 12-byte `FVector` and `FRotator` values for these
actions. Offsets below are within an individual reflected function's parameter
buffer, not offsets into an actor or a native executable. Runtime calls resolve
names, sizes, and offsets from the loaded game. They must not reuse these
12-byte layouts for newer releases that use 24-byte vectors and rotators.

| Owner and function | Observed legacy parameters |
| --- | --- |
| `Actor.K2_GetActorLocation` | `ReturnValue`: vector12 at offset 0 |
| `Pawn.AddMovementInput` | `WorldDirection`: vector12 at 0; `ScaleValue`: float4 at 12; `bForce`: bool1 at 16 |
| `Controller.SetControlRotation` | `NewRotation`: rotator12 at 0 |
| `Controller.LineOfSightTo` | `Other`: object pointer8 at 0; `ViewPoint`: vector12 at 8; `bAlternateChecks`: bool1 at 20; `ReturnValue`: bool1 at 21 |
| `FortPawn.PawnStartFire` / `PawnStopFire` | `FireModeNum`: uint8 at 0 |
| `AbilitySystemComponent.TryActivateAbilityByClass` | `InAbilityToActivate`: class pointer8 at 0; `bAllowRemoteActivation`: bool1 at 8; `ReturnValue`: bool1 at 9 |
| `FortWorldItemDefinition.GetAmmoWorldItemDefinition_BP` | `ReturnValue`: object pointer8 at 0 |
| `Actor.GetActorEyesViewPoint` | `OutLocation`: output vector12 at 0; `OutRotation`: output rotator12 at 12; neither is a return parameter |
| `Actor.GetVelocity` | `ReturnValue`: vector12 at 0 |
| `Controller.GetControlRotation` | `ReturnValue`: rotator12 at 0 |
| `FortWeapon.GetFiringRate` | `ReturnValue`: float4 at 0 |
| `FortWeapon.IsReloading` | `ReturnValue`: bool1 at 0 |
| `FortWeapon.GetMuzzleLocation` | `PatternIndex`: int32 at 0; `ReturnValue`: vector12 at 4 |
| `FortWeapon.GetWeaponDataTriggerType` | `ReturnValue`: `EFortWeaponTriggerType` enum1 at 0 |
| `FortWeapon.GetTimeToNextFire` | `ReturnValue`: float4 at 0 |
| `FortWeapon.IsProjectileWeapon` | `ReturnValue`: bool1 at 0 |
| `FortWeapon.GetProjectileSpeed` | `ChargePercent`: float4 at 0; `ReturnValue`: float4 at 4 |
| `FortWeapon.GetRange` | `ReturnValue`: float4 at 0 |
| `FortWeaponRanged.UseScopeTargeting` | `ReturnValue`: bool1 at 0 |

The aiming and firing query entries document available APIs; their presence
in the table alone does not mean every query is used by the bot runtime. A dump
establishes parameter shape, not the accuracy of a returned value on a dedicated
server or the timing of the weapon's native ability lifecycle.

Across all seven SDKs, `EFortWeaponTriggerType` has an underlying `uint8` and
values `OnPress = 0`, `Automatic = 1`, `OnRelease = 2`, and
`OnPressAndRelease = 3`; `EFortWeaponTriggerType_MAX = 4` is a sentinel.
The runtime queries projectile speed with `ChargePercent = 0.0f` to obtain the
uncharged value. This does not establish speed at other charge levels or a full
projectile trajectory model. Optional query results still need finite-value and
range checks, and the game's firing cooldown remains authoritative.

The reflected handle overload `AbilitySystemComponent.TryActivateAbility` was
not found in these SDKs. Reload therefore needs the existing by-class fallback:
resolve the equipped weapon's granted reload ability from its spec handle,
reject an ambiguous class, and let the native ability consume reserve ammo and
enforce timing. Do not substitute a direct magazine refill. The no-argument
`FortPawn.ReloadWeapon` and `FortWeapon.Reload` alternatives were not found in
these SDKs either.

## Exact SDK evidence

Paths are relative to the root of each ZIP and are intentionally recorded as
text: the reference SDK files are not vendored. Append the filename from a table
heading to the corresponding SDK directory below. Numbers are one-based source
lines at the start of the generated parameter struct in the hashed archive.

| Release | SDK directory within the archive |
| --- | --- |
| 10.40 | `Spectra-10.40-main/10.40/SDK/SDK/` |
| 11.31 | `MagmaGS-11.31-main/FortMP/SDK/` |
| 12.41 | `OGS-12.41-main/OGS-S12/SDK/SDK/` |
| 12.61 | `Asteria-12.61-main/Asteria/SDK/Source/Runtime/DevelopmentKit/SDK/` |
| 15.50 | `OGS-15.50-main/Spectra-S15/SDK/SDK/` |
| 17.30 | `17.30-Gameserver-main/17.30/SDK/SDK/` |
| 19.01 | `Forsaken-19.01-main/Forsaken/SDK/Source/Runtime/DevelopmentKit/SDK/` |

The generated `SDK.hpp` immediately above each SDK directory declares these
build strings. They describe the supplied dumps; Vanta does not treat them as
new fixed-address profiles or proof of a running executable's identity.

| Release | Declared build | Line in `SDK.hpp` |
| --- | --- | ---: |
| 10.40 | `4.23.0-9380822+++Fortnite+Release-10.40` | 10 |
| 11.31 | `4.24.0-10800459+++Fortnite+Release-11.31` | 6 |
| 12.41 | `4.25.0-12905909+++Fortnite+Release-12.41` | 10 |
| 12.61 | `4.25.0-13498980+++Fortnite+Release-12.61` | 10 |
| 15.50 | `4.26.0-15526472+++Fortnite+Release-15.50` | 10 |
| 17.30 | `4.26.1-17004569+++Fortnite+Release-17.30` | 10 |
| 19.01 | `5.0.1-18489740+++Fortnite+Release-19.01` | 10 |

### Engine_parameters.hpp

| Function | 10.40 | 11.31 | 12.41 | 12.61 | 15.50 | 17.30 | 19.01 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `Actor.K2_GetActorLocation` | 3231 | 3988 | 1496 | 15310 | 1549 | 1123 | 1592 |
| `Pawn.AddMovementInput` | 5499 | 4991 | 7532 | 15455 | 1655 | 1199 | 4590 |
| `Controller.SetControlRotation` | 5825 | 5051 | 8405 | 17329 | 9942 | 7358 | 7071 |
| `Controller.LineOfSightTo` | 5930 | 5095 | 8550 | 17474 | 10087 | 7463 | 7229 |
| `Actor.GetActorEyesViewPoint` | 2874 | 4499 | 987 | 14801 | 1040 | 758 | 1036 |
| `Actor.GetVelocity` | 3171 | 4196 | 1413 | 15227 | 1466 | 1063 | 1509 |
| `Controller.GetControlRotation` | 5858 | 5171 | 8451 | 17375 | 9988 | 7391 | 7117 |

### FortniteGame_parameters.hpp

| Function | 10.40 | 11.31 | 12.41 | 12.61 | 15.50 | 17.30 | 19.01 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `FortPawn.PawnStartFire` | 1824 | 14203 | 40837 | 21608 | 31758 | 13484 | 4865 |
| `FortPawn.PawnStopFire` | 1832 | 14195 | 40848 | 21619 | 31769 | 13492 | 4876 |
| `FortWorldItemDefinition.GetAmmoWorldItemDefinition_BP` | 8201 | 4167 | 7087 | 6518 | 24468 | 9838 | 19637 |
| `FortWeapon.GetFiringRate` | 5758 | 12785 | 23994 | 724 | 11907 | 9192 | 28397 |
| `FortWeapon.IsReloading` | 6071 | 12448 | 24482 | 1212 | 12450 | 9593 | 28980 |
| `FortWeapon.GetMuzzleLocation` | 5822 | 12712 | 24082 | 812 | 11995 | 9256 | 28503 |
| `FortWeapon.GetWeaponDataTriggerType` | 6015 | 12520 | 24383 | 1113 | 12329 | 9505 | 28848 |
| `FortWeapon.GetTimeToNextFire` | 5927 | 12608 | 24251 | 981 | 12164 | 9377 | 28672 |
| `FortWeapon.IsProjectileWeapon` | 6063 | 12456 | 24471 | 1201 | 12439 | 9585 | 28958 |
| `FortWeapon.GetProjectileSpeed` | 5862 | 12672 | 24161 | 891 | 12074 | 9312 | 28582 |
| `FortWeapon.GetRange` | 5871 | 12664 | 24174 | 904 | 12087 | 9321 | 28595 |
| `FortWeaponRanged.UseScopeTargeting` | 25301 | 63779 | 65655 | 12166 | 42541 | 26339 | 29696 |

### FortniteGame_structs.hpp

| Enum declaration | 10.40 | 11.31 | 12.41 | 12.61 | 15.50 | 17.30 | 19.01 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `EFortWeaponTriggerType` | 8002 | 7612 | 9660 | 9808 | 10616 | 11435 | 11936 |

### GameplayAbilities_parameters.hpp

| Function | 10.40 | 11.31 | 12.41 | 12.61 | 15.50 | 17.30 | 19.01 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `AbilitySystemComponent.TryActivateAbilityByClass` | 1419 | 871 | 2118 | 2016 | 2016 | 982 | 750 |

For example, the 12.41 reload contract is at
`OGS-12.41-main/OGS-S12/SDK/SDK/GameplayAbilities_parameters.hpp:2118`.

## 19.01-specific evidence

The uploaded `19.01.zip` contains the `Forsaken-19.01-main/` project. Its
`README.md` calls it a 19.01 game server with a Phoebe player-AI implementation.
The dump's UE 5.0.1 label does **not** change the reflected bot vector layout:
`CoreUObject_structs.hpp:584` declares float `FVector` coordinates, and
`CoreUObject_structs.hpp:1540` declares float `FRotator` angles. Both occupy
12 bytes in the action parameter structs above. Vanta retains its existing
pre-20.00 float layout and validates the loaded function metadata.

The exact 19.01 selector is separate from the existing 19.10 selector; choosing
one must not authorize spawning on the other. The supplied dump provides
reference evidence for 19.01 only. The following properties also retain the
expected scalar widths in that dump, with runtime lookup still required:

| Property | Evidence within the 19.01 SDK directory | Observed width |
| --- | --- | ---: |
| `FortPlayerController.WorldInventory` | `FortniteGame_classes.hpp:6376` | 8 bytes |
| `FortWeapon.AmmoCount` | `FortniteGame_classes.hpp:16688` | 4 bytes |
| `FortWeapon.ReloadAbilitySpecHandle` | `FortniteGame_classes.hpp:16719` | 4 bytes |
| `AbilitySystemComponent.OwnerActor` | `GameplayAbilities_classes.hpp:195` | 8 bytes |
| `AbilitySystemComponent.AvatarActor` | `GameplayAbilities_classes.hpp:196` | 8 bytes |

`FortPawn.GetHealth` has a float return at offset zero in
`FortniteGame_parameters.hpp:5766`. The handle-based reflected
`AbilitySystemComponent.TryActivateAbility`, `FortPawn.ReloadWeapon`, and
`FortWeapon.Reload` are absent from the supplied parameter headers; the
by-class reload contract in the table remains available.

The archive also contains explicit debug minimap registration. Relative to
`Forsaken-19.01-main/Forsaken/`,
`FortniteGame/Source/FortniteAI/Private/AI/FortAthenaAIBotController.cpp:85`
enables `DebugMinimapData.bIsOverridden`, and line 91 calls `AddBotOnMinimap`.
The latter is a manually inserted native-address wrapper at
`SDK/Source/Runtime/DevelopmentKit/SDK/FortniteGame_classes.hpp:25156`,
not a reflected function declaration. Its native address and the native
Phoebe controller's `DebugMinimapData` layout are not imported into Vanta's
managed player controller. The dump does not establish that removing this
registration also removes ordinary squad markers or weapon-noise indicators.

No standalone license file was found in the 19.01 archive, and the README
contains no redistribution license. The audit therefore adds API facts and
version metadata without copying its source implementation.

## Controller and inventory boundaries

Vanta's managed player bots use synthetic `FortPlayerControllerAthena` instances
and their reflected `WorldInventory`. The uploaded bot examples commonly spawn
native Phoebe AI controllers, which use a different controller hierarchy and an
`Inventory` property. A native Phoebe controller must not be cast to Vanta's
managed player-controller type, and its inventory layout must not be copied into
that type.

Concrete reference examples:

- `Spectra-10.40-main/10.40/Bots.h:62` calls `ServerBotManager->SpawnBot`.
  `Spectra-10.40-main/10.40/ServerBotManager.h:14` calls the original native
  spawn function and obtains an `AFortAthenaAIBotController`;
  `ServerBotManager.h:89` checks the AI controller's `Inventory`.
- `MagmaGS-11.31-main/FortMP/Utils.h:303` declares an AI-controller-based bot;
  `Utils.h:325` gives items through `PC->Inventory`, and `Utils.h:385` calls the
  Phoebe mutator's `SpawnBot`.
- `OGS-12.41-main/OGS-S12/PlayerBots.h:1211` calls the Phoebe mutator's
  `SpawnBot`; `PlayerBots.h:1216` casts the controller to
  `ABP_PhoebePlayerController_C`.
- `Asteria-12.61-main/Asteria/FortniteGame/Source/FortniteAI/Private/AI/FortAthenaAIBotController.cpp:57`
  initializes a native bot's `Inventory` before calling the original inventory
  initializer.
- `OGS-15.50-main/Spectra-S15/Bots.h:143` checks the native controller's
  `Inventory` and creates it on the next line. Its startup item loop assigns
  9,999 rounds at line 166; Vanta does not copy that behavior and uses magazine
  statistics plus bounded reserve ammo instead.
- `17.30-Gameserver-main/17.30/PlayerBots.h:22` stores an
  `AFortAthenaAIBotController` in its Phoebe bot wrapper. The behavior tree
  constructed at line 104 includes bus and warmup tasks; that example does not
  establish working combat in Vanta.

Ordinary player-controller `WorldInventory` is present in the reference SDKs,
including `Spectra-10.40-main/10.40/SDK/SDK/FortniteGame_classes.hpp:18976` and
`MagmaGS-11.31-main/FortMP/SDK/FortniteGame_classes.hpp:6996`. Its location must
still be resolved from the loaded class.

The weapon's four-byte `ReloadAbilitySpecHandle` also moves between builds:
`Spectra-10.40-main/10.40/SDK/SDK/FortniteGame_classes.hpp:3531` records 0x780,
`MagmaGS-11.31-main/FortMP/SDK/FortniteGame_classes.hpp:10032` records 0x7F8,
and `OGS-12.41-main/OGS-S12/SDK/SDK/FortniteGame_classes.hpp:18441` records
0x938. These are evidence for dynamic property lookup, not constants to import.

[Compatibility and limitations](COMPATIBILITY.md) · [Back to the project](../README.md)
