#pragma once

#include "UI/AuctionHouse/Presentation.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace UI::AuctionHouse
{
// The Filters dialog's fields; the dialog edits a copy and applies it only when it is valid.
struct AdvancedFilters
{
    std::string family = "0", ancientSet, classLine = "255", option = "0";
    std::string upgradeMin = "0", upgradeMax = "15", minExcellent = "0", minSockets = "0", maxPrice;
    bool luck = false, skill = false;
};

// One Browse category's query: the name text, the toolbar choices and the advanced filters.
struct BrowseQuery
{
    std::string search, equipment = "0", itemType = "0", option = "0", sort = "0";
    AdvancedFilters filters;
    bool usable = false;
};

// The family values of the search request (`custom-packets.md`, Family).
inline constexpr std::string_view AnyFamily = "0", NormalFamily = "1", ExcellentFamily = "2", AncientFamily = "3",
                                  SocketFamily = "4";
inline constexpr std::string_view AnyClassLine = "255";

// Whether anything besides the name text and the sort order narrows the query.
bool HasConstraints(const BrowseQuery& query);
// A clean query for the same base item and upgrade level: every other filter cleared, Can equip off.
BrowseQuery ComparisonQuery(std::string_view baseName, std::uint8_t level);
// The empty result names the active search: 'No jewels match "Dragon Armor"'.
std::string EmptyResultText(Category category, const BrowseQuery& query);
// Choices that only mean something for a family are cleared with it: removing or changing the family
// clears the set and socket constraints, and Normal clears the excellent options.
void ClearFamilyDependants(AdvancedFilters& filters);
// Why the Filters dialog cannot be applied, or empty when it can. `toolbarOption` is the Option choice
// beside the search field, which already asks for an excellent option.
std::string FilterProblem(const AdvancedFilters& filters, bool toolbarOption, Market::Zen maxPriceLimit);
// "Dark Knight (including evolutions)"; empty for any class or an unknown value.
std::string ClassLineLabel(std::string_view classLine);
} // namespace UI::AuctionHouse
