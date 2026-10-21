> **ARCHIVED:** This is the milestone-oriented development README. See the project root `README.md` for the V0.1 release overview and `UPDATE.md` for release notes.

# CIVITAS PSP

A simplified, turn-based 4X game for the PSP 1000, built with PSPSDK and the
PSP GU 2D API.

Current stage: **M9.12 (Chinese localization, explored-range minimap,
Python-generated audio, civilization-colored sprites, and release XMB art)
implementation complete and PPSSPP validated. The combined PSP 1000 hardware
pass and `m6`/`m7`/`m8`/`m9` tags are pending; the M6 analog-drift fix still
needs a hardware retest. M5 remains tagged `m5`.**

![CIVITAS M9 in Chinese](docs/screenshots/m9_main.png)

## M9.12 status (implementation complete, PPSSPP validated)

- **Chinese by default**: all 485 UI strings have Chinese translations; the
  main menu, Help, map HUD, city tabs, diplomacy, saves, pause, and result
  screens are localized.
- **Language toggle**: Options → 语言 / Language switches between 中文 and
  ENGLISH at runtime; English keeps the original 8×14 font and layout.
- **Chinese font**: generated 512×512 RGBA atlas (548 glyphs), 16×16 cells,
  UTF-8 decoding, variable-width ASCII/CJK, and a 14px Noto Sans CJK SC face.
- **Layout**: Chinese rows are spaced for 16px glyphs; Science is a 6-row
  scroll, Production 4 rows, diplomacy rows 56px, with adjusted titles and
  info panels.
- **Civilization colors**: the terrain atlas now has five unit/city palette
  rows (blue player, red Borea, gold Cyrene, purple Draven, grey barbarians).
  Team color is painted onto each icon's tent, shield, cloak, city flag/gate
  banner, or worker shirt, and a 20×3 owner bar sits under every unit/city.
- **Map connectivity**: generation flood-fills passable terrain and carves a
  mountain pass to isolated pockets; forest, hills, desert, and tundra remain
  passable. `tests/test_map.c` checks 100 seeds and full passable-tile
  connectivity.
- **Minimap**: a 114×86 dark/gold panel in the map's bottom-right corner shows
  explored range, terrain, cities, visible units, cursor, and camera viewport.
  It is built as a 64×64 GU_PSM_5551 texture (8KB) and drawn as one quad. It is
  collapsed by default; a small `小地图 L+R` button opens it and L+R toggles it
  without taking a face-button slot.
- **Audio**: `tools/gen_audio.py` synthesizes an 8-second ambient loop and
  eight sound effects; `src/audio.c` mixes them on a dedicated thread and
  outputs through a standard 44100Hz stereo `sceAudioChReserve` channel, with
  an explicit data-cache writeback before each hardware output. Options → 音乐
  toggles music; UI/select/turn/build/found/combat/research SFX are wired in.
  The first generator pass had a real normalization bug (`int(0.x)` produced
  all-zero PCM); it now scales to 32767. Generated clips peak at 80% of full
  scale, music uses 100% gain (about 80% output peak) and ducks to 20% while
  SFX play, and `tools/check_audio.py` guards against silent WAV/PCM assets.
- **Release art**: `ICON0.PNG`/`PIC1.PNG` were rebuilt with a formal
  gradient/compass/city-skyline treatment and `M9 EDITION`/`M9 BUILD` labels.
- **Menu transitions**: screen changes use a 20-frame fade, menu rows slide in
  with a short stagger, and the selected row gets a brief gold glow. This
  applies to the main menu, Options, New Game, Pause, Save/Load, Help, and the
  result screen; map/city/diplomacy also get the fade.
- **In-game manual**: the former four-page Help is now a 12-page manual
  (`游戏手册`) covering goals, controls, turns/fog, cities, citizens/borders,
  science, units/combat, diplomacy, victory/score, buildings, unit/terrain
  stats, and settings/tips. It opens from the main menu and Pause; LEFT/RIGHT
  flips pages.
- **Tests**: `tests/test_i18n.c` plus `tools/check_i18n.py` validate language
  switching, all translations, printf placeholders, glyph coverage, and
  widths. `make test` runs 13 host suites plus the i18n and audio checks.
- Save format is unchanged; M7/M8 saves load, and gameplay/UI state is
  independent of the selected language.
- **Full data sheet**: [docs/GAME_DATA.md](docs/GAME_DATA.md) lists units, buildings, terrain/resources, improvements, techs, economy formulas, combat, diplomacy, and scoring. M9.11 applied the user-tuned economy/building values from that sheet; M9.12 shows all resulting building/improvement effects in the city UI.

### M9 validation (PPSSPP)

- Final `EBOOT.PBP` booted as `CIVITAS PSP M9`.
- Inspected Chinese main menu, Options, 12-page manual, Standard map HUD, tile
  info, city overview/production/citizens/buildings/science, pause, save/load,
  diplomacy/action menu, and score result at 480×272/2×.
- Toggled to English and back from Options; both fonts render without missing
  glyphs or row overlap.
- Loaded an M8-format slot and ended a turn from the city screen; score result
  showed `分数胜利`, `分数 41 回合 200`, and autosaved.
- Verified player (blue), Borea (red), Cyrene (gold), and Draven (purple)
  units/cities on revealed test saves; the owner palette row is selected from
  `unit->owner`/`city->owner`.
- Minimap rendered explored terrain, city/unit markers, cursor, and camera
  viewport correctly; L+R opened/closed it from the bottom-right shortcut.
- Menu fade, staggered slide-in, and selection glow were exercised in PPSSPP.
- The 12-page manual was inspected at pages 1 and 12; page count and line fit
  are correct.
- The revised audio path reserves a standard 44100Hz stereo channel
  (`audio: channel=7 44100Hz stereo`), logs no output errors in PPSSPP, and
  shuts down cleanly (`audio: shutdown rc=0`); music toggle changed `on`/`off`
  in Options. WAV peaks are now 26213 (80% of full scale), and
  `tools/check_audio.py` passes.
- XMB `ICON0.PNG` and `PIC1.PNG` were inspected at 144×80 and 480×272.

![Civilization-colored units and cities](docs/screenshots/m9_civ_colors.png)

![Explored-range minimap](docs/screenshots/m9_minimap.png)

- **PSP 1000 combined test and tags pending.** Use the checklist in
  `docs/HANDOFF.md`.

## M8 status (historical; implementation complete, PPSSPP validated)

- **Help / manual**: the in-game manual is available from the main menu and
  Pause. The manual spans 12 pages covering goals, controls, turns/fog,
  cities, citizens/borders, science, combat, diplomacy, victory/score,
  buildings, unit/terrain stats, and settings/tips. LEFT/RIGHT changes pages;
  Circle returns to the caller.
- **30 techs**: 8 Ancient, 8 Classical, 7 Medieval, and 7 Renaissance. The
  original 18 bit positions are unchanged, so version-2 M7 saves still load.
- **Tech effects**: farms +1 food, libraries/universities +1 science, markets
  +1 gold, workshops/barracks +1 production, walls +1 defense, per-city
  gold/science, and +10% research. The Science tab shows the selected effect.
- **Science victory**: complete Astronomy, Scientific Method, and Navigation;
  the Science tab shows `PROJECTS n/3`.
- **AI research**: deterministic, derived from the turn counter; AI fields
  swordsmen/horsemen and builds tech-gated buildings. AI tech count contributes
  to score.
- **Standard mode**: ends after turn 200 and ranks by score. The map shows
  `TURN n/200`; the result screen clamps the displayed turn to 200. Ending a
  turn from the city screen also autosaves and applies the limit.
- **Saves**: `SAVE_VERSION` stays 2 with an unchanged `World` layout; M7 saves
  were loaded successfully in PPSSPP.
- `tests/test_m8.c` covers era distribution, prerequisites, tech effects,
  research stacking, AI research/advanced units, science goals, and standard
  score victory/defeat. `tests/test_ai.c` adds a 320-turn stability run.
  `make test` runs 12 suites.

### M8 validation (PPSSPP)

- Main menu Help navigated through pages 1–4; Pause Help returned to Pause.
- 30-tech Science list scrolled to `MILITARY SCIENCE`; the selection bar and
  `BARRACKS +1 PRODUCTION` effect line aligned correctly.
- Standard `seed=6FB11BD0 start=32,24` showed `TURN 1/200`.
- A version-2 save at turn 200 was loaded and ended from the city screen:
  `turn limit: turn=201 score=41 rival_best=0 result=1`, result `TURNS 200`,
  and autosave at turn 201.
- An old M7-format slot loaded at `turn=2 mode=1 score=24 time=3592`; the
  restored settler founded AURELIA and the city screen opened.
- **PSP 1000 combined test and tags pending.** Use the combined checklist in
  `docs/HANDOFF.md`.

## M7 status (historical; implementation complete, PPSSPP validated)

- **New Game setup**: choose Standard or Timed; timed mode offers 30/60/90
  minutes with 60 as the default. Timed clocks count wall time only while the
  map, city, or diplomacy screens are active, and the remaining time is shown
  in the map HUD.
- **Save/load**: versioned binary snapshots of `Map`, `World`, and the map
  view, with an FNV checksum and structural validation. Autosave writes after
  every completed turn; manual slots 1–3 are available from the pause menu and
  the main-menu Load screen. Save metadata shows turn, score, and timed
  remaining time; empty or corrupt saves are rejected with a message.
- **Pause menu**: Resume, Save Game, Load Game, Calibrate Stick, and Main Menu.
  Circle opens it from the map after deselecting any unit.
- **Results**: conquest, diplomatic, and science victories plus timed-mode
  score victory/defeat. The result screen shows score, turn, cities,
  population, techs, play time, and the best rival score.
- **Scoring**: population, cities, territory, buildings, units, researched
  techs, and positive treasury. M7 science victory required all 18 techs; M8
  replaces this with the three late projects.
- **Analog follow-up**: 15-sample median boot/New Game calibration, adaptive
  32–56 deadzone, convergent low-pass, minimum output, and automatic
  recentering after 45 zero-output frames. Calibrate Stick is also available
  in Options and the pause menu.
- `tests/test_save.c` covers round-trip, metadata, checksum corruption, short
  files, and argument validation. `tests/test_m7.c` covers scores, top-score
  ranking, and the updated three-goal science victory.

### M7 validation (PPSSPP)

- Standard run `seed=D37B3B67 start=32,22`: New Game setup → Start → turn 2;
  autosave wrote `auto.sav` (17,460 bytes); pause → slot 1 save; pause → load
  restored the same turn; clean exit.
- Timed run `seed=A3D034B0 start=32,24`: mode `timed`, limit 3600; after 8
  seconds the autosave header held `remaining=3592`, `play=8`; slot 2 saved and
  loaded from the main menu at `time=3592`.
- A copy of the timed save was edited to `remaining=1` with a valid checksum;
  loading it produced `time up: score=24 rival_best=67 result=2`, then the
  score-defeat result screen and a clean shutdown.
- **PSP 1000 smoke test and `m6`/`m7` tags are pending.**

## M6 status (feature-complete; PSP 1000 features pass, analog retest pending)

- `src/world.h/.c`: symmetric relations (-100..100), NAP/open-borders/alliance
  treaty masks, timed treaty counters, trade counters, gift counts, AI
  treasuries, pending AI proposals, alliance-victory countdown, and victory
  type. War and city capture tear up agreements and apply relation penalties.
- `src/diplomacy.c/.h`: five relation bands and seven player actions:
  declare war, propose peace, non-aggression pact, open borders, alliance,
  trade, and gift. NAP/open borders last 10 turns; alliances last until war or
  elimination. AI acceptance depends on relation, war state, existing treaties,
  army strength, and AI treasury.
- Gifts cost 10 gold (changed from the earlier 25-gold draft so the opening
  20-gold treasury can start the diplomacy loop). Gain starts at +12, drops by
  2 per gift to a floor of +4, and is halved during war.
- Trade: the AI pays the player 2 gold per turn for 10 turns. AI city gold now
  feeds an AI treasury (rivals start with 30), so payments are real rather than
  free money. Resource trading remains deferred.
- Open borders/alliances share vision of the partner's units and cities. They
  do **not** gate physical border movement yet; movement restriction is a
  future border-control slice.
- AI proposals: peace, NAP, and alliance offers appear in the diplomacy screen
  with accept/reject. AI-AI diplomacy makes peace after long wars and can start
  low-frequency wars after turn 24, but never against a player ally.
- Treaties block combat: `Combat_CanAttack`/`Combat_CanCaptureCity` return
  `COMBAT_ERR_TREATY`, shown as `TREATY BLOCKS WAR`.
- Diplomatic victory: alliances with all living rivals for 10 consecutive end
  turns, at least two living rivals, and no AI-AI war among them.
- `src/diplomacyview.c/.h`: 480×272 screen with rival list, relation bar,
  treaty/trade/army/proposal detail, action popup, and result messages.
  Start opens diplomacy from the map; Circle returns.
- `src/ai.c`: rivals respect relations/treaties when declaring war and use the
  diplomacy peace check.
- `tests/test_diplomacy.c`: relation bands, NAP, gifts, peace, proposals,
  diplomatic victory countdown/reset, treaty-blocked combat, open-borders
  vision/expiry, trade payments/expiry, and AI-AI war reset. `make test` ran
  eight suites at M6 (11 now, including save and M7 scoring).

### M6 validation (PPSSPP)

- Final build `seed=C6559DA5 start=35,24`: founded AURELIA and garrisoned the
  warrior; opened diplomacy with Start.
- Sent two gifts (gold `20→10→0`), raising BOREA from 0 to 22 relation; signed
  open borders, a 10-turn trade agreement, and a NAP. The action log shows
  `action=8` (gift), `action=5` (open borders), `action=7` (trade), and
  `action=4` (NAP).
- Trade paid `2` gold per turn for all 10 turns (`turns=10..1`), ending with
  the player at 20 gold. Both timed treaties expired together at turn 10
  (`treaty expired a=0 b=1 mask=3`).
- BOREA proposed a NAP on turn 10; accepting it produced
  `action=0 ... relation=34->39`, confirming AI proposal generation and
  acceptance. The capital survived the run with its garrison; no crash or soft
  lock.
- Host tests cover open-borders vision and trade expiry; PPSSPP screenshots
  remain unavailable through WSLg, so validation used the game log and window.
- **PSP 1000 smoke test and `m6` tag are pending.** Run the packaged
  `EBOOT.PBP`: boot → New Game → found AURELIA → garrison → Start → gifts /
  open borders / trade / NAP → run 10 turns → confirm trade payments, treaty
  expiry, and an AI proposal. Tag only after it passes.


## M5 status (implementation complete, PPSSPP validated)

- **Five civilizations**: AURELIA (player), BOREA, CYRENE, DRAVEN, and the
  BARBARIANS. The player starts with a settler, a warrior, and a scout, and
  founds AURELIA; each rival starts with a capital, a garrison, a warrior, and
  a settler. A barbarian raider starts within raiding distance.
- `src/combat.c` / `combat.h`: deterministic 100-HP combat. Damage is
  `30 + (attack - defense) * 5`, clamped to 5..95. Forest/hills add +1 defense;
  a city adds its `City_Defense` value. A surviving defender counterattacks.
  An attack consumes all remaining movement. Killing the defender prevents a
  counterattack; otherwise the attacker can die to the counterattack.
- `src/ai.c` / `ai.h`: synchronous AI turns for the three rivals and the
  barbarians. Rivals found up to four cities, choose technology-free production,
  expand toward scored settlement sites, path with A*, attack adjacent enemies,
  capture empty cities, declare war after turn 12, and make peace when their
  army is broken. Barbarians spawn near the player every 8 turns (maximum 4)
  and always attack.
- City capture: a military unit adjacent to a city whose center is empty can
  capture it. Killing a garrison and advancing into the center also captures
  the city. Capture reduces population by one, clears the queue, resets
  influence/locked tiles, transfers ownership, and checks victory/defeat.
- Victory/defeat: lose when the player has founded a city and owns none;
  win by owning every active rival capital. Both end in a result screen with
  turn/battle/capture statistics and a return-to-menu prompt.
- `src/world.c/.h`: five-civ state, war masks, declarations/peace, capture,
  notices, player-only economy/vision, unit healing, and `Ai_ProcessTurn`
  integration.
- `src/mapview.c/.h`: player-only selection, enemy owner colours, HP bars,
  red attack/capture cursor, attack and capture actions, war/capture notices,
  and a **GARRISON** action for moving a military unit into an adjacent empty
  friendly city. Cross on a friendly city still opens city management.
- `src/app.c`: game-over state and full-screen victory/defeat overlay.
- `tests/test_combat.c`: damage table, counterattacks, deaths, terrain/city
  defense, auto-war, city capture, and victory/defeat conditions.
- `tests/test_ai.c`: full rival start, AI expansion, war, peace, and a 200-turn
  stress simulation. `make test` ran seven suites at M5 and now runs eight.
- Not yet implemented: city HP/ranged attacks/siege, razing, AI
  research/workers, save/load, and the remaining 12 techs.

### M5 validation (PPSSPP)

- Main run `seed=28482A30 start=35,24`: 13 units, 3 rival capitals, and
  1 barbarian; founded AURELIA and garrisoned the starting warrior with the new
  action (`cost=3 moves_left=3`).
- The barbarian attacked the garrison at `(26,16)` on turn 3: damage 30,
  counter 30, HP 50/70. On the player turn, Cross on the adjacent barbarian
  produced `combat: player attack ... dmg=30 counter=30
  defender_dead=0 attacker_dead=0`; HP became 40 (player) and 20 (barbarian).
  The next barbarian attack triggered a lethal counterattack and removed it.
- AI rival expansion reached 14 cities; war notices appeared for BOREA
  (turn 13), CYRENE (turn 17), and DRAVEN (turn 21). A 34-turn run completed
  without a crash or soft lock.
- An earlier run (`seed=4BE1DAA0 start=34,24`) left AURELIA empty: the
  barbarian captured it on turn 6 (`BARBARIANS CAPTURED AURELIA`) and the app
  entered the defeat state (`game over: result=2`), validating hostile city
  capture and defeat.
- Host 200-turn simulation (`seed=0x5A5A1234`) completes with AI cities
  3→14, 101 battles, 1 capture, 55 units, and player defeat when passive.
- PPSSPP screenshots could not be captured reliably through WSLg's
  Wayland/OpenGL path; validation used the game log and the 960×532 window.
- **PSP 1000 smoke test passed** on the packaged `EBOOT.PBP`
  (`seed=96D58098 start=29,21`): boot, new game with 13 units/3 rival
  capitals/1 barbarian, founded AURELIA at `29,21`, opened and closed city
  management, ran AI turns, saw the barbarian capture AURELIA on turn 5,
  entered the defeat result (`game over: result=2`), and returned to the menu.
  This also exercised the M4.1 city-management path on hardware. Tags `m4.1`
  (`6ec3d43`) and `m5` (`bd292b2`) were applied.

## M4.1 status (implementation complete, PPSSPP validated)

- `src/improvement.c` / `improvement.h`: central improvement rules — worker
  only, technology gate, friendly-city ownership, city-center exclusion,
  existing-improvement denial, terrain/resource rules, and instant build.
- New **Worker** unit: 10 production, 4 move, 2 sight, generated pickaxe
  sprite. It can be produced in any city and cannot found cities.
- Triangle → **BUILD IMPROVEMENT** opens a five-entry list: Farm / Mine /
  Pasture / Quarry / Road. Locked entries remain visible with **LOCKED**;
  selecting one shows **REQUIRES TECH**. A successful build shows
  `BUILT <name>`, consumes all worker movement, and auto-advances.
- Rules: Farm on grassland/plains/desert; Mine on hills or iron/gems;
  Pasture on horses; Quarry on stone; Road on any land. The tile must be owned
  by a friendly city and cannot be the city center or already improved.
- Roads reduce the movement cost of entering their tile by 1 (minimum 1).
- Map markers: golden farm rows, mine shaft, pasture fence, quarry blocks, and
  a full-tile road stripe.
- `tests/test_improvements.c`: worker data, production, tech/ownership gates,
  city-center denial, all terrain/resource rules, build/movement consumption,
  yields, and road speed.

### M4.1 validation (PPSSPP)

- Final run `seed=53ABEAF9 start=34,24`: produced a Worker in AURELIA; all five
  improvements showed LOCKED at turn 2, and attempting Farm showed REQUIRES
  TECH (`result=-4`).
- Pottery (10 science) completed at turn 11; Science shows POTTERY RESEARCHED
  and the build menu unlocks Farm.
- Worker moved to owned plains `(34,25)` and built a Farm; log
  `unit: worker built FARM at 34,25`, toast `BUILT FARM`, moves `0/4`.
- Tile info shows `IMPROVEMENT FARM` and `UNIT WORKER`; the golden crop marker
  is visible at 480×272 (screenshots are 2×).
- All five host suites pass; clean build has no warnings.
- PSP 1000 smoke test passed in the same packaged `EBOOT.PBP` run recorded
  above (city management opened and closed on hardware); tag `m4.1` was
  applied to `6ec3d43`. Worker/farm interaction remains covered by the PPSSPP
  evidence, not by that particular hardware log.

## M4 status (core complete, PPSSPP validated)

- `src/tech.c` / `tech.h`: 18 technologies across 4 eras (Ancient, Classical,
  Medieval, Renaissance) with costs, prerequisites, and unlock descriptions.
- Research: each city's science is banked into the active tech each turn; a
  completed tech unlocks its buildings/units and stops research until the
  player picks the next one. Locked techs cannot be selected.
- City Science tab: empire science, active research/progress, and a scrollable
  tech list showing RESEARCHED / cost / progress / LOCKED. D-pad + Cross
  selects research; L/R switch tabs.
- Tech-gated production: Granary (Pottery), Library (Writing), Market
  (Currency), Barracks (Bronze Working), Walls (Masonry), Workshop
  (Engineering), University (Iron Working), Aqueduct (Construction). Locked
  entries show REQUIRES TECH in Production / LOCKED in Buildings.
- Two new units: **Swordsman** (Iron Working, 5 move, strength 8) and
  **Horseman** (Horseback Riding, 10 move, strength 4).
- Improvement unlocks are in the data table (Farm/Pottery, Mine/Mining,
  Pasture/Horseback, Quarry/Masonry, Road/The Wheel); worker placement was
  delivered in M4.1.
- `tests/test_tech.c`: tech data, prerequisite gating, research accumulation
  and completion, unlock data, and unit stats.

### M4 validation (PPSSPP)

- Pottery (10 science) completed at turn 11; log `tech: completed
  mask=00000002`; Writing became selectable, Bronze Working stayed locked.
- Granary changed from **REQUIRES TECH** to cost 20 after Pottery; Buildings
  showed only Granary/Monument unlocked.
- Locked tech selection showed the LOCKED toast; the 18-entry tech list
  scrolled to Printing Press/Gunpowder at 480×272.
- All five host suites pass; clean build has no warnings.
- PSP 1000: Science tab, Pottery completion, Writing/Granary unlock, and tech
  list scrolling verified on the final `EBOOT.PBP`; tagged `m4`.

## M3 status (complete)

### City model and rules

- `src/data.c` / `data.h`: centralized balance — terrain/resource/improvement
  yields, 20 city names, 9 buildings, and 12 production entries.
- `src/city.c` / `city.h`: radius-2 work area (5×5 mask), initial center 3×3
  ownership, citizen auto/manual/locked assignment, food + growth +
  starvation, production queue (max 5), border expansion + gold tile purchase,
  rush purchase, and square city vision covering the full work area.
- `src/cityview.c` / `cityview.h`: city screen with four tabs (Overview /
  Production / Citizens / Buildings), a 5×5 work grid, and end-turn toasts.
- `src/world.c` / `world.h`: treasury/science, city processing, upkeep, and a
  normal 1 settler / 1 warrior / 1 scout start.
- `tests/test_city.c`: yields, growth, production, borders, tile purchase, and
  a 30-turn city simulation.
- City center gives `2F 2P 1G 1S` free and cannot be worked; each citizen
  consumes 2 food. Growth `10 + 5*pop`; border `10 + 5*(owned-9)`; tile
  purchase `15 + 5*(owned-9)`; rush `max(5, remaining*2)`.
- Buildings: Granary 20/+2F, Monument 15/+1INF, Barracks 25/+1P, Market
  25/+4G, Library 30/+2S, Walls 20/+2DEF, Workshop 35/+2P, University
  45/+3S, Aqueduct 40/+2F; upkeep 1–2. Units: settler 20, warrior 10, scout 8.
- Market is +4 gold (raised from +2 after PPSSPP trials showed a one-city
  economy going negative with five buildings and units).
- When the treasury goes negative, city production and science are halved.

### M3 validation (PPSSPP; ELF and EBOOT)

- A PPSSPP run reached turn 31 at `pop=3 owned=12 units=2 treasury=45
  science=58` with a Granary/Market/Library/Monument/Barracks queue.
- Host 30-turn simulation: `pop=4 owned=12 buildings=5 units=1 science=52
  treasury=186` — all city tests pass.
- Citizen work/lock/auto, buying an unowned gems tile (15 gold), rush
  no-gold denial then a successful scout rush, queue-full and removal, and
  border expansion were all exercised with the D-pad/buttons in PPSSPP.
- City fog: the whole 5×5 work area is revealed (square vision), so purchased
  and grown tiles never appear black.

### Controls (city screen)

| Input | Action |
|-------|--------|
| D-pad | Move citizen tile cursor |
| Cross | Toggle a citizen onto/off a tile (Citizens); queue (Production/Buildings) |
| Circle | Back to map |
| Square | Reassign idle citizens to the best owned tiles |
| Triangle | Lock/buy a tile (Citizens); rush-purchase (Production) |
| L / R | Cycle tabs |
| Select | End turn |

## M2 status (M2, complete)

### Data and rules

- `src/unit.c` / `unit.h`: settler (6 move, sight 2), warrior (6/2), scout
  (8/3); static pool of 64 units.
- `src/city.c` / `city.h`: static pool of 32 cities, Chebyshev minimum
  distance 3, population 1, city sight 2, 10 original city names.
- `src/world.c` / `world.h`: starting layout, vision union, end turn, unit
  cycling, and city founding.
- `tests/test_units.c`: movement costs, blocking, no stacking, founding,
  turn reset, cycling, and start setup.
- Half-tile movement costs: straight 2, diagonal 3, +2 entering forest or
  hills. Mountains and water are impassable; one unit per tile; units may
  enter friendly city tiles.

### Controls and presentation

- Cursor + Cross movement: the D-pad moves a free cursor; its frame turns
  green when the selected unit can reach it, gold when it cannot, and red
  briefly when a move is denied. Cross runs an 8-way Dijkstra path within the
  unit's remaining points. This avoids awkward diagonal D-pad combos.
- A settler founds a city at its tile via Triangle -> Found City. Founding
  consumes the settler. Triangle also offers Wait and Cancel.
- Toast messages report founding errors (too close / invalid tile), no moves,
  blocked movement, and end-turn prompts.
- Select ends the turn and resets all movement. L/R cycles units that still
  have moves; after an action the view advances to the first unit with moves.
- Fog is now the union of unit and city vision; explored tiles persist. The
  cursor is no longer a vision source.
- Unit and city sprites are packed into row 4 of the existing 128×128
  `GU_PSM_5551` terrain atlas; no second texture path.
- Square shows tile info (terrain, resource, fog, unit, city); the bottom HUD
  shows the unit under the cursor, or the selected unit, or the city under the
  cursor.

### M2 start setup (temporary)

New games start with 3 settlers, 2 warriors, and 1 scout. Settlers spawn at
least `CITY_MIN_DISTANCE` (3) apart so the "found three cities" acceptance
test works before M3 production exists. This setup will be replaced by normal
city production in M3.

### M2 validation (PPSSPP 1.20.4 and PSP 1000; ELF and EBOOT)

- Seed `B6B15A16`, start `32,23`. Initial units: scout 32,23; warriors 31,24
  and 33,24; settlers 29,21, 35,21 and 32,26.
- 29,21 -> 30,21 cost 2 (moves 4/6); AURELIA founded at 30,21.
- CASTRUM at 35,21 and PORTA at 32,26 founded; log: `cities=3 units=3`.
- 32,23 -> 34,23 cost 4 (moves 4/8); the scout then entered friendly CASTRUM
  at 35,21 for cost 7 (moves 1/8).
- Mountain at 36,21: move denied with a full 8/8 budget; the tile info panel
  reports MOUNTAIN. Water/mountain blocking is isolated in host tests because
  no water is near this start.
- Turns 2 through 8 with `cities=3 units=3`: no softlock; L/R cycling works
  (PPSSPP keyboard q/w).
- 59–60fps. The packaged `EBOOT.PBP` boots, starts a new game, and ends a turn
  in PPSSPP.
- **PSP 1000 (seed `8C14D704`, start `31,23`):** six units; AURELIA at 29,21,
  CASTRUM at 31,26, PORTA at 34,21; turns 2–10 with `units=3 cities=3`;
  clean `map: back to menu` → `menu: quit` → `shutdown` → `log closed`.
  The worn stick calibrated to center `165,133`; the tester reported
  performance fine.
- Screenshots: [three cities](docs/screenshots/m2_three_cities.png),
  [movement](docs/screenshots/m2_movement.png),
  [blocked mountain](docs/screenshots/m2_blocked.png),
  [city info](docs/screenshots/m2_city_info.png),
  [action menu](docs/screenshots/m2_action_menu.png).

M2 is complete on both PPSSPP and PSP 1000 and tagged `m2`. M3 (city
management) follows above and is complete on both PPSSPP and PSP 1000, tagged
`m3` on `451256a`.

## M1 status (complete)

- Deterministic 64×48 square map (3072 tiles, ~15KB of map data) with 9
  terrains (grassland, plains, forest, hills, mountain, desert, tundra, coast,
  ocean), 4 resources (horses, iron, stone, gems), and 5 improvement slots
  (farm, mine, pasture, quarry, road).
- Generation keeps only the largest connected landmass, guarantees one of
  each resource, and picks a center-biased start tile. Land is ~72% on
  average (tests enforce 60–85% and full terrain presence).
- Three-state fog of war (unexplored / explored / visible). In M1 the cursor
  was a temporary radius-4 vision source; M2 replaces it with unit and city
  vision.
- Pixel-scrolling camera with clamping and auto-follow, D-pad cursor with key
  repeat, and calibrated analog panning.
- HUD: top status bar (tile, terrain, resource, turn, seed), bottom action bar,
  FPS counter, and a Square tile-info panel.
- Terrain/resource atlas generated by `tools/gen_art.py` and packed as
  `GU_PSM_5551` (1-bit alpha) by `tools/pack_atlas.py`.
- Host tests (`make test`) cover deterministic generation across 32 seeds,
  land connectivity, resource presence, land fraction, full terrain mix, and
  fog invariants.
- PPSSPP and PSP 1000 validation passed, including M1.1 (64×48 map) and M1.2
  (boot-time analog-stick calibration and D-pad priority).

## M0 status (complete)

- `make clean && make` succeeds with no compiler, linker, or import warnings.
- `civpsp.elf` and packaged `EBOOT.PBP` boot to the main menu in PPSSPP.
- D-pad navigation, Cross confirm, Circle back, options, placeholder toasts,
  and quit are exercised.
- Startup and shutdown entries are written to `ms0:/CIVPSP_LOG.txt`.
- Textured text uses the exact 24-byte GU vertex layout, guarded by a static
  assertion.
- Fixed a PSP 1000-only first-glyph cache corruption with 64-byte-aligned
  vertex buffers and `sceKernelDcacheWritebackRange` before each draw.
- PSP 1000 validation passed: stable 60fps, correct titles, clean Home exit.

## Requirements

- pspdev / PSPSDK (`psp-config`, `psp-gcc`)
- Python 3 with Pillow (only needed to regenerate assets)
- A host C compiler for `make test`
- PPSSPP for desktop testing
- A PSP 1000 with CFW for real-hardware testing

## Build

```bash
export PATH=$PATH:/home/zhang/pspdev/bin
cd /home/zhang/pspGameDev/civpsp
make clean && make
```

Outputs:

- `civpsp.elf` — load directly in PPSSPP for debugging.
- `EBOOT.PBP` — copy to `ms0:/PSP/GAME/CIVPSP/` on the PSP.

Regenerate art/font/string data:

```bash
make assets
```

Run the host-side logic tests (no PSP required):

```bash
make test
```

## Run

PPSSPP (absolute paths are required; relative paths resolve inside the emulated
UMD filesystem):

```bash
APPIMAGE_EXTRACT_AND_RUN=1 \
  /home/zhang/pspGameDev/ppsspp/PPSSPP-v1.20.4-anylinux-x86_64.AppImage \
  --windowed --graphics=software /home/zhang/pspGameDev/civpsp/civpsp.elf
```

Pass the absolute path to `EBOOT.PBP` instead to test the packaged build.

PSP 1000:

1. Connect the Memory Stick over USB.
2. Create `ms0:/PSP/GAME/CIVPSP/`.
3. Copy `EBOOT.PBP` there.
4. Launch from the XMB Game menu.

A boot log is written to `ms0:/CIVPSP_LOG.txt`.

## Controls

Menu:

| Input | Action |
|-------|--------|
| D-pad up/down | Move selection |
| Cross | Confirm |
| Circle | Back / cancel |
| Start | Reserved for the in-game system menu |

Map:

| Input | Action |
|-------|--------|
| D-pad | Move cursor |
| Analog stick | Pan camera |
| Cross | Open the city under the cursor; otherwise select a unit, move the selected unit, or attack/capture an adjacent enemy |
| Circle | Close info, deselect unit, or return to main menu |
| Square | Toggle tile info panel |
| Triangle | Open the unit action menu (including units standing on a city tile); with no unit, open the city under the cursor |
| L / R | Cycle to the previous / next unit with moves |
| Select | End turn (AI and barbarians move, combat resolves) |
| Start | Reserved for the in-game system menu |

Unit actions:

| Input | Action |
|-------|--------|
| Triangle → GARRISON | Move a military unit into an adjacent empty friendly city |
| Triangle → WAIT | Spend the unit's remaining movement |
| Cross on an adjacent enemy | Attack; a surviving defender counterattacks |
| Cross on an adjacent empty enemy city | Capture it with a military unit |

Worker actions:

| Input | Action |
|-------|--------|
| Triangle → Build Improvement | Open the Farm/Mine/Pasture/Quarry/Road list |
| Up / Down | Choose an improvement |
| Cross | Build; locked entries show LOCKED and produce REQUIRES TECH |
| Circle / Triangle | Back to the map |

PPSSPP keyboard mapping used during validation:

| PSP | Keyboard |
|-----|----------|
| Cross | `z` |
| Circle | `x` |
| Square | `a` |
| Triangle | `s` |
| D-pad | Arrow keys |
| L / R | `q` / `w` |
| Select | Return |
| Start | Space |

## Project layout

```text
civpsp/
├── PROJECT_PLAN.md      full design and milestone plan
├── Makefile
├── assets/              strings.csv, generated PNG/raw assets
├── docs/                status, decisions, handoff notes, screenshots
├── src/                 C sources and generated headers
│   ├── data.c/.h        rules table: yields, buildings, production
│   ├── tech.c/.h        M4 tech tree, eras, prerequisites
│   ├── combat.c/.h      M5 HP combat, counterattacks, city capture
│   ├── ai.c/.h          M5 rival/barbarian turns, expansion, A* movement
│   ├── improvement.c/.h M4.1 worker improvement rules and construction
│   ├── unit.c/.h        unit stats, HP, movement, pathfinding, vision, roads
│   ├── city.c/.h        city pool, yields, growth, queue, borders
│   ├── cityview.c/.h    city screen: 5 tabs, grid, citizens, queues, science
│   ├── world.c/.h       civs, start layout, turns, war, economy, victory
│   └── mapview.c/.h     cursor, movement, combat, menus, entities, HUD
├── tools/               Python asset/string/font generators
└── tests/               host tests (map, units, city, tech, improvements, combat, AI)
```

## Context / handoff

The project is developed in small milestones. At the end of each milestone the
status and handoff documents are updated. If a conversation approaches its
context limit, work stops after a commit and `docs/HANDOFF.md` is refreshed so a
new conversation can continue without reading the whole repository.
