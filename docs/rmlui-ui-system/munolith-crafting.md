# Munolith Compact forge

The crafting family uses a narrow 190×400 logical panel beside Inventory. The
complete native 8×4 material grid comes first; recipe names, material checks,
rates, costs, tax, warnings and socket choices follow in a bounded scroll pane.
Use the wheel or scrollbar to reach longer descriptions and lower socket rows.
Close and the native service action remain below that pane.

Native items retain their original footprints, tooltips, pickup/cancel and
transfer rules. Close and the corner control use the ordinary close path, which
refuses while items remain in the box or an item is held. Combining opens the
existing confirmation. Pending combinations lock native grids and socket/action
controls; finished results retain the item grid and hide recipe/Combine content.

Chaos Goblin, trainer, seed master and seed researcher method menus share the
silver/stone frame. Their descriptions, method choices and cancellation callbacks
are unchanged. The menu corner control uses the same cancellation as Escape.
Other generic menus keep their existing layout. Legacy and Modern keep their own
crafting documents.

Geometry and item-cell backgrounds live in Munolith RCSS/RML. The native control
follows `item_grid`; a `themed-grid` declaration suppresses duplicate native cell
chrome. Sharp text uses the existing root counter-scale bridge. The workspace
continues to own grouped placement, scale, fit and drag.

Presentation acceptance uses a disposable client with injected incoming states,
real native input and cancelled confirmations. It does not verify server crafting
transactions, recipe balancing, physical HiDPI or Classic/Enhanced parity.
