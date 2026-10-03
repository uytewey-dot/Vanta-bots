# Roadmap

[← Back to the project](../README.md)

## Open tasks

- [ ] Test live game sessions and bot behavior on each version target.
- [ ] Fix the known Late Game crash on 22.40.
- [ ] Improve One Shot FX with effects for individual playlists.
- [ ] Fix island loading in Creative / Custom Plots.
- [ ] Consider a dedicated Logs tab.

## Current AI limitations

Bots can track moving targets, choose weapon-appropriate distances, strafe, pause for distant shots, fire controlled bursts, and reposition while reloading. Looting, building, and navigation around obstacles are not implemented yet. Projectile prediction does not model gravity or homing. Charge/release-trigger weapons require additional handling. Persistent Vanta bot portraits are suppressed; native game reveal and weapon-noise indicators retain their normal behavior.

## Recent work

- [x] Add exact 19.01 with an SDK action audit and stable saved selection IDs.
- [x] Improve moving-target tracking, projectile interception, weapon-aware bursts, and shot stability.
- [x] Add approach/retreat hysteresis, strafing, reload movement, and periodic planted firing windows.
- [x] Suppress Vanta's bot map/minimap icons before possession and during cosmetic retries.
- [x] Add exact 10.40, 11.31, 12.41, 12.61, 15.50, and 17.30 targets, preserving saved selection IDs.
- [x] Audit supplied legacy SDK actions and check their reflected contracts before spawning.
- [x] Improve aim tracking, alignment checks, and weapon firing/reload timing.
- [x] Add 12.xx and 14.xx season targets.
- [x] Improve ammo loadouts and reload support for older SDKs.
- [x] Persist version selection and block bots when the loaded game does not match.
- [x] Refresh the interface and add optional animations.

## Previously marked complete

These entries are preserved from the earlier README. They do not mean the features have been retested on every game version.

- [x] Randomize Lootpool.
- [x] Players tab and distance display.
- [x] Auto Dump and dumping fixes.
- [x] Pickaxe Stutter.
- [x] Creative / Custom Plots interface.
- [x] Trickshot tab.
- [x] Start the bus through countdown/phase handling instead of `executeconsolecommand`.
