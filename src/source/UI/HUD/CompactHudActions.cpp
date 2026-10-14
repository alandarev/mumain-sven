#include "stdafx.h"
#include "CompactHud.h"
#include "ItemHotKey.h"
#include "SkillList.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/MessageBox.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Dialogs/GenericMenuDialog.h"
#include "UI/Inventory/MyInventory.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Engine/Object/ZzzInventory.h"
#include "Core/Utilities/StringUtils.h"
#include "GameLogic/Skills/SkillManager.h"
#include "GameLogic/Skills/QuickCast.h"
#include <algorithm>
#include <set>

namespace UI::HUD
{
using mu::ui::window::CItemHotKey;
namespace
{
constexpr int LifeRecovery = 10, ManaRecovery = 11, PoisonRecovery = 12;
bool AllowedConsumable(int type)
{
    return mu::ui::window::CMyInventory::CanRegisterItemHotKey(type) &&
           !(type >= ITEM_SMALL_SHIELD_POTION && type <= ITEM_LARGE_SHIELD_POTION) &&
           !(type >= ITEM_SMALL_COMPLEX_POTION && type <= ITEM_LARGE_COMPLEX_POTION);
}
} // namespace
Rml::String CompactHud::ItemName(int type, int level)
{
    const auto key = std::make_pair(type, level);
    if (const auto found = m_ItemNames.find(key); found != m_ItemNames.end())
        return found->second;
    wchar_t name[256] = {};
    GetItemName(type, level, name);
    return m_ItemNames.emplace(key, StringUtils::WideToNarrow(name)).first->second;
}
Rml::String CompactHud::SkillName(int type)
{
    if (const auto found = m_SkillNames.find(type); found != m_SkillNames.end())
        return found->second;
    wchar_t name[256] = {};
    gSkillManager.GetSkillInformation(type, 1, name, nullptr, nullptr);
    return m_SkillNames.emplace(type, StringUtils::WideToNarrow(name)).first->second;
}
void CompactHud::EnsureProfile()
{
    const auto character = StringUtils::WideToNarrow(Hero->ID);
    if (character == m_Character)
        return;
    Reset();
    m_Character = character;
    m_Profile = ActionBarProfile{};
    m_Profile.bindings[LifeRecovery] = {ActionBarProfile::Kind::Item, ITEM_LARGE_HEALING_POTION, 0, 'Q'};
    m_Profile.bindings[ManaRecovery] = {ActionBarProfile::Kind::Item, ITEM_LARGE_MANA_POTION, 0, 'W'};
    m_Profile.bindings[PoisonRecovery] = {ActionBarProfile::Kind::Item, ITEM_ANTIDOTE, 0, 'E'};
    // Hex encoding keeps non-Latin names distinct and prevents path characters in a character name.
    constexpr char hex[] = "0123456789abcdef";
    std::string fileName;
    for (unsigned char byte : character)
    {
        fileName += hex[byte >> 4];
        fileName += hex[byte & 15];
    }
    m_ProfilePath = std::filesystem::path("Config/action-bar") / (fileName + ".profile");
    m_ProfileSaved = m_Profile.Load(m_ProfilePath);
    // Recovery roles remain purpose-based; only their keys are configurable.
    const std::array<int, 3> recoveryTypes{ITEM_LARGE_HEALING_POTION, ITEM_LARGE_MANA_POTION, ITEM_ANTIDOTE};
    for (int i = 0; i < 3; ++i)
    {
        auto& binding = m_Profile.bindings[LifeRecovery + i];
        binding.kind = ActionBarProfile::Kind::Item;
        binding.identity = recoveryTypes[i];
        binding.itemLevel = 0;
    }
    m_Model.reducedMotion = m_Profile.reducedMotion;
    Dirty("compact_reduced_motion");
}
void CompactHud::SaveProfile()
{
    m_ProfileSaved = true;
    if (!m_Profile.Save(m_ProfilePath))
    {
        Menu("keys");
        m_Model.keyError = "Could not save the action profile. Changes remain active for this session.";
        Dirty("compact_key_error");
    }
}
void CompactHud::ImportLegacyHotkeys()
{
    for (int i = 0; i < ActionBarProfile::ActionCount; ++i)
    {
        const int digit = (i + 1) % ActionBarProfile::ActionCount;
        const int index = g_pSkillList->GetHotKey(digit);
        auto& binding = m_Profile.bindings[i];
        if (index >= AT_PET_COMMAND_DEFAULT && index < AT_PET_COMMAND_END)
        {
            binding.kind = ActionBarProfile::Kind::Pet;
            binding.identity = index;
        }
        else if (index >= 0 && index < MAX_MAGIC && CharacterAttribute->Skill[index] != 0)
        {
            binding.kind = ActionBarProfile::Kind::Skill;
            binding.identity = CharacterAttribute->Skill[index];
        }
    }
}
bool CompactHud::InputAllowed() const
{
    return Hero != nullptr && Hero->Dead == 0 && m_Shown && !RmlUiRuntime::Instance().IsTextInputActive() &&
           (g_MessageBox == nullptr || g_MessageBox->IsEmpty()) &&
           (mu::ui::window::g_pGenericConfirmDialog == nullptr ||
            !mu::ui::window::g_pGenericConfirmDialog->IsVisible()) &&
           (mu::ui::window::g_pGenericMenuDialog == nullptr || !mu::ui::window::g_pGenericMenuDialog->IsVisible());
}
bool CompactHud::UpdateKeys()
{
    if (!InputAllowed())
        return true;
    if (mu::ui::window::IsPress(VK_ESCAPE) && (m_Model.pickerOpen || m_Model.keysOpen))
    {
        Menu("close");
        return false;
    }
    if (m_Model.pickerOpen || m_Model.keysOpen)
        return false;
    if (mu::ui::window::IsRepeat(VK_SHIFT) || mu::ui::window::IsRepeat(VK_CONTROL) || mu::ui::window::IsRepeat(VK_MENU))
        return true;
    for (int i = 0; i < ActionBarProfile::BindingCount; ++i)
    {
        if (i == PoisonRecovery && !g_isCharacterBuff((&Hero->Object), eDeBuff_Poison))
            continue;
        const int key = m_Profile.bindings[i].key;
        if (key != 0 && mu::ui::window::IsPress(key))
        {
            Activate(i);
            return false;
        }
    }
    return true;
}
void CompactHud::Activate(int index)
{
    if (!InputAllowed() || m_Model.pickerOpen || m_Model.keysOpen || index < 0 ||
        index >= ActionBarProfile::BindingCount)
        return;
    const auto& binding = m_Profile.bindings[index];
    if (binding.kind == ActionBarProfile::Kind::Empty)
    {
        OpenPicker(index);
        return;
    }
    if (binding.kind == ActionBarProfile::Kind::Item)
    {
        if (!AllowedConsumable(binding.identity) ||
            (index == PoisonRecovery && !g_isCharacterBuff((&Hero->Object), eDeBuff_Poison)))
            return;
        CItemHotKey::UseInventoryItem(CItemHotKey::ResolveConsumable(binding.identity, binding.itemLevel));
        return;
    }
    if (binding.kind == ActionBarProfile::Kind::Pet)
    {
        if (Hero->m_pPet != nullptr)
            g_pSkillList->OnPetCellClick(binding.identity);
        return;
    }
    const int skillIndex = g_pSkillList->GetSkillIndex(binding.identity);
    if (skillIndex < 0)
        return;
    if (GameLogic::Skills::IsSelfBuff(binding.identity))
        GameLogic::Skills::TryCastSelfBuff(skillIndex);
    else
        g_pSkillList->SelectSkillIndex(skillIndex);
}
bool CompactHud::SyncSlot(int index, Slot& slot)
{
    const auto& binding = m_Profile.bindings[index];
    const bool itemBinding = binding.kind == ActionBarProfile::Kind::Item && AllowedConsumable(binding.identity);
    const bool pet = binding.kind == ActionBarProfile::Kind::Pet;
    const int skillIndex = pet ? binding.identity
                           : binding.kind == ActionBarProfile::Kind::Skill
                               ? g_pSkillList->GetSkillIndex(binding.identity)
                               : -1;
    const bool learned = skillIndex >= 0 && skillIndex < MAX_MAGIC;
    int count = 0, delay = 0, maximum = 0, manaCost = 0, agCost = 0;
    m_Items[index].Type = -1;
    bool usable = false;
    using Reason = UI::Skills::SkillAvailability::Reason;
    UI::Skills::SkillAvailability availability{Reason::NotLearned};
    if (itemBinding)
    {
        const int inventoryIndex = CItemHotKey::ResolveConsumable(binding.identity, binding.itemLevel);
        count = CItemHotKey::ResolveConsumable(binding.identity, binding.itemLevel, true);
        const auto* item = inventoryIndex >= 0 ? g_pMyInventory->FindItem(inventoryIndex) : nullptr;
        m_Items[index].Type = item ? item->Type : binding.identity;
        m_Items[index].Level = item ? item->Level : binding.itemLevel;
        usable = count > 0 && Hero->Dead == 0;
    }
    else if (learned)
    {
        usable = Hero->Dead == 0 && (!pet || Hero->m_pPet != nullptr);
        if (!pet)
        {
            gSkillManager.GetSkillInformation(binding.identity, 1, nullptr, &manaCost, nullptr, &agCost);
            delay = CharacterAttribute->SkillDelay[skillIndex];
            maximum = SkillAttribute[binding.identity].Delay;
            availability = UI::Skills::ReadSkillRequirements(binding.identity);
            if (Hero->Dead != 0)
                availability = {Reason::Dead};
            else if (GameLogic::Skills::IsSelfBuff(binding.identity) && Hero->SafeZone)
                availability = {Reason::SafeZone};
            else if (GameLogic::Skills::IsSelfBuff(binding.identity) &&
                     g_isCharacterBuff((&Hero->Object), eDeBuff_Stun))
                availability = {Reason::Stun};
            else if (GameLogic::Skills::IsSelfBuff(binding.identity) &&
                     g_isCharacterBuff((&Hero->Object), eDeBuff_Sleep))
                availability = {Reason::Sleep};
            availability =
                availability.WithResources(static_cast<int>(CharacterAttribute->Mana),
                                           static_cast<int>(CharacterAttribute->SkillMana), manaCost, agCost, delay);
            usable = availability.Usable();
        }
        else
            availability = usable ? UI::Skills::SkillAvailability{}
                                  : UI::Skills::SkillAvailability{Hero->Dead != 0 ? Reason::Dead : Reason::Pet};
    }
    const bool selected = learned && Hero->CurrentSkill == skillIndex;
    const std::array<int, 17> state{static_cast<int>(binding.kind),
                                    binding.identity,
                                    binding.itemLevel,
                                    binding.key,
                                    m_Items[index].Type,
                                    m_Items[index].Level,
                                    count,
                                    skillIndex,
                                    delay,
                                    maximum,
                                    selected,
                                    usable,
                                    static_cast<int>(availability.reason),
                                    availability.required,
                                    availability.current,
                                    manaCost,
                                    agCost};
    if (m_SlotInitialized[index] && m_SlotState[index] == state)
        return false;
    m_SlotInitialized[index] = true;
    m_SlotState[index] = state;
    slot = {};
    slot.index = index;
    slot.key = binding.key == 0 ? Rml::String{} : ActionBarProfile::KeyName(binding.key);
    slot.item = itemBinding;
    slot.usable = usable;
    slot.selected = selected;
    slot.tooltip = "Empty action. Click or right-click to assign.";
    if (itemBinding)
    {
        slot.count = std::to_string(count);
        slot.tooltip = ItemName(m_Items[index].Type, m_Items[index].Level) + " | " + slot.count + " available";
    }
    else if (binding.kind == ActionBarProfile::Kind::Skill || pet)
        DescribeSkill(index, skillIndex, availability, slot);
    return true;
}
void CompactHud::DescribeSkill(int index, int skillIndex, const UI::Skills::SkillAvailability& availability, Slot& slot)
{
    const auto& binding = m_Profile.bindings[index];
    const bool pet = binding.kind == ActionBarProfile::Kind::Pet;
    if (skillIndex < 0 || skillIndex >= MAX_MAGIC)
    {
        slot.tooltip = pet ? "Pet command" : "Skill unavailable";
        slot.requirement = availability.Description("");
        return;
    }
    slot.tooltip = pet ? "Pet command" : SkillName(binding.identity);
    const auto requirementSkill =
        pet ? binding.identity
            : gSkillManager.MasterSkillToBaseSkillIndex(static_cast<ActionSkillType>(binding.identity));
    slot.requirement = availability.Description(pet ? "" : SkillName(requirementSkill));
    slot.icon = g_pSkillList->GetSkillIconDecorator(skillIndex);
    if (pet)
        return;
    int mana = 0, ag = 0;
    gSkillManager.GetSkillInformation(binding.identity, 1, nullptr, &mana, nullptr, &ag);
    slot.costs = "Mana " + std::to_string(mana) + " | AG " + std::to_string(ag);
    const int delay = CharacterAttribute->SkillDelay[skillIndex];
    const int maximum = SkillAttribute[binding.identity].Delay;
    slot.cooldown = maximum > 0 ? std::clamp(static_cast<float>(delay) / maximum, 0.f, 1.f) : 0.f;
    if (delay > 0)
        slot.cooldownText = std::to_string(std::max(1, (delay + 999) / 1000));
    if (availability.Usable())
        slot.tooltip += GameLogic::Skills::IsSelfBuff(binding.identity) ? " | Cast immediately" : " | Select, then RMB";
}
void CompactHud::OpenPicker(int slot)
{
    if (!InputAllowed() || slot < 0 || slot >= ActionBarProfile::ActionCount)
        return;
    m_Assigning = slot;
    m_Model.pickerOpen = true;
    m_Model.keysOpen = false;
    m_Model.pickerTitle = "Assign action " + std::to_string((slot + 1) % ActionBarProfile::ActionCount) + " [" +
                          ActionBarProfile::KeyName(m_Profile.bindings[slot].key) + "]";
    Dirty("compact_picker_open");
    Dirty("compact_keys_open");
    Dirty("compact_picker_title");
    RebuildChoices();
}
void CompactHud::RebuildChoices()
{
    const auto previous = std::move(m_Model.choices);
    m_Model.choices = {};
    m_ChoicesUpdated = std::chrono::steady_clock::now();
    std::set<std::pair<int, int>> added;
    if (!m_Model.consumablesTab)
    {
        for (int i = 0; i < MAX_MAGIC; ++i)
        {
            const int skill = CharacterAttribute->Skill[i];
            if (skill == 0 || !added.insert({skill, 0}).second)
                continue;
            m_Model.choices.push_back({static_cast<int>(ActionBarProfile::Kind::Skill), skill, 0, SkillName(skill),
                                       g_pSkillList->GetSkillIconDecorator(i)});
        }
        if (Hero->m_pPet != nullptr)
            for (int i = AT_PET_COMMAND_DEFAULT; i < AT_PET_COMMAND_END; ++i)
                m_Model.choices.push_back({static_cast<int>(ActionBarProfile::Kind::Pet), i, 0,
                                           "Pet command " + std::to_string(i - AT_PET_COMMAND_DEFAULT + 1),
                                           g_pSkillList->GetSkillIconDecorator(i)});
    }
    else
    {
        auto* inventory = g_pMyInventory ? g_pMyInventory->GetInventoryCtrl() : nullptr;
        if (inventory == nullptr)
        {
            Dirty("compact_choices");
            return;
        }
        for (int i = 0; i < inventory->GetNumberOfItems(); ++i)
        {
            const auto* item = inventory->GetItem(i);
            if (item == nullptr || !AllowedConsumable(item->Type))
                continue;
            int type = item->Type;
            if (type >= ITEM_APPLE && type <= ITEM_LARGE_HEALING_POTION)
                type = ITEM_LARGE_HEALING_POTION;
            else if (type >= ITEM_SMALL_MANA_POTION && type <= ITEM_LARGE_MANA_POTION)
                type = ITEM_LARGE_MANA_POTION;
            if (!added.insert({type, item->Level}).second)
                continue;
            const int count = CItemHotKey::ResolveConsumable(type, item->Level, true);
            const int bestIndex = CItemHotKey::ResolveConsumable(type, item->Level);
            const auto* best = bestIndex >= 0 ? g_pMyInventory->FindItem(bestIndex) : item;
            const std::pair<int, int> key{type, item->Level};
            auto& preview = m_ChoiceItems[key];
            if (!preview)
                preview = std::make_unique<ITEM>();
            *preview = *(best ? best : item);
            if (!m_ChoiceTargets.contains(key))
                m_ChoiceTargets[key] = std::make_unique<RmlBridge::RenderTarget>(
                    [this, key](std::uint32_t w, std::uint32_t h)
                    { CItemHotKey::RenderItemModel(m_ChoiceItems.at(key).get(), w, h, false, true); });
            m_Model.choices.push_back(
                {static_cast<int>(ActionBarProfile::Kind::Item), type, item->Level,
                 ItemName(best ? best->Type : type, item->Level) + " (" + std::to_string(count) + ")", "none"});
        }
    }
    if (previous != m_Model.choices)
        Dirty("compact_choices");
}
void CompactHud::Assign(int kind, int identity, int itemLevel)
{
    if (!m_Model.pickerOpen)
        return;
    const bool clearing = kind == static_cast<int>(ActionBarProfile::Kind::Empty);
    const auto choice =
        std::find_if(m_Model.choices.begin(), m_Model.choices.end(), [&](const Choice& entry)
                     { return entry.kind == kind && entry.identity == identity && entry.itemLevel == itemLevel; });
    if (!clearing && choice == m_Model.choices.end())
        return;
    auto& binding = m_Profile.bindings[m_Assigning];
    binding.kind = static_cast<ActionBarProfile::Kind>(kind);
    binding.identity = clearing ? -1 : identity;
    binding.itemLevel = clearing ? 0 : itemLevel;
    Menu("close");
    SaveProfile();
}
void CompactHud::SelectKey(int slot)
{
    if (slot < 0 || slot >= ActionBarProfile::BindingCount)
        return;
    m_KeySlot = slot;
    m_Model.keyInput = ActionBarProfile::KeyName(m_Profile.bindings[slot].key);
    m_Model.keyTarget = slot < ActionBarProfile::ActionCount
                            ? "Action " + std::to_string((slot + 1) % ActionBarProfile::ActionCount)
                        : slot == LifeRecovery ? "Life recovery"
                        : slot == ManaRecovery ? "Mana recovery"
                                               : "Antidote";
    m_Model.keyError.clear();
    Dirty("compact_key_input");
    Dirty("compact_key_target");
    Dirty("compact_key_error");
}
void CompactHud::ApplyKey()
{
    if (!m_Profile.Rebind(m_KeySlot, m_Model.keyInput, m_Model.keyError))
    {
        Dirty("compact_key_error");
        return;
    }
    Menu("keys");
    SaveProfile();
}
void CompactHud::Menu(const Rml::String& action)
{
    if (action == "skills")
    {
        OpenPicker(m_Assigning);
        return;
    }
    if (action == "portal")
    {
        if (InputAllowed())
            CItemHotKey::UseInventoryItem(CItemHotKey::ResolveConsumable(ITEM_TOWN_PORTAL_SCROLL, 0));
        return;
    }
    if (action == "keys")
    {
        m_Model.keysOpen = true;
        m_Model.pickerOpen = false;
        m_Model.keySlots.clear();
        for (int i = 0; i < ActionBarProfile::BindingCount; ++i)
        {
            Slot entry;
            entry.index = i;
            entry.key = ActionBarProfile::KeyName(m_Profile.bindings[i].key);
            m_Model.keySlots.push_back(entry);
        }
        Dirty("compact_key_slots");
        SelectKey(m_KeySlot);
    }
    if (action == "defaults")
    {
        m_Profile.ResetKeys();
        Menu("keys");
        SaveProfile();
    }
    if (action == "motion")
    {
        m_Profile.reducedMotion = !m_Profile.reducedMotion;
        m_Model.reducedMotion = m_Profile.reducedMotion;
        Reset();
        m_Model.keysOpen = true;
        SaveProfile();
        Dirty("compact_reduced_motion");
    }
    if (action == "close")
        m_Model.keysOpen = m_Model.pickerOpen = false;
    Dirty("compact_keys_open");
    Dirty("compact_picker_open");
}
} // namespace UI::HUD
