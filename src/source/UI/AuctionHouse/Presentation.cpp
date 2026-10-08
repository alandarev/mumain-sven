#include "UI/AuctionHouse/Presentation.h"

#include <array>
#include <cctype>
#include <limits>
#include <charconv>

namespace UI::AuctionHouse
{
std::optional<Market::Zen> ParseZen(std::string_view text, Market::Zen limit)
{
    while (!text.empty() && text.front() == ' ')
        text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ')
        text.remove_suffix(1);
    if (text.empty())
        return std::nullopt;
    std::uint64_t multiplier = 1;
    if (text.size() >= 2 && std::tolower(static_cast<unsigned char>(text.back())) == 'k' &&
        std::tolower(static_cast<unsigned char>(text[text.size() - 2])) == 'k')
    {
        multiplier = 1000000;
        text.remove_suffix(2);
    }
    else if (!text.empty())
    {
        const char suffix = static_cast<char>(std::tolower(static_cast<unsigned char>(text.back())));
        if (suffix == 'k' || suffix == 'm' || suffix == 'b')
        {
            multiplier = suffix == 'k' ? 1000 : suffix == 'm' ? 1000000 : 1000000000;
            text.remove_suffix(1);
        }
    }
    if (multiplier > 1 && !text.empty() && text.back() == ' ')
        text.remove_suffix(1);
    std::uint64_t digits = 0, scale = 1;
    bool decimal = false, any = false, fraction = false;
    unsigned groupDigits = 0;
    bool grouped = false;
    for (char ch : text)
    {
        if (ch == ',' && !decimal && groupDigits > 0 && groupDigits <= 3 && (!grouped || groupDigits == 3))
        {
            grouped = true;
            groupDigits = 0;
            continue;
        }
        if (ch == '.' && !decimal && any && multiplier > 1 && (!grouped || groupDigits == 3))
        {
            decimal = true;
            continue;
        }
        if (ch < '0' || ch > '9')
            return std::nullopt;
        if (digits > (std::numeric_limits<std::uint64_t>::max() - 9) / 10)
            return std::nullopt;
        digits = digits * 10 + static_cast<unsigned>(ch - '0');
        any = true;
        if (!decimal)
            ++groupDigits;
        if (decimal)
        {
            if (scale > 100000000)
                return std::nullopt;
            scale *= 10;
            fraction = true;
        }
    }
    if (!any || (grouped && groupDigits != 3) || (decimal && !fraction) ||
        digits > std::numeric_limits<std::uint64_t>::max() / multiplier)
        return std::nullopt;
    const auto product = digits * multiplier;
    if (product % scale != 0 || product / scale > limit)
        return std::nullopt;
    return static_cast<Market::Zen>(product / scale);
}

std::optional<std::uint32_t> ParseQuantity(std::string_view text, std::uint32_t limit)
{
    std::uint32_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value == 0 || value > limit)
        return {};
    return value;
}

std::string ExactZen(Market::Zen value)
{
    auto text = std::to_string(value);
    for (int pos = static_cast<int>(text.size()) - 3; pos > 0; pos -= 3)
        text.insert(pos, ",");
    return text;
}

std::string CompactZen(Market::Zen value)
{
    const Market::Zen unit = value >= 1000000 ? 1000000 : 1000;
    if (value < 1000 || value % (unit / 100) != 0)
        return ExactZen(value);
    auto result = ExactZen(value / unit);
    auto fraction = (value % unit) / (unit / 100);
    if (fraction)
    {
        result += ".";
        if (fraction < 10)
            result += "0";
        result += std::to_string(fraction);
        if (result.back() == '0')
            result.pop_back();
    }
    return result + (unit == 1000000 ? "kk" : "k");
}

std::string ResultText(Market::Result result)
{
    using R = Market::Result;
    switch (result)
    {
    case R::Success:
        return {};
    case R::Unavailable:
        return "Market unavailable. Try again later.";
    case R::NotOpen:
        return "Talk to Zyro again.";
    case R::Busy:
        return "Please wait, then retry.";
    case R::InvalidRequest:
        return "Check your selection.";
    case R::NotFound:
        return "Listing no longer available.";
    case R::QuoteChanged:
        return "Price changed. Review the new total.";
    case R::InsufficientStock:
        return "Not enough stock. Choose a smaller quantity.";
    case R::InsufficientZen:
        return "Not enough Zen.";
    case R::NotEligible:
        return "This item cannot be listed.";
    case R::ListingLimitReached:
        return "Listing limit reached.";
    case R::PriceOutOfRange:
        return "Price exceeds the market limit.";
    case R::RulesChanged:
        return "Fees changed. Review again.";
    case R::OwnListing:
        return "This is your listing.";
    case R::Partial:
        return "Some entries remain. Free space or spend Zen, then collect again.";
    case R::PriceNotOnStep:
        return "Choose a price using − or +.";
    default:
        return "Market request failed. Retry.";
    }
}

std::span<const Option> Options()
{
    constexpr std::uint16_t offense = (1u << 0) | (1u << 8);
    constexpr std::uint16_t defense = (1u << 1) | (0x1fu << 2) | (1u << 9);
    static constexpr std::array options = {
        Option{"Any option", 0, 0},
        Option{"Defense success rate +10%", defense, 0x02},
        Option{"Reflect damage +5%", defense, 0x04},
        Option{"Damage decrease +4%", defense, 0x08},
        Option{"Maximum mana +4%", defense, 0x10},
        Option{"Maximum HP +4%", defense, 0x20},
        Option{"Zen drop +30%", defense, 0x01},
        Option{"Excellent damage rate +10%", offense, 0x20},
        Option{"Damage per level", offense, 0x10},
        Option{"Damage +2%", offense, 0x08},
        Option{"Attack speed +7", offense, 0x04},
        Option{"Life on kill", offense, 0x02},
        Option{"Mana on kill", offense, 0x01},
    };
    return options;
}

bool ApplyEquipmentFilter(Market::SearchFilter& filter, Equipment equipment, int option)
{
    static constexpr std::array<std::uint16_t, 5> slots = {0x3ff, 0x003, 0x07c, 0x080, 0x300};
    const auto index = static_cast<std::size_t>(equipment);
    if (index >= slots.size() || option < 0 || static_cast<std::size_t>(option) >= Options().size())
        return false;
    filter.slotMask = filter.slotMask ? filter.slotMask & slots[index] : slots[index];
    const auto& selected = Options()[option];
    if (selected.mask)
    {
        filter.slotMask &= selected.slots;
        filter.excellentMask |= selected.mask;
        if (filter.family == 0)
            filter.family = 2;
    }
    return filter.slotMask != 0;
}
} // namespace UI::AuctionHouse
