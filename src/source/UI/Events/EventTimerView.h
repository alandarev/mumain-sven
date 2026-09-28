#pragma once

#include "UI/Events/EventTimerRmlModel.h"
#include "UI/RmlBridge/RmlModelBinder.h"

#include <string>

namespace Rml
{
class ElementDocument;
}

namespace mu::ui::window
{
// The RmlUi side of the event time HUDs that share the original's layout (CBloodCastle,
// CChaosCastleTime): newui_Figure_blood (124 x 81), an optional kill count, "Time left" and the
// time in the big font. The original drew them under every panel (layer depth 1.2 / 1.3), so the
// document is in the background context, behind its other documents, like the duel and siege
// boards. Each window owns one, with its own document and data model; the window keeps its time,
// its counts and when it is shown.
class EventTimerView
{
public:
    EventTimerView(const char* modelName, const char* documentPath);

    void Build();
    void ReloadTheme();

    // Per frame, inside the window's CManager transform scope. An empty `kills` is not drawn.
    void Sync(bool visible, const POINT& pos, const std::wstring& kills, const wchar_t* timeLeft, const wchar_t* time,
              bool imminent);

private:
    const char* m_ModelName;
    const char* m_DocumentPath;
    RmlModelBinder<EventTimerRmlModel> m_RmlBinder;
    Rml::ElementDocument* m_pRmlDoc = nullptr;
};
} // namespace mu::ui::window
