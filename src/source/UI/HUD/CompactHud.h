#pragma once
#include "ActionBarProfile.h"
#include "ExperienceProgress.h"
#include "ResourceMotion.h"
#include "UI/RmlBridge/RmlRenderTarget.h"
#include <RmlUi/Core/DataModelHandle.h>
#include <array>
#include <chrono>
#include <memory>
#include <map>
#include <vector>

namespace Rml
{
class DataModelConstructor;
class ElementDocument;
} // namespace Rml
struct tagITEM;
typedef tagITEM ITEM;

namespace UI::HUD
{
class CompactHud
{
public:
    CompactHud();
    ~CompactHud();
    void Bind(Rml::DataModelConstructor& constructor);
    void Sync(Rml::ElementDocument* document, float hp, float mp, float ag, float xp, int level, bool master,
              const std::array<int, 3>& maxima);
    bool UpdateKeys();
    void SetShown(bool shown);
    void Reset();

private:
    struct Slot
    {
        int index = 0;
        bool item = false, selected = false, usable = false;
        float cooldown = 0.f;
        Rml::String icon = "none", key, count, cooldownText, tooltip;
        bool operator==(const Slot&) const = default;
    };
    struct Choice
    {
        int kind = 0, identity = -1, itemLevel = 0;
        Rml::String name, icon = "none";
        bool operator==(const Choice&) const = default;
    };
    struct Recovery
    {
        bool usable = false;
        float cooldown = 0.f;
        Rml::String key, tier, count, tooltip, cooldownText;
    };
    struct Model
    {
        float unitPx = 1.f;
        bool medium = false, narrow = false;
        std::vector<Slot> slots;
        std::vector<Choice> choices;
        std::array<Recovery, 3> recovery;
        float hpTrail = 0.f, mpTrail = 0.f;
        float hpSweep = 0.f, hpSweepOpacity = 0.f, mpSweep = 0.f, mpSweepOpacity = 0.f;
        float agTrail = 0.f, agVisual = 0.f, agCostStart = 0.f, agCost = 0.f;
        bool agInsufficient = false;
        float xpVisual = 0.f, xpGainStart = 0.f, xpGain = 0.f, xpGainOpacity = 0.f;
        bool xpLevelUp = false;
        Rml::String agTooltip, xpTooltip;
        bool pickerOpen = false, keysOpen = false, consumablesTab = false, reducedMotion = false;
        Rml::String pickerTitle, keyInput, keyError, keyTarget;
        std::vector<Slot> keySlots;
    };
    void EnsureProfile();
    void SaveProfile();
    void ImportLegacyHotkeys();
    bool InputAllowed() const;
    void Activate(int slot);
    void OpenPicker(int slot);
    void RebuildChoices();
    Rml::String ItemName(int type, int level);
    Rml::String SkillName(int type);
    void Assign(int kind, int identity, int itemLevel);
    void Menu(const Rml::String& action);
    void SelectKey(int slot);
    void ApplyKey();
    void SyncActions(Rml::ElementDocument* document);
    bool SyncSlot(int index, Slot& slot);
    void SyncItemTarget(int index, Rml::ElementDocument* document);
    void SyncMotion(float hp, float mp, float ag, float xp, int level, bool master, const std::array<int, 3>& maxima);
    void SyncCost(float ag);
    void Dirty(const char* name);

    Model m_Model;
    std::array<Slot, 3> m_RecoverySlots;
    std::array<std::array<int, 12>, ActionBarProfile::BindingCount> m_SlotState{};
    std::array<bool, ActionBarProfile::BindingCount> m_SlotInitialized{};
    std::array<int, 3> m_CostState{-1, -1, -1};
    std::array<unsigned long long, 3> m_XpState{};
    int m_CostSkill = -1, m_SelectedAgCost = 0;
    Rml::DataModelHandle m_Handle;
    ActionBarProfile m_Profile;
    ResourceMotion m_Life{200.0, 900.0}, m_Mana{120.0, 420.0}, m_Ag{100.0, 350.0, 450.0};
    ExperienceProgress m_Experience;
    std::array<int, 3> m_Maxima{};
    std::array<std::unique_ptr<RmlBridge::RenderTarget>, ActionBarProfile::BindingCount> m_Targets;
    std::unique_ptr<ITEM[]> m_Items;
    std::filesystem::path m_ProfilePath;
    Rml::String m_Character;
    bool m_ProfileSaved = false, m_Shown = false;
    std::map<std::pair<int, int>, Rml::String> m_ItemNames;
    std::map<int, Rml::String> m_SkillNames;
    std::chrono::steady_clock::time_point m_ChoicesUpdated{};
    void SyncLayout();
    int m_Assigning = 0, m_KeySlot = 0;
};
} // namespace UI::HUD
