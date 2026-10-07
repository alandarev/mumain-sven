# Auction house pricing rules

The client opens Auction House when Zyro sends the broker state. Browse, Sell,
My auctions and Collection share the existing MU theme and item models. The
server determines market availability and announces all pricing rules.

Goods are priced in exact whole Zen per unit. The broker announces the allowed
number of significant digits. With three digits, 2,450,000 Zen can move down to
2,440,000 or up to 2,460,000. Crossing a power of ten uses the adjacent valid price:
1,000,000 moves down to 999,000. Prices such as 2,449,999 are off the permitted
step. A rule of zero digits removes this restriction. Individual item listings
and quantity multiplied by unit price are not subject to this step rule.

Stepping never silently rounds an entered price. The player chooses the adjacent
price explicitly. An unavailable lower or upper price disables that direction;
unit prices must stay positive and within the broker's announced maximum.

Fees and listing durations also come from the broker. Posting costs the announced
flat fee plus the listing total multiplied by its posting basis points, divided
by 10,000 and rounded down. The sale fee uses the same rounding on the amount of
each sale. The current defaults are 5% posting, 10% on sale and 48 hours, but the
client reads the actual rules and must review changed fees before publishing.

Items and proceeds are delivered to inventory while the character is talking to
a broker, as far as inventory space and the Zen limit allow. The remainder stays
in Collection. Away from a broker, delivery waits in Collection. Notices identify
whether a sale or expiry reached inventory or is waiting for collection.

Browse equipment by category, name, excellent option, class, upgrade, sockets and
price. Can equip now limits the results to items your current character can use.
Sort by lowest or highest price, ending soon, or newest. Scrolling appends the
next page; Load more offers the same action from the keyboard.
