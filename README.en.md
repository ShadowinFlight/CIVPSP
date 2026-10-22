# CIVITAS PSP

**V0.2.2 Developer Release** · A turn-based 4X strategy game for the PSP 1000.

[简体中文](README.md) | [**English**](README.en.md)

CIVITAS is built with PSPSDK and the PSP GU 2D API. It runs at 480×272 on a
fixed 64×48 map and is designed for short-session 4X play on the PSP.

![CIVITAS main menu](docs/screenshots/v0.1_main.png)

## Contents

- [Features](#features)
- [V0.2.2 update](#v022-update)
- [Controls](#controls)
- [Building and running](#building-and-running)
- [Release package](#release-package)
- [Project structure](#project-structure)
- [Documentation](#documentation)
- [Tests](#tests)
- [Known limitations](#known-limitations)

## Features

- **Complete 4X loop**: explore a 64×48 map, found cities, expand borders,
  research 30 technologies, build armies, and win by conquest, science,
  diplomacy, or score.
- **Four civilizations and barbarians**: Aurelia (player), Borea, Cyrene,
  Draven, and hostile barbarians. Units and cities use civilization-colored
  sprite variants with owner bars.
- **City management**: 5×5 work area, citizen assignment and locking, food
  growth/starvation, production queues, gold rush, nine buildings, border
  growth, and tile purchase.
- **City siege combat**: cities have 100 HP and 100 defense. A garrison must be
  defeated first; attacks drain defense before HP. Defense is repaired only by a
  worker on or next to the city. Walls raise the cap to 150 and reduce each repair.
  The city overview shows only HP and the defense pool.
- **Military system**: settlers, warriors, scouts, swordsmen, horsemen, and
  workers; terrain and city defense, counterattacks, healing, and city capture.
- **Diplomacy**: relations, gifts, trade, non-aggression pacts, open borders,
  alliances, denouncement, AI proposals, and diplomatic victory.
- **Turn briefing**: AI/end-turn battles, city assaults, war declarations,
  peace, and AI proposals are collected in a centered briefing at the start of
  the new turn, with three events per page and paging controls.
- **Two modes**: Classic with selectable 100/200/300-turn limits and a score
  ending; Endless with no turn limit and only conquest/science/diplomatic
  victory or defeat.
- **Three difficulties**: Easy, Normal, and Hard. Difficulty affects starting
  units, AI treasuries, war timing, and barbarian pressure.
- **Chinese and English UI**: Chinese is the default; English can be selected
  in Options. The 12-page in-game manual is available from the main menu and
  Pause.
- **Science screen**: technologies are grouped by four eras, with per-era
  completion counts, costs, prerequisites, and unlocks. `L/R` jumps between
  eras.
- **Explored-range minimap**: bottom-right panel, collapsed by default and
  toggled with `L`; shows explored terrain, visible terrain, cities, units,
  cursor, and camera view.
- **Generated audio**: an 8-second ambient loop and eight sound effects, with a
  music toggle in Options.
- **PSP presentation**: custom XMB icon/background, menu fade/slide
  transitions, selection glow, and civilization color variants.

## V0.2.2 update

V0.2.2 fixes fog-of-war information leaks while keeping the V0.2.1 turn
briefing and city-overview cleanup.

### Fog-of-war fix

- When the cursor is over an enemy unit on a currently non-visible tile, the
  bottom status bar and tile info panel no longer show its type, owner, HP, or
  movement. Player units remain listed.
- Unexplored tiles no longer leak city information through the status bar or
  info panel; cities that have already been explored remain known.

### Turn briefing (V0.2.1)

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

### City combat and modes (V0.2 base)

- Cities are no longer captured by walking into an empty center: defeat the
  garrison, then assault the city over multiple attacks.
- Walls provide a 150 defense cap and slower worker repair (+15 instead of
  +25).
- Cities recover 10 HP at the end of a turn in which they were not hit; defense
  never regenerates on its own.
- The city overview shows only `HP` and the damage-absorbing city defense pool;
  the old `Defense` strength row is gone.
- New Game offers Easy/Normal/Hard; Classic offers 100/200/300 turns; Timed
  mode is removed.
- Diplomacy adds Denounce: costs 8 relation and is limited to once per rival
  per turn.
- `L` toggles the minimap, `R` selects the next player unit with movement, and
  the analog camera moves the selection tile with the view.
- The Science screen groups technologies by era, shows era progress,
  prerequisites, and costs.

### Saves and compatibility (V0.2 base)

- Saves are version 3. Version 2 saves are migrated: old Standard saves become
  Classic 200-turn games, old Timed saves become Endless, and city combat fields
  are initialized to full values.
- The briefing queue is not saved and does not change the save format.
- The `v0.2` tag still points at `c733769`, and `v0.2.1` remains unchanged;
  V0.2.2 is a new patch package in `release/v0.2.2/`.

## Controls

| Input | Action |
|---|---|
| D-Pad | Move map cursor |
| Analog stick | Pan camera; the selection tile follows the view center |
| Cross | Select / confirm / act |
| Circle | Back / pause |
| Triangle | Unit action menu |
| Square | Tile information |
| L | Toggle minimap |
| R | Select the next player unit with movement |
| Start | Diplomacy |
| Select | End turn |

City screen: `L/R` switches tabs, `Cross` queues production or selects a
citizen action, `Triangle` buys/locks, `Square` auto-assigns citizens. On the
Science tab, `L/R` jumps to the previous/next era. A worker on or next to a friendly
city gets a Repair City Defense action in the `Triangle` menu.

## Building and running

### Requirements

- [pspdev](https://github.com/pspdev/pspdev) toolchain on `PATH`
- GNU Make
- Python 3 with Pillow for regenerating assets
- Noto Sans CJK / DejaVu fonts if regenerating the Chinese font and XMB art

### Build

```bash
export PATH=$PATH:/home/zhang/pspdev/bin
make clean
make -j4
```

This produces `EBOOT.PBP` and `civpsp.elf` in the project root.

To regenerate strings, fonts, audio, XMB art, and the terrain atlas:

```bash
make assets
```

### Test

```bash
make test
```

The V0.2.2 gate runs **14 host suites** plus Chinese/English and generated
audio checks. V0.2.1 adds `test_turn_report` for queue contents, capacity, and
clearing; the V0.2 suites continue to cover city assault, garrison shielding,
walls/repair/HP recovery, denouncement, difficulty starts, save v3 round-trips,
and v2 migration. The V0.2.2 fog fix still needs a hardware or PPSSPP visual
check to confirm hidden enemy unit data stays out of the bottom status bar and
tile info panel.

### Run in PPSSPP

```bash
PPSSPP --windowed --graphics=software civpsp.elf
```

You can also open `EBOOT.PBP` directly in PPSSPP. For a PSP, copy the EBOOT to:

```text
ms0:/PSP/GAME/CIVPSP/EBOOT.PBP
```

`ICON0.PNG` and `PIC1.PNG` are embedded in the EBOOT by the build.

## Release package

The V0.2.2 development package is in:

```text
release/v0.2.2/
├── EBOOT.PBP
├── civpsp-v0.2.2.zip
└── SHA256SUMS
```

`civpsp-v0.2.2.zip` contains the EBOOT ready to copy to the PSP. V0.2.1 remains
in `release/v0.2.1/`; V0.2 remains in `release/v0.2/` at the frozen tag `v0.2`
(`c733769`); V0.1 remains in `release/v0.1/`.

## Project structure

```text
.
├── src/                 PSP game code and generated assets
│   ├── app.c            state machine, menus, save/load, transitions
│   ├── map.c            map generation, fog, connectivity
│   ├── mapview.c        map rendering, cursor, minimap, actions
│   ├── city.c           city yields, growth, production, borders, defense
│   ├── cityview.c       city screen and era-grouped science screen
│   ├── tech.c           30-tech tree and effects
│   ├── world.c          civilizations, turns, economy, scoring, difficulty
│   ├── combat.c         damage, defense, city assault, capture
│   ├── diplomacy.c      relations, treaties, trade, denouncement
│   ├── turn_report.c    AI/end-turn event briefing queue
│   ├── save.c           version-3 save format and v2 migration
│   ├── audio.c          audio thread and mixer
│   ├── text.c           English/Chinese text rendering
│   └── gen/             generated strings, fonts, audio PCM, atlas
├── assets/
│   ├── strings.csv      English/Chinese UI strings
│   └── src/             source PNG/WAV assets
├── tools/               Python asset/string/audio generators
├── tests/               host logic tests
├── docs/                status, handoff, decisions, data sheet, screenshots
├── release/             local release packages (binaries ignored by Git)
├── Makefile
├── ICON0.PNG / PIC1.PNG
├── UPDATE.md
├── UPDATE.en.md
├── PROJECT_PLAN.md
└── README.md / README.en.md
```

## Documentation

- [UPDATE.en.md](UPDATE.en.md) — V0.2.2 / V0.2.1 / V0.2 release notes and changelog
- [UPDATE.md](UPDATE.md) — Chinese release notes
- [docs/GAME_DATA.md](docs/GAME_DATA.md) — complete unit/building/tech/economy/siege data
- [docs/HANDOFF.md](docs/HANDOFF.md) — development handoff and hardware checklist
- [docs/STATUS.md](docs/STATUS.md) — detailed milestone and validation history
- [docs/DECISIONS.md](docs/DECISIONS.md) — design decisions and simplifications
- [PROJECT_PLAN.md](PROJECT_PLAN.md) — original scope and milestone plan
- [docs/README_MILESTONES.md](docs/README_MILESTONES.md) — archived milestone README

## Tests

- 14 host test suites: map, units, city, tech, improvements, combat,
  turn-report queue, AI, diplomacy, analog, save, M7 scoring, M8 tech/science,
  M9 i18n
- `tools/check_i18n.py`: translation coverage, placeholders, glyphs, width
- `tools/check_audio.py`: WAV format and non-silent peaks

PPSSPP has exercised the main menu, manual, map, city tabs, diplomacy, saves,
minimap, score result, and both languages. The V0.2 EBOOT also passed a
functional smoke test covering Hard/Classic 300, `L`/`R`, founding a city, the
city and Science screens, denouncement, city assault with and without a
garrison, and v2 Standard save migration. The V0.2.1 build also validated the
turn briefing: the log showed `ai combat ... dmg=30 counter=30`,
`report: open turn=2 events=1`, and `report: dismissed turn=2` after Cross. The
V0.2.2 rebuild reports `CIVITAS PSP V0.2.2` in its boot log; the fog fix was
triggered by PSP 1000 feedback and should be rechecked on hardware. X11 captures
are black under WSLg, so this round validates logs and functional paths rather
than final layout; the remaining risk areas (Walls, difficulty, Endless, era
grouping) are covered by host tests. The latest package should still be
rechecked on a PSP 1000.

## Known limitations

- V0.2.2 is a development snapshot; final PSP 1000 validation of the latest
  package is still in progress. PPSSPP is the primary validated target.
- No naval units; coast/ocean are barriers.
- Trade is gold-only; open borders share vision but do not gate movement.
- Borders expand by influence or purchase; there is no culture/border movement
  gating yet.
- No resource trading, overlapping sound effects, or a settings-menu minimap
  toggle.
- One active sound effect at a time (plus ambient music).
- Save format is version 3; v2 and earlier M7/M8/M9 development saves load
  through the migration rules.

## Version

**V0.2.2** is the patch release after V0.2.1, fixing fog-of-war leaks for
enemy units and unexplored city tiles while keeping the V0.2.1 turn briefing,
city-overview cleanup, and V0.2's city sieges, difficulty, Classic/Endless
modes, denouncement, era-grouped Science screen, and save v3. The `v0.2` and
`v0.2.1` tags and their packages remain unchanged; V0.2.2 has its own package
and tag. V0.1 remains the first GitHub development release; milestone tags stay
in the repository history.
