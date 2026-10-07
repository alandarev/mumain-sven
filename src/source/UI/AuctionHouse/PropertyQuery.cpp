#include "UI/AuctionHouse/Presentation.h"
#include <algorithm>
#include <cctype>

namespace UI::AuctionHouse
{
namespace
{
std::string Lower(std::string_view text)
{
    std::string result(text);
    for (auto& ch : result)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return result;
}

bool RemovePhrase(std::string& text, std::string_view phrase)
{
    const auto lower = Lower(text), needle = Lower(phrase);
    for (auto pos = lower.find(needle); pos != std::string::npos; pos = lower.find(needle, pos + 1))
    {
        const auto end = pos + needle.size();
        if ((pos && std::isalnum(static_cast<unsigned char>(lower[pos - 1]))) ||
            (end < lower.size() && std::isalnum(static_cast<unsigned char>(lower[end]))))
            continue;
        text.replace(pos, needle.size(), " ");
        return true;
    }
    return false;
}
} // namespace

bool ParsePropertyQuery(Market::SearchFilter& filter, std::string& name)
{
    for (std::size_t i = 1; i < Options().size(); ++i)
    {
        const auto& option = Options()[i];
        auto shortLabel = option.label.substr(0, option.label.find(" +"));
        bool matched = false;
        while (RemovePhrase(name, option.label) || RemovePhrase(name, shortLabel))
            matched = true;
        if (!matched)
            continue;
        filter.slotMask &= option.slots;
        filter.excellentMask |= option.mask;
        if (filter.family == 0)
            filter.family = 2;
        if (!filter.slotMask)
            return false;
    }
    while (RemovePhrase(name, "Luck"))
        filter.flags |= 2;
    while (RemovePhrase(name, "Skill"))
        filter.flags |= 4;
    std::string cleaned;
    for (const char ch : name)
    {
        if (ch == ' ' || ch == '&' || ch == ',')
        {
            if (!cleaned.empty() && cleaned.back() != ' ')
                cleaned += ' ';
        }
        else
            cleaned += ch;
    }
    if (!cleaned.empty() && cleaned.back() == ' ')
        cleaned.pop_back();
    name = std::move(cleaned);
    return true;
}

bool MatchesGoodsName(std::string_view name, Category category, std::string_view query)
{
    const auto lower = Lower(name);
    const bool jewel = lower.find("jewel") != std::string::npos || lower.find("gem") != std::string::npos;
    if (category == Category::Equipment || (category == Category::Jewels) != jewel)
        return false;
    const auto terms = Lower(query);
    for (std::size_t pos = 0; pos < terms.size();)
    {
        const auto end = terms.find(' ', pos);
        const auto term = terms.substr(pos, end == std::string::npos ? end : end - pos);
        if (!term.empty() && lower.find(term) == std::string::npos)
            return false;
        if (end == std::string::npos)
            break;
        pos = end + 1;
    }
    return true;
}
} // namespace UI::AuctionHouse
