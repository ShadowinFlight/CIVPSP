[中文](UPDATE.md) | [English](UPDATE.en.md)

# UPDATE

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
- 12-page in-game manual (`游戏手册`) accessible from the main menu and Pause.
- Explored-range minimap in the bottom-right corner, collapsed behind a
  `小地图 L+R` shortcut button; `L + R` toggles it.
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

See the control table in [README.md](README.md#controls). Key additions:

- `L + R`: toggle minimap
- `Select`: end turn
- `Start`: diplomacy
- `Options → 音乐`: toggle background music
- `Options → 语言`: switch Chinese/English

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
