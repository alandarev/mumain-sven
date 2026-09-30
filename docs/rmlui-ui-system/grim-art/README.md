# Black Glass HUD preview

The active HUD now follows the operator-selected Black Glass concept: silver/gunmetal,
black stone, separated trays, vivid red/blue faceted gauges and current-only captions.
The earlier bronze implementations below are **rejected history**, not the current design.
The same atlas now also frames character, inventory-family, party and shared windows. Friends retains native geometry with coordinated dark surfaces. Full interaction acceptance is pending.

## Selected resource captions

The operator selected **B** (`caption-inset`): HP/mana gauges align with the main
HUD row, with current-only numbers near the bottom and a black outline/drop shadow.
SD/AG show their fills without visible labels or numbers; hover still shows their
resource names and current/maximum values. The existing input regions are unchanged.
The earlier opaque-plate variant remains historical comparison styling in
`grim_captions.rcss`; it is not the active direction.

## Current artwork and reproduction

`black-glass-source.png` was generated with the built-in image generator on 2026-09-29
using the selected concept. `black-glass.prompt.txt` records the exact prompt. The source
is a 4-column, 3-row sheet on intentional magenta. With operator approval, the offline
packer removes the matte, recovers straight-alpha edges, rejects divider noise from crop
bounds, neutralises residual chroma in neutral materials and packs registered sprites.
Square utility pictograms are cropped separately to avoid stretching them in tall buttons.

```sh
python3 tools/pack_hud_atlas.py docs/rmlui-ui-system/grim-art/black-glass-source.png src/bin/Data/Interface/RmlUi/themes/grim
python3 -m unittest discover -s tools -p test_pack_hud_atlas.py -v
python3 -m unittest discover -s tools -p test_png_to_ozt.py -v
```

The output is a 1024-square OZT plus generated `black_glass_sprites.rcss`. Transparent
pixels are actual alpha, not painted checkerboard. Source art is never loaded at runtime.
The neutral crystal is tinted and cropped by the existing progress element; the rim is
an independent foreground element, so fill changes cannot move or crop the metal.
The dark empty crystal stays behind the fill. Six-digit caption fields have fixed bounds.
Potion backgrounds paint below native 3D items. No runtime or default-theme changes.

Select `[UI] RmlTheme=grim` in runtime config and restart. Roll back with `legacy` and
restart. Do not hot-switch this rig. The full HUD is a direction-review prototype, not
complete interaction/parity acceptance.

## Superseded bronze implementation history


This first pass adds an optional aged-bronze HUD. Other windows still inherit
Legacy. The full Grim theme is in development; inventory and social windows
have not received their bespoke visual pass.

Set `[UI] RmlTheme=grim` in the runtime `config.ini` before starting the client.
Restart after changing the selection. Return to `RmlTheme=legacy` and restart to
roll back. Do not hot-switch this testing rig. The default configuration is unchanged.

The HUD retains the four potion slots, five skill slots and current-skill slot,
five utility actions, SD/AG and the existing experience decile display. HP and
mana use single faceted gauges with larger current-only captions. Hover still
shows current/maximum values. DejaVu Sans and native text sizing are retained.

## Artwork and conversion

`bronze-atlas-source.png` was generated for this theme with the built-in image
generator on 2026-09-29, using the operator-selected aged-bronze concept as a
material reference. Exact generation and correction prompts are in `prompts.txt`.
The first draft's baked checkerboard was rejected. This selected atlas uses an
intentional opaque charcoal backing; it does not claim transparent cutouts.
Original game textures are referenced in place, never copied into this artwork.

From the client repository root, with Python and Pillow available:

```sh
python3 tools/png_to_ozt.py docs/rmlui-ui-system/grim-art/bronze-atlas-source.png src/bin/Data/Interface/RmlUi/themes/grim/bronze-atlas.OZT --fit
python3 -m unittest discover -s tools -p test_png_to_ozt.py -v
```

The 1254-square source is downsampled once to the loader's 1024-pixel limit.
The OZT contains a four-byte prefix, uncompressed 32-bit TGA header and bottom-up
BGRA pixels. Alpha remains straight; the renderer owns premultiplication.
`bronze_sprites.rcss` declares explicit crops excluding the atlas gutters.
Tests cover orientation, channels, partial/zero alpha and invalid dimensions.

## Baseline and scope

The complete Legacy folder was copied from client
`074270b4ec4f0c6f84dfb5b476e99d992d44e2ed`. Its base is
`c13bc0e9f6f025fd1542d85cef1f80ee2f12ff1f`, with testing-rig part
`baf1355052904ca00709b0c678421c0f595242ea`. Development intentionally pins that
base for the prototype. Upstream has advanced; publication needs fresh integration.

The correction pass adds `gem-sockets-source.png` and `gem-sockets-prompt.txt`,
generated with the built-in tool using the selected HUD reference. Complete
sockets replace separately overlaid opaque gemstones, which had cut through the
ornate frame. The four registered crops align red/blue/poison/empty states. The
crystal occupies approximately y=10%..87.5% of each socket crop; the RML maps the
live fraction to that span, with zero explicitly empty. SD/AG use their own
progress elements, and final-size font bindings keep the resource captions sharp.
The shared HUD bed and three-piece EXP rail join previously disconnected groups.
Convert the second atlas using the same command with `gem-sockets-source.png` and
`gem-sockets.OZT`. Neither source claims transparency: a further transparent-output
attempt returned baked checkerboard and was rejected, never staged.

The custom art is confined to `main_frame.rml`, its new background companion,
`grim_hud.rcss`, sprite definitions and theme metadata/tokens. Existing Legacy
and Modern content is unchanged. HP/mana use RmlUi's existing progress element
with the original fraction and poison bindings; it crops the sprite's geometry
and UVs together beneath the scaled HUD. Native potion models remain above the
background context. No runtime, server, protocol or default-theme changes.

The rest of the copied documents/styles/templates remain inherited. The dormant
shared `loading.rml` references a missing `loading.rcss` in the baseline too;
the active loading screen uses `title_scene.rml`. Full dependency/coverage
acceptance must resolve or explicitly classify this before completing the change.

The current paired replay and manual-compare tools require an original-client
schema-3 baseline and a schema-4 Legacy target. They refuse two themed schema-4
clients. Individual diagnostic captures and manual interaction checks are used
for this prototype; they are not guarded paired-parity evidence. Full-theme
acceptance and complete HUD interaction coverage remain pending.

For a later upstream deliverable, extract only `themes/grim/`, this artwork
folder and the offline packer/tests onto a clean compatible upstream branch.
Never publish the derived rig or copy private game data into that branch.
