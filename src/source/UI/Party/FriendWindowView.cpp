#include "stdafx.h"

#include "UI/Party/FriendWindowView.h"

#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/Party/UIWindows.h"
#include "UI/RmlBridge/RmlColor.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "Core/Utilities/StringUtils.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>

namespace
{
constexpr const char* DocumentPath = "Data/Interface/RmlUi/friend_window.rml";
constexpr const char* ModelPlaceholder = "data-model=\"friend_window\"";
} // namespace

FriendWindowRmlBuilder::FriendWindowRmlBuilder(int originX, int originY)
    : m_OriginX(originX), m_OriginY(originY), m_Transform(UI::Scaling::GetActiveTransform())
{
}

void FriendWindowRmlBuilder::Fill(const char* role, double x, double y, double width, double height)
{
    if (width <= 0.0 || height <= 0.0)
        return;
    FriendWindowPart part;
    part.kind = FriendWindowPart::Fill;
    part.role = role;
    part.left = static_cast<float>(x - m_OriginX);
    part.top = static_cast<float>(y - m_OriginY);
    part.width = static_cast<float>(width);
    part.height = static_cast<float>(height);
    m_Parts.push_back(std::move(part));
}

void FriendWindowRmlBuilder::Sprite(const char* role, double x, double y, double width, double height)
{
    if (width <= 0.0 || height <= 0.0)
        return;
    FriendWindowPart part;
    part.kind = FriendWindowPart::Sprite;
    part.role = role;
    part.left = static_cast<float>(x - m_OriginX);
    part.top = static_cast<float>(y - m_OriginY);
    part.width = static_cast<float>(width);
    part.height = static_cast<float>(height);
    m_Parts.push_back(std::move(part));
}

void FriendWindowRmlBuilder::Text(const wchar_t* text, double x, double y, DWORD color, bool bold, double boxWidth,
                                  int align)
{
    if (text == nullptr || text[0] == L'\0')
        return;
    FriendWindowPart part;
    part.kind = FriendWindowPart::Text;
    part.role = "text";
    // RenderText() takes whole units.
    part.left = static_cast<float>(static_cast<int>(x) - m_OriginX);
    part.top = static_cast<float>(static_cast<int>(y) - m_OriginY);
    part.width = static_cast<float>(boxWidth);
    part.text = StringUtils::WideToNarrow(text);
    part.textPx = UI::Scaling::NativeTextPixelSize(bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal,
                                                   m_Transform);
    part.align = align;
    part.bold = bold;
    part.color = UI::RmlBridge::RgbaToCss(color);
    m_Parts.push_back(std::move(part));
}

void FriendWindowRmlBuilder::Button(CUIButton& button)
{
    const auto x = static_cast<float>(button.GetPosition_x());
    const auto y = static_cast<float>(button.GetPosition_y());
    const auto w = static_cast<float>(button.GetWidth());
    const auto h = static_cast<float>(button.GetHeight());
    const bool disabled = button.GetState() == UISTATE_DISABLE;
    const bool pressed = !disabled && button.IsPressedLook();
    if (pressed)
        Sprite("button pressed", x + 1, y + 1, w - 1, h - 1);
    else
        Sprite(disabled ? "button disabled" : "button", x, y, w, h);

    const wchar_t* caption = button.GetCaption();
    if (caption == nullptr)
        return;
    g_pRenderText->SetFont(g_hFont);
    const SIZE size = g_pRenderText->MeasureText(caption, lstrlen(caption));
    const float left = (w - static_cast<float>(size.cx) + 0.5f) / 2;
    const float top = (h - static_cast<float>(size.cy) + 0.5f) / 2;
    if (pressed)
        Text(caption, x + 1 + left, y + 2 + top, RGBA(230, 220, 200, 255));
    else
        Text(caption, x + left, y + 1 + top, RGBA(230, 220, 200, 255));
}

void FriendWindowRmlBuilder::CheckBox(double x, double y, bool checked)
{
    Fill("check-edge", x, y, 9, 1);
    Fill("check-edge", x, y + 8, 9, 1);
    Fill("check-edge", x, y, 1, 9);
    Fill("check-edge", x + 8, y, 1, 9);
    if (checked)
        Sprite("check-mark", x + 2, y + 2, 5, 5);
}

template <typename List> void FriendWindowRmlBuilder::ListScrollBar(List& list)
{
    const TextListScrollBarGeometry bar = list.ComputeLegacyScrollBar();
    const auto x = static_cast<float>(list.GetPosition_x());
    const auto bottom = static_cast<float>(list.GetPosition_y());
    const auto top = bottom - static_cast<float>(list.GetHeight());
    const auto width = static_cast<float>(list.GetWidth());

    const bool upPressed =
        MouseLButtonPush && ::CheckMouseIn(static_cast<int>(x + width - 12), static_cast<int>(top - 1), 13, 13) == TRUE;
    Sprite(upPressed ? "scroll-up pressed" : "scroll-up", x + width - 12, top - 1, 13, 13);
    const bool downPressed = MouseLButtonPush && ::CheckMouseIn(static_cast<int>(x + width - 12),
                                                                static_cast<int>(bottom - 12), 13, 13) == TRUE;
    Sprite(downPressed ? "scroll-down pressed" : "scroll-down", x + width - 12, bottom - 12, 13, 13);

    Fill("scroll-track", x + width - bar.barWidth + 1, bar.rangeTop, 1, bar.rangeBottom - bar.rangeTop);
    Fill("scroll-track", x + width, bar.rangeTop, 1, bar.rangeBottom - bar.rangeTop);

    const float thumbX = x + width - bar.barWidth + 2;
    const float thumbWidth = bar.barWidth - 2;
    if (list.GetLineNum() >= list.GetBoxSize())
    {
        Sprite("scroll-thumb", thumbX, bar.thumbTop, thumbWidth, bar.thumbHeight);
        Sprite("scroll-thumb-top", thumbX, bar.thumbTop, thumbWidth, 1);
        Sprite("scroll-thumb-bottom", thumbX, bar.thumbTop + bar.thumbHeight - 1, thumbWidth, 1);
    }
    else
    {
        // The original filled the whole track and closed it at the thumb's height (not the track's).
        Sprite("scroll-thumb", thumbX, bar.rangeTop, thumbWidth, bar.rangeBottom - bar.rangeTop);
        Sprite("scroll-thumb-top", thumbX, bar.rangeTop, thumbWidth, 1);
        Sprite("scroll-thumb-bottom", thumbX, bar.rangeTop + bar.thumbHeight - 1, thumbWidth, 1);
    }
}

template void FriendWindowRmlBuilder::ListScrollBar<CUIChatPalListBox>(CUIChatPalListBox&);
template void FriendWindowRmlBuilder::ListScrollBar<CUIWindowListBox>(CUIWindowListBox&);
template void FriendWindowRmlBuilder::ListScrollBar<CUILetterListBox>(CUILetterListBox&);

FriendWindowView::FriendWindowView(DWORD windowUIID)
    : m_WindowUIID(windowUIID), m_ModelName("friend_window_" + std::to_string(windowUIID))
{
    UI::RmlBridge::RegisterForThemeReload(this, [this] { ReloadTheme(); });
}

FriendWindowView::~FriendWindowView()
{
    UI::RmlBridge::UnregisterForThemeReload(this);
    Unload();
}

void FriendWindowView::Build()
{
    if (m_pDoc || !RmlUiRuntime::Instance().IsCreated())
        return;
    const bool created = m_Binder.Create(RmlUiRuntime::Instance().GetContext(), m_ModelName,
                                         [](Rml::DataModelConstructor& c, FriendWindowRmlModel& model)
                                         {
                                             c.Bind("root_x", &model.rootX);
                                             c.Bind("root_y", &model.rootY);
                                             c.Bind("root_scale", &model.rootScale);
                                             auto part = c.RegisterStruct<FriendWindowPart>();
                                             part.RegisterMember("kind", &FriendWindowPart::kind);
                                             part.RegisterMember("role", &FriendWindowPart::role);
                                             part.RegisterMember("left", &FriendWindowPart::left);
                                             part.RegisterMember("top", &FriendWindowPart::top);
                                             part.RegisterMember("width", &FriendWindowPart::width);
                                             part.RegisterMember("height", &FriendWindowPart::height);
                                             part.RegisterMember("text", &FriendWindowPart::text);
                                             part.RegisterMember("text_px", &FriendWindowPart::textPx);
                                             part.RegisterMember("align", &FriendWindowPart::align);
                                             part.RegisterMember("bold", &FriendWindowPart::bold);
                                             part.RegisterMember("color", &FriendWindowPart::color);
                                             c.RegisterArray<std::vector<FriendWindowPart>>();
                                             c.Bind("parts", &model.parts);
                                         });
    if (!created)
        return;
    m_pDoc = UI::RmlBridge::LoadThemedDocument(RmlUiRuntime::Instance().GetContext(), DocumentPath, ModelPlaceholder,
                                               "data-model=\"" + m_ModelName + "\"");
}

void FriendWindowView::Unload()
{
    if (!RmlUiRuntime::Instance().IsCreated())
    {
        m_pDoc = nullptr;
        return;
    }
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (m_pDoc)
    {
        context->UnloadDocument(m_pDoc);
        m_pDoc = nullptr;
    }
    m_Binder.Destroy(context);
}

void FriendWindowView::ReloadTheme()
{
    if (!m_pDoc)
        return;
    Unload();
    m_Binder.GetModel().parts.clear();
    Build();
}

bool FriendWindowView::Sync(CUIBaseWindow* window, bool shown)
{
    if (window == nullptr || !shown)
    {
        UI::RmlBridge::SyncDocumentVisibility(m_pDoc, false);
        return false;
    }
    Build();
    if (!m_pDoc)
        return false;

    FriendWindowRmlModel& model = m_Binder.GetModel();
    const UI::Scaling::Transform transform = UI::Scaling::GetActiveTransform();
    const float rootX = static_cast<float>(window->GetPosition_x()) * transform.scaleX + transform.offsetX;
    const float rootY = static_cast<float>(window->GetPosition_y()) * transform.scaleY + transform.offsetY;
    if (model.rootX != rootX || model.rootY != rootY || model.rootScale != transform.scaleX)
    {
        model.rootX = rootX;
        model.rootY = rootY;
        model.rootScale = transform.scaleX;
        m_Binder.MarkDirty("root_x");
        m_Binder.MarkDirty("root_y");
        m_Binder.MarkDirty("root_scale");
        // Text sizes follow the scale; the parts carry them.
    }

    FriendWindowRmlBuilder builder(window->GetPosition_x(), window->GetPosition_y());
    window->CollectRmlView(builder);
    std::vector<FriendWindowPart> parts = builder.TakeParts();
    if (parts != model.parts)
    {
        model.parts = std::move(parts);
        m_Binder.MarkDirty("parts");
    }

    const bool wasVisible = m_pDoc->IsVisible();
    UI::RmlBridge::SyncDocumentVisibility(m_pDoc, true);
    return !wasVisible;
}

void FriendWindowView::PullToFront()
{
    if (m_pDoc && m_pDoc->IsVisible())
        m_pDoc->PullToFront();
}

FriendWindowViews::FriendWindowViews() = default;
FriendWindowViews::~FriendWindowViews() = default;

void FriendWindowViews::Sync(const std::list<CUIBaseWindow*>& windows, bool familyShown)
{
    // Drop the documents of removed windows.
    for (auto it = m_Views.begin(); it != m_Views.end();)
    {
        const bool present = std::any_of(windows.begin(), windows.end(),
                                         [&](CUIBaseWindow* w) { return w != nullptr && w->GetUIID() == it->first; });
        if (present)
            ++it;
        else
            it = m_Views.erase(it);
    }

    bool restack = false;
    std::list<DWORD> order;
    for (CUIBaseWindow* window : windows)
    {
        if (window == nullptr || !window->HasRmlView())
            continue;
        auto& view = m_Views[window->GetUIID()];
        if (!view)
            view = std::make_unique<FriendWindowView>(window->GetUIID());
        const bool shown = familyShown && window->GetState() != UISTATE_HIDE && window->GetState() != UISTATE_READY;
        if (view->Sync(window, shown))
            restack = true;
        if (shown)
            order.push_back(window->GetUIID());
    }

    // Stack the shown documents in the manager's draw order, in front of the other windows.
    if (restack || order != m_Order)
    {
        for (DWORD uiid : order)
            m_Views[uiid]->PullToFront();
        m_Order = std::move(order);
    }
}

void CUIWindowMgr::SyncRmlViews(bool familyShown)
{
    if (!m_pRmlViews)
        m_pRmlViews = std::make_unique<FriendWindowViews>();
    // Draw order first, then the windows the arrange list does not hold (hidden ones keep their
    // documents).
    std::list<CUIBaseWindow*> windows;
    for (DWORD uiid : m_WindowArrangeList)
    {
        auto it = m_WindowMap.find(uiid);
        if (it != m_WindowMap.end())
            windows.push_back(it->second);
    }
    for (const auto& [uiid, window] : m_WindowMap)
    {
        if (std::find(m_WindowArrangeList.begin(), m_WindowArrangeList.end(), uiid) == m_WindowArrangeList.end())
            windows.push_back(window);
    }
    m_pRmlViews->Sync(windows, familyShown);
}
