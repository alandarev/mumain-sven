#pragma once

#include "UI/AuctionHouse/PriceEntry.h"

#include <functional>
#include <optional>

namespace Rml
{
class DataModelConstructor;
class Element;
class Event;
} // namespace Rml

namespace UI::AuctionHouse
{
// Up and Down on the price field; Shift moves ten valid prices.
std::optional<PriceStepInput> PriceStepFromKey(const Rml::Event& event);
// The wheel steps only while it hovers the − or + button, one valid price per notch (ten with Shift).
std::optional<PriceStepInput> PriceStepFromWheel(const Rml::Event& event);
// − and + clicks: ten valid prices with Shift.
unsigned PriceStepsFromClick(const Rml::Event& event);
// Whether the price field loses the focus to a press on an enabled − or + button. That press steps
// once from the typed value, so the blur must not commit (round) the price first (D3).
bool BlurForStepper(const Rml::Event& blur);

// What the price field's events do; the window queues them for its next update.
struct PriceFieldHandlers
{
    // The typed text changed; `commit` for Enter and for leaving the field other than for − or +.
    std::function<void(bool commit)> edit;
    std::function<void(PriceStepInput input)> step;
};

// Binds price_change, price_commit, price_key, price_wheel, minus and plus of auction_house.rml.
void BindPriceFieldEvents(Rml::DataModelConstructor& constructor, const PriceFieldHandlers& handlers);

// Selects the whole content of a text field that still has the focus, so typing replaces it.
void SelectContentIfFocused(Rml::Element* input);
} // namespace UI::AuctionHouse
