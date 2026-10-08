#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
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
}

void Window::OpenFilters()
{
    auto& m = m_View.GetModel();
    if (m.busy || m.review)
        return;
    m.draft = {m.family,       m.ancientSet, m.classLine, m.option2, m.upgradeMin, m.upgradeMax,
               m.minExcellent, m.minSockets, m.maxPrice,  m.luck,    m.skill};
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
    auto& m = m_View.GetModel();
    m.family = m.draft.family;
    m.ancientSet = m.draft.ancientSet;
    m.classLine = m.draft.classLine;
    m.option2 = m.draft.option;
    m.upgradeMin = m.draft.upgradeMin;
    m.upgradeMax = m.draft.upgradeMax;
    m.minExcellent = m.draft.minExcellent;
    m.minSockets = m.draft.minSockets;
    m.maxPrice = m.draft.maxPrice;
    m.luck = m.draft.luck;
    m.skill = m.draft.skill;
    DismissFilters();
    FilterChanged();
}

void Window::ClearFilter(const Rml::String& key)
{
    auto& m = m_View.GetModel();
    if (key == "family")
        m.family = "0";
    else if (key == "set")
        m.ancientSet.clear();
    else if (key == "class")
        m.classLine = "255";
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
    const std::array classes = {"Dark Wizard", "Dark Knight", "Fairy Elf",   "Magic Gladiator",
                                "Dark Lord",   "Summoner",    "Rage Fighter"};
    for (std::size_t i = 0; i < classes.size(); ++i)
        if (m.classLine == std::to_string(i))
            chips.push_back({"class", classes[i]});
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
