#include "GameLogic/AuctionHouse/AuctionHouseNameSearch.h"
#include "GameLogic/AuctionHouse/AuctionHousePacketFormat.h"

#include <algorithm>
#include <cwctype>

namespace GameLogic::AuctionHouse
{
namespace
{
bool MatchesWords(std::wstring_view name, const std::wstring& queryText)
{
    std::wstring_view query(queryText);
    while (!query.empty())
    {
        const auto first = query.find_first_not_of(L' ');
        if (first == std::wstring_view::npos)
            return true;
        query.remove_prefix(first);
        const auto end = query.find(L' ');
        const auto term = query.substr(0, end);
        const auto found = std::search(name.begin(), name.end(), term.begin(), term.end(),
                                       [](wchar_t a, wchar_t b) { return std::towlower(a) == std::towlower(b); });
        if (found == name.end())
            return false;
        if (end == std::wstring_view::npos)
            return true;
        query.remove_prefix(end);
    }
    return true;
}
} // namespace

NameMatch ResolveNames(std::wstring_view text, std::uint16_t slotMask, std::uint16_t groupMask,
                       std::span<const ItemName> catalogue, std::vector<std::uint16_t>& out)
{
    out.clear();
    const std::wstring query(text);
    for (const auto& item : catalogue)
    {
        if (item.name.empty() || item.type >= 16 * 512 || (slotMask != 0 && (item.slotMask & slotMask) == 0) ||
            (groupMask != 0 && (groupMask & (1u << (item.type / 512))) == 0))
            continue;
        if (!MatchesWords(item.name, query))
            continue;
        out.push_back(item.type);
        if (out.size() > MaximumNameMatches)
        {
            out.clear();
            return NameMatch::TooBroad;
        }
    }
    return out.empty() ? NameMatch::NoMatch : NameMatch::Ok;
}
} // namespace GameLogic::AuctionHouse
