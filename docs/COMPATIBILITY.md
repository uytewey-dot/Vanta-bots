# Compatibility and bot behavior

Open **Player Bot**, choose the target version, and turn on **Enable Bot AI**.
The tab is available before the match starts. **Automatic (current game)** uses
the loaded game's version. **Chapter 2 / Season 2 (12.xx)** and
**Chapter 2 / Season 4 (14.xx)** permit patches in the selected season;
**10.40**, **11.31**, **12.41**, **12.61**, **15.50**, **17.30**, **19.01**, **19.10**,
**24.20**, **26.30**, **28.30**, **31.41**, or **32.11** selections
permit bot spawning and AI only when that exact release is loaded. This setting
constrains bots; it does not change the actual game release or its SDK addresses.
The selection is saved immediately across game versions, even when **Auto Host**
or **Save Settings** is off. Resetting preferences restores **Automatic**.

Click a version card under **Bot Version** to select it. Seasons **12.xx** and
**14.xx**, and releases **10.40**, **11.31**, **12.41**, **12.61**, **15.50**,
**17.30**, **19.01**, **19.10**,
**24.20**, and **26.30** use the existing dynamic SDK discovery and reflected bot
actions, including their version-dependent vector/name layouts. The three newer
releases use the exact shipping profiles below. Adding these choices does not
establish live-game compatibility: server sessions and bots on each selected release
still need testing in their corresponding game builds.

Seven exact releases were checked against the supplied Spectra,
MagmaGS, OGS, Asteria, 17.30 Gameserver, and 19.01 source archives. Their action contracts
use 12-byte vectors/rotators, one-byte fire-mode arguments, and class-based
ability activation for reload. Before `spawnbot` creates actors on these seven
releases, the running game's required properties and action parameters are
validated. A missing or incompatible action blocks the command and logs the
specific failure. This check also applies when **Automatic** or a season-wide
choice is selected on one of those releases. It does not validate every server
hook or replace testing a match. See [Legacy SDK references](LEGACY_SDK_REFERENCES.md)
for archive fingerprints and evidence.

The 19.01 archive declares build `5.0.1-18489740+++Fortnite+Release-19.01`.
Its vectors and rotators still contain three 32-bit floats; the engine's major
version does not imply the later double-precision layout. The existing dynamic
path retains the correct 12-byte arguments for this release.

Spawn a bot in game with
`cheat spawnbot 1 scar` (or another ranged weapon from the item aliases). Bots
created without a ranged weapon remain unarmed for ranged combat. Ranged weapons
in the bot's starting loadout or requested by the command receive a full magazine
and up to three spare magazines of the matching ammo, capped by its stack size.
Reloading consumes that reserve through the game's normal ability lifecycle.
Set the detection and engagement ranges in the same tab. Turning AI off stops weapon
fire and leaves the existing stationary practice bots available. These settings
are saved with the server preferences.

AI runs on the server tick and uses the game's movement and weapon functions.
Bots choose hostile living participants and require line of sight before firing.
Control rotation approaches the target at a bounded rate. Observed angular motion
reduces tracking lag without aiming hitscan weapons ahead of the current target.
Firing requires aim alignment, a short reaction delay, and a settling window;
losing alignment, the target, or line of sight releases the trigger. Held fire
also checks alignment between the 30 Hz aim decisions.

Reflected weapon range, scope use, firing rate, and trigger type inform engagement
distance and burst length when available. Invalid or unlimited native ranges
fall back to the configured engagement range, which remains the upper limit.
Bots approach, retreat, alternate strafing direction, and reposition during
reloads. Distant or precision shots require movement to settle; periodic planted
windows also allow shooting when an obstacle blocks further approach. This is
direct movement through the game's collision system, not pathfinding. Scope
metadata informs tactics; the bot does not force targeting flags or activate ADS.

Reload status prevents firing into an active reload. Unsupported charge/release
weapons are not fired automatically. Projectile lead solves constant-velocity
interception only when a projectile weapon reports a finite usable speed, with a
0.35-second prediction limit. Hitscan weapons use direct aim. This does not model
projectile gravity, homing, or every weapon-specific firing behavior.
Reloads use the weapon's ability handle on the player's Ability System Component
(ASC) when available. On Chapter 2 builds that expose only
`TryActivateAbilityByClass`, the bot resolves the equipped weapon's granted reload
ability and activates it locally. A native rejection is respected; the AI does
not refill ammo directly or bypass reload timing. Ambiguous ability classes are
rejected. Other legacy builds can use reflected reload functions if available.
Bot creation checks the ability system and inventory before publishing the bot
to the match and removes temporary actors if those prerequisites fail.
Looting, building, and navigation around obstacles are not implemented.

Vanta-created bots no longer receive its forced map/minimap portrait components.
Suppression is registered before possession, survives AI/version/icon preference
changes, and blocks delayed cosmetic retries. Previously configured Vanta icons
are hidden and Vanta's portrait resource override is restored when still present.
Weak controller/pawn identities keep human players and reused actor addresses
separate. This change covers the persistent icons created by Vanta; ordinary
team rules, native reveals, pings, and weapon-noise indicators are unaffected.

Chapter 2 reflection was checked against the public SDKs for
[12.41, CL 12905909](https://github.com/Maranyja/Fortnite-12.41-SDK/tree/e4797481d479703308bffaab817eb4bfbcb5eab6)
and [14.60](https://github.com/pilottX11/Retrac-SDK-14.60/tree/9907b3a15b9610495583c58a41f6b2e9534ab963).
Both expose `TryActivateAbilityByClass` with a class pointer, a remote-activation
boolean and a boolean result, plus `GetAmmoWorldItemDefinition_BP`. Parameters
are resolved by reflection at runtime. Other patches in these seasons retain
the existing dynamic SDK path; neither dump inspection nor compilation verifies
a live match on those patches.

## Experimental shipping profiles

One DLL includes **experimental** profiles for these exact shipping builds and
selects the matching profile automatically from the game's build identity:

| Fortnite | Unreal Engine | Changelist | Exact build string |
| --- | --- | --- | --- |
| 28.30 | 5.4 | 31511038 | `5.4.0-31511038+++Fortnite+Release-28.30` |
| 31.41 | 5.5 | 37324991 | `5.5.0-37324991+++Fortnite+Release-31.41` |
| 32.11 | 5.5 | 38202817 | `5.5.0-38202817+++Fortnite+Release-32.11` |

Different engine versions, changelists, and metadata-only fallback identities
cannot enable these profiles. Other supported legacy releases retain their
existing version path. Opt-in player bot AI is included for all three profiles,
with movement, hostile target selection, line-of-sight checks, weapon fire, and
reload through the bot's ASC.

RVAs and layouts are pinned to shipping-game SDK dumps:

- **28.30:** [Helix-Dev-Q/FortniteDumps at `a90cd2ba76e22ffe55cea3fbf0812f592ec754f7`](https://github.com/Helix-Dev-Q/FortniteDumps/blob/a90cd2ba76e22ffe55cea3fbf0812f592ec754f7/Chapter%205%20Season%201/SDK/5.4.0-31511038%2B%2B%2BFortnite%2BRelease-28.30-FortniteGame.7z), archive `5.4.0-31511038+++Fortnite+Release-28.30-FortniteGame.7z`.
- **31.41:** [Ducki67/Fn-SDK at `f195a7101664deda221194ab0f826865f9d7e05b`](https://github.com/Ducki67/Fn-SDK/blob/f195a7101664deda221194ab0f826865f9d7e05b/31.41%20SDK.rar), archive `31.41 SDK.rar`.
- **32.11:** [DauntingEmperor/32.11-SDK at `b91ba8a8b721450e7ff5c2ad5c6faaa1b6fea066`](https://github.com/DauntingEmperor/32.11-SDK/tree/b91ba8a8b721450e7ff5c2ad5c6faaa1b6fea066), shipping `SDK.hpp`, `SDK/Basic.hpp`, and `SDK/CoreUObject_classes.hpp`.

The 32.11 source is selected by its shipping RVAs and layouts. A matching release
string alone does not establish compatibility with an editor/UEFN dump.

Startup validates the build identity, native addresses, object array, name
conversion, and bot action layouts before installing server hooks; failed
validation stops initialization with a specific error. These profiles require
Iris and select it through the reflected game-mode field before creating the
net driver. The dump does not expose the driver's native Iris pointer: runtime
discovery accepts a unique live replication system with the profile's expected
bridge class inside the documented driver tail. These checks do not establish
that every native finder has the correct ABI.

Reflected names and `TArray<FName>` use the shipping build's four-byte name
storage, preserving neighbouring fields when names are assigned. Reloads use
the equipped weapon's reload ability on the bot's live ASC. The 32.11 profile
also handles its dump-specific encoded metadata and shuffled reflection fields.

**All three profiles remain experimental.** Windows x64 cross-compilation with
Clang/LLD 19 and the official MSVC/Windows SDK checks compilation and linking.
Native finder ABIs and live game/server sessions for these profiles remain
untested; a compiled DLL does not establish working gameplay. Portable tests
cover exact build selection, object-array bounds, four-byte name writes, and
32.11 metadata transforms using synthetic fixtures.


[← Back to the project](../README.md) · [Building and tests](BUILDING.md)
