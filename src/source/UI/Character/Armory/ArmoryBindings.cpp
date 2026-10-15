#include "stdafx.h"
#include "ArmoryController.h"
#include "UI/Social/PhotoViewer.h"
#include "UI/Scaling/UITransform.h"
#include "UI/Core/WindowSystem.h"
#include "UI/RmlBridge/RmlTheme.h"
#include <cmath>
#include <RmlUi/Core.h>
#include <algorithm>
#include <charconv>
#include <numeric>

namespace UI::Character
{
void ArmoryController::Bind(Rml::DataModelConstructor& c)
{
    m_Handle = c.GetModelHandle();
    auto attribute = c.RegisterStruct<AttributeView>();
    attribute.RegisterMember("index", &AttributeView::index);
    attribute.RegisterMember("label", &AttributeView::label);
    attribute.RegisterMember("addition", &AttributeView::addition);
    attribute.RegisterMember("input", &AttributeView::input);
    attribute.RegisterMember("total", &AttributeView::total);
    attribute.RegisterMember("note", &AttributeView::note);
    attribute.RegisterMember("explanation", &AttributeView::explanation);
    attribute.RegisterMember("proposed", &AttributeView::proposed);
    attribute.RegisterMember("highlighted", &AttributeView::highlighted);
    c.RegisterArray<std::vector<AttributeView>>();
    auto row = c.RegisterStruct<RowView>();
    row.RegisterMember("index", &RowView::index);
    row.RegisterMember("label", &RowView::label);
    row.RegisterMember("value", &RowView::value);
    row.RegisterMember("detail", &RowView::detail);
    row.RegisterMember("expanded", &RowView::expanded);
    row.RegisterMember("highlighted", &RowView::highlighted);
    c.RegisterArray<std::vector<RowView>>();
    auto category = c.RegisterStruct<CategoryView>();
    category.RegisterMember("name", &CategoryView::name);
    category.RegisterMember("rows", &CategoryView::rows);
    c.RegisterArray<std::vector<CategoryView>>();
    c.RegisterArray<std::vector<Rml::String>>();
    c.RegisterArray<std::array<Rml::String, 3>>();
    c.RegisterArray<std::array<bool, 3>>();
    c.Bind("armory_attributes", &m_Attributes);
    c.Bind("armory_categories", &m_Categories);
    c.Bind("armory_calculation", &m_Calculation);
    c.Bind("armory_resources", &m_Resources);
    c.Bind("armory_resource_details", &m_ResourceDetails);
    c.Bind("armory_resource_expanded", &m_ResourceExpanded);
    c.Bind("armory_resource_highlighted", &m_ResourceHighlighted);
    c.Bind("armory_title", &m_Title);
    c.Bind("armory_status", &m_Status);
    c.Bind("armory_points", &m_Points);
    c.Bind("armory_budget", &m_Budget);
    c.Bind("armory_editing", &m_Editing);
    c.Bind("armory_ready", &m_Ready);
    c.Bind("armory_waiting", &m_Waiting);
    c.Bind("armory_valid", &m_Valid);
    c.Bind("armory_over_budget", &m_OverBudget);
    c.Bind("armory_ratings", &m_Ratings);
    c.Bind("armory_retry", &m_Retry);
    c.Bind("armory_skill_name", &m_SkillName);
    c.Bind("armory_skill_output", &m_SkillOutput);
    c.Bind("armory_skill_kind", &m_SkillKind);
    c.Bind("armory_costs", &m_Costs);
    c.Bind("armory_restriction", &m_Restriction);
    c.Bind("armory_target", &m_Target);
    BindActions(c);
}
void ArmoryController::BindActions(Rml::DataModelConstructor& c)
{
    c.BindEventCallback("armory_equipment_click",
                        [this](auto, Rml::Event& event, const Rml::VariantList& args)
                        {
                            const int button = event.GetParameter<int>("button", 0);
                            if (m_Shown && args.size() == 1 && button >= 0 && button <= 1 &&
                                UI::RmlBridge::ThemeUsesEmbeddedInventory())
                                g_pMyInventory->HandleArmoryEquipmentSlot(args[0].Get<int>(-1), button == 1);
                        });
    c.BindEventCallback("armory_skill_hover",
                        [this](auto, auto& event, const Rml::VariantList& args)
                        {
                            const bool hovered = !args.empty() && args[0].Get<bool>();
                            if (hovered != m_SkillHovered)
                            {
                                Hover(-1, -1);
                                m_SkillHovered = hovered;
                            }
                            if (hovered)
                            {
                                const float cursorGap =
                                    UI::Scaling::SizeY(UI::Scaling::PanelTransform(WindowWidth, WindowHeight), 24.f);
                                m_SkillTooltip.anchorX = event.template GetParameter<float>("mouse_x", 0.f);
                                m_SkillTooltip.anchorY = event.template GetParameter<float>("mouse_y", 0.f) + cursorGap;
                            }
                        });
    c.BindEventCallback("armory_resource_expand",
                        [this](auto, auto&, const Rml::VariantList& args)
                        {
                            if (!args.empty())
                            {
                                const int i = args[0].Get<int>(-1);
                                if (i >= 0 && i < 3)
                                {
                                    m_ResourceExpanded[i] = !m_ResourceExpanded[i];
                                    Dirty("armory_resource_expanded");
                                }
                            }
                        });
    c.BindEventCallback("armory_edit", [this](auto, auto&, const auto&) { Edit(); });
    c.BindEventCallback("armory_cancel", [this](auto, auto&, const auto&) { CancelEditor(); });
    c.BindEventCallback("armory_apply", [this](auto, auto&, const auto&) { Apply(); });
    c.BindEventCallback("armory_retry",
                        [this](auto, auto&, const auto&)
                        {
                            if (m_Waiting)
                                Send(true, true);
                        });
    c.BindEventCallback("armory_step",
                        [this](auto, auto&, const Rml::VariantList& args)
                        {
                            if (args.size() != 2)
                                return;
                            int index = args[0].Get<int>(-1);
                            if (index >= 0 && index < 5)
                                Change(index, m_EditRequest.additions[index] + args[1].Get<int>());
                        });
    c.BindEventCallback("armory_input",
                        [this](auto, Rml::Event& event, const Rml::VariantList& args)
                        {
                            if (args.size() != 1)
                                return;
                            const auto text = event.GetParameter<Rml::String>("value", "");
                            int value = 0;
                            auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
                            if (text.empty() || (error == std::errc{} && end == text.data() + text.size()))
                                Change(args[0].Get<int>(-1), value, text.empty());
                            else
                                RefreshEditor();
                        });
    c.BindEventCallback("armory_hover",
                        [this](auto, auto&, const Rml::VariantList& args)
                        {
                            if (args.size() == 2)
                                Hover(args[0].Get<int>(-1), args[1].Get<int>(-1));
                        });
    c.BindEventCallback("armory_attribute_hover",
                        [this](auto, auto&, const Rml::VariantList& args)
                        {
                            if (!args.empty())
                                HoverAttribute(args[0].Get<int>(-1));
                        });
    c.BindEventCallback("armory_expand",
                        [this](auto, auto&, const Rml::VariantList& args)
                        {
                            if (args.size() != 1)
                                return;
                            int index = args[0].Get<int>(-1);
                            if (index < 0 || index >= static_cast<int>(m_Snapshot.rows.size()))
                                return;
                            const auto& id = m_Snapshot.rows[index].id;
                            if (m_Expanded.contains(id))
                                m_Expanded.erase(id);
                            else
                                m_Expanded.insert(id);
                            for (auto& group : m_Categories)
                                for (auto& row : group.rows)
                                    row.expanded = m_Expanded.contains(m_Snapshot.rows[row.index].id);
                            Dirty("armory_categories");
                        });
    c.BindEventCallback("armory_ratings",
                        [this](auto, auto&, const auto&)
                        {
                            m_Ratings = !m_Ratings;
                            Dirty("armory_ratings");
                        });
}
namespace
{
Rml::String Number(std::uint32_t value)
{
    auto text = std::to_string(value);
    for (int i = static_cast<int>(text.size()) - 3; i > 0; i -= 3)
        text.insert(i, ",");
    return text;
}
} // namespace
void ArmoryController::RefreshEditor()
{
    m_Attributes.clear();
    for (const auto& attribute : m_Snapshot.attributes)
    {
        int addition = m_Editing ? m_EditRequest.additions[attribute.index] : 0;
        auto total = Number(static_cast<std::uint32_t>(std::lround(attribute.total)) + addition);
        auto bonus = static_cast<int>(std::lround(attribute.total)) - static_cast<int>(attribute.base);
        Rml::String note = bonus == 0 ? ""
                                      : Number(attribute.base) + " base · " + (bonus > 0 ? "+" : "") +
                                            std::to_string(bonus) + " equipment / effects";
        if (bonus != 0 && attribute.sources.size() == 1)
        {
            note = Number(attribute.base) + " base";
            std::set<std::string> groups;
            for (const auto& source : attribute.sources)
            {
                if (!source.group.empty() && !groups.insert(source.group).second)
                    continue;
                note += " · " + source.amount + " " + source.name;
            }
        }
        if (attribute.sources.size() > 1)
            note = Number(attribute.base) + " base · " + (bonus >= 0 ? "+" : "") + std::to_string(bonus) + " from " +
                   std::to_string(attribute.sources.size()) + " contributions";
        m_Attributes.push_back({attribute.index, addition, attribute.label, total, note,
                                "Base " + Number(attribute.base) + " · Pending +" + std::to_string(addition),
                                m_Editing ? m_EditText[attribute.index] : "0", addition > 0});
    }
    const int spent = std::accumulate(m_EditRequest.additions.begin(), m_EditRequest.additions.end(), 0);
    const auto remaining = static_cast<long long>(m_EditRequest.expectedPoints) - spent;
    m_OverBudget = remaining < 0;
    m_Budget = remaining >= 0 ? "Remaining " + Number(remaining) : "Over budget " + Number(-remaining);
    m_Valid = m_Ready && spent > 0 && remaining >= 0 && !m_Waiting;
    Dirty("armory_attributes");
    Dirty("armory_budget");
    Dirty("armory_valid");
    Dirty("armory_over_budget");
}
void ArmoryController::Edit()
{
    if (!m_Ready || m_Waiting || m_Snapshot.freePoints == 0)
        return;
    m_EditRequest = {};
    m_EditText.fill("0");
    m_EditRequest.expectedPoints = m_Snapshot.freePoints;
    m_Status.clear();
    Dirty("armory_status");
    for (const auto& attribute : m_Snapshot.attributes)
        m_EditRequest.bases[attribute.index] = attribute.base;
    m_Editing = true;
    Dirty("armory_editing");
    RefreshEditor();
}
bool ArmoryController::CancelEditor()
{
    if (!m_Editing || m_Waiting)
        return false;
    m_Editing = false;
    m_EditRequest = {};
    Dirty("armory_editing");
    RefreshEditor();
    return true;
}
void ArmoryController::Change(int index, int value, bool emptyInput)
{
    if (!m_Editing || m_Waiting || index < 0 || index >= 5)
        return;
    const auto found = std::find_if(m_Snapshot.attributes.begin(), m_Snapshot.attributes.end(),
                                    [index](const auto& attribute) { return attribute.index == index; });
    if (found == m_Snapshot.attributes.end() || found->maximum <= m_EditRequest.bases[index])
        return;
    const auto maximum = std::min<std::uint32_t>(UINT16_MAX, found->maximum - m_EditRequest.bases[index]);
    m_EditRequest.additions[index] = static_cast<std::uint16_t>(std::clamp(value, 0, static_cast<int>(maximum)));
    m_EditText[index] = emptyInput ? "" : std::to_string(m_EditRequest.additions[index]);
    RefreshEditor();
}
void ArmoryController::Apply()
{
    if (!m_Editing || !m_Valid || m_Waiting)
        return;
    if (!Send(true))
        return;
    m_Status = "Waiting for server confirmation…";
    Dirty("armory_status");
    RefreshEditor();
}
} // namespace UI::Character
