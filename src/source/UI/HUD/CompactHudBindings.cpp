#include "stdafx.h"
#include "CompactHud.h"
#include <RmlUi/Core/Event.h>

namespace UI::HUD
{
void CompactHud::Dirty(const char* name)
{
    if (m_Handle)
        m_Handle.DirtyVariable(name);
}
void CompactHud::Bind(Rml::DataModelConstructor& c)
{
    m_Handle = c.GetModelHandle();
    c.Bind("compact_unit_px", &m_Model.unitPx);
    c.Bind("compact_medium", &m_Model.medium);
    c.Bind("compact_narrow", &m_Model.narrow);
    auto slot = c.RegisterStruct<Slot>();
    slot.RegisterMember("index", &Slot::index);
    slot.RegisterMember("item", &Slot::item);
    slot.RegisterMember("selected", &Slot::selected);
    slot.RegisterMember("usable", &Slot::usable);
    slot.RegisterMember("cooldown", &Slot::cooldown);
    slot.RegisterMember("icon", &Slot::icon);
    slot.RegisterMember("key", &Slot::key);
    slot.RegisterMember("count", &Slot::count);
    slot.RegisterMember("cooldown_text", &Slot::cooldownText);
    slot.RegisterMember("tooltip", &Slot::tooltip);
    c.RegisterArray<std::vector<Slot>>();
    c.Bind("slots", &m_Model.slots);
    c.Bind("compact_key_slots", &m_Model.keySlots);
    auto choice = c.RegisterStruct<Choice>();
    choice.RegisterMember("kind", &Choice::kind);
    choice.RegisterMember("identity", &Choice::identity);
    choice.RegisterMember("item_level", &Choice::itemLevel);
    choice.RegisterMember("name", &Choice::name);
    choice.RegisterMember("icon", &Choice::icon);
    c.RegisterArray<std::vector<Choice>>();
    c.Bind("compact_choices", &m_Model.choices);
    c.Bind("hp_trail_fraction", &m_Model.hpTrail);
    c.Bind("mp_trail_fraction", &m_Model.mpTrail);
    c.Bind("hp_heal_sweep", &m_Model.hpSweep);
    c.Bind("hp_heal_opacity", &m_Model.hpSweepOpacity);
    c.Bind("mp_heal_sweep", &m_Model.mpSweep);
    c.Bind("mp_heal_opacity", &m_Model.mpSweepOpacity);
    c.Bind("ag_trail_fraction", &m_Model.agTrail);
    c.Bind("ag_visual_fraction", &m_Model.agVisual);
    c.Bind("ag_cost_start", &m_Model.agCostStart);
    c.Bind("ag_cost_fraction", &m_Model.agCost);
    c.Bind("ag_insufficient", &m_Model.agInsufficient);
    c.Bind("compact_ag_tooltip", &m_Model.agTooltip);
    c.Bind("xp_visual_fraction", &m_Model.xpVisual);
    c.Bind("xp_gain_start", &m_Model.xpGainStart);
    c.Bind("xp_gain_fraction", &m_Model.xpGain);
    c.Bind("xp_gain_opacity", &m_Model.xpGainOpacity);
    c.Bind("xp_level_up", &m_Model.xpLevelUp);
    c.Bind("xp_tooltip", &m_Model.xpTooltip);
    c.Bind("compact_picker_open", &m_Model.pickerOpen);
    c.Bind("compact_keys_open", &m_Model.keysOpen);
    c.Bind("compact_items_tab", &m_Model.consumablesTab);
    c.Bind("compact_picker_title", &m_Model.pickerTitle);
    c.Bind("compact_key_input", &m_Model.keyInput);
    c.Bind("compact_key_error", &m_Model.keyError);
    c.Bind("compact_key_target", &m_Model.keyTarget);
    c.Bind("compact_reduced_motion", &m_Model.reducedMotion);
    const std::array<Rml::String, 3> prefixes{"hp", "mp", "antidote"};
    for (int i = 0; i < 3; ++i)
    {
        auto& recovery = m_Model.recovery[i];
        const auto& prefix = prefixes[i];
        c.Bind(prefix + "_usable", &recovery.usable);
        c.Bind(prefix + "_cooldown", &recovery.cooldown);
        c.Bind(prefix + "_cooldown_text", &recovery.cooldownText);
        c.Bind(prefix == "antidote" ? "antidote_key" : prefix + "_key", &recovery.key);
        c.Bind(prefix + "_tier", &recovery.tier);
        c.Bind(prefix + "_count", &recovery.count);
        c.Bind(prefix == "antidote" ? "antidote_tooltip" : prefix + "_item_tooltip", &recovery.tooltip);
    }
    c.BindEventCallback("compact_activate",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (!args.empty())
                                Activate(args[0].Get<int>());
                        });
    c.BindEventCallback("compact_assign",
                        [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& args)
                        {
                            if (event.GetParameter<int>("button", 0) == 1 && !args.empty())
                                OpenPicker(args[0].Get<int>());
                        });
    c.BindEventCallback("compact_recover",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.empty())
                                return;
                            const auto type = args[0].Get<Rml::String>();
                            Activate(type == "life" ? 10 : type == "mana" ? 11 : 12);
                        });
    c.BindEventCallback("compact_choose",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 3)
                                Assign(args[0].Get<int>(), args[1].Get<int>(), args[2].Get<int>());
                        });
    c.BindEventCallback("compact_tab",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            m_Model.consumablesTab = !args.empty() && args[0].Get<int>() != 0;
                            RebuildChoices();
                            Dirty("compact_items_tab");
                        });
    c.BindEventCallback("compact_menu",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (!args.empty())
                                Menu(args[0].Get<Rml::String>());
                        });
    c.BindEventCallback("compact_key_select",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (!args.empty())
                                SelectKey(args[0].Get<int>());
                        });
    c.BindEventCallback("compact_key_apply",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { ApplyKey(); });
}
} // namespace UI::HUD
