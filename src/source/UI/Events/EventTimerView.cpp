#include "stdafx.h"

#include "UI/Events/EventTimerView.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Render/Text/CUIRenderTextSDLTtf.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/Scaling/UITransform.h"

#include <RmlUi/Core/ElementDocument.h>

using namespace mu::ui::window;

namespace
{
constexpr float kFrameWidth = 124.f;

Rml::Context* TimerContext()
{
    Rml::Context* context = RmlUiRuntime::Instance().GetBackgroundContext();
    return context != nullptr ? context : RmlUiRuntime::Instance().GetContext();
}

template <typename T>
void SyncField(RmlModelBinder<EventTimerRmlModel>& binder, T EventTimerRmlModel::* field, const char* name, T value)
{
    EventTimerRmlModel& model = binder.GetModel();
    if (model.*field == value)
        return;
    model.*field = std::move(value);
    binder.MarkDirty(name);
}

// RenderText(x, y, text, 124, 0, RT3_SORT_CENTER) in `font`: the size it drew `text` at.
float TextPxInFrame(UI::Scaling::FontRole role, HFONT font, const UI::Scaling::Transform& transform,
                    const std::wstring& text)
{
    g_pRenderText->SetFont(font);
    const int width = g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx;
    return UI::Scaling::NativeTextPixelSizeInBox(role, transform, static_cast<float>(width), kFrameWidth);
}
} // namespace

mu::ui::window::EventTimerView::EventTimerView(const char* modelName, const char* documentPath)
    : m_ModelName(modelName), m_DocumentPath(documentPath)
{
}

void mu::ui::window::EventTimerView::Build()
{
    if (m_pRmlDoc || !RmlUiRuntime::Instance().IsCreated())
        return;

    const bool modelCreated = m_RmlBinder.Create(TimerContext(), m_ModelName,
                                                 [](Rml::DataModelConstructor& c, EventTimerRmlModel& model)
                                                 {
                                                     c.Bind("scale_x", &model.scaleX);
                                                     c.Bind("scale_y", &model.scaleY);
                                                     c.Bind("inverse_scale_x", &model.inverseScaleX);
                                                     c.Bind("inverse_scale_y", &model.inverseScaleY);
                                                     c.Bind("panel_x", &model.panelX);
                                                     c.Bind("panel_y", &model.panelY);
                                                     c.Bind("kills_text", &model.killsText);
                                                     c.Bind("kills_text_px", &model.killsTextPx);
                                                     c.Bind("time_left_text", &model.timeLeftText);
                                                     c.Bind("time_left_text_px", &model.timeLeftTextPx);
                                                     c.Bind("time_text", &model.timeText);
                                                     c.Bind("time_text_px", &model.timeTextPx);
                                                     c.Bind("imminent", &model.imminent);
                                                 });
    if (modelCreated)
        m_pRmlDoc = UI::RmlBridge::LoadThemedDocument(TimerContext(), m_DocumentPath);
}

void mu::ui::window::EventTimerView::ReloadTheme()
{
    if (!m_pRmlDoc)
        return;
    Rml::Context* context = TimerContext();
    m_RmlBinder.Destroy(context);
    context->UnloadDocument(m_pRmlDoc);
    m_pRmlDoc = nullptr;

    Build();
}

void mu::ui::window::EventTimerView::Sync(bool visible, const POINT& pos, const std::wstring& kills,
                                          const wchar_t* timeLeft, const wchar_t* time, bool imminent)
{
    Build();
    if (!m_pRmlDoc)
        return;

    UI::RmlBridge::SyncDocumentVisibilityBehind(m_pRmlDoc, visible);
    if (!visible)
        return;

    // CManager scopes LayoutMode::Hud around the window: W/640 x H/480, no offset.
    const UI::Scaling::Transform transform = UI::Scaling::GetActiveTransform();
    SyncField(m_RmlBinder, &EventTimerRmlModel::scaleX, "scale_x", transform.scaleX);
    SyncField(m_RmlBinder, &EventTimerRmlModel::scaleY, "scale_y", transform.scaleY);
    SyncField(m_RmlBinder, &EventTimerRmlModel::inverseScaleX, "inverse_scale_x", 1.0f / transform.scaleX);
    SyncField(m_RmlBinder, &EventTimerRmlModel::inverseScaleY, "inverse_scale_y", 1.0f / transform.scaleY);
    SyncField(m_RmlBinder, &EventTimerRmlModel::panelX, "panel_x", static_cast<float>(pos.x));
    SyncField(m_RmlBinder, &EventTimerRmlModel::panelY, "panel_y", static_cast<float>(pos.y));

    const std::wstring timeLeftText = timeLeft;
    const std::wstring timeText = time;
    SyncField(m_RmlBinder, &EventTimerRmlModel::killsText, "kills_text", StringUtils::WideToNarrow(kills.c_str()));
    SyncField(m_RmlBinder, &EventTimerRmlModel::killsTextPx, "kills_text_px",
              kills.empty() ? 0.f : TextPxInFrame(UI::Scaling::FontRole::Normal, g_hFont, transform, kills));
    SyncField(m_RmlBinder, &EventTimerRmlModel::timeLeftText, "time_left_text", StringUtils::WideToNarrow(timeLeft));
    SyncField(m_RmlBinder, &EventTimerRmlModel::timeLeftTextPx, "time_left_text_px",
              TextPxInFrame(UI::Scaling::FontRole::Normal, g_hFont, transform, timeLeftText));
    SyncField(m_RmlBinder, &EventTimerRmlModel::timeText, "time_text", StringUtils::WideToNarrow(time));
    SyncField(m_RmlBinder, &EventTimerRmlModel::timeTextPx, "time_text_px",
              TextPxInFrame(UI::Scaling::FontRole::Big, g_hFontBig, transform, timeText));
    SyncField(m_RmlBinder, &EventTimerRmlModel::imminent, "imminent", imminent);
}
