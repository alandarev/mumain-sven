#pragma once

#include <functional>

namespace Rml
{
class DataModelConstructor;
} // namespace Rml

namespace UI::AuctionHouse
{
// What the result list's row events of auction_house.rml do; each gets the row index (-1 for
// Collect all). The window queues them for its next update.
struct RowEventHandlers
{
    std::function<void(int index)> select, cancel, claim;
};

// Binds select (the row), cancel_listing and claim (the buttons inside a row). A button press is
// only the button's action: it does not reach the row, whose selection would drop a Cancel or
// Collect still waiting for a read in flight (ReviewGate, RR-01).
void BindRowEvents(Rml::DataModelConstructor& constructor, const RowEventHandlers& handlers);
} // namespace UI::AuctionHouse
