# Full Warehouse

Munolith shows the base 8×15 vault (120 cells) in one frame beside Inventory.
When the server reports purchased extra storage, its complete 8×15 grid appears
alongside the base grid: all 240 cells remain visible without pages or scrolling.
The unexpanded frame stays narrow. Both versions share Inventory's window scale,
aligned top edge, linked title dragging and measured group fitting.

The two native grids retain their owners and original wire slots: base 0–119,
extra 120–239. Each item must fit wholly within its grid. The wider presentation
neither grants storage nor moves existing items. Zen, estimated storage fee,
Deposit, Withdraw and PIN controls are shared beneath the grids.

Left-click picks up and places items. Shift-click or right-click transfers a
stored item to Inventory; Inventory's existing Shift transfer deposits into
available storage. Server replies determine whether the transfer succeeded.
Protected manual and automatic withdrawals use the existing PIN keypad. An
accepted PIN resumes the saved source slot and destination; cancellation,
rejection or closing clears pending automatic transfers. Use the money dialog's
OK button to submit its edited amount. Its existing focused-field Enter path
can read the previous model value; that shared dialog behavior is unchanged.

## Opt-in and implementation

```ini
[Capabilities]
EmbeddedVault=1
```

Missing or disabled capability retains the separate extra vault and H toggle.
Munolith enables it and omits the standalone extension workspace slot. H does
not hide the purchased extra grid in the combined frame. Live theme switching
refreshes this capability and native presentation ownership.

`storage.rml` and `storage_bg.rml` share anchors at (15,45) and (185,45), with
20-unit cells. The frame measures 190×425 design units without extra storage
and 360×425 with it. Shared Inventory chrome keeps native item models between
the background grid and foreground labels/controls. `StorageInventory` delegates
embedded input, updates and rendering to `StorageInventoryExt`; availability
uses server ownership plus the base window rather than standalone visibility.
Acknowledgements still reach the original native control.

Protected continuation stores an absolute vault slot rather than trying to
find an item again at a saved cursor position. Manual drops into Inventory
also pass through the Warehouse protection action. Money, PIN set/remove,
pricing and acquisition protocols remain the existing ones.

## Verification

`test_warehouse_layout.cpp` checks capability defaults and reload, both native
RmlUi grid anchors, visible cells, frame widths and footer bounds. Native
acceptance additionally checks both capacities at 1024×768, 1920×1080 and
3840×2160, shared scales, linked dragging, actual item transfers, bottom-row
hover and footprint refusal, protected continuation and Legacy/Modern switching.
The superproject's `tests/uitest/munolith_warehouse.py` replays layout acceptance
against an explicitly prepared, backed-up disposable fixture without packet
injection or database writes. Restore it only after the owned client fully exits.
