#pragma once

#include "UI/AuctionHouse/PriceEntry.h"

#include <optional>

namespace Rml
{
class Element;
class Event;
} // namespace Rml

namespace UI::AuctionHouse
{
// Up and Down on the price field; Shift moves ten valid prices.
std::optional<PriceStepInput> PriceStepFromKey(const Rml::Event& event);
// The wheel steps only over the focused price field, one valid price per notch (ten with Shift).
std::optional<PriceStepInput> PriceStepFromWheel(const Rml::Event& event);
// − and + clicks: ten valid prices with Shift.
unsigned PriceStepsFromClick(const Rml::Event& event);
// Selects the whole content of a text field that still has the focus, so typing replaces it.
void SelectContentIfFocused(Rml::Element* input);
} // namespace UI::AuctionHouse
