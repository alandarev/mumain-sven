#pragma once

#include "UI/AuctionHouse/BrowseQuery.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace UI::AuctionHouse
{
// The item being priced in the Sell tab while Browse compares its market (D9).
struct MarketComparison
{
    // The priced item as the Sell tab names it ("Excellent Dragon Armor +9") and its base item.
    std::string subject;
    std::uint16_t type = 0;
    std::uint8_t level = 0;
    std::string baseName;
    // The inventory item to select again when the player goes back to selling.
    int sellSlot = -1;
    std::uint32_t sellKey = 0;
};

// Browse keeps one query per category (D10) and, while a market comparison runs, the player's own
// queries from before it, so that ending the comparison gives them back.
class BrowseState
{
public:
    Category Active() const
    {
        return m_Active;
    }
    // Keeps `live` as the active category's query and returns the saved query of `category`.
    const BrowseQuery& SwitchTo(Category category, const BrowseQuery& live);
    // Keeps the player's queries, then makes Equipment active with the clean same-base query.
    const BrowseQuery& BeginComparison(const BrowseQuery& live, MarketComparison comparison);
    // Restores the queries and the category from before the comparison; returns the active query.
    const BrowseQuery& EndComparison();
    const std::optional<MarketComparison>& Comparison() const
    {
        return m_Comparison;
    }
    // The comparison banner belongs to the Equipment list it filled.
    bool ComparisonShown() const
    {
        return m_Comparison && m_Active == Category::Equipment;
    }
    // The comparison uses its exact base item while the name text is still the base item's name.
    std::optional<std::uint16_t> ComparedType(std::string_view search) const;

private:
    static std::size_t Index(Category category)
    {
        return static_cast<std::size_t>(category);
    }

    std::array<BrowseQuery, 3> m_Queries;
    Category m_Active = Category::Equipment;
    std::optional<MarketComparison> m_Comparison;
    std::array<BrowseQuery, 3> m_QueriesBeforeComparison;
    Category m_ActiveBeforeComparison = Category::Equipment;
};
} // namespace UI::AuctionHouse
