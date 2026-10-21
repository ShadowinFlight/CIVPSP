# Handoff

This file is the recovery point for a new conversation. Keep it short and
current.

## Project

- Path: `/home/zhang/pspGameDev/civpsp`
- Plan: `PROJECT_PLAN.md`
- Current release: **V0.1, first GitHub development release (post-M9.12)**.
  Complete 4X game, Chinese/English UI, 30-tech tree, diplomacy, saves,
  minimap, audio, 12-page manual, civilization colors, menu transitions, and
  release artwork. PPSSPP validated; the latest package still needs a final
  PSP 1000 hardware pass. Internal M6–M9 tags remain pending; M5 is tagged
  `m5`; M4.1 is tagged `m4.1` after its PSP 1000 smoke test.
- Release package: `release/v0.1/`; notes: `UPDATE.md`; public README: `README.md`.
- M0: `71408a2` (tag `m0`)
- M1: `4272caa` (tag `m1` on `b88ffd7`)
- M1.1 map tuning: `3a51f87`
- M1.2 analog fix: `ddbfb87`
- M2 implementation: `820fffe` (tag `m2` on `75652d5`)
- M3 implementation: `451256a` (tag `m3`)
- M4 implementation: `f6f891e` (tag `m4`)
- M4.1 implementation: `6ec3d43` (tag `m4.1`)
- M5 implementation: `bd292b2` (tag `m5`)
- M6 implementation: `68d8d78` (tag `m6` pending analog hardware retest)
- Analog stabilization: `ea5f018`
- M7 implementation: `ee02b3a` (tag `m7` pending PSP 1000 smoke test)
- M8 implementation: `33eb44c` (tag `m8` pending combined PSP 1000 test)
- M9 implementation: `5b4141e` (tag `m9` pending combined PSP 1000 test)

## Build / run

```bash
export PATH=$PATH:/home/zhang/pspdev/bin
cd /home/zhang/pspGameDev/civpsp
make clean && make
make test          # 13 host suites; no PSP required
```

- `civpsp.elf` for PPSSPP.
- `EBOOT.PBP` for `ms0:/PSP/GAME/CIVPSP/`.

PPSSPP requires an absolute path:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 \
  /home/zhang/pspGameDev/ppsspp/PPSSPP-v1.20.4-anylinux-x86_64.AppImage \
  --windowed --graphics=software /home/zhang/pspGameDev/civpsp/civpsp.elf
```

Use the absolute path to `EBOOT.PBP` to smoke-test the packaged build.

## What works

### M9 (implementation complete, PPSSPP validated, PSP 1000 pending)

- `tools/gen_font.py` / `tools/pack_atlas.py` / `src/gen/font_zh.h`: 512×256
  RGBA Chinese atlas with 548 glyphs. ASCII uses 8px advance, CJK 16px;
  Chinese uses Noto Sans CJK SC Medium at 14px. `src/text.c` decodes UTF-8,
  looks up glyphs by binary search, and measures variable-width text.
- `tools/gen_strings.py`: emits English and Chinese tables,
  `Str_SetLanguage`/`Str_GetLanguage`, and fallback. All 485 string IDs are
  translated in `assets/strings.csv`; `tools/check_i18n.py` validates
  placeholders, glyph coverage, and widths (max 368px).
- `src/render.c`: selects the English or Chinese font texture per frame;
  both atlases are cache-written at boot.
- `src/app.c`: Chinese is the default; Options has Language and Music rows
  with Left/Right/Cross toggles. All menu title and list spacing adjusted for
  16px Chinese lines; pause/new-game/save/load/help titles no longer collide.
- `src/cityview.c`: production list 4 rows/18px, science list 6 rows/18px,
  citizens 18px steps, buildings effect column shifted left.
- `src/diplomacyview.c`: 34px top bar, 56px rival rows, 18px detail lines.
- `src/mapview.c`: 17px tile-info rows, 18px unit/city detail lines.
- Civilization sprite variants: `tools/gen_art.py` writes five owner rows
  (player/Borea/Cyrene/Draven/barbarian) into a 128×256 GU_PSM_5551 atlas.
  Team color replaces the settler tent, warrior shield, scout cloak, city
  flag/gate banner, and worker shirt; `src/mapview.c` selects the row from
  `owner` and draws a 20×3 owner bar under each unit/city.
- Map connectivity: `src/map.c` now treats mountains as barriers for the
  walkability flood fill, keeps the largest passable component, and carves the
  shortest mountain pass to each isolated pocket. Forest/hills/desert/tundra
  remain passable; unreachable pockets that cannot be connected become
  mountains. `tests/test_map.c` checks 100 seeds and all passable tiles
  reachable from the start.
- Minimap: `src/mapview.c` draws a 114×86 dark/gold panel at the map's
  bottom-right, with no title bar. A 64×64 GU_PSM_5551 texture (8KB) holds
  explored/visible terrain, city/unit markers, cursor, and camera viewport;
  `Render_DrawMinimap()` in `src/render.c` draws it as one quad. A compact
  `小地图 L+R` button is shown while collapsed; L+R toggles the panel. No save
  changes.
- Audio: `tools/gen_audio.py` synthesizes an 8s ambient loop and eight SFX;
  `src/audio.c` uses a standard **44100Hz stereo `sceAudioChReserve` channel**,
  duplicates 22050Hz source frames, writes back the mix buffer before hardware
  output, and mixes on a dedicated thread (`-lpspaudio`). The generator had a
  real silent-PCM bug (`int(0.x)`); normalization now scales to 32767 and
  targets 80% peak. Music uses 100% gain (about 80% output peak) and ducks to
  20% under SFX; `tools/check_audio.py` guards WAV peaks. Options → 音乐 toggles music;
  UI/turn/build/found/combat/research SFX are wired.
- `ICON0.PNG` / `PIC1.PNG`: rebuilt with gradient/compass/city-skyline art,
  Chinese taglines, and `M9 EDITION` / `M9 BUILD`.
- Menu transitions: `src/app.c` fades each state change over 20 frames, slides
  menu rows in from the right with an index stagger, and flashes an 8-frame
  gold glow around the newly selected row. `menu_slide()` and
  `selection_glow_color()` are shared by menu/value/slot rows.
- M9.11 balance: applied the user-edited `docs/GAME_DATA.md` values to
  `src/data.c` / `src/city.h`: terrain gold, richer improvements, building
  bonuses, and food-per-population 3. The in-game manual page 10/11 was
  updated and host tests now assert the new values.
- M9.12 city UI: `format_effect()` now concatenates all building bonuses using
  compact F/P/G/S/I/D labels; tile grid/detail includes improvement science.
- In-game manual: `HELP_PAGE_COUNT` is 12; `help_pages[]` covers goals,
  controls, turns/fog, cities, citizens/borders, science, units/combat,
  diplomacy, victory/score, buildings, unit/terrain stats, and settings/tips.
  The main-menu and Pause labels read `游戏手册`.
- `tests/test_i18n.c`, `tools/check_i18n.py`, and `tools/check_audio.py`; `make test` runs 13 suites
  plus the i18n and audio checks.
- PPSSPP: EBOOT log `CIVITAS PSP M9`; Chinese main menu, Options, 12-page manual,
  map HUD, tile info, minimap, city overview/production/citizens/buildings/
  science, pause, save/load, diplomacy/action menu, and score result; English
  toggle; M8 save loading; player/Borea/Cyrene/Draven color variants on
  revealed test saves. Audio reserves a standard 44100Hz channel and logs no
  output errors; WAV peaks are non-zero and music on/off was verified in
  Options. Menu transition mid-fade/glow captures and manual pages 1/12
  verified. A 2000-seed sweep
  found 0 isolated walkable components after the map connectivity fix. No
  missing glyphs or overlaps in captured screens.
- **PSP 1000 combined validation pending.** Pay attention to Chinese clarity,
  the 1MB font atlas, the ~2MB EBOOT/audio thread, and minimap readability.

### M8 (historical; implementation complete, PPSSPP validated, PSP 1000 pending)

- `src/tech.*`: 30 techs (8 Ancient, 8 Classical, 7 Medieval, 7 Renaissance)
  with appended enum values so M7 save bits stay valid. `TechDef` now carries
  a data-driven effect and value.
- `src/city.*`: tech-aware yields/processing (`City_ComputeYieldsWithTech`,
  `City_ProcessTurnWithTech`, `City_TurnsLeftWithTech`,
  `City_DefenseWithTech`) for farms, libraries, universities, markets,
  workshops, barracks, walls, per-city gold/science, and research percentage.
- `src/world.*`: derived AI research (`World_CivResearched`, one tech per 8
  turns from the turn counter), AI tech count in score, three science goals
  (Astronomy / Scientific Method / Navigation), and
  `World_FinishScoreGame` for standard turn-limit scoring.
- `src/ai.c`: AI fields advanced units and tech-gated buildings using its
  derived research.
- `src/app.c`: four-page Help from main menu and Pause; standard mode ends
  after turn 200; autosave and turn-limit handling also apply when a turn is
  ended from the city screen.
- `src/mapview.*`: bottom-bar `TURN n/200` in standard mode, `TIME MM:SS` in
  timed mode; top HUD spacing adjusted for 480×272.
- `src/cityview.c`: 30-tech scrolling list with corrected selection bar and a
  selected-tech effect line; Science tab shows `PROJECTS n/3`.
- `assets/strings.csv`: Help, new tech names/effects, `GAME LIMIT`,
  `TURN LIMIT REACHED`, and M8 boot/version strings (422 total).
- `tests/test_m8.c` covers era distribution, prereqs, effects, research
  stacking, derived AI research, advanced AI units, science goals, and standard
  score victory/defeat. `tests/test_ai.c` adds a 320-turn stability run.
  `tests/test_m7.c` now tests the three-goal science victory. `make test` runs
  12 suites.
- PPSSPP: Help pages 1–4; Pause Help return; 30-tech Science scrolling
  (`MILITARY SCIENCE`, `BARRACKS +1 PRODUCTION`); Standard `seed=6FB11BD0`
  showing `TURN 1/200`; an M7-format slot loading at turn 2; and a synthetic
  turn-200 standard save ending from the city screen with
  `turn limit: turn=201 score=41 rival_best=0 result=1`, result `TURNS 200`,
  and an autosave at turn 201.
- **PSP 1000 combined validation pending.** Use the checklist below.

### M7 (historical; implementation complete, PPSSPP validated, PSP 1000 pending)

- `src/app.c`: New Game setup with Standard/Timed choice and 30/60/90-minute
  timed limits (default 60); wall-clock accumulation only in MAP/CITY/
  DIPLOMACY states; timed HUD; Pause menu (Resume, Save, Load, Calibrate
  Stick, Main Menu); main-menu Load; Options calibration; result screen and
  return to menu.
- `src/save.*`: version-2 `SaveHeader` + raw `Map`/`World`/`SaveView`
  snapshot, FNV-1a checksum, magic/version/size/mode checks, structural
  validation, metadata reads for the load menu, and safe rejection of empty,
  short, corrupt, or incompatible files. Files are 17,460 bytes under
  `ms0:/PSP/SAVEDATA/CIVPSP/{auto,slot1,slot2,slot3}.sav`.
- Autosave runs after every completed turn. Manual slots 1–3 are available
  from Pause → Save Game; Load lists autosave plus all three slots from both
  the pause menu and main menu. Metadata shows turn, score, and timed
  remaining time.
- `src/world.*`: player/rival scores, best-rival ranking, science-victory
  check, and `WORLD_VICTORY_SCORE` with result/defeat type. Score counts
  population ×6, cities ×10, owned territory, buildings ×5, units,
  researched techs ×10, and positive treasury/20.
- `src/mapview.*`: timed clock display, pause request, autosave request, and
  `MapView_Restore` preserving cursor, selected unit, and camera.
- `src/analog.*` / `src/input.*`: 15-sample median calibration at boot/New
  Game, adaptive 32–56 deadzone, convergent low-pass filter, minimum output
  threshold, and automatic recenter after 45 zero-output frames; manual
  recalibration is exposed in Options and Pause.
- `tests/test_save.c` and `tests/test_m7.c`; `make test` runs 11 suites.
- PPSSPP: standard run `seed=D37B3B67 start=32,22` wrote `auto.sav`, saved
  slot 1, and loaded it; timed run `seed=A3D034B0 start=32,24` logged
  `mode=timed limit=3600`, saved `remaining=3592 play=8`, and loaded at
  `time=3592`. A forced 1-second timed save produced
  `time up: score=24 rival_best=67 result=2`, the score-defeat result screen,
  and clean shutdown.
- **PSP 1000 smoke test pending.** Do not tag `m7` until the packaged
  `EBOOT.PBP` passes.

### M6 (features pass PSP 1000; analog retest pending before tag)

- `src/world.*`: symmetric -100..100 relations, five bands, NAP/open
  borders/alliance masks, separate NAP and open-borders 10-turn counters,
  trade counters, gift counts, AI treasuries, proposals, alliance-victory
  countdown, and victory type. War/capture clear agreements and apply relation
  penalties.
- `src/diplomacy.*`: seven actions (war, peace, NAP, open borders, alliance,
  trade, gift); relation/war/treaty/army/treasury acceptance checks; gifts
  cost 10, gain +12 falling by 2 to +4, halved at war; AI pays 2 gold/turn for
  10 trade turns; AI proposals and accept/reject; AI-AI peace/war; diplomatic
  victory (all living rivals allied for 10 turns, at least 2 rivals, no AI-AI
  war).
- `src/diplomacyview.*`: 480×272 diplomacy screen with rival list, relation
  bar/band, treaty/trade/army/proposal details, action popup, and messages.
  Start opens it; Circle returns to the map.
- `src/ai.c`: rivals respect relations/treaties when declaring war and use the
  diplomacy peace check.
- `src/combat.*`: attacks/captures blocked by NAP/alliance return
  `COMBAT_ERR_TREATY`; the map toast is `TREATY BLOCKS WAR`.
- `src/mapview.*`: Start opens diplomacy; treaty-blocked combat toast.
- Open borders/alliances share vision of the partner's units/cities. Physical
  border movement is not gated yet. Trade is gold-only (resources deferred).
- `tests/test_diplomacy.c`: relations, NAP, gifts, peace, proposals, victory
  countdown/reset, treaty-blocked combat, open-borders vision/expiry, trade
  payments/expiry, and AI-AI-war reset. `make test` runs eight suites.
- PPSSPP final build (`seed=C6559DA5 start=35,24`): two gifts (20→10→0 gold,
  relation 0→12→22), open borders, trade, and NAP; trade paid 2 gold for all
  10 turns; timed treaties expired (`mask=3`); BOREA proposed a NAP on turn 10
  and accepting it logged `relation=34->39`; stable with a garrisoned capital.
- PSP 1000 feature smoke passed: relations, gifts, treaties, trade, proposals,
  and diplomacy screens worked; the only issue was camera drift from the worn
  analog stick.
- Analog stabilization (`ea5f018`) is implemented and host/PPSSPP tested.
  Do not tag `m6` until a hardware retest confirms the drift is gone.

### M5 (implementation complete, PPSSPP validated)

- Civ owners: AURELIA (0), BOREA (1), CYRENE (2), DRAVEN (3), BARBARIANS (4).
  `World_Begin` spawns the player start, three rival capitals, and a raider;
  `World_Init` is still a player-only fixture used by older host tests.
- `src/combat.*`: 100 HP, damage `30 + (attack-defense)*5` clamped 5..95,
  forest/hills +1 defense, city `City_Defense`, counterattack, death,
  auto-war, movement consumption, empty-center city capture.
- `src/ai.*`: rivals expand to 4 cities, choose technology-free production,
  A* move, attack/capture, declare war after turn 12 and make peace when weak.
  Barbarians spawn every 8 turns (max 4) and are always hostile.
- `src/world.*`: five-civ state, war masks/notices, player-only
  economy/vision, healing, capture, victory/defeat, and synchronous AI.
- `src/mapview.*`: player-only selection, owner colours, HP bars, red
  attack/capture cursor, enemy info, attack/capture, GARRISON action, notices.
- `src/app.c`: victory/defeat overlay and return-to-menu.
- `tests/test_combat.c`: damage/counter/death, defense, auto-war, capture, and
  victory/defeat. `tests/test_ai.c`: starts, expansion, war/peace, 200-turn
  simulation. `make test` ran seven suites at M5 (eight now).
- PPSSPP evidence (`seed=28482A30 start=35,24`): 13 units/3 AI cities/1
  barbarian; AURELIA founded and warrior garrisoned; barbarian attack at
  `(26,16)` (30/30 damage, HP 50/70); player attack (30/30, HP 40/20); next
  barbarian attack died to the counter; AI reached 14 cities; BOREA/CYRENE/
  DRAVEN war notices on turns 13/17/21; 34 turns stable.
- Earlier run (`seed=4BE1DAA0`): undefended AURELIA was captured on turn 6 and
  `game over: result=2` entered defeat.
- City capture by the player and the victory overlay are covered by host
  tests but not yet exercised end-to-end in PPSSPP.
- **PSP 1000 smoke passed** (`seed=96D58098 start=29,21`): 13 units/3 AI
  cities/1 barbarian; AURELIA founded at `29,21`; city screen opened and
  closed; AI turns ran; barbarians captured the undefended capital on turn 5;
  `game over: result=2`; return to menu. Tags `m4.1` and `m5` applied.

### M4.1 (implementation complete, PPSSPP validated)

- `src/improvement.*`: worker-only validation, tech gate, friendly-city
  ownership, city-center denial, existing-improvement denial, terrain/resource
  rules, and instant construction that consumes all movement.
- `src/unit.*`: Worker (4 move, 2 sight, strength 0, cannot found cities),
  atlas unit column 4 (pickaxe sprite). Roads reduce entry cost by 1, minimum 1.
- `src/data.*`: `PROD_WORKER` costs 10 and requires no tech.
- `src/mapview.*`: Worker action menu (Build/Wait/Cancel), scroll-free
  5-entry build list with LOCKED labels, REQUIRES TECH and BUILT messages,
  golden farm rows / mine / pasture / quarry / road markers, and unit-first
  Triangle handling on city tiles.
- `tests/test_improvements.c`: worker data/production, tech/ownership rules,
  city-center denial, terrain/resource rules, build/move consumption, yields,
  and road speed. `make test` runs five suites.
- PPSSPP final run (`seed=53ABEAF9 start=34,24`): Worker produced in AURELIA;
  all five improvements LOCKED at turn 2; Farm attempt result=-4 and REQUIRES
  TECH toast; Pottery completed turn 11; Farm built at `34,25` with log
  `unit: worker built FARM at 34,25`; tile info reports IMPROVEMENT FARM.
- Screenshots: `docs/screenshots/m41_worker_locked.png`,
  `m41_requires_tech.png`, `m41_farm_unlocked.png`, `m41_farm_built.png`,
  `m41_farm_info.png`, `m41_science_researched.png`.

### M4 (core complete, PPSSPP validated)

- `src/tech.*`: 18 techs over 4 eras, costs/prereqs/unlock strings.
- `src/data.*`: `required_tech` on all production defs; tech-gated buildings
  and units; improvement unlock mapping.
- `src/unit.*`: Swordsman (5/2, strength 8) and Horseman (10/2, strength 4).
- `src/world.*`: researched bitmask, active research, per-turn science
  investment, completion (stops until the next choice).
- `src/cityview.*`: 5th Science tab (empire science, active progress, scrollable
  18-tech list, RESEARCHED/cost/LOCKED), D-pad+Cross selection; production and
  buildings lists show REQUIRES TECH / LOCKED and refuse locked items.
- `tests/test_tech.c`: tech data, prereqs, research flow, unlock data, units.

### M3 (complete)

- `src/data.*`: terrain/resource/improvement yields, 20 city names, 9 building
  definitions (Granary 20/+2F, Monument 15/+1INF, Barracks 25/+1P, Market
  25/+4G, Library 30/+2S, Walls 20/+2DEF, Workshop 35/+2P, University 45/+3S,
  Aqueduct 40/+2F; upkeep 1–2) and 12 production entries.
- `src/city.*`: radius-2 work area (5×5 mask from `CITY_CENTER_INDEX 12`),
  initial center 3×3 ownership, citizen auto/manual/locked assignment, food +
  growth + starvation, production queue (max 5, no duplicate buildings),
  border growth `10 + 5*(owned-9)`, gold tile purchase
  `15 + 5*(owned-9)`, rush `max(5, remaining*2)`, and **square** city vision
  covering the whole work area (so 5×5 corners are never black fog).
- `src/cityview.*`: four-tab city screen (Overview / Production / Citizens /
  Buildings), 5×5 grid at 36px, unified ownership outline, citizen
  work/lock/purchase, production/building lists, rush, NET gold/science line,
  and end-turn event toasts.
- `src/world.*`: treasury/science, `World_ProcessCities` (gold/science sum,
  building+unit upkeep, halving when broke), normal **1 settler / 1 warrior /
  1 scout** start (replaces the M2 3-settler setup).
- `tests/test_city.c`: yields, growth/starvation, queue/rush, tile purchase &
  border reveal, and a 30-turn city simulation.

### M2 (complete)

- `src/unit.*`: settler 6/2, warrior 6/2, scout 8/3; 64-unit static pool;
  spawn/remove/query; straight 2, diagonal 3, +2 forest/hills; mountains/water
  impassable; one unit per tile; friendly city entry; 8-way Dijkstra bounded
  by remaining points; turn reset; vision.
- `src/city.*`: 32-city pool; land-only founding; Chebyshev minimum distance
  3; population 1; vision 2; 10 names.
- `src/world.*`: 3 settlers + 2 warriors + 1 scout start (temporary M2 test
  setup), settlers at least 3 apart; vision union; turn advance; unit cycling;
  founding consumes the settler.
- `src/mapview.*`: cursor selection, gold/green/red cursor, path movement,
  Found City / Wait / Cancel menu, toast errors, units/cities, city/unit HUD,
  tile info, unit/city vision.
- Fog: explored persists; visible = union of unit and city sight. The cursor
  is no longer a vision source.
- Art: unit and city sprites in terrain atlas row 4 (`128×128 GU_PSM_5551`).
- Select = End Turn; L/R = cycle units with moves; Triangle = actions; Square
  = info; Circle = close/deselect/back.
- Start of turn auto-advance after Wait/Found selects the first unit with
  moves; if none, “PRESS SELECT TO END TURN”.

### M1 / M1.1 / M1.2 (complete)

- Deterministic 64×48 map, 9 terrains, 4 resources, 5 improvement slots,
  largest-landmass keep, guaranteed resources, three-state fog, pixel camera,
  cursor, calibrated analog pan, tile info, HUD/FPS.
- Host map tests across 32 seeds (determinism, connectivity, resources, land
  fraction, terrain mix, fog).
- PSP 1000 validation passed.

### M0 (complete)

- Main menu, options placeholder, quit, English bitmap font, boot log, XMB
  assets, 60fps, cache-safe text vertices, clean Home exit.

## M4.1 validation evidence

- PPSSPP final run: `seed=53ABEAF9 start=34,24`, AURELIA at `34,24`.
- Worker queued/rushed (rush cost 20, treasury 20→0); HUD `WORKER MOVES 0/4`,
  then 4/4 after ending the turn.
- Turn 2 build menu: Farm/Mine/Pasture/Quarry/Road all LOCKED; selecting Farm
  logged `unit: build FARM failed at 34,24 result=-4` and showed REQUIRES TECH.
- Pottery completed at turn 11 (SCI 10); Science shows POTTERY RESEARCHED and
  Farm is unlocked in the build list.
- Farm built on owned plains `34,25`; log `unit: worker built FARM at 34,25`,
  toast BUILT FARM, worker moves 0/4; tile info shows IMPROVEMENT FARM, UNIT
  WORKER.
- `make test` passes five suites; clean build has no warnings.
- PSP 1000 smoke test passed as part of the tagged M5 build (city management
  opened/closed on hardware). Worker/farm remains validated in PPSSPP; tag
  `m4.1` applied to `6ec3d43`.

## M4 validation evidence

- PPSSPP: Pottery (10 science) completed at turn 11;
  `tech: completed mask=00000002`; Writing selectable (20), Bronze Working
  locked.
- Granary: REQUIRES TECH before Pottery, cost 20 after; Buildings shows only
  Granary/Monument unlocked.
- Locked Gunpowder selection shows LOCKED; tech list scrolls to Printing
  Press/Gunpowder at 480×272; 5 tabs fit.
- `make test` passes `test_map`, `test_units`, `test_city`, `test_tech`; clean
  build has no warnings.
- PSP 1000: Science tab, Pottery completion, Writing/Granary unlock, and tech
  list scrolling all verified on the final EBOOT; tag `m4`.

## M3 validation evidence

- PPSSPP reached turn 31: `pop=3 owned=12 units=2 treasury=45 science=58`
  with a Granary/Market/Library/Monument/Barracks queue.
- Host 30-turn sim: `pop=4 owned=12 buildings=5 units=1 science=52
  treasury=186`; all city tests pass.
- Exercised in PPSSPP: founding AURELIA, all four tabs, center-info
  (2F/2P/1G/1S + OWNED CITY CENTER), citizen work/unwork/auto/lock, buying an
  unowned gems tile for 15 gold, rush no-gold denial + successful scout rush,
  queue-full toast, Square removal, production-list scrolling, and border
  expansion twice in one run.
- City fog is square 5×5, so bought/grown tiles immediately appear.
- Final clean build: no compiler/linker/import warnings; `make` and `make test`
  both pass.

## M2 validation evidence

- Seed `B6B15A16`, start `32,23`.
- Units: scout 32,23; warriors 31,24 and 33,24; settlers 29,21, 35,21, 32,26.
- 29,21→30,21 cost 2, 4/6 left; AURELIA at 30,21.
- CASTRUM at 35,21; PORTA at 32,26; `cities=3 units=3`.
- 32,23→34,23 cost 4, 4/8 left; entry into CASTRUM 34,23→35,21 cost 7, 1/8.
- Mountain 36,21 denied at 8/8; tile info MOUNTAIN. Host tests isolate water
  and mountain blocking (no water near this start).
- Turns 2–8 stable; no softlock. q/w cycle logs for L/R.
- 59–60fps. EBOOT boot/New Game/end-turn smoke passed in PPSSPP.
- PSP 1000 (seed `8C14D704`, start 31,23): six units; AURELIA at 29,21,
  CASTRUM at 31,26, PORTA at 34,21; turns 2–10 with `units=3 cities=3`; clean
  `map: back to menu` → `menu: quit` → `shutdown` → `log closed`. Worn stick
  calibrated to `165,133`; tester reported performance fine.
- Screenshots: `docs/screenshots/m2_three_cities.png`,
  `m2_movement.png`, `m2_blocked.png`, `m2_city_info.png`,
  `m2_action_menu.png`.

## Known issues

- M7 saves are raw version-2 struct snapshots and only guarantee compatibility
  with the same build; there is no migration path yet. Save overwrite has no
  confirmation and autosave is overwritten every turn.
- Science victory uses all 18 currently available techs; the planned 30-tech
  tree remains future work. Score victory is evaluated only when a timed game
  expires. AI civilizations do not research, so their scores have no tech
  component.
- The save files are plain files under `SAVEDATA` (no `PARAM.SFO`/save icon),
  so the PSP system save manager may not display them.
- City HP/ranged attacks/siege, razing, AI research, and AI workers remain
  unimplemented. AI expansion/combat is intentionally light.
- The tech tree has 18 of the planned 30 techs across 4 eras; adding rows is
  data-only.
- Improvement construction is instant and consumes all worker movement. No
  multi-turn work timer, worker automation, or pillaging yet.
- Building upkeep (1–2) plus unit upkeep is tight for a one-city economy;
  Market +4 is the intended offset.
- Map Circle with no unit selected opens the pause menu; Circle again
  resumes. A selected unit is deselected first.
- Cross on a city tile opens the city screen; Triangle prefers the unit action
  menu, so workers/garrisons can act while standing in a city.
- Tile purchase/border uses 8-way adjacency (diagonal corners allowed).
- Water blocking is still only host-tested (no guaranteed water near start).
- M9 ships Chinese by default with an English toggle in Options; the
  translation/font is generated from `assets/strings.csv`.
- Under WSLg, XTest keyboard input works after `XSetInputFocus` on the current
  `CIVITAS` window (`xwininfo -root -tree`; the ID and 960×532/544 size change
  per launch). The PPSSPP WebSocket debugger can get stuck in `CLOSE_WAIT`, so
  XTest is the reliable automation path.
- City capture by the player and the victory overlay are host-tested but not
  yet exercised end-to-end in PPSSPP.
- Audio is synthesized and mixed in software; volume/quality and the audio
  thread need a PSP 1000 listening pass. Trade is gold-only, open borders share
  vision rather than gating movement, and NAP/open borders have no renewal UI.
  M8 AI research is a derived one-tech-per-8-turns abstraction; science
  projects are three late techs; the Science list is flat rather than
  era-grouped. The minimap is collapsed by default with an L+R shortcut and a
  visible button; no settings-menu toggle yet.

## Next tasks

1. Run the combined PSP 1000 checklist below on the packaged `EBOOT.PBP`.
2. If it passes, tag `m6` on `ea5f018`, `m7` on `ee02b3a`, `m8` on `33eb44c`,
   and `m9` on `5b4141e`. If hardware exposes an issue, fix it and repeat the
   host/PPSSPP gates.
3. Post-M9 polish: era-grouped Science tree, worker work timer,
   border-movement gating, resource trading, and release packaging.

### Combined PSP 1000 checklist (analog + M6–M9)

1. Boot the packaged `EBOOT.PBP`; confirm the log version is M9, the main menu
   is in Chinese, and the map camera does not drift with the stick released.
   Use Pause → 校准摇杆, then confirm no crawl.
2. Main menu → 游戏手册: verify all 12 pages with LEFT/RIGHT and Circle
   returns; check pages 1, 6, 9, and 12 for readability.
3. Options → 语言: switch between 中文 and 英文, then switch back. Confirm
   both fonts render and menus fit. Toggle 音乐 on/off and confirm the ambient
   track starts/stops while SFX remain.
4. Start a Standard game. Confirm the map bottom shows `回合 1/200`, the
   bottom-right `小地图 L+R` button appears, L+R opens a 114×86 minimap with
   explored terrain/city markers/camera viewport, and there is no timed clock.
   Observe the short fade/slide when entering menus and the gold selection
   glow while moving through Options/Pause.
5. Found a city, open it, and check all five tabs. In 科研, scroll through the
   30-tech list; confirm the selection bar/detail line and `项目 0/3`. Select a
   tech and end a turn from the city screen; confirm autosave.
6. M6: open diplomacy with Start; send a gift, sign a NAP, open borders, trade,
   and (if relation permits) an alliance; run 10 turns and confirm trade
   payments/treaty expiry.
7. M7: Pause → 保存游戏 → slot 1; load slot 1; load from the main menu.
   Confirm turn/score/time metadata. Pause → 游戏手册 uses the same 12-page
   manual.
8. M8: load an old M7/M8 save (slot 2 or 3 if present). Confirm it loads and
   the M9 UI appears.
9. Standard ending: the turn-200 score path is already validated in PPSSPP
   with a turn-200 save. On hardware, confirm `回合 n/200` and end a normal
   turn to verify autosave. A full 200-turn run is optional.
10. From the PSP XMB, inspect the ICON0/PIC1 artwork (if installed in
    `PSP/GAME/CIVPSP/`); confirm the new gradient/compass/skyline art appears.
11. Quit to menu and power off cleanly; report any crash, soft lock, visual
    overflow, missing Chinese glyph, audio glitch, or input lag.

## Key files

- `src/main.c` — boot, callbacks, main loop.
- `src/app.c` — menu/map/city/diplomacy/pause/save/load/results state machine,
  New Game setup, timed clock, save orchestration.
- `src/mapview.c` / `mapview.h` — cursor, path movement, action menu, entities,
  HUD, info panel, fog rendering, explored-range minimap.
- `src/unit.c` / `unit.h` — stats, costs, Dijkstra, vision, road movement.
- `src/improvement.c` / `improvement.h` — worker improvement validation and
  instant construction.
- `src/city.c` / `city.h` — yields, growth, queue, production, borders,
  purchase, square vision.
- `src/cityview.c` / `cityview.h` — city screen tabs, grid, citizens, queues.
- `src/data.c` / `data.h` — central rules: yields, buildings, production,
  tech requirements.
- `src/tech.c` / `tech.h` — 30-tech tree, eras, prerequisites, unlock text,
  and data-driven tech effects.
- `src/combat.c` / `combat.h` — M5 damage, defense, counterattack, capture,
  treaty checks; M8 tech-aware city defense.
- `src/ai.c` / `ai.h` — M5 rivals/barbarians/expansion/A*/wars plus M8 derived
  research-driven units and buildings.
- `src/diplomacy.c` / `diplomacy.h` — M6 actions, gifts, trade, proposals,
  AI-AI diplomacy, relation decay, diplomatic victory.
- `src/diplomacyview.c` / `diplomacyview.h` — M6 480×272 diplomacy screen.
- `src/world.c` / `world.h` — civs, start layout, turns, war, capture,
  economy/upkeep, victory/defeat, vision, relations/treaties/trade/AI
  treasuries, M7 score, M8 derived AI research, three science goals, and the
  standard turn-limit score ending.
- `src/map.c` / `map.h` — map generation and fog helpers.
- `src/input.c` / `input.h` — edge/repeat/held queries; all return booleans,
  so read one direction per call. `src/analog.c` / `analog.h` handles stick
  calibration and drift suppression.
- `src/save.c` / `save.h` — M7 versioned save/load, validation, checksum.
- `src/gen/strings_gen.*`, `src/gen/font_en.h`, `src/gen/font_zh.h`,
  `src/gen/terrain_gen.h` — generated localized strings and atlases.
- `src/text.c` / `text.h` — English fixed-cell and Chinese UTF-8 variable-width
  text rendering/measurement.
- `src/audio.c` / `audio.h` / `src/gen/audio_gen.h` — SRC audio thread, mixer,
  and generated PCM arrays.
- `assets/src/audio/*.wav` — Python-generated preview SFX/ambient;
  `tools/check_audio.py` verifies they are non-silent.
- `assets/strings.csv` — 485 English/Chinese UI strings.
- `tools/gen_art.py`, `tools/gen_font.py`, `tools/gen_audio.py`,
  `tools/pack_atlas.py`, `tools/gen_strings.py`, `tools/check_i18n.py` —
  assets/translation/audio pipeline.
- `tests/test_map.c`, `tests/test_units.c`, `tests/test_city.c`,
  `tests/test_tech.c`, `tests/test_improvements.c`, `tests/test_combat.c`,
  `tests/test_ai.c`, `tests/test_diplomacy.c`, `tests/test_analog.c`,
  `tests/test_save.c`, `tests/test_m7.c`, `tests/test_m8.c`,
  `tests/test_i18n.c` — host tests. `tests/log_stub.c` makes AI/diplomacy
  diagnostics linkable on the host.
- `docs/GAME_DATA.md` — complete unit/building/terrain/tech/economy data sheet.
- `docs/screenshots/m2_*.png`, `m3_*.png`, `m4_*.png`, `m41_*.png`,
  `m8_*.png`, `m9_*.png` — validation captures.

## Decisions

See `docs/DECISIONS.md` for the full history. Current M9 decisions:

- Chinese is the default UI language; English remains selectable in Options.
- Strings are generated in both languages with English fallback; the CSV
  `zh` column is the translation source.
- Chinese uses a separate 512×256 RGBA atlas with variable-width glyphs
  (8px ASCII, 16px CJK); English keeps the original 8×14 atlas so existing
  English layouts remain stable.
- Only glyphs used by the string table plus printable ASCII are packed, and
  Chinese uses a 14px Noto Sans CJK SC face to keep descenders inside 16px
  cells.
- Chinese layout uses 16–18px line spacing and adjusted list sizes rather than
  scaling the whole UI; the science list is 6 rows, production 4 rows, and
  diplomacy rows 56px.
- `ICON0.PNG`/`PIC1.PNG` are regenerated for M9 with Chinese taglines and
  `M9 BUILD`.
- M8 tech/AI/science/turn-limit decisions remain in force; saves are
  unchanged.

### Historical M8 decisions

- M8 appends 12 techs after the original 18 enum values so version-2 M7 save
  bit positions remain valid; `SAVE_VERSION` and `World`/`Map` layouts stay
  unchanged.
- Technology effects are one small data-driven effect per tech, applied in
  city/world yields rather than special-cased UI.
- Science victory uses three late techs (Astronomy, Scientific Method,
  Navigation) as stand-ins for research projects.
- AI research is derived from the turn counter (one tech per 8 turns) instead
  of saved per-civ state; this keeps save compatibility and deterministic
  behavior.
- Standard mode ends after turn 200 and ranks by the existing score formula;
  the result screen clamps the displayed turn to 200.
- Help lives in the app as four static pages reachable from the main and pause
  menus; it is not a separate tutorial state machine.
- The Science list remains a flat 8-row scroll with the selected effect line;
  era grouping is deferred.

### Historical M7 decisions

- Cursor + Cross pathfinding instead of direct D-pad movement, so diagonal
  movement does not require simultaneous D-pad presses.
- Straight 2, diagonal 3, +2 forest/hills; mountains/water impassable; no
  stacking; friendly city tiles enterable.
- Settler 6/2, warrior 6/2, scout 8/3.
- City minimum Chebyshev distance 3; founding consumes the settler; sight 2;
  population 1; 10 names.
- Fog from unit/city vision, not the cursor.
- Unit/city sprites in terrain atlas row 4.
- Triangle menu Found City / Wait / Cancel; toast errors; Select end turn;
  L/R cycle.
- M3/M4 use the normal 1 settler / 1 warrior / 1 scout start.
- M4 science is invested into one active tech; completion stops until the
  player chooses the next tech (no silent auto-research).
- M4 tech gates are data-driven (`required_tech` on production defs); locked
  items stay visible with REQUIRES TECH/LOCKED so the tree is discoverable.
- M4.1 Worker costs 10 and is always available; improvements are the tech-gated
  content.
- M4.1 improvements require friendly-city ownership, exclude the city center,
  are instant, and consume all worker movement. Roads reduce entry cost by 1
  (minimum 1).
- M4.1 Triangle prefers the unit menu on a city tile; Cross still opens the
  city.
- M5 100 HP, `30 + (attack-defense)*5` damage clamped 5..95, terrain/city
  defense, surviving-defender counterattack, attack consumes movement.
- M5 capture needs an adjacent military unit and an empty center; capture
  costs one population and resets the city queue.
- M5 three AI civs plus barbarians; AI runs synchronously, caps at four
  cities, declares war after turn 12, and sues for peace when weak.
- M5 player-only economy/vision; AI has no research/workers yet.
- M5 Triangle → GARRISON for entering an empty friendly city; the unit stays
  selected. Cross on the city still opens management.
- Market +4 gold remains the one-city economy offset.
- M6 relations are symmetric and banded HOSTILE/COLD/NEUTRAL/FRIENDLY/ALLIED.
- M6 gifts cost 10 (reduced from the 25-gold draft so the opening treasury can
  start the loop); gain +12 falling by 2 to +4, halved at war.
- M6 NAP and open borders each last 10 turns with separate counters; alliances
  last until war/elimination. War/capture tear up agreements.
- M6 trade is AI-pays-player 2 gold/turn for 10 turns; rivals have a real
  treasury fed by city gold and start with 30. Resource trade is deferred.
- M6 open borders/alliances share vision, not physical border passage.
- M6 AI proposes peace/NAP/alliance; AI-AI diplomacy is low-frequency and never
  starts a war against a player ally.
- M6 diplomatic victory needs all living rivals allied for 10 consecutive end
  turns, at least two living rivals, and no AI-AI war among them.
- M7 saves are version-2 raw struct snapshots with an FNV-1a checksum; the
  header carries mode, turn, timed seconds, seed, RNG, and score. This is the
  simplest reliable format without allocation or migration.
- M7 autosaves after each completed turn; manual saves are explicit slots 1–3.
- M7 timed mode counts wall time only while map/city/diplomacy is active;
  menus and pause do not consume it. At expiry the player wins on a tie with
  the best living rival.
- M7 score weights: population ×6, cities ×10, territory ×1, buildings ×5,
  units 4 + HP/25, techs ×10, positive treasury/20.
- M7 science victory means all 18 available techs; the planned 30-tech tree is
  deferred.
- Analog stabilization uses a 15-sample median center, adaptive 32–56
  deadzone, convergent low-pass filter, minimum output threshold, and 45-frame
  zero-output recenter; manual calibration is available in Options/Pause.

## Tests

- Build: pass — clean, no warnings.
- Host logic (`make test`): pass — 13 suites: map, units, city, tech,
  improvements, combat, AI (including 200- and 320-turn simulations),
  diplomacy, analog, save, M7 scoring/science goals, M8 tech/effects/science/
  score, and M9 language toggle; plus `tools/check_i18n.py` and
  `tools/check_audio.py`.
- PPSSPP ELF/EBOOT: pass — M4.1 worker/improvement, M5 rival/barbarian/combat/
  defeat, M6 diplomacy flow, M7 standard/timed save/load plus timed expiry, M8
  Help/30-tech Science/standard turn limit/score result, and M9 Chinese and
  English UI, Help, map/city/science, diplomacy, save/load, tile info, pause,
  and score result.
- PPSSPP EBOOT: M9 EBOOT booted (`CIVITAS PSP M9`) and rendered the Chinese
  main menu; the current M9 `EBOOT.PBP` has not yet been run on hardware.
- PSP 1000: M0/M1/M1.1/M1.2, M2, M3, M4, M4.1, and M5 pass. M6 feature smoke
  passed but analog retest is pending; M7, M8, and M9 pending.
