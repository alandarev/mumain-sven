# Munolith Inventory

Inventory holds carried items; Character Armory holds worn equipment and the
character preview. The satchel starts at 64 cells (8×8), and shows the first two
owned 32-cell extensions directly below it. All 128 cells fit without scrolling
at 1024×768 using the shared window scale. The HUD keeps its separate scale.
The capacity label counts free cells rather than items.

Character and item-transfer companions arrange to the left of Inventory with
matching top edges and a small gap. Drag either title to move the linked panes
together. Moving Inventory alone sets the same preferred anchor, so reopening
Character joins that location whenever there is room. Closing a companion keeps
the remaining pane steady. At narrower resolutions the whole group fits inside
the space between the menu and HUD; temporary fitting keeps your preference for
when you return to a larger screen. Item pickup freezes rearrangement until you
drop or cancel the held item. Friends and other independent utilities keep their
own placement. No extra docking controls are needed.

Left-click picks up an item and the next click places it. Escape restores a
picked-up item. Right-click uses a consumable or equips gear; an occupied slot
or missing attribute/class requirement produces feedback. Shift-click performs
fast transfer into an open warehouse, trade, NPC shop or crafting window.
Without one of those windows it transfers between bag sections. NPC sales keep
their existing confirmations. Ordinary potion use while a shop/crafting session
is open explains that the session must be closed first, matching server rules.

The subtle horizontal dividers mark storage sections. Items cannot straddle a
section boundary, even when enough total cells are free. Invalid placement
keeps the pickup available for another destination or cancellation.

The hammer beside Zen toggles Repair; L is retained. Escape leaves Repair
before closing Inventory. Hover shows eligibility and the client repair
estimate; the server determines the actual charge. Worn items can be picked up,
unequipped and repaired through Armory. Right-click an equipped Dark Horse or
Dark Spirit to open its Pet details. Quest and Master Skills are in the corner
Menu and retain T/A.

## Options for other games

The active theme's optional `theme.ini` `[Inventory]` section controls presentation:

```ini
[Inventory]
PersonalStore=0
ExpansionLimit=2
```

These are Munolith's bundled defaults. Set `PersonalStore=1` to restore the
Personal Store menu entry and S key, plus store purchase windows. An omitted
key defaults to enabled, preserving other themes. Set `ExpansionLimit=4` for
four-extension expansion guidance; omitted keys default to four. Values are
bounded to the supported 0–4 range. Reload the theme or restart after editing.

Only the first two additions are embedded in the satchel. Characters owning
three or four additions always retain access to the remaining items through
**Extra items** or K, even if the configured expansion limit is lower. Existing
wire slots and Personal Store's reserved indices are unchanged. Other themes
retain their original equipment/inventory presentation.

These settings neither grant capacity nor enforce server policy. The expansion
entry currently explains that unlocking is unavailable and the configured
maximum; no acquisition method or price has been chosen. A game which removes
Personal Store or limits new entitlements must enforce those rules on its
server as well. This UI change preserves the compatible server behavior.

## Personal shops

Munolith's personal-shop setup and browsing windows use matching compact counters
beside Inventory. Enter the shop name above the native 8×4 store grid. The seller's
Open/Apply and Close store actions retain the game's existing locks and confirmations;
Close dismisses the setup window. Right-clicking a listed item opens its existing
price dialog. Buyers see the shop name above the same grid; item hints show prices
and selecting an item opens the native purchase confirmation. The warning area
scrolls when needed. Drag either header to move the shared Inventory group.

## Shared hover descriptions

Munolith item, skill and button descriptions use a dark green background and gold
frame. Native colored requirements, modifiers, bold headings and spacer rows stay
intact. Hover descriptions follow the pointer and fit within the viewport.

Pet-life indicators use flat green/gold tiles with a health track. Equipment at half durability or below shows a matching warning tile; yellow, orange and red indicate increasing wear. Hover a warning to see the item name and current/maximum durability. Warnings follow the uncovered world edge beside open item panels.
