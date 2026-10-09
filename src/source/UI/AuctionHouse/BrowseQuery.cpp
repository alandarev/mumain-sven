#include "UI/AuctionHouse/BrowseQuery.h"

#include "UI/AuctionHouse/PriceEntry.h"

#include <array>
#include <charconv>

namespace UI::AuctionHouse
{
namespace
{
constexpr std::string_view NoChoice = "0";
constexpr std::string_view LowestUpgrade = "0", HighestUpgrade = "15";
constexpr std::array<std::string_view, 7> ClassLines = {"Dark Wizard", "Dark Knight", "Fairy Elf",   "Magic Gladiator",
                                                        "Dark Lord",   "Summoner",    "Rage Fighter"};

std::string_view CategoryNoun(Category category)
{
    switch (category)
    {
    case Category::Jewels:
        return "jewels";
    case Category::Materials:
        return "materials";
    default:
        return "equipment";
    }
}

std::optional<int> Upgrade(std::string_view text)
{
    int value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        return std::nullopt;
    return value;
}
} // namespace

bool HasConstraints(const BrowseQuery& query)
{
    const auto& f = query.filters;
    return query.usable || query.equipment != NoChoice || query.itemType != NoChoice || query.option != NoChoice ||
           f.family != AnyFamily || !f.ancientSet.empty() || f.classLine != AnyClassLine || f.option != NoChoice ||
           f.upgradeMin != LowestUpgrade || f.upgradeMax != HighestUpgrade || f.minExcellent != NoChoice ||
           f.minSockets != NoChoice || !f.maxPrice.empty() || f.luck || f.skill;
}

BrowseQuery ComparisonQuery(std::string_view baseName, std::uint8_t level)
{
    BrowseQuery query;
    query.search = baseName;
    query.filters.upgradeMin = query.filters.upgradeMax = std::to_string(level);
    return query;
}

std::string EmptyResultText(Category category, const BrowseQuery& query)
{
    const auto noun = std::string(CategoryNoun(category));
    // "Equipment" is a mass noun: "No equipment matches", but "No jewels match".
    const char* verb = category == Category::Equipment ? " matches" : " match";
    if (!query.search.empty())
        return "No " + noun + verb + " \"" + query.search + "\"";
    if (category == Category::Equipment && HasConstraints(query))
        return "No " + noun + verb + " these filters";
    return "No " + noun + " listed";
}

void ClearFamilyDependants(AdvancedFilters& filters)
{
    if (filters.family != AncientFamily)
        filters.ancientSet.clear();
    if (filters.family != SocketFamily)
        filters.minSockets = NoChoice;
    if (filters.family == NormalFamily)
    {
        filters.option = NoChoice;
        filters.minExcellent = NoChoice;
    }
}

std::string FilterProblem(const AdvancedFilters& filters, bool toolbarOption, const Market::Rules& rules)
{
    const auto low = Upgrade(filters.upgradeMin), high = Upgrade(filters.upgradeMax);
    if (!low || !high)
        return "Choose an upgrade range.";
    if (*low > *high)
        return "Upgrade min +" + filters.upgradeMin + " is above max +" + filters.upgradeMax + ".";
    if (!filters.maxPrice.empty())
    {
        const auto price = ParseZen(filters.maxPrice, rules.maxPrice);
        if (!price || *price == 0)
        {
            const auto help = DescribePriceHelp(rules);
            return "Max price: enter a whole number or " + (help.examples.empty() ? "590k" : help.examples) +
                   ", up to " + ExactZen(rules.maxPrice) + ".";
        }
    }
    if (filters.family == NormalFamily &&
        (toolbarOption || filters.option != NoChoice || filters.minExcellent != NoChoice))
        return "Normal items have no excellent options. Clear the Option choice or pick another family.";
    return {};
}

std::string ClassLineLabel(std::string_view classLine)
{
    for (std::size_t i = 0; i < ClassLines.size(); ++i)
        if (classLine == std::to_string(i))
            return std::string(ClassLines[i]) + " (including evolutions)";
    return {};
}
} // namespace UI::AuctionHouse
