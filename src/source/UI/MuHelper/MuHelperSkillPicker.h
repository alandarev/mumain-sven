#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/Types.h>

#include <vector>

namespace Rml
{
    class ElementDocument;
}

namespace mu::ui::window
{
    class CManager;

    // One skill the player can assign, placed where native's fan-out put it.
    struct MuHelperSkillPickerEntry
    {
        Rml::String decorator;  // "image(skill-icon-skill2-lit-3-8)" -- see UI::MuHelper::SkillIconDecorator()
        // For a theme's skill library (#skill_library): the skill and its name.
        int skillType = 0;
        Rml::String name;
    };

    struct MuHelperSkillPickerRmlModel
    {
        // The config window's drawn top left and scale, in window pixels: the flyout hangs off it.
        float originX = 0.f, originY = 0.f, scale = 1.f;
        // Whether an attack slot opened the flyout (else a buff slot): the theme fans the entries
        // out from that slot's row.
        bool attack = false;
        float textPx = 0.f; // the native text size, physical px, for a skill library's names
        std::vector<MuHelperSkillPickerEntry> entries;
    };

    // The borderless flyout the MU Helper config window opens beside an attack or buff slot, listing
    // the skills that slot accepts. Picking one assigns it and closes the flyout.
    //
    // Only the icons take the pointer, and a click on one assigns it. Everything else passes
    // through -- to the world, so the character keeps moving while the flyout is open (native
    // blocked that; changed deliberately), and to the config window, which is how a second click on
    // the same slot reaches it and closes the flyout.
    class CMuHelperSkillPicker : public CObject
    {
    public:

        CMuHelperSkillPicker();
        ~CMuHelperSkillPicker() override;

        bool Create(CManager* pNewUIMng);
        void Release();

        bool UpdateMouseEvent() override;
        bool UpdateKeyEvent() override;
        bool Update() override;
        bool Render() override;
        // A theme that lists the skills as a library (#skill_library) docks it in a workspace slot.
        Rml::ElementDocument* GetPlacedDocument() const override;
        float GetLayerDepth() override;


        void FilterByAttackSkills();
        void FilterByBuffSkills();

    private:
        void PrepareSkillsToRender();
        void Pick(int index);
        // A skill library picks by skill type; applied on the next Update(), outside the event.
        void ApplyPendingChoice();
        int m_PendingSkill = -1;

        static bool IsAttackSkill(int iSkillType);
        static bool IsBuffSkill(int iSkillType);
        static bool IsHealingSkill(int iSkillType);
        static bool IsDefenseSkill(int iSkillType);

        void BuildRmlUi();
        void SyncRmlModel();
        void SyncOrigin();

        CManager* m_pNewUIMng = nullptr;

        bool m_bFilterByAttackSkills = false;
        bool m_bFilterByBuffSkills = false;
        std::vector<int> m_aiSkillsToRender;
        bool m_bEntriesDirty = true;

        void BindRmlModel(Rml::DataModelConstructor& c, MuHelperSkillPickerRmlModel& model);
        UI::RmlBridge::ThemedView<MuHelperSkillPickerRmlModel> m_RmlView{"mu_helper_skill_picker",
            [this](Rml::DataModelConstructor& c, MuHelperSkillPickerRmlModel& model) { BindRmlModel(c, model); },
            {{"Data/Interface/RmlUi/mu_helper_skill_picker.rml"}},
            {.afterBuild = [this] { m_bEntriesDirty = true; }}};
    };
}
