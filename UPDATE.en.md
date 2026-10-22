[简体中文](UPDATE.md) | [**English**](UPDATE.en.md)

# UPDATE

## V0.2.2 — Fog-of-War Information Fix

**Release date:** 2026-10-22
**Type:** GitHub development snapshot (patch release)
**Platform:** PSP 1000 / PPSSPP

V0.2.2 fixes the fog-of-war information leak reported on PSP 1000 hardware
while keeping V0.2.1's turn briefing and city-overview cleanup.

### Fix

- When the cursor is over an enemy unit on a currently non-visible tile, the
  bottom status bar and tile info panel no longer show its type, owner, HP, or
  movement. Player units remain listed.
- Unexplored tiles no longer leak city information through the status bar or
  info panel; cities that have already been explored remain known.
- Unit sprites were already drawn only on visible tiles; this fixes the
  separate leak through the cursor status bar and info panel.

### Version and compatibility

- The in-game version string, boot log, and XMB art labels are updated to
  `V0.2.2`.
- The save format remains **version 3** and is compatible with V0.2.1 and V0.2
  saves.
- All 14 host suites plus i18n/audio checks continue to pass; the fix does not
  change save queues or turn logic.

### Validation status

- **Host tests:** `make test` passes (14 suites plus i18n/audio).
- **PPSSPP:** the V0.2.2 EBOOT boot log reports `CIVITAS PSP V0.2.2`; the
  fog-HUD behavior still needs confirmation in a visible environment or on
  hardware.
- **PSP 1000:** the issue was found on hardware; V0.2.2 needs a hardware retest
  to confirm hidden enemy units no longer expose info.

### Release files

```text
release/v0.2.2/EBOOT.PBP
release/v0.2.2/civpsp-v0.2.2.zip
release/v0.2.2/SHA256SUMS
```

### Checksums

See [`release/v0.2.2/SHA256SUMS`](release/v0.2.2/SHA256SUMS).

### Upgrading from V0.2.1

Replace `EBOOT.PBP`; the save format and paths are unchanged. The `v0.2.1` tag
and `release/v0.2.1/` remain at their original released state.

## V0.2.1 — Turn Briefing and City Overview Cleanup

**Release date:** 2026-10-22
**Type:** GitHub development snapshot (patch release)
**Platform:** PSP 1000 / PPSSPP

V0.2.1 adds the AI/end-turn centered turn briefing and cleans up the city
overview on top of V0.2. V0.2's city sieges, difficulty, Classic/Endless modes,
denouncement, era-grouped Science screen, and save v3 remain unchanged.

### Highlights

#### Turn briefing

- AI/end-turn unit battles, city assaults, war declarations, peace, and AI
  proposals are collected in a transient queue and shown in a centered
  briefing at the start of the new turn.
- Each page shows three events; `L/R` pages, and `Cross/Circle` closes the
  briefing.
- Player-initiated attacks keep the existing bottom toast so normal play is not
  interrupted.
- Each battle records the tile, both civilizations and unit types,
  damage/counter damage, unit deaths, city defense/HP damage, and capture
  result.
- Battles involving the player are always recorded; AI-vs-AI battles are
  recorded only on player-visible tiles, so the briefing does not leak fog of
  war.
- The queue holds at most 32 events and is not saved. New game, load,
  dismissal, and the start of `World_EndTurn` clear it.

#### City overview cleanup

- The city overview now shows only `HP` and the damage-absorbing city defense
  pool; the old `Defense` strength row is gone.
- The internal `City_Defense` strength still affects damage calculations but is
  no longer displayed as a separate value.

#### Version and assets

- The in-game version string, boot log, and XMB art labels are updated to
  `V0.2.1`.
- Strings, fonts, audio, terrain atlas, and XMB art are regenerated.
- The save format remains **version 3** and continues to load migrated V0.2 and
  V0.1 saves.

### Installation

Copy `EBOOT.PBP` to:

```text
ms0:/PSP/GAME/CIVPSP/EBOOT.PBP
```

or open the EBOOT directly in PPSSPP.

### Validation status

- **Host tests:** 14 suites plus i18n/audio checks pass; the new
  `test_turn_report` covers queue contents, capacity, and clearing.
- **PPSSPP:** the V0.2.1 build loaded a fixture with an adjacent AI warrior at
  war and ended the turn. The log recorded
  `ai combat ... dmg=30 counter=30`, `report: open turn=2 events=1`, and
  `report: dismissed turn=2` after Cross; the boot log reported
  `CIVITAS PSP V0.2.1`.
- X11 captures are black under WSLg, so this validates the flow and log path
  rather than final visual layout; PSP 1000 hardware validation is still
  pending.

### Release files

```text
release/v0.2.1/EBOOT.PBP
release/v0.2.1/civpsp-v0.2.1.zip
release/v0.2.1/SHA256SUMS
```

### Checksums

See [`release/v0.2.1/SHA256SUMS`](release/v0.2.1/SHA256SUMS).

### Upgrading from V0.2

V0.2.1 loads V0.2 version-3 saves directly; the save format is unchanged. Back
up `ms0:/PSP/SAVEDATA/CIVPSP/` first. The `v0.2` tag and `release/v0.2/` remain
at their original released state.

## V0.2 — City Sieges and Difficulty

**Release date:** 2026-10-22
**Type:** GitHub development snapshot
**Platform:** PSP 1000 / PPSSPP

V0.2 reworks city combat on top of the complete V0.1 4X loop, adds
difficulty, Classic/Endless modes, denouncement, and the era-grouped Science
screen, and upgrades the save format to v3.

### Highlights

#### City siege combat

- Cities have **100 max HP** and **100 max defense**. A city cannot be
  assaulted while a garrison unit still stands on its center.
- Each assault drains the defense pool first, then city HP; the city is
  captured only when HP reaches zero. A captured city transfers at full
  HP/defense with population reduced by one.
- A city that was not hit by any player or AI attack during a turn recovers
  **+10 HP** at the end of that turn. Defense never regenerates on its own.
- A worker on or next to a city can use Repair City Defense from the unit menu: **+25**
  normally, **+15** with Walls, capped at the current maximum.
- Walls raise the defense cap from 100 to **150** and reduce each worker
  repair; completing Walls does not free-heal the current defense pool.
- Tile information, the city overview, and the city status panel show HP and
  defense.
- AI rivals use the same `Combat_AttackCity` rules. Killing a garrison never
  captures the city by itself.

#### Modes and difficulty

- **Classic mode**: choose **100 / 200 / 300 turns** at New Game; reaching the
  limit resolves the game with the existing score formula.
- **Endless mode**: replaces Timed mode, has no turn limit, and ends only by
  conquest, science, diplomacy, victory, or defeat.
- **Difficulty**:
  - Easy: one extra player warrior, AI treasury -10, later wars and higher army
    thresholds, fewer/slower barbarians.
  - Normal: the V0.1 baseline.
  - Hard: one extra warrior and +20 treasury for every AI, earlier wars and a
    wider relation threshold, faster/higher-cap barbarians, and one extra
    barbarian warrior near the player start.

#### Diplomacy, controls, and science

- Diplomacy adds Denounce: **-8 relation**, once per rival per turn, reset at
  the end of the turn.
- `L` toggles the minimap; `R` selects the next player unit with movement.
- The analog camera now moves the selection tile with the view center.
- The Science screen groups technologies by four eras, with per-era completion
  counts, costs, prerequisites, and unlocks; `L/R` jumps between eras.

#### Saves and compatibility

- Save format is **version 3**; it adds city HP/defense/last-hit turn,
  difficulty, and per-turn denouncement state.
- Version 2 saves migrate automatically:
  - Old Standard saves become Classic **200-turn** games.
  - Old Timed saves become **Endless**; legacy countdown fields are cleared.
  - Old cities start at full HP/defense with last-hit turn 0; difficulty
    defaults to Normal.
- Autosave, three manual slots, and pause/main-menu loading continue to work.
- Version strings, XMB art labels, and documentation are synchronized to V0.2.

### Fixes and stability work

- City HP/defense recovery is resolved after all player and AI attacks at the
  end of the turn, so a besieged city cannot heal before the AI strikes.
- Added legacy city-pool bounds checks before v2 migration so a corrupted save
  cannot write past fixed arrays.
- Fixed the duplicated `+N` text in the Repair City Defense message and
  localized the city-assault result detail.
- Added host coverage for city assault, garrison shielding, Walls, repair,
  end-turn HP recovery, denouncement, difficulty starts, v3 round-trips, and
  v2 migration.
- Regenerated strings, fonts, audio, terrain atlas, and XMB art.

### Installation

Copy `EBOOT.PBP` to:

```text
ms0:/PSP/GAME/CIVPSP/EBOOT.PBP
```

or open the EBOOT directly in PPSSPP.

### Controls

See the full table in [README.en.md](README.en.md#controls). V0.2 changes:

- `L`: toggle minimap
- `R`: next player unit with movement
- Worker `Triangle → Repair City Defense`: repair a friendly city on or next to the worker
- Science `L/R`: previous/next era

### Validation status

- **Host tests (V0.2 package):** 13 suites plus i18n/audio checks pass; new
  coverage includes city assault, garrisons, Walls/repair/HP recovery,
  denouncement, difficulty, and save migration. V0.2.1 has 14 suites; see the
  V0.2.1 release section above.
- **PPSSPP:** the final V0.2 EBOOT passed a functional smoke test: Hard/Classic
  300 start (17 units, 3 cities, difficulty 2), `L` minimap, `R` unit cycling,
  founding a city, city and Science screens, denouncement (relation 0→-8), an
  ungarrisoned city assault and capture, a garrisoned city where the garrison
  was killed first and the city captured on the next turn, and a generated v2
  Standard save migrated to Classic 200. WSLg screenshots are unavailable, so
  this validates logs and functional paths rather than final visual layout;
  language switching and hardware visuals still need a final pass.
- **PSP 1000:** earlier M6–M9 feature smoke passed; a full V0.2 city-siege,
  difficulty, and v2-load run still needs hardware validation.

### Known limitations

- No naval units; coast/ocean remain impassable.
- Trade is gold-only; open borders share vision but do not gate movement.
- No resource trading, overlapping sound effects, or a settings-menu minimap
  toggle.
- City assault has no ranged attack or siege engines; the garrison and city
  must be handled by the same adjacent army across turns.
- Development build: balance and UI may change after hardware feedback.

### Release files

```text
release/v0.2/EBOOT.PBP
release/v0.2/civpsp-v0.2.zip
release/v0.2/SHA256SUMS
```

### Checksums

See [`release/v0.2/SHA256SUMS`](release/v0.2/SHA256SUMS).

### Upgrading from V0.1

V0.2 loads V0.1 version-2 saves directly. Old Standard saves continue as
Classic 200-turn games; old Timed saves continue as Endless games and their
remaining time is ignored. Back up `ms0:/PSP/SAVEDATA/CIVPSP/` first.

## V0.1 — First Development Release

**Release date:** 2026-10-21
**Type:** GitHub development snapshot
**Platform:** PSP 1000 / PPSSPP

V0.1 is the first public development release of CIVITAS PSP. It consolidates
the internal M0–M9.12 development line into a playable, documented build.

### Highlights

#### Gameplay

- 64×48 deterministic map with forests, hills, mountains, desert, tundra,
  coast, ocean, resources, fog of war, and connected walkable terrain.
- Player civilization plus three AI civilizations and barbarians.
- City founding, 5×5 work area, citizen assignment/locking, food growth,
  starvation, production queues, gold rush, nine buildings, border growth, and
  tile purchase.
- Six unit types: settler, warrior, scout, swordsman, horseman, worker.
- Terrain and city defense, counterattacks, healing, city capture, and
  civilization-colored unit/city sprites.
- 30 technologies across four eras, with prerequisites, unlocks, and
  data-driven yield effects.
- Diplomacy with relations, gifts, trade, non-aggression pacts, open borders,
  alliances, AI proposals, and diplomatic victory.
- Conquest, science, diplomacy, and score victories; Standard (200 turns) and
  Timed (30/60/90 minutes) modes.
- Three manual save slots plus autosave using the version-2 save format.

#### Presentation and usability

- Chinese is the default UI language; English is selectable in Options.
- 12-page in-game manual (`GAME MANUAL`) accessible from the main menu and
  Pause.
- Explored-range minimap in the bottom-right corner, collapsed behind a
  `MINIMAP` shortcut button; `L + R` toggles it.
- Generated ambient loop and eight sound effects, with a music toggle in
  Options.
- XMB `ICON0.PNG` and `PIC1.PNG` with gradient/compass/city-skyline artwork and
  `V0.1 DEV` / `V0.1 BUILD` labels.
- Menu fade, staggered slide-in, and selection glow transitions.
- User-tuned economy data: terrain gold, richer improvements and buildings,
  and food-per-population 3.

### Fixes and stability work

- Fixed map generation producing walkable pockets enclosed by mountains;
  walkable terrain is now fully connected.
- Fixed Chinese glyph coverage, layout row spacing, coordinate comma rendering,
  and dense Science/Production/Diplomacy lists.
- Fixed silent generated audio: normalization now scales to 16-bit full scale,
  with a non-silent WAV regression check.
- Reworked PSP audio output to a standard 44.1kHz stereo channel with explicit
  data-cache writeback; generated clips peak at 80% and music ducks under SFX.
- Added civilization color variants and owner bars to units/cities.
- Added minimap, 12-page manual, menu transitions, and music toggle.

### Compatibility

- Save format: **version 2**.
- Map/World layout unchanged from the M7/M8 development line.
- M7/M8/M9 development saves should load in V0.1 unless a future format change
  is explicitly documented.

### Installation

Copy `EBOOT.PBP` to:

```text
ms0:/PSP/GAME/CIVPSP/EBOOT.PBP
```

or open the EBOOT directly in PPSSPP.

### Controls

See the control table in [README.en.md](README.en.md#controls). Key additions:

- `L + R`: toggle minimap
- `Select`: end turn
- `Start`: diplomacy
- `Options → MUSIC`: toggle background music
- `Options → LANGUAGE`: switch Chinese/English

### Validation status

- **Host tests:** 13 suites plus i18n/audio checks pass.
- **PPSSPP:** menus/manual, map, city tabs, diplomacy, saves, minimap, score
  result, English/Chinese toggle, and audio initialization/shutdown exercised.
- **PSP 1000:** earlier M6–M9 feature smoke passed; the latest V0.1 audio,
  minimap, and final package still need a final hardware listening/visual pass.

### Known limitations

- No naval units; coast/ocean remain impassable.
- Trade is gold-only; open borders share vision but do not gate movement.
- No culture/border movement gating, difficulty selector, or resource trading.
- Technology screen is a flat scroll, not an era-grouped tree.
- One active sound effect at a time (plus ambient music).
- Development build: balance and UI may change after hardware feedback.

### Release files

```text
release/v0.1/EBOOT.PBP
release/v0.1/civpsp-v0.1.zip
release/v0.1/SHA256SUMS
```

### Checksums

Filled in when the V0.1 package is built:

```text
EBOOT.PBP       c57fa5634ee45b516f10959242f92f85508a02f4bba75ef7f4ce898a93f831cc
civpsp-v0.1.zip eaecad54483c3e7e62b4833afdf0b2f54aaf794aefcf84860a61499dd0ad116a
```

### Development history

The internal milestones were:

- M0: project skeleton, GU, input, font, menu
- M1: 64×48 map, terrain/resources, camera, fog
- M2: units, movement, turn loop
- M3: city management
- M4 / M4.1: technologies, workers, improvements
- M5: AI, barbarians, combat, capture
- M6: diplomacy, trade, alliances
- M7: modes, save/load, results, analog calibration
- M8: 30-tech expansion, Help, science projects, standard score ending
- M9.x: Chinese localization, civilization colors, minimap, audio, manual,
  XMB art, menu transitions, data balance

See [docs/README_MILESTONES.md](docs/README_MILESTONES.md) for the archived
milestone-oriented README.
