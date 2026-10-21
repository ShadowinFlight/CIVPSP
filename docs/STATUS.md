# Status

Last updated: 2026-10-21

## Current milestone

**V0.1 — First GitHub development release (post-M9.12). Includes the complete
4X game, Chinese/English UI, 30-tech tree, diplomacy, saves, minimap, audio,
12-page manual, civilization colors, menu transitions, and release artwork.
PPSSPP validated; the latest package still needs a final PSP 1000 hardware
pass. Internal M6–M9 tags remain pending; M5 is tagged `m5` and M4.1 is tagged
`m4.1`.**

## M9 (implementation complete, PPSSPP validated, PSP 1000 pending)

### Implemented

- **Chinese font** (`tools/gen_font.py`, `tools/pack_atlas.py`,
  `src/gen/font_zh.h`): generated 16×16 glyph cells for all 485 Chinese
  strings plus the digits and hard-coded punctuation used by the UI. The atlas
  is 512×512 RGBA (1MB); ASCII keeps an 8px advance and CJK uses 16px.
  Chinese glyphs use Noto Sans CJK SC Medium at 14px so descenders stay inside
  the cell.
- **Runtime language toggle** (`tools/gen_strings.py`, `src/text.c`,
  `src/render.c`, `src/app.c`): generated English and Chinese tables with
  fallback; `Str_SetLanguage`/`Str_GetLanguage`; UTF-8 decoding, variable
  advances, width/height measurement, and texture switching. Chinese is the
  default; Options → Language switches to English or back.
- **Translation**: all 485 string IDs have Chinese values. `tools/check_i18n.py`
  validates coverage, printf placeholder order, glyph coverage, and line width
  (maximum 368px at 480px wide).
- **Layout adjustments for Chinese**: 17px map-info rows with a 121px field
  block; 4-row production list, 6-row science list, 17–18px city rows; larger
  diplomacy list rows and title bar; pause/new-game/save/load title spacing.
- **Civilization-colored sprites** (`tools/gen_art.py`, `src/mapview.c`,
  `src/gen/terrain_gen.h`): the 128×256 GU_PSM_5551 atlas has five unit/city
  palette rows. Team color is applied to the settler tent, warrior shield,
  scout cloak, city flag/gate banner, and worker shirt; a 20×3 owner bar sits
  under every entity. Player blue, Borea red, Cyrene gold, Draven purple,
  barbarians grey. Owner row selection is clamped to the five rows.
- **Minimap** (`src/render.c/.h`, `src/mapview.c`, `S_MINIMAP_TITLE`): a
  114×86 dark/gold panel at the map's bottom-right shows explored terrain,
  dimmed explored-only tiles, bright currently-visible tiles, city/unit
  markers, cursor, and camera viewport. It has no title bar. The 64×48 map is
  cached as a 64×64 GU_PSM_5551 texture (8KB) and drawn as one quad, avoiding
  the renderer's 1024-rect limit. A small `小地图 L+R` button appears while
  collapsed; L+R toggles it. No save-format changes.
- **Audio** (`tools/gen_audio.py`, `src/audio.c/.h`,
  `src/gen/audio_gen.h`): Python synthesizes an 8s ambient loop plus eight
  SFX. `src/audio.c` now reserves a standard **44100Hz stereo
  `sceAudioChReserve` channel**, duplicates the 22050Hz source frames, writes
  the mix buffer back from the data cache before each `sceAudioOutputPannedBlocking`,
  and mixes on a dedicated thread. This replaces the SRC path that did not
  play correctly on the PSP 1000. Options → 音乐 toggles music; UI, turn,
  build, found-city, combat, and research sounds are wired.
- **Map walkable connectivity** (`src/map.c`): generation now flood-fills
  passable terrain (mountains and water are barriers; forest, hills, desert,
  and tundra remain passable) and carves the shortest mountain pass to every
  isolated walkable pocket. Unreachable pockets that cannot be connected are
  converted to mountains. `tests/test_map.c` checks 100 seeds and requires all
  passable tiles to be reachable from the start.
- **Release art**: `ICON0.PNG` and `PIC1.PNG` rebuilt with a formal
  gradient/compass/city-skyline treatment, Chinese taglines, and
  `M9 EDITION` / `M9 BUILD` labels.
- **Menu transitions** (`src/app.c`): a 20-frame full-screen fade follows every
  app state change; menu rows slide in from the right with a 2-frame stagger;
  the selected row gets an 8-frame gold glow. The fade also covers map, city,
  diplomacy, pause, save/load, help, and result states.
- **M9.11 data balance**: applied the edited `docs/GAME_DATA.md` values:
  terrain gold (grass/plains/forest +1/+2/+1), richer improvements
  (farm +2F/+1G, mine +1P/+2G/+1S, pasture +1F/+1G, quarry +1P/+1G/+1S),
  building bonuses (monument/library/workshop/university/aqueduct), and
  food per population 2→3. Tests and the in-game manual were updated.
- **M9.12 display pass**: building rows now list every nonzero effect with
  compact F/P/G/S/I/D labels, and city tile detail/grid show improvement
  science (mines/quarries).
- **In-game manual** (`src/app.c`, `assets/strings.csv`): the Help screen is
  now a 12-page `游戏手册` with sections for goals, controls, turns/fog,
  city management, citizens/borders, science, units/combat, diplomacy,
  victory/score, buildings, unit/terrain stats, and settings/tips. It is
  reachable from the main menu and Pause; LEFT/RIGHT flips pages.
- **Tests**: `tests/test_i18n.c` covers the language toggle and lookup;
  `tests/test_map.c` now checks 100 seeds plus two known isolated-pocket
  seeds for deterministic generation, full land connectivity, full
  passable-tile connectivity, fog invariants, and terrain/resource coverage.
  `tools/check_i18n.py` and `tools/check_audio.py` run in `make test`.
  13 host suites plus the i18n/audio checks pass.
- Save format and gameplay state are unchanged; M7/M8 saves remain compatible.
- Complete numeric reference: `docs/GAME_DATA.md` (units, buildings, terrain,
  improvements, techs, city economy, combat, diplomacy, scoring).

### M9 acceptance evidence (PPSSPP)

- Final `EBOOT.PBP` booted with log header `CIVITAS PSP M9`.
- Chinese main menu, Options (Language 中文), 12-page manual, Standard map HUD,
  city overview/production/citizens/buildings/science tabs, pause, save/load,
  diplomacy and action menu, map tile info, and score result were inspected at
  480×272/2×. No missing glyphs or overlapping rows in the captured screens.
- English mode was toggled from Options and rendered with the original English
  atlas and layout.
- M8 save compatibility: slot 1 (M8 format) loaded at `turn=2 mode=1 score=41`
  and opened the Chinese city screen.
- Standard turn-limit result rendered in Chinese: `分数胜利`, `分数 41
  回合 200`, `城市 1 人口 1 科技 0`, then autosave.
- Civilization colors: revealed test saves centered on Borea, Cyrene, and
  Draven showed red, gold, and purple units/cities; the player map showed blue
  units/city. The coordinate commas render correctly after adding the comma
  glyph to the Chinese atlas.
- Map connectivity sweep: before the fix, 97 of 2000 seeds had an isolated
  walkable component (1–35 tiles); after the fix, 0 of 2000. The committed
  100-seed map test also passes.
- Minimap: full-map and fog-limited PPSSPP captures show explored terrain,
  city/unit markers, camera viewport, the compact bottom-right panel, and the
  collapsed `小地图 L+R` button; L+R opened/closed it.
- Audio: PPSSPP log identifies `audio: channel=7 44100Hz stereo, ambient
  176400 source samples`; shutdown reports `audio: shutdown rc=0`; Options
  music on/off works. Generated WAVs now peak at 26213 (80% of full scale)
  and `tools/check_audio.py` passes.
- XMB art: new 144×80 icon and 480×272 background inspected; both are embedded
  in the current EBOOT.
- Menu transitions: mid-fade Options and selection-glow captures were taken;
  the settled layout matches the prior screens.
- Manual: pages 1 and 12 inspected at 480×272/2×; 12-page indicator and
  five-line pages fit without clipping.
- Revised audio: PPSSPP logs `audio: channel=7 44100Hz stereo, ambient 176400
  source samples` and no output errors. Hardware playback still needs the
  user's retest.
- Screenshots: `docs/screenshots/m9_*.png`, including
  `m9_civ_colors.png`, `m9_minimap.png`, `m9_minimap_button.png`, `m9_options_music.png`, `m9_menu_transition.png`, `m9_menu_selection.png`, `m9_manual_1.png`, `m9_manual_12.png`,
  `m9_xmb_icon.png`, and `m9_xmb_pic1.png`.

### M9 known simplifications / risks

- Language selection is per session (not persisted) and defaults to Chinese.
- The Chinese atlas is 1MB in the binary; audio adds a thread and the
  8-second ambient loop. PSP 1000 memory/performance/audio quality need the
  hardware pass.
- The minimap is collapsed by default and toggled with L+R; the visible
  shortcut button is the discoverability aid. A settings-menu toggle can be
  added later if needed.
- UI text is translated, but internal logs and some technical abbreviations
  remain English by design.
- Legacy placeholder strings from earlier milestones are translated but not
  visible in current UI.

### PSP 1000 validation (pending)

Do **not** tag `m9` yet. Use the combined checklist in `docs/HANDOFF.md`;
pay particular attention to Chinese text clarity at 480×272, texture memory,
and the language toggle.

## M8 (historical; implementation complete, PPSSPP validated, PSP 1000 pending)

### Implemented

- **Help / tutorial** (`src/app.c`, `assets/strings.csv`): a four-page Help
  screen reachable from the main menu and Pause. Pages cover movement,
  turns/map controls, city/research controls, and diplomacy/victory. LEFT/RIGHT
  changes pages, Cross advances, Circle returns to the caller (main menu or
  pause).
- **30-tech tree** (`src/tech.c/.h`): 8 Ancient, 8 Classical, 7 Medieval, and
  7 Renaissance techs. The original 18 enum values retain their bit positions;
  the 12 new techs are appended, so version-2 M7 saves still load.
- **Tech effects** (`src/city.c`, `src/world.c`, `src/tech.c`): one data-driven
  effect per tech. Implemented effects include farms +1 food, libraries/
  universities +1 science, markets +1 gold, workshops/barracks +1 production,
  walls +1 defense, +1 gold/science per city, and +10% research. The Science
  tab shows the selected tech's effect line.
- **Science victory**: the three late goals are Astronomy, Scientific Method,
  and Navigation. `World_ScienceGoalsComplete` replaces the old all-tech
  check; the Science tab shows `PROJECTS n/3`.
- **AI research and production** (`src/ai.c`, `src/world.c`): AI research is
  deterministic and derived from the turn counter (about one tech per 8
  turns). AI production uses the derived mask to field swordsmen/horsemen and
  build libraries, markets, workshops, and universities. AI tech count now
  contributes to score.
- **Standard mode ending** (`src/world.c`, `src/app.c`, `src/mapview.c`):
  after turn 200 the game ends by score. The map bottom bar shows
  `TURN n/200`; the result screen clamps the displayed turn to 200. Ending a
  turn from the city screen takes the same autosave/limit path as ending it
  from the map.
- **Tests** (`tests/test_m8.c`, `tests/test_ai.c`, `tests/test_m7.c`):
  era distribution, prerequisite validity, tech effects, research-percent
  stacking, derived AI research, AI advanced-unit selection, three science
  goals, standard score victory/defeat, and a 320-turn stability run.
  `make test` runs 12 suites.
- **Save compatibility**: `SAVE_VERSION` remains 2 and the `Map`/`World`
  layout is unchanged. An M7-format slot loaded successfully in PPSSPP.

### M8 acceptance evidence (PPSSPP)

- Main menu: five items including `HELP / HOW TO PLAY`; Help pages 1–4
  navigated with RIGHT, page counter `PAGE n/4`, and Circle returned to the
  main menu.
- Pause: six items including Help; Help opened from Pause and returned to
  Pause. The pause footer now has an opaque strip so the map HUD does not show
  through it.
- Science tab: 8 visible rows over the 30-tech list, scrolling to
  `MILITARY SCIENCE`, selection bar aligned with the row, and the effect line
  showing `BARRACKS +1 PRODUCTION`.
- New Game Standard: `seed=6FB11BD0 start=32,24 mode=standard limit=0`; map
  showed `TURN 1/200` in the bottom bar.
- Standard finish: a version-2 save was edited to turn 200 with a recomputed
  checksum, loaded, and ended from the city screen. Log:
  `turn limit: turn=201 score=41 rival_best=0 result=1`; result screen showed
  `SCORE 41  TURNS 200` and autosave wrote `turn=201 mode=0`.
- M7 save compatibility: old slot 2 loaded at `turn=2 mode=1 score=24
  time=3592`; AURELIA was founded from the restored settler and the city screen
  opened.
- Long-run host stability: 320 turns, AI tech 30/30/30, 53 units, 14 cities,
  no invalid units/cities or crash. The passive-player simulation ends in
  defeat, which is expected because the player never moves or defends.

### M8 known simplifications

- AI research is an abstract deterministic schedule, not stored per-civ
  research; this preserves the M7 save layout.
- Science “projects” are represented by three late technologies, not a
  separate multi-turn project UI.
- The 30-tech Science list is a flat scrollable list, not an era-grouped
  visual tree.
- M8 itself had no audio or new art pass; M9.4 adds both. Difficulty
  selector, border-movement gating, and resource trading remain future work.
- Standard-mode score compares the player with the best living rival only;
  ties go to the player.

### PSP 1000 validation (pending)

Do **not** tag `m8` yet. Use the combined checklist in `docs/HANDOFF.md`:
analog drift, M6 diplomacy/trade, M7 save/load, M8 Help, 30-tech Science,
standard `TURN n/200`, and the 200-turn/score result.

## M7 (historical; implementation complete, PPSSPP validated, PSP 1000 pending)

### Implemented

- **New Game setup** (`src/app.c`): Standard or Timed mode; Timed offers
  30/60/90 minutes, default 60. Start logs seed, start tile, mode, and limit.
- **Timed clock**: `sceKernelGetSystemTimeLow` wall time, accumulated only in
  MAP/CITY/DIPLOMACY states. Menus and pause do not consume the limit. The map
  bottom bar replaces FPS with `TIME MM:SS` while timed. Time-up compares the
  player score with the best living rival and sets score victory/defeat.
- **Save format** (`src/save.c/.h`, version 2): raw `Map`, `World`, and
  `SaveView` snapshot plus a `SaveHeader` containing magic, version, size,
  mode, turn, remaining/play/limit seconds, map seed, RNG state, player score,
  and an FNV-1a checksum. Structural validation covers map tiles, unit/city
  bounds and owners, counts, victory state, and camera/cursor values.
- **Slots**: `auto.sav` plus `slot1.sav`–`slot3.sav` under
  `ms0:/PSP/SAVEDATA/CIVPSP`. Autosave runs after every completed turn. Manual
  saves are available from Pause → Save Game; Load Game lists autosave and the
  three slots in the main menu and pause menu. Menu metadata shows turn, score,
  and remaining time; empty/corrupt saves are marked and rejected safely.
- **Pause and options** (`src/app.c`): Resume, Save Game, Load Game, Calibrate
  Stick, Main Menu. The main-menu Options screen also exposes calibration.
- **Results and scoring** (`src/world.c/.h`, `src/app.c`): conquest, diplomatic,
  and science victories, plus timed score victory/defeat. Score counts
  population ×6, cities ×10, owned territory, buildings ×5, units, researched
  techs ×10, and positive treasury/20. Results show score, turn, cities,
  population, techs, play time, and best rival score.
- **Science victory**: all 18 currently available techs. The planned 30-tech
  tree is still future work, so this is the M7 definition.
- **Map state restore** (`MapView_Restore`): cursor, selected unit, and camera
  survive save/load; `Selected` is clamped to the restored unit pool.
- **Tests**: `tests/test_save.c` (round-trip, metadata, checksum corruption,
  short files, argument validation) and `tests/test_m7.c` (score formula,
  top-score ranking, science victory). `make test` runs 11 suites.

### M7 acceptance evidence (PPSSPP)

- Standard run `seed=D37B3B67 start=32,22`: New Game setup → Standard → turn 2;
  autosave wrote 17,460-byte `auto.sav`; Pause → Save Game → slot 1 wrote
  17,460 bytes; Pause → Load Game → slot 1 restored `turn=2 mode=0 score=24`.
- Timed run `seed=A3D034B0 start=32,24`: `mode=timed limit=3600`; after 8
  seconds the autosave header contained `remaining=3592 play=8`; slot 2 saved
  and loaded from the main menu at `time=3592`.
- A copy of the timed save was edited to `remaining=1` with a recomputed valid
  checksum. Loading it produced `time up: score=24 rival_best=67 result=2`,
  then the score-defeat result screen and clean shutdown.
- WSLg still cannot capture PPSSPP screenshots reliably; evidence is the game
  log, save headers, and window state.

### PSP 1000 validation (pending)

Do **not** tag `m7` yet. On the packaged `EBOOT.PBP`: confirm the analog fix;
start Standard and Timed games; verify the timed HUD; end a turn and confirm
`save: write slot=0`; save/load slot 1; use main-menu Load; trigger a result
screen; quit cleanly.

### Known M7 simplifications

- Saves are raw struct snapshots and are only guaranteed to load with the same
  build/version; there is no migration path yet.
- Timed score victory only compares the player with the best living rival; ties
  go to the player.
- Science victory uses all currently available techs, not the planned
  30-tech tree.
- Save overwrite has no confirmation; autosave is overwritten every turn.
- The save directory is plain files under `SAVEDATA`; there is no PSP
  `PARAM.SFO`/save icon, so the system save manager may not display it.
- AI civilizations do not research, so their score has no tech component.
- Score victory is only evaluated when a timed game expires, not in Standard
  mode.

## M6 (features pass PSP 1000; analog retest pending before tag)

### Implemented

- **Relations** (`src/world.h/.c`): symmetric -100..100 values, five bands
  (HOSTILE / COLD / NEUTRAL / FRIENDLY / ALLIED), gifts, war/peace/capture
  modifiers, and a small drift every third turn.
- **Treaties**: NAP and open borders last 10 turns; alliances last until war or
  elimination. War and capture clear agreements. `World_CanDeclareWar` blocks
  war while a NAP/alliance exists, and combat returns `COMBAT_ERR_TREATY` when
  a treaty still blocks the attack/capture.
- **Seven actions** (`src/diplomacy.c/.h`): declare war, propose peace, NAP,
  open borders, alliance, trade, and gift. AI acceptance checks relation,
  current war, existing treaties, relative army strength, and AI treasury.
- **Gifts**: 10 gold; gain +12, decreasing by 2 per gift to a floor of +4,
  halved during war.
- **Trade**: AI pays the player 2 gold per turn for 10 turns. Rivals receive
  city gold into an AI treasury (starting 30), so the payments are funded.
  Resource trading is not implemented yet.
- **Open borders**: shares vision of the partner's units and cities. Physical
  border movement is not gated yet; this is a deliberate M6 simplification.
- **AI proposals** (`src/diplomacy.c`): peace after a war, NAP, and alliance
  offers; accept/reject updates the relation and clears the proposal with a
  cooldown. AI-AI diplomacy makes peace after long wars and can start
  low-frequency wars after turn 24, but not against a player ally.
- **Diplomatic victory**: alliances with all living rivals for 10 consecutive
  end turns, at least two living rivals, and no AI-AI war among them. Any war
  resets the countdown.
- **Diplomacy screen** (`src/diplomacyview.c/.h`): 480×272 rival list, relation
  bar/band, treaty/trade/army/proposal details, action popup, and messages.
  Start opens it from the map; Circle returns.
- **Tests**: `tests/test_diplomacy.c` covers relations, NAP, gifts, peace,
  proposals, victory countdown/reset, treaty-blocked combat, open-borders
  vision/expiry, trade payments/expiry, and the AI-AI-war reset. `make test`
  runs eight suites at M6 (11 now, with save and M7 scoring).

### M6 acceptance evidence (PPSSPP)

Final build, `seed=C6559DA5 start=35,24`:
- Founded AURELIA and garrisoned the starting warrior; opened diplomacy with
  Start.
- Sent two gifts (`gold 20→10→0`, relation `0→12→22`), signed open borders
  (`action=5`), trade (`action=7`), and NAP (`action=4`) with BOREA.
- Trade paid 2 gold for all 10 turns (`turns=10..1`); the player treasury
  ended at 20. Timed treaties then expired together
  (`treaty expired a=0 b=1 mask=3`).
- BOREA proposed a NAP on turn 10; accepting it logged
  `action=0 relation=34->39`.
- The capital survived the run with its garrison; no crash or soft lock.
  Open-borders vision and trade expiry are additionally pinned by host tests.

### PSP 1000 validation

- User hardware run: M6 features (relations, gifts, treaties, trade, proposals,
  and diplomacy screens) worked; the only issue was camera drift while using
  the worn analog stick.
- Analog stabilization was implemented after that run: 15-sample median
  boot/New Game calibration, adaptive 32–56 deadzone based on resting-center
  bias, convergent low-pass filter, minimum output threshold, and automatic
  recentering after 45 zero-output frames. Calibrate Stick is available in
  Options and the pause menu. Host tests and PPSSPP pass; hardware retest is
  pending.
- Do **not** tag `m6` yet. Confirm the analog drift is gone on the packaged
  `EBOOT.PBP`, then apply annotated tag `m6` on `ea5f018`.

## M5 (complete, tagged `m5`)

### PSP 1000 smoke test (2026-10-21)

Packaged `EBOOT.PBP`, `seed=96D58098 start=29,21`:
- New game: 13 units, 3 rival capitals, 1 barbarian, treasury 20.
- Founded AURELIA at `29,21`; opened city management (`pop=1 owned=9`) and
  returned to the map.
- AI turns 2–4 completed; the undefended capital was captured by barbarians on
  turn 5 (`owner=4 city=AURELIA`), producing `notice=BARBARIANS CAPTURED
  AURELIA`.
- `game over: result=2 turn=5` and return to menu worked. This is the intended
  undefended-capital defeat path, not a crash.
- Tags: `m4.1` on `6ec3d43`, `m5` on `bd292b2`.

This hardware run did not include worker/farm or a player-initiated attack;
those remain covered by the earlier PPSSPP evidence below.


### Implemented

- **Civ model**: AURELIA (player), BOREA, CYRENE, DRAVEN, and BARBARIANS.
  `World_Begin` creates the player start plus three rival capitals and a
  barbarian raider; `World_Init` remains the player-only test fixture.
- **Combat** (`src/combat.c/.h`): 100 HP, deterministic damage
  `30 + (attack - defense) * 5` clamped to 5..95, forest/hills +1 defense,
  city `City_Defense`, counterattack by a surviving defender, movement
  consumption, death removal, auto-war when attacking a major civ, and city
  capture.
- **City capture**: a military unit adjacent to an empty enemy city can
  capture it; killing a garrison and advancing into the center also captures.
  Capture reduces population by one, clears queue/production/food/influence,
  unlocks tiles, transfers ownership, and sets a notice.
- **AI** (`src/ai.c/.h`): three rivals found up to four cities, choose
  technology-free production, expand toward scored sites, use A* pathing,
  target and attack enemies, capture cities, declare war after turn 12, and
  make peace when their army is broken. Barbarians spawn near the player every
  8 turns up to 4 and are always hostile.
- **World integration** (`src/world.c/.h`): civ state, war masks, notice text,
  capture, player-only economy/vision, healing, victory/defeat, and synchronous
  AI processing in `World_EndTurn`.
- **Map UI** (`src/mapview.c/.h`): player-only selection, owner colours, HP
  bars, red attack/capture cursor, enemy info, attack/capture actions, war and
  capture notices, AI turn logging, and a **GARRISON** action for an adjacent
  empty friendly city.
- **Game over** (`src/app.c`): victory/defeat overlay with turn/battle/capture
  statistics and return to menu. Defeat when a founded player has no cities;
  victory when the player owns every active rival capital.
- **Tests**: `tests/test_combat.c` and `tests/test_ai.c`; `make test` ran seven
  suites at M5 (eight now, with diplomacy) including a 200-turn AI stress
  simulation.

### Rules

- An attack consumes all remaining movement. A defender that survives
  counterattacks; a lethal counterattack can kill the attacker.
- Attacking a major civilization at peace automatically declares war.
  Barbarians are always at war and cannot make peace.
- AI war timing is turn 12 + 4 turns per rival; the AI sues for peace when its
  military falls to one unit. Player diplomacy arrives in M6.
- The player's economy/vision are owner-filtered; AI cities still grow and
  produce, but the AI does not research or build workers yet.
- City capture currently has no city HP, ranged attack, siege, or razing.
  Ownership transfers immediately after the garrison/center condition is met.

### M5 acceptance evidence (PPSSPP)

| Acceptance item | Result |
|-----------------|--------|
| Rival civilizations and barbarians | New game logged 13 units, 3 rival cities, and 1 barbarian; rival owners 1–3 and barbarian owner 4 were present |
| AI expansion | AI cities grew from 3 to 14 during a 34-turn run; AI production/queue processing and settler settlement worked |
| Player combat, HP, counterattack | `seed=28482A30`: barbarian attacked the AURELIA garrison at `(26,16)` (damage 30, counter 30, HP 50/70); the player then attacked the adjacent barbarian (`dmg=30 counter=30`), leaving HP 40/20 |
| Unit death | The next barbarian attack triggered a lethal counterattack and logged `attacker_dead=1`; the barbarian was removed |
| Garrison action | Triangle → GARRISON moved the starting warrior into AURELIA (`cost=3 moves_left=3`); the unit stayed selected for the next turn |
| War notices | BOREA declared war on turn 13, CYRENE on turn 17, DRAVEN on turn 21; notices were displayed in the turn message/log |
| Hostile city capture and defeat | Earlier run `seed=4BE1DAA0`: the barbarian captured undefended AURELIA on turn 6 (`BARBARIANS CAPTURED AURELIA`); `game over: result=2` entered the defeat state |
| City capture by the player / victory overlay | Covered by `test_combat` (capture transfer, capture restrictions, victory/defeat conditions); not yet exercised end-to-end in PPSSPP |
| Stability | 34-turn PPSSPP run completed without crash/soft lock; host 200-turn simulation completed (`ai_cities=3->14 battles=101 captures=1 units=55 game_over=2`) |

PPSSPP screenshots remain unavailable through the current WSLg
Wayland/OpenGL capture path; validation used the game log and window geometry.
The final packaged `EBOOT.PBP` still needs a PSP 1000 pass.

## M4.1 (implementation complete, PPSSPP validated)

### Implemented

- `src/improvement.c/.h`: pure improvement rules — worker-only use, movement,
  technology gate, friendly-city ownership, city-center exclusion, existing
  improvement, terrain/resource fit, and instant construction.
- `src/unit.*`: **Worker** (4 move, 2 sight, strength 0, cannot found cities)
  in the 64-unit pool. It uses a generated pickaxe sprite in atlas unit
  column 4.
- `src/data.*`: Worker production entry (cost 10, no technology requirement).
  Farm/Pottery, Mine/Mining, Pasture/Horseback Riding, Quarry/Masonry, and
  Road/The Wheel remain centralized in the improvement tech map.
- Map actions: Triangle on a worker opens **BUILD IMPROVEMENT / WAIT /
  CANCEL**. The build list shows Farm/Mine/Pasture/Quarry/Road with LOCKED
  labels. Selecting a locked improvement shows **REQUIRES TECH**; a successful
  build shows `BUILT <name>`, logs the action, consumes all worker
  movement, and auto-advances.
- Map rendering: golden crop rows for Farm, a dark mine shaft, a pasture
  fence, quarry blocks, and a full-tile road stripe.
- `src/unit.c`: entering a road tile reduces movement cost by 1, minimum 1.
- `tests/test_improvements.c`: worker data/production, tech gating, ownership,
  city-center denial, terrain/resource rules, build/consume-move, yields, and
  road speed. `make test` now runs five suites.

### Rules

- The Worker costs 10 production, has 4 movement and 2 sight, and is always
  available; improvements, not the worker, are technology-gated.
- A worker can improve only a land tile owned by a city of its owner, not a
  city-center tile, and not a tile that already has an improvement. It must
  have at least 1 movement left.
- Farm: grassland/plains/desert. Mine: hills or iron/gems. Pasture: horses.
  Quarry: stone. Road: any land tile.
- Construction is instant and consumes all remaining movement; there is no
  multi-turn work timer yet.
- Cross on a city tile opens the city. Triangle now prefers the unit action
  menu, so a worker or garrison standing in a city can still act.

### M4.1 acceptance evidence (PPSSPP)

Final run: `seed=53ABEAF9 start=34,24`; AURELIA founded at `34,24`.

| Acceptance item | Result |
|-----------------|--------|
| Produce a worker | Queued and rush-bought **Worker** in AURELIA's Production tab (rush cost 20, treasury 20→0); HUD showed `WORKER MOVES 0/4`, then 4/4 on the next turn |
| Show tech-locked improvements | Turn 2: Farm/Mine/Pasture/Quarry/Road all showed **LOCKED**; selecting Farm produced the **REQUIRES TECH** toast and log `unit: build FARM failed ... result=-4` |
| Research Pottery | Pottery (10 science) completed at turn 11; Science tab shows **POTTERY RESEARCHED**, and Farm became unlocked in the build menu |
| Build a Farm | Worker moved from the city center to owned plains `(34,25)` and built a Farm; log `unit: worker built FARM at 34,25`; toast `BUILT FARM`; worker moves `0/4` |
| Display the improvement at 480×272 | Gold crop-row marker visible on the map; tile info shows `IMPROVEMENT FARM` and `UNIT WORKER`; build list/footer fit without overflow |
| Rules coverage | `test_improvements` covers ownership, city-center denial, all five terrain/resource rules, tech locking, movement consumption, yields, and road speed |

Screenshots:
`docs/screenshots/m41_worker_locked.png`,
`m41_requires_tech.png`, `m41_farm_unlocked.png`,
`m41_farm_built.png`, `m41_farm_info.png`, and
`m41_science_researched.png`.

### PSP 1000 validation

M4.1 shipped in the same tagged build as M5. The PSP 1000 smoke test
(`seed=96D58098`) confirmed boot, city management opening/closing, AI turns,
barbarian capture, defeat, and return to menu on hardware. The worker/farm
sequence was validated in PPSSPP (see the acceptance table above); tag `m4.1`
was applied to `6ec3d43`.

## M4 (core complete, PPSSPP validated)

### Implemented

- `src/tech.c` / `tech.h`: 18 technologies across 4 eras (Ancient, Classical,
  Medieval, Renaissance) with costs, prerequisites, and unlock descriptions.
- `src/data.*`: a `required_tech` on every production entry; building and unit
  unlocks (Granary/Pottery, Library/Writing, Market/Currency, Barracks/Bronze
  Working, Walls/Masonry, Workshop/Engineering, University/Iron Working,
  Aqueduct/Construction, Swordsman/Iron Working, Horseman/Horseback Riding);
  improvement unlock mapping (Farm/Pottery, Mine/Mining, Pasture/Horseback,
  Quarry/Masonry, Road/The Wheel).
- `src/unit.*`: new **Swordsman** (5 move, strength 8) and **Horseman**
  (10 move, strength 4) unit types, produced through the city queue.
- `src/world.*`: research state (`researched` bitmask, active tech, progress);
  city science is banked into the active research each turn; completion stops
  research until the player selects the next tech.
- `src/cityview.*`: a fifth **Science** tab with an empire science total,
  active research/progress, a scrollable 18-tech list, RESEARCHED / current
  progress / cost / LOCKED states, and D-pad + Cross selection. Production and
  Buildings lists now show **REQUIRES TECH / LOCKED** and refuse locked items.
- `tests/test_tech.c`: tech data, prerequisite gating, research accumulation,
  completion, unlock data, and new-unit stats. `make test` runs four host
  suites.

### Rules

- Research needs the previous techs: e.g. Writing←Pottery, Bronze
  Working←Mining, Currency←Writing, Iron Working←Bronze Working, Engineering←
  Masonry+Mathematics. Locked techs cannot be selected.
- Each city's science (center 1, Library +2, University +3) is added to the
  active research each turn. Science is also still accumulated as an empire
  total for display.
- A completed tech stops research; overflow is discarded so the next choice is
  deliberate.
- Buildings/units with a tech requirement are visible but greyed and cannot be
  queued until researched, with a REQUIRES TECH toast.
- Improvements are tech-gated in the data table; worker-based improvement
  placement was delivered in M4.1.

### M4 acceptance evidence

| Acceptance item | Result |
|-----------------|--------|
| Research accumulates and completes | PPSSPP: Pottery (10 science) completed at turn 11; log `tech: completed mask=00000002` |
| Prerequisite locking | Writing locked until Pottery; after completion Writing selectable at 20; Iron Working/Currency still locked |
| Locked tech denied | Selecting Gunpowder shows the LOCKED toast |
| Tech unlock gates production | Granary shows REQUIRES TECH before Pottery, then cost 20 after; Buildings shows Granary/Monument unlocked and 7 buildings LOCKED |
| Scrollable tech UI at 480×272 | 18-tech list scrolls to Printing Press/Gunpowder; 5 tabs and footer fit |
| Four eras | Ancient / Classical / Medieval / Renaissance data and era accessors covered by `test_tech` |
| New units | Swordsman/Horseman definitions and production entries covered by host tests |

### PSP 1000 validation (2026-10-20)

The hardware smoke test passed with the final `EBOOT.PBP`:
- Started a new game, founded AURELIA, opened the Science tab, and selected
  Pottery.
- Ran ~10 turns; Pottery completed, Writing unlocked, and Granary became
  buildable.
- Scrolled the full tech list and confirmed the five tabs/footer are readable
  at 480×272.
- Tag `m4` was applied: `m4` on `f6f891e`.

## M3 (complete)

### Implemented

- `src/data.c` / `data.h`: centralized rules table — terrain/resource/
  improvement yields, 20 city names, 9 buildings (with cost, upkeep, effect)
  and 12 production-definition entries.
- `src/city.c` / `city.h`: radius-2 work area (5×5 mask), initial center 3×3
  ownership, citizen assignment (auto + manual work/lock), food/growth,
  production queue (max 5), building/unit completion, border expansion (influence
  plus gold tile purchase), rush purchase, and square city vision covering the
  full work area.
- `src/world.c` / `world.h`: treasury/science, city processing, unit+building
  upkeep, negative-treasury halving of production/science, and a normal 1
  settler / 1 warrior / 1 scout start (replacing the temporary M2 3-settler
  setup).
- `src/cityview.c` / `cityview.h`: full city screen with four tabs
  (Overview / Production / Citizens / Buildings), a 5×5 work grid, citizen
  work/lock/purchase controls, production/building lists, rush purchase,
  unified territory border, gold/science net line, and end-turn event toasts.
- `src/mapview.c` / `mapview.h`: city borders and HUD gold/science; Cross or
  Triangle over a friendly city opens the city screen.
- `tests/test_city.c`: yields, growth/starvation, production, border growth,
  tile purchase, building list, and a 30-turn city simulation.
- M3 strings in `assets/strings.csv`; the version/build/log strings now read
  M3.

### Rules

- City center gives `2F 2P 1G 1S` for free and cannot be worked by a citizen;
  the center tile's terrain/resource is informational. Population works owned
  outer tiles; each citizen consumes 2 food/turn.
- Growth `10 + 5*pop`, food store cap `3*pop`; population max 20.
- Border grows on influence `10 + 5*(owned-9)`; a tile can also be bought for
  `15 + 5*(owned-9)` gold. Buying or border growth reveals the tile at once.
- City vision is square over the entire 5×5 work area, so no workable tile is
  shown as fog.
- Production: 12 options — settler 20, warrior 10, scout 8, and 9 buildings
  (Granary 20/+2F, Monument 15/+1INF, Barracks 25/+1P, Market 25/+4G,
  Library 30/+2S, Walls 20/+2DEF, Workshop 35/+2P, University 45/+3S,
  Aqueduct 40/+2F). Units may be queued twice; a building only once.
- Rush cost `max(5, remaining*2)`.
- Economy: city gold/summed + building upkeep + unit upkeep (`1 per 3 units`,
  rounded up); when the treasury is negative, city production and science are
  halved.
- Market was set to +4 gold after PPSSPP testing showed a one-city economy
  going negative with five buildings and five units; +4 keeps a typical
  30-turn run solvent.
- End-turn city events (growth, starvation, border, production) surface as
  toasts on the city screen.

### M3 acceptance evidence

| Acceptance item | Result |
|-----------------|--------|
| Manage one city for 30 turns | PPSSPP run reached turn 31: `pop=3 owned=12 units=2 treasury=45 science=58`; host 30-turn sim `pop=4 owned=12 buildings=5 units=1 science=52 treasury=186` |
| Correct yields / growth / production | Overview totals match `yield.food/growth_stored`, `yield.production`, `yield.gold`, `yield.science`; host tests assert each |
| All actions controller-reachable | D-pad cursor, Cross work/confirm, Triangle buy/lock, Square auto, L/R tabs, Select end-turn all exercised in PPSSPP |
| Readable at 480×272 | City screenshots show clean text at PPSSPP 480×272 |
| Gold tile purchase | Bought a gems tile `(27,25)` for 15 gold; treasury 20→5; tile revealed |
| Rush purchase | No-gold denied and a successful rush (scout, cost 16) both observed |
| Queue full / removal | QUEUE IS FULL toast and Square removal observed |
| Border expansion | `border expanded to N tiles` logged twice in the 30-turn run |
| 30-turn integration | Tabs, growth, production, market economy, and toasts all shown on screen |

The PPSSPP M3 runs used packaged `civpsp.elf` (and `EBOOT.PBP`); fog corners of
the 5×5 work area are visible, and city tiles reveal when bought/grown.

### PSP 1000 validation (2026-10-20)

The hardware smoke test passed with the packaged `EBOOT.PBP`:
- Booted, started a new game (1 settler / 1 warrior / 1 scout), founded
  AURELIA, and opened the city screen.
- Cycled all four tabs (Overview / Production / Citizens / Buildings).
- Queued 5 buildings, ran ~30 turns, and confirmed growth, borders, positive
  treasury, and readable 480×272 text.
- Tag `m3` was applied: `m3` on `451256a`.

## M2 (complete)

### Implemented

- `src/unit.c` / `unit.h`: settler, warrior, and scout definitions; static
  64-unit pool; spawn/remove/query; half-tile movement costs; 8-way Dijkstra
  pathfinding; turn reset; vision.
- `src/city.c` / `city.h`: static 32-city pool; Chebyshev minimum distance 3;
  land-only founding; population 1; city vision 2; 10 original names.
- `src/world.c` / `world.h`: M2 starting layout, vision union, end turn, unit
  cycling, first-unit-with-moves, and settler consumption on founding.
- `src/mapview.c` / `mapview.h`: cursor selection, reachable preview, path
  movement, action menu, toasts, unit/city rendering, city/unit HUD, info
  panel, and unit/city vision.
- `tests/test_units.c`: costs, mountain/water blocking, no stacking, diagonal
  movement, city founding limits, settler consumption, turn reset, cycling,
  and the start setup.
- Unit and city sprites were added to row 4 of the existing 128×128
  `GU_PSM_5551` terrain atlas; resource transparency is unchanged.
- New M2 strings in `assets/strings.csv`; the version/build/log strings now
  read M2.

### Rules

- Movement: straight 2 points, diagonal 3, +2 entering forest or hills.
  Mountains and water are impassable; one unit per tile; friendly city tiles
  are enterable.
- Unit stats: settler 6 move / sight 2, warrior 6 / 2, scout 8 / 3.
- Cities: minimum Chebyshev distance 3, founded at the settler's tile,
  consumes the settler, population 1, sight 2.
- Turn: Select ends the turn, resets movement, and advances the turn counter.
  L/R cycles units that still have moves. Actions auto-advance to the first
  unit with moves.
- Fog: visibility is the union of all unit and city vision; explored tiles
  persist. The cursor no longer reveals terrain.
- UI: D-pad moves the cursor; its frame is gold normally, green when the
  selected unit can reach it, and red briefly on a denied move. Cross selects
  or moves; Triangle opens Found City / Wait / Cancel; Square toggles info.

### M2 acceptance evidence

| Acceptance item | Result |
|-----------------|--------|
| Found 3 cities | AURELIA at 30,21; CASTRUM at 35,21; PORTA at 32,26; log `cities=3` |
| Move units | 29,21→30,21 cost 2 (4/6 left); 32,23→34,23 cost 4 (4/8 left); city entry 34,23→35,21 cost 7 (1/8 left) |
| 8-way movement | Host tests cover straight 2, diagonal 3, forest/hills +2, and a two-diagonal scout move; the runtime uses the same Dijkstra |
| Cannot cross mountains/water | In-game mountain at 36,21 denied with a full 8/8 budget; host tests isolate mountain/water blocking |
| End-turn loop, no softlock | Turns 2 through 8 logged with `units=3 cities=3`; menus, info, cycling, and moves exercised between turns |
| Unit cycling | PPSSPP q/w logged `unit: cycle prev/next` and moved the selection |
| Context menu | Found City and Wait exercised; Cancel closes the menu; menu captured |
| Fog / vision | Unit and city vision revealed the start area; explored tiles persisted |
| Info / HUD | Tile info reports MOUNTAIN; city info reports AURELIA POP 1 |
| Performance | 59–60fps in PPSSPP |
| Packaged build | `EBOOT.PBP` boots, starts a new game, and ends a turn in PPSSPP |
| PSP 1000 | Seed 8C14D704, start 31,23; 3 cities, turns 2–10, clean quit; no errors |

The PPSSPP acceptance game used seed `B6B15A16`, start `32,23`; the screenshots
are from that run. The PSP 1000 used seed `8C14D704`, start `31,23`.

### PSP 1000 validation (2026-10-20)

The hardware smoke test passed with the packaged `EBOOT.PBP`:

- Boot, main menu, New Game: `seed=8C14D704 start=31,23 units=6`.
- Initial units: scout 31,23; warriors 30,24 and 32,24; settlers 28,21, 34,21,
  and 31,26.
- 28,21→29,21 cost 4 (2/6 left); AURELIA founded at 29,21. CASTRUM at 31,26
  and PORTA at 34,21 followed; log `cities=3 units=3`.
- Turns 2–10 with `units=3 cities=3`; movement, failed moves, selection, and
  cycling exercised between turns; no softlock.
- Clean exit: `map: back to menu` → `menu: quit` → `shutdown` → `log closed`.
- The worn analog stick calibrated to center `165,133` (new-game analog
  `13,2`); the tester reported performance fine.

Validation screenshots: `docs/screenshots/m2_three_cities.png`,
`m2_movement.png`, `m2_blocked.png`, `m2_city_info.png`, and
`m2_action_menu.png`.

### M2 temporary setup

New games start with 3 settlers, 2 warriors, and 1 scout. Settlers spawn at
least 3 tiles apart so the three-city acceptance test does not depend on M3
production. This is a milestone-only setup and must be replaced when city
production is added.

## M1 / M1.1 / M1.2 (complete)

- Deterministic 64×48 map with 9 terrains, 4 resources, 5 improvement slots.
- Largest-connected-landmass keep, guaranteed resources, center-biased start,
  ~72% average land (tests enforce 60–85% and all 9 terrains).
- Three-state fog, pixel camera with clamping/auto-follow, D-pad cursor with
  repeat, calibrated analog panning, tile info panel, HUD and FPS counter.
- 128×128 `GU_PSM_5551` terrain/resource atlas generated by `tools/gen_art.py`
  and packed by `tools/pack_atlas.py`.
- M1 host tests: 32-seed determinism, connectivity, resources, land fraction,
  full terrain mix, and fog invariants.
- PPSSPP and PSP 1000 validation passed for M1, M1.1 (64×48 map), and M1.2
  (boot-time stick calibration, 28-unit deadzone, D-pad priority).

## M0 (complete)

- PSPSDK build, GU 2D renderer, generated English font, input abstraction,
  main menu/options, boot log, XMB icon/background, and exit callback.
- Fixed the PSP 1000 cache-line glyph corruption with 64-byte-aligned vertex
  buffers and D-cache writeback before each draw.
- PSP 1000 validation passed: stable 60fps, correct titles, clean Home exit.

## Known issues / notes

- M5 adds three rival civilizations, barbarians, combat, city capture, AI
  turns, and victory/defeat on top of the M4.1 single-city management layer.
  Player diplomacy, AI research/workers, city HP/ranged attacks/siege, razing,
  and save/load are not implemented yet.
- The M4 tree currently has 18 techs across 4 eras (the plan calls for up to
  30); the structure is ready for more rows. The AI only chooses
  technology-free units/buildings.
- Improvement construction is instant and consumes the worker's remaining
  movement. There is no multi-turn work timer, worker automation, or
  improvement pillaging yet.
- Building upkeep (1–2 gold) plus unit upkeep is tight for a one-city economy;
  Market +4 gold is the intended offset. Avoid overbuilding upkeep until gold
  income supports it.
- Circle on the map with no unit selected returns to the main menu (M0
  behavior); nothing is saved.
- Cross on a friendly city tile opens the city screen directly, even with a
  unit selected. Triangle prefers the unit action menu, so a worker or garrison
  can act while standing in a city. Use Triangle → GARRISON to move an
  adjacent military unit into an empty city.
- Tile purchase allows 8-way adjacency (including diagonal corners), matching
  the border-growth rule; territory can expand corner-to-corner.
- Text is English only; `assets/strings.csv` has an empty `zh` column for the
  later Chinese localization.
- PPSSPP automation: XTest key events reach PPSSPP after
  `XSetInputFocus`; the WebSocket debugger is optional. WSLg OpenGL screenshots
  are unreliable, so use the game log for validation.
- No save/load, audio, or player-facing diplomacy yet. Combat and
  victory/defeat now exist.

## Tests

- `make clean && make`: pass; no compiler, linker, or import warnings.
- `make test`: pass — 13 host suites (including M8 tech/effects/score and M9
  language toggle) plus `tools/check_i18n.py` (485 translations, placeholders,
  glyph coverage, max width) and `tools/check_audio.py` (non-silent WAV/PCM
  assets).
- PPSSPP ELF/EBOOT: M9 Chinese/English UI, Help, map/city/science, diplomacy,
  save/load, tile info, minimap, pause, and score result exercised; M7/M8
  saves load. Audio initialized (`channel=7 44100Hz stereo`), music toggle worked,
  and shutdown reported `audio: shutdown rc=0`.
- PSP 1000: M0, M1, M1.1, M1.2, M2, M3, M4, M4.1, and M5 pass. M6 features
  pass but analog retest is pending; M7, M8, and M9 hardware validation pending.

## Next tasks

1. Run the combined PSP 1000 checklist in `docs/HANDOFF.md` on the final
   `EBOOT.PBP`; do not tag until it passes.
2. If hardware passes, tag `m6` on `ea5f018`, `m7` on `ee02b3a`, `m8` on
   `33eb44c`, and `m9` on `5b4141e`.
3. Post-M9 polish: era-grouped Science tree, multi-turn worker work,
   border-movement gating, resource trading, and a difficulty selector.
