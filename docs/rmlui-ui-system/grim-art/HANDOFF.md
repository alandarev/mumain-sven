# Grim / Black Glass theme — paused archive, 2026-09-30

This branch preserves work in progress for later resumption. It is not a release or a merge-ready contribution. The owner requested archiving the current direction and releasing the development environment.

## Contents and compatibility

The branch contains only the new `themes/grim/` folder, generated source artwork and prompts, offline atlas/conversion helpers and their tests, and these notes. Its parent is the audited RmlUi base `c13bc0e9f6f025fd1542d85cef1f80ee2f12ff1f`. **The development/testing overlay is deliberately excluded.** The theme was exercised on a later combined development build; this archive base alone is not claimed to provide every binding or document used by the copied theme. Preserve the content, then reconcile canonical documents and bindings against the chosen current RmlUi base before resuming development. Do not copy a testing overlay into an upstream contribution.

`archive-manifest.json` records SHA-256 hashes of all 165 preserved source files as extracted. The handoff and manifest are additional archive documentation. Source artwork includes rejected bronze experiments for provenance; the active design uses Black Glass.

## Selected design

- Black glass/stone wells, thin silver/gunmetal frames, familiar native MU icons and item models.
- HP/mana faceted gauges align with the main HUD band. Current-only numbers sit at the bottom with a black outline/shadow. SD/AG have no permanent numbers or labels; existing hover hints remain.
- Potion hover is a transparent silver outline, replacing the olive-green tinted rectangle.
- Character option A / Refined Classic preserves the narrow dock and stat order. Class-dependent cards, larger allocation buttons, sharp text, labeled Quest/Pet/Master shortcuts and a local progression/fruit disclosure improve legibility. Command, curse, caster and Rage Fighter fields retain their native conditional data.
- Inventory, merchant, storage/trade/personal-shop/mix companions, party HUD/management, Friends and shared login/dialog/button/tooltip surfaces follow the same materials. Native item grids, hitboxes and gameplay events are preserved. Friends retains native geometry and uses a light selected-row background for its black native text.

No runtime C++, packet/server logic, default configuration, Legacy or Modern theme edits are included.

## Historical verification — not archive-branch certification

The combined development build passed its final build and **353/353 tests**. Thirty-three theme RML overrides preserved canonical IDs, text bindings and event arguments; 564 asset references resolved in that build. The scoped upstream C++ quality selector reported no applicable changes, not an analysed C++ pass.

Actual game screenshots were inspected at 1920×1080, 1280×720 and 1024×768. All 18 base/evolved/master class presentations were checked, including zero allocation points, large attributes/resources and long Rage Fighter lines. Packet-staged class coverage proves presentation only. A real point allocation, inventory pickup/move/restore, ordinary merchant purchase/sale, party acceptance and leave were exercised on disposable characters. Legacy and Modern load smoke succeeded. The original full-resolution evidence is retained with the local job archive, not bundled into this public source branch.

These results belong to the recorded development combination, not a fresh standalone build of this archive branch. Rebuild and retest after reconciling its dependencies.

## Open verification work

- Complete HUD hotkeys/cooldowns and normal/master EXP transition coverage.
- Equip/repair, insufficient money/full inventory, expensive-sale and gambling outcomes.
- Five-member targeting, leader kick and fresh party-decline outcomes.
- Full Friends child-dialog lifecycle, letter read/chat, Enter/Tab and scrolling. A stale child dialog intercepted scripted clicks until the owned clients were restarted; it was not classified as a theme regression or counted as passing coverage.
- Native floating Friends positions need manual repositioning after resolution changes; column/control dimensions remain native. System chat can paint above Friends. Fixing these runtime behaviours is a separate scope decision.
- Full guarded comparison parity remains open; individual screenshot/interaction checks are not equivalent to paired parity.

## Resume and rollback

1. Select a compatible current RmlUi base and compare its canonical IDs, models, conditions, events and native geometry with every Grim override. Preserve this archive branch unchanged while integrating on a new branch.
2. Keep the accepted HUD and familiar character option A. Reuse the existing atlas; do not return to the rejected bronze direction.
3. Stage assets through the normal client build. Select `[UI] RmlTheme=grim` before launch and restart; use `legacy` and restart to roll back. Hot-switching was not trusted in the development rig.
4. Re-run syntax/drift/dependency guards, build/tests, then actual client checks at all three sizes. Cover the open cases above before claiming full acceptance.

Atlas regeneration and exact prompts are in [README.md](README.md). Logical RCSS `.tga` references resolve to `.OZT` through the game loader; directly referencing `.OZT` produced missing textures. Native 3D items must remain above the background context. Fixed gem rims must remain separate from cropped/tinted resource fills.

Written with AI assistance; the documented development screenshots and representative interactions were inspected in the running client. This archive does not claim upstream review or complete acceptance.
