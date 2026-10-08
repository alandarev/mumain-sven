#include "UI/AuctionHouse/PriceEntry.h"

#include "GameLogic/AuctionHouse/AuctionHouse.h"
#include "UI/AuctionHouse/Presentation.h"

#include <limits>

namespace UI::AuctionHouse
{
namespace
{
constexpr auto AnyZen = std::numeric_limits<Market::Zen>::max();

std::string MaximumPriceText(const Market::Rules& rules)
{
    return "The maximum price is " + ExactZen(rules.maxPrice) + " Zen.";
}

// A known lowest price as a valid starting price for an empty field.
std::optional<Market::Zen> ValidLowest(const Market::Rules& rules, Market::Zen lowest)
{
    if (lowest == 0)
        return std::nullopt;
    return Market::RoundPriceDown(rules, lowest);
}
} // namespace

void PriceEntry::Reset()
{
    m_Text.clear();
    m_Note.clear();
    m_Source = PriceSource::None;
    m_Edited = false;
}

bool PriceEntry::Edit(std::string_view text)
{
    if (text == m_Text)
        return false;
    m_Text = text;
    m_Note.clear();
    m_Source = m_Text.empty() ? PriceSource::None : PriceSource::Typed;
    m_Edited = true;
    return true;
}

void PriceEntry::Write(Market::Zen price)
{
    m_Text = ExactZen(price);
}

std::optional<Market::Zen> PriceEntry::Commit(const Market::Rules& rules)
{
    if (m_Text.empty())
        return std::nullopt;
    const auto parsed = ParseZen(m_Text, AnyZen);
    if (!parsed)
    {
        m_Note = PriceFormatHelp;
        return std::nullopt;
    }
    if (*parsed > rules.maxPrice)
    {
        m_Note = MaximumPriceText(rules);
        return std::nullopt;
    }
    const auto rounded = Market::RoundPriceDown(rules, *parsed);
    if (!rounded)
        return std::nullopt;
    Write(*rounded);
    if (*rounded != *parsed)
        m_Note = "Rounded to " + ExactZen(*rounded);
    return rounded;
}

bool PriceEntry::Step(const Market::Rules& rules, PriceStepInput input, Market::Zen lowest)
{
    const auto [up, steps] = input;
    if (steps == 0)
        return false;
    std::optional<Market::Zen> stepped;
    if (m_Text.empty())
    {
        const auto start = ValidLowest(rules, lowest);
        if (!start)
            return false;
        // The first step lands on the lowest price (+) or one step below it (−).
        stepped = up ? start : Market::PreviousPrice(rules, lowest);
        if (stepped && steps > 1)
            stepped = Market::StepPrice(rules, *stepped, up, steps - 1).value_or(*stepped);
    }
    else
    {
        const auto parsed = ParseZen(m_Text, AnyZen);
        if (!parsed || *parsed == 0)
        {
            m_Note = PriceFormatHelp;
            return false;
        }
        if (*parsed > rules.maxPrice)
            stepped = up ? std::nullopt : Market::RoundPriceDown(rules, rules.maxPrice);
        else
            stepped = Market::StepPrice(rules, *parsed, up, steps);
    }
    if (!stepped)
        return false;
    Write(*stepped);
    m_Note.clear();
    m_Source = PriceSource::Typed;
    m_Edited = true;
    return true;
}

PriceStepState PriceEntry::Steps(const Market::Rules& rules, Market::Zen lowest) const
{
    PriceStepState state;
    if (m_Text.empty())
    {
        const auto start = ValidLowest(rules, lowest);
        state.up = start.has_value();
        state.down = start && Market::PreviousPrice(rules, lowest);
        if (!state.up)
            state.reason = "Type a price first.";
        return state;
    }
    const auto parsed = ParseZen(m_Text, AnyZen);
    if (!parsed || *parsed == 0)
    {
        state.reason = PriceFormatHelp;
        return state;
    }
    if (*parsed > rules.maxPrice)
    {
        state.down = true;
        state.reason = MaximumPriceText(rules);
        return state;
    }
    state.down = Market::PreviousPrice(rules, *parsed).has_value();
    state.up = Market::NextPrice(rules, *parsed).has_value();
    if (!state.down)
        state.reason = "This is the lowest price.";
    else if (!state.up)
        state.reason = MaximumPriceText(rules);
    return state;
}

void PriceEntry::Choose(const Market::Rules& rules, Market::Zen price)
{
    Write(price);
    m_Note.clear();
    m_Source = PriceSource::Typed;
    m_Edited = true;
    Commit(rules);
}

bool PriceEntry::Prefill(const Market::Rules& rules, std::optional<std::string_view> draft, Market::Zen lowest)
{
    if (m_Edited)
        return false;
    const auto before = m_Text;
    const auto beforeSource = m_Source;
    m_Note.clear();
    if (draft && !draft->empty())
    {
        m_Text = *draft;
        m_Source = PriceSource::Typed;
        m_Edited = true;
        return true;
    }
    if (const auto valid = ValidLowest(rules, lowest))
    {
        Write(*valid);
        m_Source = PriceSource::Lowest;
    }
    else
    {
        m_Text.clear();
        m_Source = PriceSource::None;
    }
    return m_Text != before || m_Source != beforeSource;
}

std::string_view PriceSourceLabel(PriceSource source)
{
    switch (source)
    {
    case PriceSource::Lowest:
        return "Current lowest";
    default:
        return {};
    }
}

std::optional<Market::Zen> UndercutPrice(const Market::Rules& rules, Market::Zen lowest)
{
    if (lowest == 0)
        return std::nullopt;
    return Market::PreviousPrice(rules, lowest);
}
} // namespace UI::AuctionHouse
