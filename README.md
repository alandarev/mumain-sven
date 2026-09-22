# Legacy HUD parity screenshots

Before/after evidence for `fix(ui): restore legacy HUD readout and tooltip parity`.
This image-only branch is not part of the feature contribution.

- Before: legacy RmlUi instrumented rig `341a920a`.
- After: revalidated instrumented rig `b1522858`, based on upstream `af531ce8`.
- The after rig's three legacy-theme assets are byte-identical to feature HEAD `69b6688c304f3806be66b04460554b09d9ab33f7`.
- Default UI scale; world rendering disabled for inspection.
- Crops are unretouched; all are native-size except the explicitly labeled 2x nearest-neighbor AG detail. Labels were added outside the screenshot pixels.
- Cursor positions differ. EXP state differs between before and after, so these pictures demonstrate frame/readout appearance, not equal-state progress semantics.

## 1024x768 counters and EXP frame

![Before and after at 1024x768](hud-1024-before-after.png)

Source captures: `1024x768_legacy_341a920a_20260922-122620.png` and `1024x768_legacy_b1522858_20260922-142919_full-unhovered.png`.
Crop rectangle: `(0, 660, 1024, 768)` in each full frame.

## 1280x800 health hover

![Before and after at 1280x800](health-1280-before-after.png)

Source captures: `1280x800_legacy_341a920a_20260922-122949.png` and `1280x800_legacy_b1522858_20260922-142924_hp-entered.png`.
Crop rectangle: `(100, 685, 1180, 800)` in each full frame.

## Expanded feature gallery

`gallery-provenance.json` records the source capture filenames, crop rectangles and zoom for each panel. No live client or source asset was changed to produce these sheets.

- `location-typography.png`: pre-feature `341a920a` versus revalidated `b1522858`, equal location text.
- `menu-tooltip-typography.png`: all five menu hints, earlier settled audit before the sibling-hint fix versus final `b1522858`. This is an intermediate pre-fix state, not the pre-feature base.
- `resource-exp-hints.png`: same earlier audit versus final; missing SD/AG/mana hints and EXP typography/placement.
- `helper-typography-parity.png`: original client `688ed1b7` versus final `b1522858`. This is a parity reference, NOT a before/after capture of this contribution.
- `ag-occlusion-fix.png`: intermediate `0c7c8d96` before the layer fix versus final `b1522858`, equal 12345 fixtures, 2x nearest-neighbor enlargement. Demonstrates the formerly hidden final digit.

All final crops use the same-base revalidated rig's byte-identical feature assets. Earlier audit panels can already contain other fixes in this series; their captions deliberately name the specific remaining defect.
