#pragma once

// RmlUi presentation of the friends family (CUIWindowMgr's windows, UIWindows.h): one document
// (friend_window.rml) and one data model per open window. The native windows and controls keep
// their data, hit tests, drags, resizing, buttons and messages; each frame the document is rebuilt
// from what their Render() drew -- the same geometry, as named parts (FriendWindowRmlModel.h) --
// and the documents are stacked in the manager's draw order.

#include "UI/Party/FriendWindowRmlModel.h"
#include "UI/RmlBridge/RmlModelBinder.h"
#include "UI/Scaling/UITransform.h"

#include <list>
#include <map>
#include <memory>
#include <string>

namespace Rml
{
class ElementDocument;
}

class CUIBaseWindow;
class CUIButton;

// Collects the parts of one window, in native (FloatingWorkspace reference) coordinates, relative
// to the window's top-left corner.
class FriendWindowRmlBuilder
{
public:
    FriendWindowRmlBuilder(int originX, int originY);

    void Fill(const char* role, double x, double y, double width, double height);
    void Sprite(const char* role, double x, double y, double width, double height);
    // RenderText(x, y, text) in g_hFont (g_hFontBold when bold); a box width > 0 clips (align 0)
    // or centres (align 1) the text in it.
    void Text(const wchar_t* text, double x, double y, DWORD color, bool bold = false, double boxWidth = 0.0,
              int align = 0);
    // CUIButton::Render().
    void Button(CUIButton& button);
    // RenderCheckBox().
    void CheckBox(double x, double y, bool checked);
    // The old-style list scroll bar the friends family's list boxes draw (their RenderInterface()).
    template <typename List> void ListScrollBar(List& list);

    std::vector<FriendWindowPart> TakeParts()
    {
        return std::move(m_Parts);
    }

private:
    double m_OriginX;
    double m_OriginY;
    UI::Scaling::Transform m_Transform;
    std::vector<FriendWindowPart> m_Parts;
};

// One window's document.
class FriendWindowView
{
public:
    explicit FriendWindowView(DWORD windowUIID);
    ~FriendWindowView();
    FriendWindowView(const FriendWindowView&) = delete;
    FriendWindowView& operator=(const FriendWindowView&) = delete;

    // Rebuilds the document from the window (nullptr or !shown: hidden). Returns true when the
    // document became visible this frame.
    bool Sync(CUIBaseWindow* window, bool shown);
    void PullToFront();
    void ReloadTheme();

private:
    void Build();
    void Unload();

    DWORD m_WindowUIID;
    std::string m_ModelName;
    Rml::ElementDocument* m_pDoc = nullptr;
    RmlModelBinder<FriendWindowRmlModel> m_Binder;
};

// The documents of every window of the manager with an RmlUi view.
class FriendWindowViews
{
public:
    FriendWindowViews();
    ~FriendWindowViews();

    // windows: the manager's windows in draw order (back to front); nullptr entries are skipped.
    void Sync(const std::list<CUIBaseWindow*>& windows, bool familyShown);

private:
    std::map<DWORD, std::unique_ptr<FriendWindowView>> m_Views;
    std::list<DWORD> m_Order; // the draw order the documents were last stacked in
};
