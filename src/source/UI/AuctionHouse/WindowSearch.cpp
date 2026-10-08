#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include "Core/Utilities/StringUtils.h"
#include <charconv>
#include <cctype>

namespace UI::AuctionHouse
{
namespace
{
constexpr int AnyClassLineValue = 0xFF;
constexpr std::uint8_t AncientFamilyValue = 3;
// The search request's Flags (`custom-packets.md`): can equip now, luck, skill.
constexpr std::uint8_t UsableFlag = 1, LuckFlag = 2, SkillFlag = 4;

std::optional<int> Number(const Rml::String& text, int maximum)
{
    int value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value < 0 || value > maximum)
        return {};
    return value;
}

} // namespace

bool Window::CompileFilter()
{
    auto& model = m_View.GetModel();
    model.loading = false;
    m_Filter = {};
    const auto category = Number(model.category, 2), equipment = Number(model.equipment, 4);
    const auto option2 = Number(model.option2, static_cast<int>(Options().size()) - 1);
    const auto option = Number(model.option, static_cast<int>(Options().size()) - 1);
    const auto family = Number(model.family, 4), classLine = Number(model.classLine, 255), sort = Number(model.sort, 3);
    const auto low = Number(model.upgradeMin, 15), high = Number(model.upgradeMax, 15);
    const auto excellent = Number(model.minExcellent, 6), sockets = Number(model.minSockets, 5);
    if (!category || !equipment || !option || !option2 || !family || !classLine || !sort || !low || !high ||
        !excellent || !sockets || *low > *high || (*classLine > 6 && *classLine != 255))
    {
        SetStatus("Check your filters.");
        m_Dirty = true;
        return false;
    }
    m_Filter.sort = *sort;
    m_Filter.classLine = *classLine;
    m_Filter.family = *family;
    m_Filter.upgradeMin = *low;
    m_Filter.upgradeMax = *high;
    m_Filter.minExcellentCount = *excellent;
    m_Filter.minSockets = *sockets;
    if (!model.maxPrice.empty())
    {
        const auto amount = ParseZen(model.maxPrice, Market::GetState().GetRules().maxPrice);
        if (!amount || !*amount)
        {
            SetStatus("Enter a valid maximum price.");
            m_Dirty = true;
            return false;
        }
        m_Filter.maxPrice = *amount;
    }
    model.goods = *category != 0;
    model.types.clear();
    model.types.push_back({"0", "All types"});
    std::string note;
    if (!model.goods ? !CompileEquipmentFilter(*equipment, note) : !CompileGoodsTypes(model.search, *category))
        return false;
    m_Filter.flags = (model.usable ? UsableFlag : 0) | (model.luck ? LuckFlag : 0) | (model.skill ? SkillFlag : 0);
    SetStatus(note, false);
    m_Dirty = true;
    return true;
}

bool Window::CompileEquipmentFilter(int equipment, std::string& note)
{
    auto& model = m_View.GetModel();
    if (!ApplyEquipmentFilter(m_Filter, static_cast<Equipment>(equipment), 0))
    {
        SetStatus("Option does not apply to this category.");
        m_Dirty = true;
        return false;
    }
    BuildTypeChoices(equipment);
    if (model.itemType != "0" && !ApplyTypeChoice())
        return false;
    model.options.clear();
    for (std::size_t i = 0; i < Options().size(); ++i)
        if (!Options()[i].mask || (Options()[i].slots & m_Filter.slotMask))
            model.options.push_back({std::to_string(i), std::string(Options()[i].label)});
    const auto lastOption = static_cast<int>(Options().size()) - 1;
    if (const auto option = Number(model.option, lastOption);
        option && *option && !(Options()[*option].slots & m_Filter.slotMask))
        model.option = "0";
    if (const auto option = Number(model.option2, lastOption);
        option && *option && !(Options()[*option].slots & m_Filter.slotMask))
        model.option2 = "0";
    std::string search = model.search;
    // A market comparison searches its exact base item, not every item whose name contains it.
    const auto comparedType = m_Browse.ComparedType(search);
    const auto secondOption = Number(model.option2, lastOption), selectedOption = Number(model.option, lastOption);
    if (!selectedOption || !secondOption || !ApplyEquipmentFilter(m_Filter, Equipment::All, *secondOption) ||
        !ApplyEquipmentFilter(m_Filter, Equipment::All, *selectedOption) ||
        (!comparedType && !ParsePropertyQuery(m_Filter, search)))
    {
        SetStatus("No matches.");
        m_Dirty = true;
        return false;
    }
    note = ClearUnavailableFilters();
    if (comparedType)
        m_Filter.entries.push_back({*comparedType, 0});
    else if (!ResolveSearchNames(search))
        return false;
    return ResolveSearchSet();
}

bool Window::ResolveSearchNames(const std::string& search)
{
    if (search.empty())
        return true;
    std::vector<std::uint16_t> matches;
    const auto result =
        Market::ResolveItemTypes(StringUtils::NarrowToWide(search), m_Filter.slotMask, m_Filter.groupMask, matches);
    if (result != Market::NameMatch::Ok)
    {
        // An unknown name leaves the list empty, and the empty list names the search.
        SetStatus(result == Market::NameMatch::TooBroad ? "Narrow the item name." : "");
        m_Dirty = true;
        return false;
    }
    for (auto type : matches)
        m_Filter.entries.push_back({type, 0});
    return true;
}

bool Window::ResolveSearchSet()
{
    const auto& model = m_View.GetModel();
    if (model.ancientSet.empty())
        return true;
    std::vector<Market::SearchEntry> set;
    const auto result = Market::ResolveAncientSet(StringUtils::NarrowToWide(model.ancientSet), m_Filter.slotMask,
                                                  m_Filter.groupMask, set);
    if (result != Market::NameMatch::Ok)
    {
        SetStatus(result == Market::NameMatch::TooBroad ? "Choose a set item type." : "No matches.");
        m_Dirty = true;
        return false;
    }
    m_Filter.entries.insert(m_Filter.entries.end(), set.begin(), set.end());
    if (m_Filter.family == 0)
        m_Filter.family = AncientFamilyValue;
    return true;
}

// Choices that depend on the equipment and type choice are cleared when that choice rules them out.
std::string Window::ClearUnavailableFilters()
{
    auto& model = m_View.GetModel();
    std::string note;
    model.skillApplies = Market::SkillApplies(m_Filter.slotMask, m_Filter.groupMask);
    if (!model.skillApplies && model.skill)
    {
        model.skill = false;
        note = "Skill filter removed: this equipment has no skill.";
    }
    if (!model.ancientSet.empty())
    {
        const auto line =
            static_cast<std::uint8_t>(Number(model.classLine, AnyClassLineValue).value_or(AnyClassLineValue));
        const auto sets = Market::AncientSets(m_Filter.slotMask, m_Filter.groupMask, line);
        if (std::find(sets.begin(), sets.end(), StringUtils::NarrowToWide(model.ancientSet)) == sets.end())
        {
            model.ancientSet.clear();
            note += (note.empty() ? "" : " ") + std::string("Set filter removed: none of its items fit this choice.");
        }
    }
    if (!note.empty())
        RefreshFilterSummary();
    return note;
}

void Window::BuildTypeChoices(int equipment)
{
    auto& choices = m_View.GetModel().types;
    auto add = [&choices](int mask, const char* label) { choices.push_back({std::to_string(mask), label}); };
    if (equipment == 1)
    {
        // Group selections use a negative marker in markup, avoiding an ambiguous slot/group bit.
        choices.push_back({"g0", "Swords"});
        choices.push_back({"g1", "Axes"});
        choices.push_back({"g2", "Maces / scepters"});
        choices.push_back({"g3", "Spears"});
        choices.push_back({"g4", "Bows / crossbows"});
        choices.push_back({"g5", "Staffs / sticks"});
        choices.push_back({"g6", "Shields"});
    }
    else if (equipment == 2)
    {
        add(1 << 2, "Helms");
        add(1 << 3, "Armor");
        add(1 << 4, "Pants");
        add(1 << 5, "Gloves");
        add(1 << 6, "Boots");
    }
    else if (equipment == 4)
    {
        add(1 << 8, "Pendants");
        add(1 << 9, "Rings");
    }
}

bool Window::ApplyTypeChoice()
{
    const auto& value = m_View.GetModel().itemType;
    const auto& choices = m_View.GetModel().types;
    if (std::none_of(choices.begin(), choices.end(), [&value](const auto& choice) { return choice.value == value; }))
    {
        SetStatus("Choose an item type.");
        m_Dirty = true;
        return false;
    }
    if (value.front() == 'g')
    {
        m_Filter.groupMask = 1u << (value.back() - '0');
        m_Filter.slotMask &= value == "g6" ? (1u << 1) : (1u << 0);
    }
    else
    {
        const auto mask = Number(value, 0x3ff);
        if (mask)
            m_Filter.slotMask &= *mask;
        if (!mask || !m_Filter.slotMask)
        {
            SetStatus("Option does not apply to this type.");
            m_Dirty = true;
            return false;
        }
    }
    return true;
}

// The set list follows the chosen equipment and class; a set outside it is no longer a choice.
void Window::RefreshSetChoices(const Rml::String& classLine)
{
    auto& m = m_View.GetModel();
    const auto line = Number(classLine, AnyClassLineValue).value_or(AnyClassLineValue);
    m.sets.clear();
    m.sets.push_back({"", "Any set"});
    for (const auto& set : Market::AncientSets(m_Filter.slotMask, m_Filter.groupMask, static_cast<std::uint8_t>(line)))
    {
        const auto name = StringUtils::WideToNarrow(set.c_str());
        m.sets.push_back({name, name});
    }
    if (std::none_of(m.sets.begin(), m.sets.end(), [&m](const auto& set) { return set.value == m.draft.ancientSet; }))
        m.draft.ancientSet.clear();
    m_Dirty = true;
}

bool Window::CompileGoodsTypes(const std::string& search, int category)
{
    // An empty type selector requests the server's entire goods directory. Category
    // and localized name matching are applied as every page arrives, never to one page only.
    m_GoodsTypes.clear();
    (void)search;
    (void)category;
    if (!ItemAttribute)
    {
        SetStatus("Item names unavailable.");
        return false;
    }
    return true;
}
} // namespace UI::AuctionHouse
