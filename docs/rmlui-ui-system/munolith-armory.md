# Munolith Character Armory

The Munolith Character document presents equipped gear and a live, rotatable
character preview separately from Inventory. Its title includes the character name,
class and level. Equipment tooltips use native item information in the shared
RmlUi tooltip; main/offhand sit below
the model and jewelry sits on its right in Pendant, Ring, Ring order. Equipment
icons use close framing to make the models easier to recognize; long weapons and
large armor may be cropped inside the icon while slot borders and labels stay clear.
Drag the model horizontally to rotate and
vertically to tilt the viewpoint. The wheel zooms within bounded limits. Closing
and reopening Character restores its initial view. Other themes retain their
Character screen and mail retains its existing photo controls.

Click equipped gear to pick it up. The held item follows the pointer over Character
and Inventory, including the character preview. Click a valid slot to place it, or
press Escape to restore it. Pending item transfers hide the held image until the
native transaction completes.

Equipment tooltips follow the pointer within the actual viewport. When there is
insufficient room below it, the tooltip flips above the pointer before any final
screen-edge adjustment. Inventory uses the same placement path.

The selected skill follows the action bar. Output is normal damage before target
defenses, with costs and cast restrictions. Hover its gold-underlined output to
inspect a concise calculation tooltip; leaving the number or closing Character
hides it. The default card keeps the calculation prose out of the stats area.
Healing/buff/utility skills have distinct effect descriptions. Resources, offense,
survivability, recovery and equipment modifiers use evaluated server values. Clicking
a stat discloses its contributing items; hover/focus highlights those items. Defense
uses one row: a shared value when PvM/PvP agree, or both labeled values when they
differ. Its disclosure explains both contexts and includes both sets of sources.
Damage Reduction names the evaluated wing/pet reduction stage; armor reduction
retains its separate stage. Clicking
Life/Mana/AG discloses their sources. Optional zero rows and empty categories are
omitted. Hit/evasion compares the ordinary roll against a visible, alive opponent;
without an opponent it is unavailable. Raw PvM/PvP ratings are a separate disclosure.

Free Points opens an inline planner with pending additions, typed amounts, proposed
totals, remaining budget and Apply/Cancel. Minus removes pending additions only.
Escape discards an editable plan before closing Character. A submitted plan remains
pending until an authoritative reply; the retry checks the same operation.

The matching server must support custom Armory version 1: C1 `7B/50` request (48 bytes)
and C2 `7B/51` response (bounded UTF-8 JSON). XML packet definitions in the server are
the wire source of truth. Client `ArmoryPacketFormat` validates payload bounds,
version, numeric values, row/source limits and attribute indices; `ArmoryPackets`
correlates request id and character. Queries run while visible, with immediate skill
updates and periodic refresh. Unsupported servers leave explained stats unavailable
and allocation disabled. No generated packet bindings or package pins are changed.

Apply is one complete server-validated plan, with expected bases/budget, class caps
and idempotent replay protection. Its acknowledgement updates native base values and
free points without the general CharacterInformation packet, which replays map entry
in this client. The usual server attribute observer updates resource maxima. Pending
acknowledgements are consumed even if Character was closed before the reply.

RML/RCSS owns layout and presentation. `ArmoryController` activates when the document
contains the Armory preview element and owns copies used by native render targets.
The panel fits the workspace safe area at 1024×768 and 1920×1080, with physical text
rasterization and internal scrolling. Empty previews stay hidden, targets disable
when closed, and tooltip cleanup runs outside focus/blur callbacks to avoid reentry.
Queued announcements pause while Character is open and resume when it closes.
The notice gate is independent of the HUD picker/modal gate so one panel cannot
clear another panel's ownership. No SD, fruit management or persistent item-info
panel is included.

The Armory uses the shared `data-scale="window"` profile, with no fixed height cap.
Munolith defines a 1024×768 design viewport in theme.ini. Normal windows fit linearly
below that viewport and grow by the square root of the viewport ratio above it:
1.0× at 1024×768, about 1.19× at 1080p and 1.68× at 4K, before density and user
preferences. The workspace still fits the result inside the available safe area.
The HUD keeps its independent existing growth rule. Closing detaches model gesture
listeners; releasing outside the studio ends a drag.
