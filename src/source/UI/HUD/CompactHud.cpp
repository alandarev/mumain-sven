#include "stdafx.h"
#include "CompactHud.h"
#include "ItemHotKey.h"
#include "SkillList.h"
#include "Notices.h"
#include "UI/Core/WindowSystem.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "Core/Utilities/StringUtils.h"
#include "GameLogic/Skills/SkillManager.h"
#include "Data/GameConfig/GameConfig.h"
#include "UI/Scaling/UITransform.h"
#include "UI/Placement/WindowPlacement.h"
#include <RmlUi/Core/ElementDocument.h>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace UI::HUD
{
using mu::ui::window::CItemHotKey;
namespace
{
constexpr int FirstRecoverySlot = ActionBarProfile::ActionCount;
const std::array<Rml::String, ActionBarProfile::BindingCount> ItemElementIds{
    "action_item_view_0", "action_item_view_1", "action_item_view_2", "action_item_view_3", "action_item_view_4",
    "action_item_view_5", "action_item_view_6", "action_item_view_7", "action_item_view_8", "action_item_view_9",
    "health_item_view",   "mana_item_view",     "antidote_item_view"};
constexpr float MediumHudWidth = 754.f;
constexpr float NarrowHudWidth = 488.f;
Rml::String ItemTier(int type)
{
    if (type == ITEM_LARGE_HEALING_POTION || type == ITEM_LARGE_MANA_POTION)
        return "L";
    if (type == ITEM_MEDIUM_HEALING_POTION || type == ITEM_MEDIUM_MANA_POTION)
        return "M";
    if (type == ITEM_SMALL_HEALING_POTION || type == ITEM_SMALL_MANA_POTION)
        return "S";
    return {};
}
} // namespace
CompactHud::CompactHud() : m_Items(std::make_unique<ITEM[]>(ActionBarProfile::BindingCount))
{
    m_Model.slots.resize(ActionBarProfile::ActionCount);
    for (int i = 0; i < ActionBarProfile::BindingCount; ++i)
    {
        m_Items[i].Type = -1;
        m_Targets[i] = std::make_unique<RmlBridge::RenderTarget>(
            [this, i](std::uint32_t w, std::uint32_t h)
            { CItemHotKey::RenderItemModel(&m_Items[i], w, h, false, true); });
    }
}
CompactHud::~CompactHud() = default;
void CompactHud::SetShown(bool shown)
{
    m_Shown = shown && RmlBridge::ThemeUsesCompactHud();
    UI::Notices::SetHudDialogOpen(m_Shown && (m_Model.pickerOpen || m_Model.keysOpen));
    if (!m_Shown)
    {
        for (auto& target : m_Targets)
            target->SetEnabled(false);
        for (auto& [key, target] : m_ChoiceTargets)
            target->SetEnabled(false);
    }
}
void CompactHud::Reset()
{
    m_Life.Reset();
    m_Mana.Reset();
    m_Ag.Reset();
    m_Experience.Reset();
    m_Maxima = {};
    m_SlotInitialized.fill(false);
    m_CostState = {-1, -1, -1};
    m_XpState = {};
    m_Model.pickerOpen = m_Model.keysOpen = false;
    UI::Notices::SetHudDialogOpen(false);
    Dirty("compact_picker_open");
    Dirty("compact_keys_open");
    for (auto& target : m_Targets)
        target->SetEnabled(false);
    for (auto& [key, target] : m_ChoiceTargets)
        target->SetEnabled(false);
}
void CompactHud::SyncMotion(float hp, float mp, float ag, float xp, int level, bool master,
                            const std::array<int, 3>& maxima)
{
    if (maxima != m_Maxima)
    {
        m_Life.Reset();
        m_Mana.Reset();
        m_Ag.Reset();
        m_Maxima = maxima;
    }
    const double now =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const bool snapResources = m_Profile.reducedMotion || Hero->Dead != 0;
    const auto life = m_Life.Update(hp, now, snapResources);
    const auto mana = m_Mana.Update(mp, now, snapResources);
    const auto ability = m_Ag.Update(ag, now, snapResources);
    const auto experience = m_Experience.Update(xp, level, master, now, m_Profile.reducedMotion);
    auto sync = [this](float& field, float value, const char* name)
    {
        if (field != value)
        {
            field = value;
            Dirty(name);
        }
    };
    sync(m_Model.hpTrail, life.trail, "hp_trail_fraction");
    sync(m_Model.mpTrail, mana.trail, "mp_trail_fraction");
    sync(m_Model.hpSweep, life.sweep, "hp_heal_sweep");
    sync(m_Model.hpSweepOpacity, life.sweepOpacity, "hp_heal_opacity");
    sync(m_Model.mpSweep, mana.sweep, "mp_heal_sweep");
    sync(m_Model.mpSweepOpacity, mana.sweepOpacity, "mp_heal_opacity");
    sync(m_Model.agTrail, ability.trail, "ag_trail_fraction");
    sync(m_Model.agVisual, ability.value, "ag_visual_fraction");
    sync(m_Model.xpVisual, experience.value, "xp_visual_fraction");
    sync(m_Model.xpGainStart, experience.gainStart, "xp_gain_start");
    sync(m_Model.xpGain, experience.gainFraction, "xp_gain_fraction");
    sync(m_Model.xpGainOpacity, experience.gainOpacity, "xp_gain_opacity");
    if (m_Model.xpLevelUp != experience.levelUp)
    {
        m_Model.xpLevelUp = experience.levelUp;
        Dirty("xp_level_up");
    }
}
void CompactHud::SyncCost(float ag)
{
    const int skill =
        Hero->CurrentSkill >= 0 && Hero->CurrentSkill < MAX_MAGIC ? CharacterAttribute->Skill[Hero->CurrentSkill] : 0;
    if (skill != m_CostSkill)
    {
        m_CostSkill = skill;
        m_SelectedAgCost = 0;
        int manaCost = 0;
        if (skill != 0)
            gSkillManager.GetSkillInformation(skill, 1, nullptr, &manaCost, nullptr, &m_SelectedAgCost);
    }
    const std::array<int, 3> state{static_cast<int>(CharacterAttribute->SkillMana), m_Maxima[2], skill};
    if (state == m_CostState)
        return;
    m_CostState = state;
    m_Model.agCost = m_Maxima[2] > 0 ? std::clamp(static_cast<float>(m_SelectedAgCost) / m_Maxima[2], 0.f, 1.f) : 0.f;
    m_Model.agCostStart = std::max(0.f, ag - m_Model.agCost);
    m_Model.agInsufficient = CharacterAttribute->SkillMana < m_SelectedAgCost;
    m_Model.agTooltip = "AG " + std::to_string(CharacterAttribute->SkillMana) + " / " + std::to_string(m_Maxima[2]) +
                        " | Selected skill: " + std::to_string(m_SelectedAgCost) + " AG";
    Dirty("ag_cost_fraction");
    Dirty("ag_cost_start");
    Dirty("ag_insufficient");
    Dirty("compact_ag_tooltip");
}
void CompactHud::SyncItemTarget(int index, Rml::ElementDocument* document)
{
    auto* icon = document->GetElementById(ItemElementIds[index]);
    auto& target = *m_Targets[index];
    const bool filled = m_Profile.bindings[index].kind == ActionBarProfile::Kind::Item && m_Items[index].Type >= 0;
    const bool visible = m_Shown && filled && icon != nullptr && icon->GetParentNode()->IsVisible();
    target.SetEnabled(visible);
    if (icon == nullptr)
    {
        target.BindImage(nullptr);
        return;
    }
    target.BindImage(icon, visible);
}
void CompactHud::SyncActions(Rml::ElementDocument* document)
{
    bool changed = false;
    for (int i = 0; i < ActionBarProfile::BindingCount; ++i)
    {
        auto& slot = i < FirstRecoverySlot ? m_Model.slots[i] : m_RecoverySlots[i - FirstRecoverySlot];
        if (SyncSlot(i, slot))
        {
            if (i < FirstRecoverySlot)
                changed = true;
            else
            {
                const int recoveryIndex = i - FirstRecoverySlot;
                auto& recovery = m_Model.recovery[recoveryIndex];
                recovery.usable = slot.usable;
                recovery.key = slot.key;
                recovery.count = slot.count;
                recovery.tier = ItemTier(m_Items[i].Type);
                recovery.tooltip = slot.tooltip;
                const std::array<const char*, 3> prefixes{"hp", "mp", "antidote"};
                for (const auto* suffix : {"_usable", "_key", "_count", "_tier"})
                    Dirty((Rml::String(prefixes[recoveryIndex]) + suffix).c_str());
                Dirty(recoveryIndex == 0   ? "hp_item_tooltip"
                      : recoveryIndex == 1 ? "mp_item_tooltip"
                                           : "antidote_tooltip");
            }
        }
        SyncItemTarget(i, document);
    }
    if (changed)
        Dirty("slots");
}
void CompactHud::SyncLayout()
{
    const float unit = RmlBridge::ThemeHudUnitPixels(WindowHeight);
    const bool scaleChanged = m_Model.unitPx != unit;
    if (scaleChanged)
    {
        m_Model.unitPx = unit;
        Dirty("compact_unit_px");
    }
    const float availableWidth = WindowWidth / unit;
    const bool medium = availableWidth <= MediumHudWidth;
    const bool narrow = availableWidth <= NarrowHudWidth;
    const bool layoutChanged = m_Model.medium != medium || m_Model.narrow != narrow;
    if (m_Model.medium != medium)
    {
        m_Model.medium = medium;
        Dirty("compact_medium");
    }
    if (m_Model.narrow != narrow)
    {
        m_Model.narrow = narrow;
        Dirty("compact_narrow");
    }
    if (scaleChanged || layoutChanged)
        UI::Placement::Invalidate();
}
void CompactHud::Sync(Rml::ElementDocument* document, float hp, float mp, float ag, float xp, int level, bool master,
                      const std::array<int, 3>& maxima)
{
    if (document == nullptr || !RmlBridge::ThemeUsesCompactHud() || Hero == nullptr || Hero->ID[0] == 0)
        return;
    SyncLayout();
    UI::Notices::SetHudDialogOpen(m_Shown && (m_Model.pickerOpen || m_Model.keysOpen));
    EnsureProfile();
    if (!m_ProfileSaved)
        ImportLegacyHotkeys();
    SyncMotion(hp, mp, ag, xp, level, master, maxima);
    SyncCost(ag);
    const bool masterXp = master;
    const auto current = masterXp ? Master_Level_Data.lMasterLevel_Experince : CharacterAttribute->Experience;
    const auto next = masterXp ? Master_Level_Data.lNext_MasterLevel_Experince : CharacterAttribute->NextExperience;
    const std::array<unsigned long long, 3> xpState{current, next, masterXp};
    if (xpState != m_XpState || m_Model.xpTooltip.empty())
    {
        m_XpState = xpState;
        m_Model.xpTooltip = "Experience " + std::to_string(current) + " / " + std::to_string(next) + " | " +
                            std::to_string(next > current ? next - current : 0) + " remaining" + " (" +
                            std::to_string(static_cast<int>(xp * 100.f)) + "%)";
        Dirty("xp_tooltip");
    }
    SyncActions(document);
    if (m_Model.pickerOpen && std::chrono::steady_clock::now() - m_ChoicesUpdated >= std::chrono::milliseconds(125))
        RebuildChoices();
    const float available = WindowHeight / m_Model.unitPx - (m_Model.medium ? 178.f : 266.f);
    const float rows = static_cast<float>((m_Model.choices.size() + 1) / 2);
    const float height = std::max(48.f, std::min({rows * 48.f + 2.f, m_Model.medium ? 150.f : 240.f, available}));
    if (m_Model.choiceHeight != height)
    {
        m_Model.choiceHeight = height;
        Dirty("compact_choice_height");
    }
    SyncChoiceTargets(document);
}
void CompactHud::SyncChoiceTargets(Rml::ElementDocument* document)
{
    for (auto& [key, target] : m_ChoiceTargets)
    {
        const auto id = "choice_item_" + std::to_string(key.first) + "_" + std::to_string(key.second);
        auto* image = document->GetElementById(id);
        const bool visible = m_Shown && m_Model.pickerOpen && m_Model.consumablesTab && image && image->IsVisible();
        target->SetEnabled(visible);
        target->BindImage(image, visible);
    }
}
} // namespace UI::HUD
