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
the suffix `k`, `kk`, `m` or `b` and at most two decimals (`590k`, `1.5kk`, `2.3b`).
Other forms, such as `1.000kk`, are refused with a hint naming the accepted formats.
When the price is committed — Enter, leaving the field, Review listing, or a step —
a price off its step is rounded down to the nearest valid price and the field says
so ("Rounded to 93,000,000"); nothing is rounded while you are still typing.

The − and + buttons, the Up and Down keys and the mouse wheel over the focused price
field move to the previous or next valid price, and ten valid prices with Shift. A
price off its step moves to the valid price below or above it. With an empty field,
+ starts at the lowest price of other sellers of the same goods and − one step
below it; otherwise the buttons stay disabled until a price is typed. At 1 and at
the broker's maximum the corresponding direction is disabled and the field says why.

While you price goods, the Sell tab shows the lowest price of other sellers, the
stock at that price and the stock of all quoted tiers ("Lowest 600,000 · 20 in
stock · 50 total"), and up to eight price tiers of the market, refreshed every five
seconds. Match lowest
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

Fees and listing durations also come from the broker. Posting costs the announced
flat fee plus the listing total multiplied by its posting basis points, divided
by 10,000 and rounded down. The sale fee uses the same rounding on the amount of
each sale. The current defaults are 5% posting, 10% on sale and 48 hours, but the
client reads the actual rules and must review changed fees before publishing.

Items and proceeds are delivered to inventory while the character is talking to
a broker, as far as inventory space and the Zen limit allow. The remainder stays
in Collection. Away from a broker, delivery waits in Collection. Notices identify
whether a sale or expiry reached inventory or is waiting for collection.

While Browse shows a selected jewel or material, its quote refreshes every five
seconds ("Updated 3 s ago"; Refresh asks at once). The refresh pauses while a review
is open or a request is pending, and only one auction tab polls at a time, so a
window stays far below the broker's limit of five requests per second. A new price
tier, a changed stock ("4 → 10") and a changed lowest price ("595k → 590k") are
marked for about three seconds. The price table's In your order column shows how
many units of the entered quantity each price fills, cheapest first, with the order
total below it; units filled beyond the eight shown tiers appear as "+N at higher
prices · X Zen", and "Only N available" when less is for sale than entered. The
purchase review repeats that split per price. After a purchase the goods stay
selected and their quote refreshes.

Browse equipment by category, name, excellent option, class, upgrade, sockets and
price. Can equip now limits the results to items your current character can use.
Sort by lowest or highest price, ending soon, or newest. Scrolling appends the
next page; Load more offers the same action from the keyboard.
