# Auction house pricing rules

The client opens Auction House when Zyro sends the broker state. Browse, Sell,
My auctions and Collection share the existing MU theme and item models. The
server determines market availability and announces all pricing rules.

Prices are whole Zen and move in round steps of about 1 %. The step of a price is
the largest number of the form 1, 2 or 5 × 10ⁿ that is at most the price divided by
10^precision, and at least 1; a price is valid when it is a multiple of its step.
The broker announces the precision (default 2, so every step is 0.4–1 % of the
price; 0 removes the restriction). With precision 2, 600,000 moves down to 595,000
or up to 605,000, 1,000,000 moves down to 995,000, and every whole number below 200
is valid. The rule applies to the unit price of goods and to the price of an item,
never to a quantity multiplied by a unit price. The server refuses a price that is
off its step; the client never sends one.

Type a price as digits with optional comma grouping (`590,000`) or as a number with
the suffix `k`, `kk`, `m` or `b` and at most two decimals (`590k`, `1.5kk`, `1.2b`).
Other forms, such as `1.000kk`, are refused with a hint naming the accepted formats.
Below the field a short help shows examples that are valid prices under the
announced rules (never above the maximum price or off a step), the grid and the keys
("Prices round down to ~1 % steps · ↑/↓ one step · Shift ten.") and the allowed range
("1 – 2,000,000,000 Zen").
When the price is committed — Enter, leaving the field, Review listing, or a step —
a price off its step is rounded down to the nearest valid price and the field says
so ("Rounded to 93,000,000"); nothing is rounded while you are still typing.

The − and + buttons, the Up and Down keys and the mouse wheel over the focused price
field move to the previous or next valid price, and ten valid prices with Shift. A
price off its step moves to the valid price below or above it, also when you press
− or + right after typing it: leaving the field for a step does not round first, so
93,240,000 goes to 93,000,000 with − and to 93,500,000 with +. With an empty field,
+ starts at the lowest price of other sellers of the same goods and − one step
below it; otherwise the buttons stay disabled until a price is typed. At 1 and at
the broker's maximum the corresponding direction is disabled and the field says why.

While you price goods, the Sell tab shows the lowest price of other sellers, the
stock at that price and the stock of all quoted tiers ("Lowest 600,000 · 20 in
stock · 50 total"), refreshed every five seconds. Up to eight price tiers of the
market are folded into "Market depth (N prices)"; opening it is remembered until the
window closes. Match lowest
copies the lowest price and Undercut sets the highest valid price below it. A
refresh never replaces a price you have typed. Equipment is never priced from
other listings. Max (n) fills in the whole quantity you hold; clicking into a
quantity or price field selects its content, so typing replaces it.

The client remembers the price of every listing you publish, per server and
account, so all characters of an account share it. Goods are remembered by type
and level (loose jewels and bundles differ); equipment by type, upgrade level and
every option (excellent options, luck, skill, additional option, ancient set,
sockets, harmony and the level-380 option), never by its serial, slot or
durability. Selecting an item to sell fills in, in this order, the price you were
editing for that item in this window, the remembered price ("Last price", rounded
down if the step rule changed), for goods the current lowest price of other
sellers ("Current lowest"), or nothing. Forget deletes a remembered price. The
memory keeps the 256 most recently published prices in
`Config/auction-prices/<hash>.prices` beside the client; it holds no password or
session data.

The Sell tab keeps the total, the posting fee, the sale fee, "Receive if all sell",
the listing count and Review listing pinned below the scrolling details, so they
stay in view at 1024×768 also for long equipment options; a blocking note (Zen
shortfall, listing limit) sits directly above the button. The window keeps one size
and position for every tab and category; its content scrolls inside it.

Fees and listing durations also come from the broker. Posting costs the announced
flat fee plus the listing total multiplied by its posting basis points, divided
by 10,000 and rounded down. The sale fee uses the same rounding on the amount of
each sale. The current defaults are 5% posting, 10% on sale and 48 hours, but the
client reads the actual rules and must review changed fees before publishing.

Items and proceeds are delivered to inventory while the character is talking to
a broker, as far as inventory space and the Zen limit allow. The remainder stays
in Collection. Away from a broker, delivery waits in Collection. Notices identify
whether a sale or expiry reached inventory or is waiting for collection.

Every action ends with a receipt built from the broker's answer: what was listed
and the posting fee charged, what was bought, the Zen actually paid and how many
units went to the bag and how many to Collection, what a cancellation returned and
where, and what a claim collected ("Collected 3 deliveries · 2 items, 11,250,000
Zen"; Collection counts are deliveries, and one delivery can hold several items),
with what was kept back and why ("1 delivery left: no bag space", Zen limit). A
receipt offers Open Collection or My auctions where that is where the result went,
and the Collection badge counts the new entry at once. Receipts and errors stay
until your next action in the window (a tab, a row, a button, typing) or their ×,
so their links stay usable. When the broker does not answer an action at all, the
window says "Outcome unknown. Reopen Zyro and check your bag, Collection and My
auctions before retrying." Collect and Collect all act at once, without a review.

A sale or expiry while the window is open appears as its own line above the receipt
("Sold 6 × Jewel of Bless · 3,186,000 Zen added to your bag", "… waiting in your
Collection" with Open Collection, "Expired: Dragon Armor +9 · returned to your
Collection"); up to three stack, newest first. A notice never closes an open review
and never replaces a receipt or an error; it stays until your next action or its ×.
With the window closed it goes to the chat log. Server announcements (event
openings and the like) are not drawn across the open window; they go to the chat
log instead.

The listing review shows the unit price line for goods ("6 × 590,000 = 3,540,000
Zen"), the options of equipment, the posting fee with its terms ("5 %, charged now,
not refunded"), the sale fee and "Receive if all sell". The sale fee is taken from
each sale and rounded down per sale, so selling in parts can only leave you more:
"Receive if all sell" is exact when every unit's fee is a whole number of Zen
(at a 10 % fee, every price from 1,000 Zen up) and otherwise is a lower bound
(two at 199: "at least 359"; two single sales give 360). Buying equipment your
character cannot wear shows a Cannot Equip warning but stays allowed. Review
purchase is disabled, with the reason under it, when you lack the Zen for the
quoted total ("Need 4,740,000 Zen, you have 1,000,000") or the listing is your own;
with an invalid quantity it is dimmed and pressing it moves you to the quantity
field. My auctions rows read "6 remaining of 8 · 590k Zen each · 48h"; the cancel
review names the exact unit price, the remaining quantity, the options of equipment
and whether the posting fee is refunded ("Posting fee not refunded").
Next to the fields the Sell tab says how many units you hold or the allowed range,
the active listings against the limit ("Listings 19/20"), an exact posting-fee
shortfall ("Need 177,000 Zen, you have 50,000") and why an item cannot be sold (not
tradable, not accepted by the market, moved); Browse says the same for a purchase.
A refused listing keeps your quantity and price. In Collection a sold row names the
item, the quantity and the net Zen; selecting it shows the gross and the sale fee.

While Browse shows a selected jewel or material, its quote refreshes every five
seconds ("Updated 3 s ago"; Refresh asks at once). The refresh pauses while a review
is open or a request is pending, and only one auction tab polls at a time, so a
window stays far below the broker's limit of five requests per second. A new price
tier, a changed stock ("4 → 10") and a changed lowest price ("595k → 590k") are
marked for about three seconds; a line such as "Lowest fell 600k → 590k" stays for
about ten seconds, or until you select another row or open a review. The price table's In your order column shows how
many units of the entered quantity each price fills, cheapest first, with the order
total below it; units filled beyond the eight shown tiers appear as "+N at higher
prices · X Zen", and "Only N available" when less is for sale than entered. When
the quote shows all eight tiers, more may be for sale above them: the window then
asks the goods list for the exact count of that goods in the same refresh (two
requests every five seconds); until it answers, the row reads "40+ available" and
a larger quantity is not refused. The
purchase review repeats that split per price. After a purchase the goods stay
selected and their quote refreshes.

Browse equipment by category, name, excellent option, class, upgrade, sockets and
price. Can equip now limits the results to items your current character can use.
Sort by lowest or highest price, ending soon, or newest. Scrolling appends the
next page; Load more offers the same action from the keyboard.
