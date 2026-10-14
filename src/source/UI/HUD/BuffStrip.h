
#pragma once

#include <chrono>

#include "UI/Core/WindowManager.h"
#include "UI/RmlBridge/RmlElementTooltip.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <vector>

namespace Rml { class ElementDocument; }

namespace mu::ui::window
{
    // RmlUi-backed active-buff strip, a variable-length list (unlike CMuHelperBar's fixed layout).
    // Right-clicking Infinity Arrow or Swell of Magic Power asks to cancel it, as the original did.
    class CBuffStrip : public CObject
    {
    public:
        CBuffStrip();
        virtual ~CBuffStrip();

        bool Create(CManager* pNewUIMng);
        void Release();

        // Vestigial -- RmlUi/CSS owns this widget's position now (.center-x in buff_strip.rml).
        bool UpdateMouseEvent();
        bool UpdateKeyEvent();
        bool Update();
        bool Render();

        float GetLayerDepth();	//. 0.95f

        void OpenningProcess();
        void ClosingProcess();

        // Same MAIN_SCENE-only Update() gate and fix as CMuHelperBar::SyncDocVisibility() (MuHelperBar.h).
        void SyncDocVisibility(bool sceneAllowsShow);


    private:
        void BuildRmlUi();

        struct BuffEntry
        {
            // Pre-computed pixel grid-slot position, bound as plain values (no arithmetic in RML).
            // Full decorator string, e.g. "image(atlas1-23)", selecting a named @spritesheet rect
            // from buff_strip.rcss (one rect per 20x28 tile; BuildIconDecorator() in BuffStrip.cpp
            // generates the name). Bound wholesale via data-style-decorator.
            //
            // Note: an absolutely-positioned oversized child does NOT get clipped by RmlUi even
            // with overflow:hidden on its container (ContainerBox::Close() computes the clip rect
            // before ClosePositionedElements() places such children) -- don't retry a clipped-atlas
            // approach; named @spritesheet rects are the working mechanism.
            Rml::String decorator;
            // A corner strip's (ThemeUsesCornerBuffStrip()): the remaining time, and the slot in
            // `buffs` its events name.
            Rml::String remaining;
            int slot = 0;
            bool expiring = false;
            bool operator==(const BuffEntry&) const = default;
        };
        struct BuffStripRmlModel
        {
            std::vector<BuffEntry> buffs;
            // The same buffs split for a corner strip.
            std::vector<BuffEntry> beneficial;
            std::vector<BuffEntry> harmful;
            float unitPx = 1.f;
            float viewportWidth = 1024.f;
        };
        void BindRmlModel(Rml::DataModelConstructor& c, BuffStripRmlModel& model);
        UI::RmlBridge::ThemedView<BuffStripRmlModel> m_RmlView{"buff_strip",
            [this](Rml::DataModelConstructor& c, BuffStripRmlModel& model) { BindRmlModel(c, model); },
            {{"Data/Interface/RmlUi/buff_strip.rml"}}};

        CManager* m_pNewUIMng = nullptr;

        // The buffs shown, in slot order, for mapping a slot's event back to its buff.
        std::vector<int> m_ShownBuffs;
        // The hovered slot's buff, in the shared tooltip.
        UI::RmlBridge::ElementTooltip m_Tooltip;
        void SyncTooltip();

        void SyncRmlModel();
        void SyncBuffEntries(bool cornerStrip);
        void OnBuffRightClick(int slot);
        std::chrono::steady_clock::time_point m_NextEntrySync{};
        bool m_CornerStrip = false;
    };
}
