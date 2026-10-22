# Release artifacts

Generated release packages live here. Binary artifacts (`EBOOT.PBP`, ZIPs) are
ignored by Git; upload the ZIP from the current release directory as a GitHub
Release asset.

## V0.2.2

- `EBOOT.PBP` — PSP executable
- `civpsp-v0.2.2.zip` — packaged PSP build (contains `EBOOT.PBP`)
- `SHA256SUMS` — published checksums

V0.2.2 fixes the fog-of-war HUD/info-panel leak for enemy units on non-visible
tiles and unexplored cities. It keeps V0.2.1's turn briefing and city-overview
cleanup, plus V0.2's city siege combat, difficulty, Classic/Endless modes,
denouncement, era-grouped Science screen, and save v3. The in-game version and
XMB labels are updated to V0.2.2.

## V0.2.1

- `EBOOT.PBP` — PSP executable
- `civpsp-v0.2.1.zip` — packaged PSP build (contains `EBOOT.PBP`)
- `SHA256SUMS` — published checksums

V0.2.1 adds the centered turn briefing for AI/end-turn battles and diplomacy,
plus the city-overview cleanup. It keeps V0.2's city siege combat, difficulty,
Classic/Endless modes, denouncement, era-grouped Science screen, and save v3.
The in-game version and XMB labels are updated to V0.2.1.

`release/v0.2.1/` is frozen at tag `v0.2.1`; it does not include the V0.2.2 fog
fix.

## V0.2

- `EBOOT.PBP` — PSP executable
- `civpsp-v0.2.zip` — packaged PSP build (contains `EBOOT.PBP`)
- `SHA256SUMS` — published checksums

V0.2 adds city HP/defense sieges, garrison shielding, worker repair, Walls,
Easy/Normal/Hard difficulty, Classic/Endless modes, denouncement, era-grouped
Science, `L`/`R` controls, and save v3 with v2 migration.

`release/v0.2/` is frozen at tag `v0.2` (`c733769`); it does not include the
V0.2.1 changes.

## V0.1

- `EBOOT.PBP` — PSP executable
- `civpsp-v0.1.zip` — packaged PSP build (contains `EBOOT.PBP`)
- `SHA256SUMS` — published checksums

Build instructions and release notes are in the repository root (`README.md`,
`README.en.md`, `UPDATE.md`, `UPDATE.en.md`).
