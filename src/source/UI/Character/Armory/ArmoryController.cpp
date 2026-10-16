#include "stdafx.h"
#include "ArmoryController.h"
#include "UI/Social/PhotoViewer.h"
#include "UI/HUD/ItemHotKey.h"
#include "UI/HUD/SkillList.h"
#include "UI/HUD/Notices.h"
#include "UI/HUD/Skills/SkillAvailability.h"
#include "UI/Tooltip/LegacyTextListTooltip.h"
#include "UI/Scaling/UITransform.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "UI/Core/WindowSystem.h"
#include "Core/Utilities/StringUtils.h"
#include "Character/CharacterManager.h"
#include "GameLogic/Skills/SkillManager.h"
#include "GameLogic/Pets/GIPetManager.h"
#include "Engine/Object/ZzzInventory.h"
#include "Network/Server/WSclient.h"
#include <RmlUi/Core/ElementDocument.h>
extern int SelectedCharacter;
#include "Network/Server/ArmoryPackets.h"
#include <RmlUi/Core.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace UI::Character
{
namespace
{
Rml::String Number(float value)
{
    auto text = std::to_string(static_cast<long long>(std::lround(value)));
    for (int i = static_cast<int>(text.size()) - 3; i > 0; i -= 3)
        text.insert(i, ",");
    return text;
}
Rml::String Detail(const ArmoryRow& row)
{
    Rml::String result = row.explanation;
    std::set<std::string> groups;
    for (const auto& source : row.sources)
    {
        if (!source.group.empty() && !groups.insert(source.group).second)
            continue;
        result += "\n" + source.name + " · " + source.stat + " " + source.amount;
        if (!source.group.empty())
            for (const auto& member : row.sources)
                if (member.group == source.group && member.slot != source.slot)
                    result += " + " + member.name;
    }
    return result;
}
int SelectedSkill()
{
    return Hero && Hero->CurrentSkill >= 0 && Hero->CurrentSkill < MAX_MAGIC
               ? CharacterAttribute->Skill[Hero->CurrentSkill]
               : 0;
}
std::uint16_t TargetId()
{
    if (SelectedCharacter < 0 || SelectedCharacter >= MAX_CHARACTERS_CLIENT)
        return 0xffff;
    const auto& target = CharactersClient[SelectedCharacter];
    return target.Object.Live && target.Dead == 0 ? static_cast<std::uint16_t>(target.Key) : 0xffff;
}
void Image(Rml::Element* image, RmlBridge::RenderTarget& target, bool enabled)
{
    target.SetEnabled(enabled);
    if (!image)
    {
        target.BindImage(nullptr);
        return;
    }
    target.BindImage(image, enabled);
}
} // namespace
ArmoryController::ArmoryController() : m_Items(std::make_unique<ITEM[]>(12))
{
    for (int slot = 0; slot < 12; ++slot)
    {
        m_Items[slot].Type = -1;
        m_Targets[slot] = std::make_unique<RmlBridge::RenderTarget>(
            [this, slot](auto width, auto height)
            {
                mu::ui::window::CItemHotKey::RenderEquipmentModel(&m_Items[slot], width, height, m_HoveredSlot == slot);
            });
    }
}
ArmoryController::~ArmoryController()
{
    Release();
}
void ArmoryController::Dirty(const char* name)
{
    if (m_Handle)
        m_Handle.DirtyVariable(name);
}
void ArmoryController::Suspend()
{
    m_PhotoControl.Detach();
    RmlBridge::Tooltip::Hide(&m_SkillTooltip);
    m_SkillHovered = false;
    if (m_Shown && (m_HoveredSlot >= 0 || m_TooltipNeedsHide))
        UI::Tooltip::HideLegacyTextList();
    m_TooltipNeedsHide = false;
    m_Shown = false;
    UI::Notices::SetArmoryOpen(false);
    m_HoveredSlot = m_HoveredRow = m_HoveredAttribute = -1;
    if (m_Viewer)
    {
        m_Viewer->ResetView();
        m_Viewer->Target().SetEnabled(false);
    }
    for (auto& target : m_Targets)
        target->SetEnabled(false);
    CancelEditor();
}
void ArmoryController::Release()
{
    Suspend();
    m_Document = {};
    m_Handle = {};
    Network::Server::Armory::Reset();
}
bool ArmoryController::Sync(Rml::ElementDocument* document, bool shown)
{
    if (!document || !document->GetElementById("armory_preview"))
    {
        if (m_Shown)
            Suspend();
        return false;
    }
    m_Document = document->GetObserverPtr();
    if (!shown)
    {
        if (m_Shown)
            Suspend();
        if (m_Waiting)
            SyncNetwork();
        return true;
    }
    if (!m_Shown)
    {
        m_Shown = true;
        UI::Notices::SetArmoryOpen(true);
        m_Ready = false;
        if (!m_Waiting)
            m_LastRequest = {};
        m_OpenedAt = std::chrono::steady_clock::now();
        m_Status = "Reading character statistics…";
        Dirty("armory_status");
    }
    const auto character = StringUtils::WideToNarrow(CharacterAttribute->Name);
    if (character != m_Character)
    {
        m_Character = character;
        m_Editing = m_Waiting = m_Retry = m_Ready = false;
        m_Snapshot = {};
        m_Attributes.clear();
        m_Categories.clear();
        m_Expanded.clear();
        m_ResourceDetails = {};
        m_ResourceExpanded = {};
        if (m_ViewerReady)
            m_Viewer->CopyPlayer();
        m_LastRequest = {};
        m_OpenedAt = std::chrono::steady_clock::now();
        for (const char* field : {"armory_attributes", "armory_categories", "armory_editing", "armory_waiting"})
            Dirty(field);
    }
    m_Title = StringUtils::WideToNarrow(CharacterAttribute->Name) + " · " +
              StringUtils::WideToNarrow(gCharacterManager.GetCharacterClassText(CharacterAttribute->Class)) +
              " · Level " + std::to_string(CharacterAttribute->Level);
    Dirty("armory_title");
    SyncNetwork();
    m_Resources = {Number(CharacterAttribute->Life) + " / " + Number(CharacterAttribute->LifeMax),
                   Number(CharacterAttribute->Mana) + " / " + Number(CharacterAttribute->ManaMax),
                   Number(CharacterAttribute->SkillMana) + " / " + Number(CharacterAttribute->SkillManaMax)};
    Dirty("armory_resources");
    SyncTargets(document);
    return true;
}
void ArmoryController::SyncNetwork()
{
    if (auto snapshot = Network::Server::Armory::Take())
        Accept(std::move(*snapshot));
    const auto now = std::chrono::steady_clock::now();
    if (m_Waiting)
    {
        if (now - m_LastRequest > std::chrono::seconds(6) && !m_Retry)
        {
            m_Retry = true;
            m_Status = "Still waiting for server confirmation. Retry checks the same operation.";
            Dirty("armory_retry");
            Dirty("armory_status");
        }
        return;
    }
    const int skill = SelectedSkill();
    if (skill != m_Skill || (!m_Editing && now - m_LastRequest > std::chrono::seconds(1)))
        Send(false);
    if (!m_Ready && now - m_OpenedAt > std::chrono::seconds(4))
    {
        m_Status = "Character details are unavailable on this server.";
        Dirty("armory_status");
    }
    if (m_Ready && m_Snapshot.skill.number == skill && skill > 0)
    {
        int mana = 0, ag = 0;
        gSkillManager.GetSkillInformation(skill, 1, nullptr, &mana, nullptr, &ag);
        auto availability = UI::Skills::ReadSkillRequirements(skill).WithResources(
            CharacterAttribute->Mana, CharacterAttribute->SkillMana, mana, ag,
            CharacterAttribute->SkillDelay[Hero->CurrentSkill]);
        if (Hero->Dead != 0)
            availability = {UI::Skills::SkillAvailability::Reason::Dead};
        else if (g_isCharacterBuff((&Hero->Object), eDeBuff_Stun))
            availability = {UI::Skills::SkillAvailability::Reason::Stun};
        else if (g_isCharacterBuff((&Hero->Object), eDeBuff_Sleep))
            availability = {UI::Skills::SkillAvailability::Reason::Sleep};
        else if (Hero->SafeZone && availability.Usable())
            availability = {UI::Skills::SkillAvailability::Reason::SafeZone};
        m_Restriction = availability.Usable() ? m_Snapshot.skill.restriction : availability.Description(m_SkillName);
        Dirty("armory_restriction");
    }
}
bool ArmoryController::Send(bool apply, bool retry)
{
    static std::uint32_t sequence = 0;
    if (!retry)
    {
        m_Request = apply ? m_EditRequest : ArmoryRequest{};
        m_Request.requestId = ++sequence;
        if (m_Request.requestId == 0)
            m_Request.requestId = ++sequence;
        m_Request.apply = apply;
        m_Request.skill = SelectedSkill();
        m_Request.target = TargetId();
    }
    Network::Server::Armory::Expect(m_Request.requestId, StringUtils::WideToNarrow(CharacterAttribute->Name));
    if (!SocketClient || !SocketClient->IsConnected() || !SocketClient->ToGameServer()->SendArmoryRequest(m_Request))
    {
        m_Status = "Connection unavailable. Your planned points have not been discarded.";
        Dirty("armory_status");
        return false;
    }
    m_Skill = m_Request.skill;
    m_LastRequest = std::chrono::steady_clock::now();
    if (apply || retry)
    {
        m_Waiting = true;
        m_Retry = false;
        Dirty("armory_waiting");
        Dirty("armory_retry");
    }
    return true;
}
void ArmoryController::Accept(ArmorySnapshot snapshot)
{
    const bool allocation = m_Waiting;
    m_Snapshot = std::move(snapshot);
    m_Ready = true;
    m_Waiting = m_Retry = false;
    if (allocation || m_Status != "Applied")
        m_Status = m_Snapshot.status;
    if (allocation && m_Snapshot.status == "Applied")
    {
        WORD* bases[] = {&CharacterAttribute->Strength, &CharacterAttribute->Dexterity, &CharacterAttribute->Vitality,
                         &CharacterAttribute->Energy, &CharacterAttribute->Charisma};
        for (const auto& attribute : m_Snapshot.attributes)
            *bases[attribute.index] = static_cast<WORD>(std::min<std::uint32_t>(attribute.base, UINT16_MAX));
        CharacterAttribute->LevelUpPoint =
            static_cast<WORD>(std::min<std::uint32_t>(m_Snapshot.freePoints, UINT16_MAX));
        CharacterMachine->CalculateAll();
        m_Editing = false;
        m_EditRequest = {};
    }
    m_Points = Number(m_Snapshot.freePoints);
    RefreshSkill();
    m_Target = m_Snapshot.target;
    m_Categories.clear();
    for (std::size_t i = 0; i < m_Snapshot.rows.size(); ++i)
    {
        const auto& row = m_Snapshot.rows[i];
        if (row.category == "Resources")
        {
            if (i < 3)
                m_ResourceDetails[i] = Detail(row);
            continue;
        }
        auto group = std::find_if(m_Categories.begin(), m_Categories.end(),
                                  [&](const auto& g) { return g.name == row.category; });
        if (group == m_Categories.end())
        {
            m_Categories.push_back({row.category, {}});
            group = std::prev(m_Categories.end());
        }
        group->rows.push_back(
            {static_cast<int>(i), row.label, row.value, Detail(row), m_Expanded.contains(row.id), false});
    }
    Dirty("armory_resource_details");
    RefreshEditor();
    RefreshHighlights();
    for (const char* field :
         {"armory_ready", "armory_waiting", "armory_retry", "armory_status", "armory_points", "armory_skill_name",
          "armory_skill_output", "armory_skill_kind", "armory_costs", "armory_restriction", "armory_calculation",
          "armory_target", "armory_categories", "armory_editing"})
        Dirty(field);
}
void ArmoryController::RefreshSkill()
{
    m_SkillName = m_Snapshot.skill.name;
    m_SkillOutput = m_Snapshot.skill.output;
    m_SkillKind = m_Snapshot.skill.kind;
    m_Costs = m_Snapshot.skill.costs;
    m_Restriction = m_Snapshot.skill.restriction;
    m_Calculation = m_Snapshot.skill.calculation;
    m_SkillTooltip.lines.clear();
    if (m_Calculation.empty())
        return;
    m_SkillTooltip.lines.push_back({m_SkillName, RmlBridge::Tooltip::LineColor::Yellow, true});
    for (const auto& stage : m_Calculation)
        m_SkillTooltip.lines.push_back({stage});
}
void ArmoryController::Hover(int slot, int row)
{
    m_SkillHovered = false;
    if (slot != m_HoveredSlot)
        m_LastPetRequest = {};
    m_HoveredSlot = slot;
    m_HoveredRow = row;
    m_HoveredAttribute = -1;
    if (slot < 0)
        m_TooltipNeedsHide = true;
    RefreshHighlights();
}
void ArmoryController::HoverAttribute(int index)
{
    m_SkillHovered = false;
    m_HoveredSlot = m_HoveredRow = -1;
    m_HoveredAttribute = index;
    m_TooltipNeedsHide = true;
    RefreshHighlights();
}
void ArmoryController::RefreshHighlights()
{
    std::array<bool, 12> highlighted{};
    if (m_HoveredRow >= 0 && m_HoveredRow < static_cast<int>(m_Snapshot.rows.size()))
        for (const auto& source : m_Snapshot.rows[m_HoveredRow].sources)
            if (source.slot >= 0 && source.slot < 12)
                highlighted[source.slot] = true;
    if (m_HoveredAttribute >= 0)
        for (const auto& attribute : m_Snapshot.attributes)
            if (attribute.index == m_HoveredAttribute)
                for (const auto& source : attribute.sources)
                    if (source.slot >= 0 && source.slot < 12)
                        highlighted[source.slot] = true;
    for (auto& view : m_Attributes)
    {
        view.highlighted = false;
        for (const auto& attribute : m_Snapshot.attributes)
            if (attribute.index == view.index)
                view.highlighted = m_HoveredSlot >= 0 &&
                                   std::any_of(attribute.sources.begin(), attribute.sources.end(),
                                               [this](const auto& source) { return source.slot == m_HoveredSlot; });
    }
    Dirty("armory_attributes");
    if (auto* document = m_Document.get())
        for (int slot = 0; slot < 12; ++slot)
            if (auto* element = document->GetElementById("armory_slot_" + std::to_string(slot)))
                element->SetClass("contributes", highlighted[slot]);
    for (auto& category : m_Categories)
        for (auto& row : category.rows)
        {
            const auto& sources = m_Snapshot.rows[row.index].sources;
            row.highlighted =
                m_HoveredSlot >= 0 && std::any_of(sources.begin(), sources.end(),
                                                  [this](const auto& source) { return source.slot == m_HoveredSlot; });
        }
    Dirty("armory_categories");
    for (std::size_t i = 0; i < m_ResourceHighlighted.size(); ++i)
        m_ResourceHighlighted[i] = i < m_Snapshot.rows.size() && m_HoveredSlot >= 0 &&
                                   std::any_of(
                                       m_Snapshot.rows[i].sources.begin(), m_Snapshot.rows[i].sources.end(),
                                       [this](const auto& source) { return source.slot == m_HoveredSlot; });
    Dirty("armory_resource_highlighted");
}
void ArmoryController::SyncTargets(Rml::ElementDocument* document)
{
    const auto now = std::chrono::steady_clock::now();
    bool changed = !m_ViewerReady;
    if (now - m_LastEquipment > std::chrono::milliseconds(250))
    {
        for (int slot = 0; slot < 12; ++slot)
        {
            const auto& item = CharacterMachine->Equipment[slot];
            changed |= std::memcmp(&m_Items[slot], &item, sizeof(ITEM)) != 0;
            m_Items[slot] = item;
        }
        m_LastEquipment = now;
    }
    if (!m_ViewerReady)
    {
        m_PhotoControl.Detach();
        m_Viewer = std::make_unique<CUIPhotoViewer>();
        m_Viewer->Init(0);
        m_Viewer->SetCanControl(false);
        m_Viewer->SetZoom(0.78f);
        m_ViewerReady = true;
    }
    if (changed)
        m_Viewer->CopyPlayer();
    m_PhotoControl.AttachOrbit(document, *m_Viewer, "armory_model_controls", "armory_preview");
    m_PhotoControl.Sync();
    Image(document->GetElementById("armory_preview"), m_Viewer->Target(), true);
    for (int slot = 0; slot < 12; ++slot)
        Image(document->GetElementById("armory_item_" + std::to_string(slot)), *m_Targets[slot],
              m_Items[slot].Type >= 0);
}
void ArmoryController::RenderTooltip()
{
    // Hiding a RmlUi tooltip can dispatch focus/blur. Do this outside those callbacks
    // to avoid recursively updating the tooltip document while it is being hidden.
    if (m_TooltipNeedsHide)
    {
        m_TooltipNeedsHide = false;
        if (m_HoveredSlot < 0)
            UI::Tooltip::HideLegacyTextList();
    }
    if (RenderSkillTooltip())
        return;
    if (!m_Shown || m_HoveredSlot < 0 || m_HoveredSlot >= 12 || m_Items[m_HoveredSlot].Type < 0)
        return;
    m_Items[m_HoveredSlot].bySelectedSlotIndex = m_HoveredSlot;
    if (g_pMyInventory->GetRepairMode() == SEASON3B::REPAIR_MODE_ON)
    {
        RenderRepairInfoAtPx(g_fWindowMouseX, g_fWindowMouseY, &m_Items[m_HoveredSlot], false);
        return;
    }
    const auto& item = m_Items[m_HoveredSlot];
    if (item.Type == ITEM_DARK_HORSE_ITEM || item.Type == ITEM_DARK_RAVEN_ITEM)
    {
        const auto now = std::chrono::steady_clock::now();
        if (SocketClient && SocketClient->IsConnected() && now - m_LastPetRequest > std::chrono::seconds(1))
        {
            SocketClient->ToGameServer()->SendPetInfoRequest(item.Type == ITEM_DARK_HORSE_ITEM ? PetType::DarkHorse
                                                                                               : PetType::DarkRaven,
                                                             StorageType::Inventory, m_HoveredSlot);
            m_LastPetRequest = now;
        }
        giPetManager::RenderPetItemInfo(UI::Scaling::WindowPixelTransform(WindowWidth, WindowHeight),
                                        static_cast<int>(g_fWindowMouseX), static_cast<int>(g_fWindowMouseY),
                                        &m_Items[m_HoveredSlot], 0);
        return;
    }
    RenderItemInfoAtPx(g_fWindowMouseX, g_fWindowMouseY, &m_Items[m_HoveredSlot], false);
}

bool ArmoryController::RenderSkillTooltip()
{
    if (!m_Shown || !m_SkillHovered || m_SkillTooltip.lines.empty() || !m_Document)
    {
        RmlBridge::Tooltip::Hide(&m_SkillTooltip);
        return false;
    }
    // The Armory uses design units; native tooltip metrics use the shared panel units.
    m_SkillTooltip.transform = UI::Scaling::PanelTransform(WindowWidth, WindowHeight);
    RmlBridge::Tooltip::Show(m_SkillTooltip, &m_SkillTooltip);
    return true;
}
} // namespace UI::Character
