# Compact merchant counter

Themes opt into this presentation with `[Capabilities] CompactNpcShop=1`.
Munolith enables it; missing capability keeps the original shop and gambler
flows. The counter retains the native eight-column item grid, original item
footprints and wire slots. Only empty trailing rows are removed. It joins
Inventory's existing window group and uses the same cell pitch and scale.

The merchant's title comes from the interaction target. The readout beneath
stock shows the hovered offer, estimated purchase price including nonzero tax,
and its action. Prices remain `Est.` until the server provides authoritative
quotes. Inventory has the only wallet; its brief Zen delta follows server data.

Click buys one offer. Shift-click a basic recovery potion or antidote opens a
quantity picker. Its 1–100 offers field can be typed or adjusted with +/−.
The picker shows items per offer and total delivered items, which can differ
from offer count. Enter confirms; Escape cancels. A confirmed batch sends one
ordinary buy request at a time, awaiting each reply. Stop/Escape cancels future
requests and lets an in-flight request finish. Changed stock identity, tax,
price or units also stop further requests. Nothing is optimistically inserted
or charged; confirmed items receive a short footprint outline in Inventory.

Shift-click carried items or drop a held carried item into the stock grid to
sell. The original restrictions still apply. Protected items show their name,
model and estimated proceeds before confirmation. Cancellation or a refused
sale restores the picked item. Right-click retains ordinary item usage and
its existing restrictions while a merchant session is open.

Repair services appear only for the existing repair-capable NPCs. L toggles
repair; A (or Shift-L) requests Repair All. Escape leaves repair mode before
closing the shop. Some servers send a separate repair-result packet; OpenMU
instead confirms repairs through durability/money updates. Pending Repair All
clears on observed durability improvement, or after three seconds without
confirmation, without claiming a successful repair. The server's normal
messages still explain unavailable repairs. Estimates may differ from charges.

No new protocol, acquisition policy, personal-store policy, cart, buyback,
category panel or server pricing rule is introduced by this counter.
