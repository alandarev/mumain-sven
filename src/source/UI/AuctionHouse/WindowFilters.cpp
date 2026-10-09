#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include "Core/Utilities/StringUtils.h"
#include <RmlUi/Core/Event.h>
#include <array>

namespace UI::AuctionHouse
{
void Window::BindFilterEvents(Rml::DataModelConstructor& c)
{
    auto event = [this, &c](const char* name, const std::function<void(const Rml::VariantList&)>& action)
    {
        c.BindEventCallback(name, [this, action](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                            { m_Events.push_back([action, args] { action(args); }); });
    };
    event("open_filters", [this](const auto&) { OpenFilters(); });
    event("apply_filters", [this](const auto&) { ApplyFilters(); });
    event("cancel_filters", [this](const auto&) { DismissFilters(); });
    event("clear_filter",
          [this](const auto& args)
          {
              if (!args.empty())
                  ClearFilter(args.front().template Get<Rml::String>());
          });
    event("draft_luck",
          [this](const auto&)
          {
              auto& draft = m_View.GetModel().draft;
              draft.luck = !draft.luck;
              m_Dirty = true;
          });
    event("draft_skill",
          [this](const auto&)
          {
              auto& draft = m_View.GetModel().draft;
              draft.skill = !draft.skill;
              m_Dirty = true;
          });
    event("draft_family",
          [this](const auto&)
          {
              auto& model = m_View.GetModel();
              ClearFamilyDependants(model.draft);
              model.filterError.clear();
              m_Dirty = true;
          });
    event("draft_class",
          [this](const auto&)
          {
              auto& model = m_View.GetModel();
              RefreshSetChoices(model.draft.classLine);
              model.filterError.clear();
          });
}

void Window::OpenFilters()
{
    auto& m = m_View.GetModel();
    if (m.busy || m.review)
        return;
    m.draft = AppliedFilters();
    if (!m.skillApplies)
        m.draft.skill = false;
    m.filterError.clear();
    RefreshSetChoices(m.draft.classLine);
    m.filtersOpen = true;
    m_Dirty = true;
}

void Window::DismissFilters()
{
    m_View.GetModel().filtersOpen = false;
    m_Dirty = true;
}

void Window::ApplyFilters()
{
    if (!CheckDraftFilters())
        return;
    SetAppliedFilters(m_View.GetModel().draft);
    DismissFilters();
    FilterChanged();
}

// The dialog stays open while its choices cannot be searched, and says why.
bool Window::CheckDraftFilters()
{
    auto& m = m_View.GetModel();
    auto problem = FilterProblem(m.draft, m.option != "0", Market::GetState().GetRules());
    if (problem.empty() && !m.draft.ancientSet.empty())
    {
        std::vector<Market::SearchEntry> members;
        const auto result = Market::ResolveAncientSet(StringUtils::NarrowToWide(m.draft.ancientSet), m_Filter.slotMask,
                                                      m_Filter.groupMask, members);
        if (result == Market::NameMatch::TooBroad)
            problem = "This set has too many items to search at once. Choose an equipment type first.";
        else if (result == Market::NameMatch::NoMatch)
            problem = "The chosen equipment has no items of this set.";
    }
    m.filterError = problem;
    m_Dirty = true;
    return problem.empty();
}

AdvancedFilters Window::AppliedFilters() const
{
    const auto& m = m_View.GetModel();
    return {m.family,       m.ancientSet, m.classLine, m.option2, m.upgradeMin, m.upgradeMax,
            m.minExcellent, m.minSockets, m.maxPrice,  m.luck,    m.skill};
}

void Window::SetAppliedFilters(const AdvancedFilters& filters)
{
    auto& m = m_View.GetModel();
    m.family = filters.family;
    m.ancientSet = filters.ancientSet;
    m.classLine = filters.classLine;
    m.option2 = filters.option;
    m.upgradeMin = filters.upgradeMin;
    m.upgradeMax = filters.upgradeMax;
    m.minExcellent = filters.minExcellent;
    m.minSockets = filters.minSockets;
    m.maxPrice = filters.maxPrice;
    m.luck = filters.luck;
    m.skill = filters.skill;
}

void Window::ClearFilter(const Rml::String& key)
{
    auto& m = m_View.GetModel();
    if (key == "family")
    {
        auto filters = AppliedFilters();
        filters.family = AnyFamily;
        ClearFamilyDependants(filters);
        SetAppliedFilters(filters);
    }
    else if (key == "set")
        m.ancientSet.clear();
    else if (key == "class")
        m.classLine = AnyClassLine;
    else if (key == "option")
        m.option2 = "0";
    else if (key == "upgrade")
    {
        m.upgradeMin = "0";
        m.upgradeMax = "15";
    }
    else if (key == "excellent")
        m.minExcellent = "0";
    else if (key == "sockets")
        m.minSockets = "0";
    else if (key == "price")
        m.maxPrice.clear();
    else if (key == "luck")
        m.luck = false;
    else if (key == "skill")
        m.skill = false;
    else
        return;
    FilterChanged();
}

void Window::RefreshFilterSummary()
{
    auto& m = m_View.GetModel();
    auto& chips = m.activeFilters;
    chips.clear();
    const std::array families = {"", "Normal", "Excellent", "Ancient", "Socket"};
    for (std::size_t i = 1; i < families.size(); ++i)
        if (m.family == std::to_string(i))
            chips.push_back({"family", families[i]});
    if (!m.ancientSet.empty())
        chips.push_back({"set", m.ancientSet});
    if (auto label = ClassLineLabel(m.classLine); !label.empty())
        chips.push_back({"class", std::move(label)});
    for (std::size_t i = 1; i < Options().size(); ++i)
        if (m.option2 == std::to_string(i))
            chips.push_back({"option", std::string(Options()[i].label)});
    if (m.upgradeMin != "0" || m.upgradeMax != "15")
        chips.push_back({"upgrade", "+" + m.upgradeMin + "–" + m.upgradeMax});
    if (m.luck)
        chips.push_back({"luck", "Luck"});
    if (m.skill)
        chips.push_back({"skill", "Skill"});
    if (m.minExcellent != "0")
        chips.push_back({"excellent", m.minExcellent + "+ options"});
    if (m.minSockets != "0")
        chips.push_back({"sockets", m.minSockets + "+ sockets"});
    if (!m.maxPrice.empty())
        chips.push_back({"price", "≤ " + m.maxPrice + " Zen"});
}
} // namespace UI::AuctionHouse
