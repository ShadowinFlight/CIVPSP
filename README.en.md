# CIVITAS PSP

**V0.3.1 Developer Release** · A turn-based 4X strategy game for the PSP 1000.

[简体中文](README.md) | [**English**](README.en.md)

CIVITAS is built with PSPSDK and the PSP GU 2D API. It runs at 480×272 on a
fixed 64×48 map and is designed for short-session 4X play on the PSP.

!\[CIVITAS main menu](PIC1.PNG)

## Contents

* [Features](#features)
* [V0.3.1 update](#v031-update)
* [Controls](#controls)
* [Building and running](#building-and-running)
* [Release package](#release-package)
* [Project structure](#project-structure)
* [Documentation](#documentation)
* [Tests](#tests)
* [Known limitations](#known-limitations)

## Features

* **Complete 4X loop**: explore a 64×48 map, found cities, expand borders,
research 30 technologies, build armies, and win by conquest, science,
diplomacy, or score.
* **Four civilizations and barbarians**: Aurelia (player), Borea, Cyrene,
Draven, and hostile barbarians. Units and cities use civilization-colored
sprite variants with owner bars.
* **City management**: 5×5 work area, citizen assignment and locking, food
growth/starvation, production queues, gold rush, nine buildings, border
growth, and tile purchase.
* **City siege combat**: cities have 100 HP and 100 defense. A garrison must be
defeated first; attacks drain defense before HP. Defense is repaired only by a
worker on or next to the city. Walls raise the cap to 150 and reduce each repair.
The city overview shows only HP and the defense pool.
* **Military system**: settlers, warriors, scouts, swordsmen, horsemen,
workers, and medics; terrain and city defense, counterattacks, healing, and
city capture.
* **Medic**: a 12-cost non-combat support unit with 4 movement / 2 sight / 0
strength. Friendly units resting within one tile (including diagonals) heal
an extra 10 HP; multiple medics do not stack, and medics cannot attack.
* **Forest chop**: a worker on an owned forest can chop it when the nearest
city has a production queue. The forest becomes grassland (resources and
improvements are kept), the worker spends all movement, and the city gains
+20 production.
* **Diplomacy**: relations, gifts, trade, non-aggression pacts, open borders,
alliances, denouncement, AI proposals, and diplomatic victory.
* **Turn briefing**: AI/end-turn battles, city assaults, war declarations,
peace, and AI proposals are collected in a centered briefing at the start of
the new turn, with three events per page and paging controls.
* **Two modes**: Classic with selectable 100/200/300-turn limits and a score
ending; Endless with no turn limit and only conquest/science/diplomatic
victory or defeat.
* **Three difficulties**: Easy, Normal, and Hard. Difficulty affects starting
units, AI treasuries, war timing, and barbarian pressure.
* **Chinese and English UI**: Chinese is the default; English can be selected
in Options. The 12-page in-game manual is available from the main menu and
Pause.
* **Science screen**: technologies are grouped by four eras, with per-era
completion counts, costs, prerequisites, and unlocks. `L/R` jumps between
eras.
* **Explored-range minimap**: bottom-right panel, collapsed by default and
toggled with `L`; shows explored terrain, visible terrain, cities, units,
cursor, and camera view.
* **Scout auto-explore**: enable it from a scout's action menu to move toward
the nearest unexplored tile every turn until movement runs out; disable it at
any time.
* **Smooth terrain transitions**: explored tiles blend terrain colors with
explored left/right/up/down neighbours using alpha gradients, softening hard
tile borders without revealing terrain through fog. Dark tile outlines are
removed to avoid a visible grid.
* **Generated audio**: eight era/peace-war background loops, each with eight
one-second diatonic chords and seventh-chord color, selected from the player's
era and war state, plus eight sound effects and a music toggle in Options.
* **PSP presentation**: custom XMB icon/background, menu fade/slide
transitions, selection glow, gold-stamped menu headings, and civilization
color variants.

## V0.3.1 update

V0.3.1 rebuilds the background-music harmony, adds a non-combat medic and
worker forest chopping, and removes hard terrain tile outlines while keeping
V0.3's era music, smooth transitions, auto-explore, gold headings, and every
existing system.

### Harmony and music

* Each of the four eras still has a peace and a war loop, but every 8-second
loop now uses eight diatonic chords at about one second each with
major-seventh, minor-seventh, and dominant-seventh color.
* Ancient Am-F-C-G-Am-Dm-Fmaj7-Em; Classical Cmaj7-Am-Fmaj7-G7-C-F-Dm7-G7;
Medieval Gm-F-Eb-F-Gm-Cm-Eb-Dm7; Renaissance D-A7-Bm7-F#m-Gmaj7-D-Em7-A7.
* War variants keep the two-semitone downward transpose plus darker pads and
percussion. The loop-seam crossfade and roughly 0.4-second track crossfade
are unchanged.

### Medic

* Adds a `MEDIC` production item costing 12 with no technology prerequisite:
4 movement / 2 sight / 0 strength, and no attack or city founding.
* A friendly unit resting within one tile (including diagonals) heals an extra
10 HP. Multiple medics do not stack.
* Medics can move and explore, but cannot attack, found cities, or build
improvements.

### Forest chop

* A worker on a forest owned by its civilization can select `CHOP FOREST` in
the action menu.
* The chop spends the worker's remaining movement, turns the forest into
grassland, keeps the tile's resource and existing improvement, and gives +20
production to the nearest owned city with a non-empty production queue.
* With no queue, the chop does not consume the forest or movement; the action
stays visible and explains that a queue is required. City centers, unowned
forest, and non-forest tiles do not offer the chop action.

### Terrain seams

* Terrain tiles no longer draw a dark rectangular outline. The four-direction
alpha transitions remain, so terrain changes stay soft without a visible
grid.
* The atlas grows to 256×256 to hold a sixth unit/city icon column; city sprite
indices are unchanged.

### Compatibility and validation

* Saves remain **version 3**. `UNIT\\\\\\\_MEDIC`, `PROD\\\\\\\_MEDIC`, and related enum
values are appended, so V0.3 and older saves load directly.
* The 14 host suites, i18n, and audio checks pass. `test\\\\\\\_units` covers medic
stats, attack rejection, aura healing, non-stacking, and friendly-territory
healing; `test\\\\\\\_improvements` covers chop success, rejection cases, and
production credit; `test\\\\\\\_combat` covers the civilian attack error.
* The PPSSPP boot log reports `CIVITAS PSP V0.3.1`. WSLg captures are still
black, so final visual and audio acceptance remains pending on hardware.
* The `v0.3` tag and `release/v0.3/` stay frozen; V0.3.1 uses a new tag and
`release/v0.3.1/`.

## V0.3 update (history)

V0.3 adds era/peace-war background music, smooth terrain transitions, scout
auto-explore, and gold-stamped menu headings while keeping the V0.2.2 fog fix
and every existing system.

### Era and war music

* Adds eight roughly 8-second loops: peace and war variants for the Ancient,
Classical, Medieval, and Renaissance eras.
* Menus, the manual, Options, and New Game use Ancient/Peace. In play, the loop
follows the player's highest era and whether a living major rival is at war.
* War variants are transposed down two semitones and use darker pads and
percussion. Track changes crossfade over roughly 0.4 seconds, and each
generated loop has a crossfaded seam.
* Music still toggles from Options; the eight sound effects are unchanged.

### Smooth terrain

* A transition atlas blends each tile with its left, right, up, and down
neighbour when that neighbour has been explored.
* Transitions only face explored tiles, so gradients cannot leak terrain
through fog of war.
* Transitions use linear filtering, while base terrain stays pixel-crisp; the
detail layer, units, cities, and fog keep their existing draw order.

### Scout auto-explore

* A scout's action menu gains an `AUTO-EXPLORE` toggle. Enabling it spends the
current movement immediately and repeats at end of turn after movement is
reset.
* BFS picks the nearest reachable unexplored tile while avoiding impassable
terrain, cities, and all units; the scout stays put if no target is reachable.
* The HUD appends `AUTO` to the unit label. The toggle lives in bit `0x80` of
`Unit.acted`, so no save field is added.

### Menu headings

* Main menu, manual, Options, New Game, Pause, Save, and Load headings now use
layered deep shadow, dark bevel, bright gold body, and top-left highlight
passes.
* Headings use linear font filtering and padded Chinese/Japanese/Korean atlas
cells so large text stays smooth without sampling adjacent glyphs.

### Compatibility and validation

* Saves remain **version 3** and load in V0.2.2 / V0.2.1 / V0.2; older readers
ignore the auto-explore bit.
* The 14 host suites still pass. `test\\\\\\\_units` adds auto-explore, reset
preservation, end-turn integration, era detection, and war-state coverage;
i18n and generated-audio checks pass.
* The `v0.2`, `v0.2.1`, and `v0.2.2` tags and packages stay frozen; V0.3 uses a
new tag and package.

## V0.2.2 update (history)

V0.2.2 fixes fog-of-war information leaks while keeping the V0.2.1 turn
briefing and city-overview cleanup.

### Fog-of-war fix

* When the cursor is over an enemy unit on a currently non-visible tile, the
bottom status bar and tile info panel no longer show its type, owner, HP, or
movement. Player units remain listed.
* Unexplored tiles no longer leak city information through the status bar or
info panel; cities that have already been explored remain known.

### Turn briefing (V0.2.1)

* AI/end-turn unit battles, city assaults, war declarations, peace, and AI
proposals are collected in a transient queue and shown in a centered
briefing at the start of the new turn.
* Each page shows three events; `L/R` pages, and `Cross/Circle` closes the
briefing.
* Player-initiated attacks keep the existing bottom toast so normal play is not
interrupted.
* Each battle records the tile, both civilizations and unit types,
damage/counter damage, unit deaths, city defense/HP damage, and capture
result.
* Battles involving the player are always recorded; AI-vs-AI battles are
recorded only on player-visible tiles, so the briefing does not leak fog of
war.
* The queue holds at most 32 events and is not saved. New game, load,
dismissal, and the start of `World\\\\\\\_EndTurn` clear it.

### City combat and modes (V0.2 base)

* Cities are no longer captured by walking into an empty center: defeat the
garrison, then assault the city over multiple attacks.
* Walls provide a 150 defense cap and slower worker repair (+15 instead of
+25).
* Cities recover 10 HP at the end of a turn in which they were not hit; defense
never regenerates on its own.
* The city overview shows only `HP` and the damage-absorbing city defense pool;
the old `Defense` strength row is gone.
* New Game offers Easy/Normal/Hard; Classic offers 100/200/300 turns; Timed
mode is removed.
* Diplomacy adds Denounce: costs 8 relation and is limited to once per rival
per turn.
* `L` toggles the minimap, `R` selects the next player unit with movement, and
the analog camera moves the selection tile with the view.
* The Science screen groups technologies by era, shows era progress,
prerequisites, and costs.

### Saves and compatibility (V0.2 base)

* Saves are version 3. Version 2 saves are migrated: old Standard saves become
Classic 200-turn games, old Timed saves become Endless, and city combat fields
are initialized to full values.
* The briefing queue is not saved and does not change the save format.
* The `v0.2` tag still points at `c733769`, and `v0.2.1` remains unchanged;
V0.2.2 is a new patch package in `release/v0.2.2/`.

## Controls

|Input|Action|
|-|-|
|D-Pad|Move map cursor|
|Analog stick|Pan camera; the selection tile follows the view center|
|Cross|Select / confirm / act|
|Circle|Back / pause|
|Triangle|Unit action menu|
|Square|Tile information|
|L|Toggle minimap|
|R|Select the next player unit with movement|
|Start|Diplomacy|
|Select|End turn|

City screen: `L/R` switches tabs, `Cross` queues production or selects a
citizen action, `Triangle` buys/locks, `Square` auto-assigns citizens. On the
Science tab, `L/R` jumps to the previous/next era. A worker on or next to a
friendly city gets a Repair City Defense action in the `Triangle` menu, and a
worker on an owned forest with a city queue gets Chop Forest. A scout's
`Triangle` menu gains the auto-explore toggle on any turn. Medics cannot attack,
but they speed up resting friendly units within one tile.

## Building and running

### Requirements

* [pspdev](https://github.com/pspdev/pspdev) toolchain on `PATH`
* GNU Make
* Python 3 with Pillow for regenerating assets
* Noto Sans CJK / DejaVu fonts if regenerating the Chinese font and XMB art

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

The V0.3.1 gate runs **14 host suites** plus Chinese/English and
generated-audio checks. `test\\\\\\\_units` adds medic stats, attack rejection, aura
healing, non-stacking, and friendly-territory healing while keeping
auto-explore, acted-bit preservation across reset, end-turn integration, era
detection, and war-state detection. `test\\\\\\\_improvements` adds chop success,
no-queue/unowned/city-center/non-forest rejection, and production credit;
`test\\\\\\\_combat` covers the medic civilian attack error. `test\\\\\\\_save` checks that
the auto-explore high bit survives a save round-trip. The existing suites
continue to cover city assault, garrison shielding, walls/repair/HP recovery,
denouncement, difficulty starts, save v3 round-trips, and v2 migration.
`check\\\\\\\_audio.py` validates all eight music loops and eight sound effects,
rejecting stale `ambient.wav`.

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

The V0.3.1 development package is in:

```text
release/v0.3.1/
├── EBOOT.PBP
├── civpsp-v0.3.1.zip
└── SHA256SUMS
```

`civpsp-v0.3.1.zip` contains the EBOOT ready to copy to the PSP. V0.3 remains
frozen in `release/v0.3/` at tag `v0.3` (`9fd3b46`); V0.2.2 remains in
`release/v0.2.2/` at tag `v0.2.2` (`d5da6d2`); V0.2.1 remains in
`release/v0.2.1/`; V0.2 remains in `release/v0.2/` at the frozen tag `v0.2`
(`c733769`); V0.1 remains in `release/v0.1/`. Old tags and packages are not
moved.

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
│   ├── turn\\\\\\\_report.c    AI/end-turn event briefing queue
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
├── PROJECT\\\\\\\_PLAN.md
└── README.md / README.en.md
```

## Documentation

* [UPDATE.en.md](UPDATE.en.md) — V0.3.1 / V0.3 / V0.2.2 / V0.2.1 / V0.2 release notes and changelog
* [UPDATE.md](UPDATE.md) — Chinese release notes
* [docs/GAME\_DATA.md](docs/GAME_DATA.md) — complete unit/building/tech/economy/siege data
* [docs/HANDOFF.md](docs/HANDOFF.md) — development handoff and hardware checklist
* [docs/STATUS.md](docs/STATUS.md) — detailed milestone and validation history
* [docs/DECISIONS.md](docs/DECISIONS.md) — design decisions and simplifications
* [PROJECT\_PLAN.md](PROJECT_PLAN.md) — original scope and milestone plan
* [docs/README\_MILESTONES.md](docs/README_MILESTONES.md) — archived milestone README

## Tests

* 14 host test suites: map, units, city, tech, improvements, combat,
turn-report queue, AI, diplomacy, analog, save, M7 scoring, M8 tech/science,
M9 i18n
* `tools/check\\\\\\\_i18n.py`: translation coverage, placeholders, glyphs, width
* `tools/check\\\\\\\_audio.py`: WAV format, non-silent peaks, all eight music loops,
and stale `ambient.wav` removal

PPSSPP has exercised the main menu, manual, map, city tabs, diplomacy, saves,
minimap, score result, and both languages. The V0.2 EBOOT also passed a
functional smoke test covering Hard/Classic 300, `L`/`R`, founding a city, the
city and Science screens, denouncement, city assault with and without a
garrison, and v2 Standard save migration. The V0.2.1 build validated the turn
briefing: the log showed `ai combat ... dmg=30 counter=30`,
`report: open turn=2 events=1`, and `report: dismissed turn=2` after Cross. The
V0.2.2 build reports `CIVITAS PSP V0.2.2` in its boot log and retains the fog
fix path. The V0.3 PPSSPP boot log reports `CIVITAS PSP V0.3` and
`audio: channel=7 44100Hz stereo, 8 era/mood music tracks`. The V0.3.1 boot log
reports `CIVITAS PSP V0.3.1`; medics, forest chopping, the eight-chord loops,
and outline removal are covered by host tests and generated-asset checks. X11
captures are still black under WSLg, so final gold-title, terrain-seam, and
music acceptance must be checked on a PSP 1000 or a PPSSPP setup that can
capture frames.

## Known limitations

* V0.3.1 is a development snapshot; final PSP 1000 validation of the latest
package is still in progress. PPSSPP is the primary validated target.
* The AI does not produce or use medics; medics and forest chopping currently
exist for the player only.
* No naval units; coast/ocean are barriers.
* Trade is gold-only; open borders share vision but do not gate movement.
* Borders expand by influence or purchase; there is no culture/border movement
gating yet.
* No resource trading, overlapping sound effects, or a settings-menu minimap
toggle.
* One active sound effect at a time (plus background music).
* Background music uses roughly 8-second generated loops with eight one-second
chords per loop and crossfades when the era or war state changes; it is not a
full soundtrack.
* Save format is version 3; v2 and earlier M7/M8/M9 development saves load
through the migration rules.

## Version

**V0.3.1** is the feature update after V0.3, adding eight-chord era music, a
non-combat medic, forest chopping, and removal of terrain tile outlines while
keeping V0.3's era music, smooth terrain, scout auto-explore, gold headings,
V0.2.2's fog fix, V0.2.1's turn briefing, the city-overview cleanup, and V0.2's
city sieges, difficulty, Classic/Endless modes, denouncement, era-grouped
Science screen, and save v3. The `v0.2`, `v0.2.1`, `v0.2.2`, and `v0.3` tags and
packages remain frozen; V0.3.1 has its own package and tag. V0.1 remains the
first GitHub development release; milestone tags stay in the repository
history.

