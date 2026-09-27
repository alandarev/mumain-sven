#pragma once

#include "UI/Core/WindowSystem.h"
#include "UI/Combat/SiegeWarObserver.h"
#include "UI/Widgets/UIControls.h"
#include "Scenes/MainScene.h"
#include "Core/Time/FrameTimerScheduler.h"
#include "UI/Dialogs/MessageBox.h"
#include "UI/Core/UIManager.h"
#include "UI/Windows/MsgWin.h"

#include <RmlUi/Core.h>

extern int LoadingWorld;

namespace mu::ui::window
{
// Headless RmlUi: documents lay out and take focus, nothing is drawn.
struct UiLifecycleNullRenderer : Rml::RenderInterface
{
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override
    {
        return 0;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override
    {
        return 0;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override
    {
        return 0;
    }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};

// Test-only ownership bridge. No renderer/texture Create, gameplay input or
// network processing runs. The registered objects and observation methods are real.
class UiLifecycleFixture
{
public:
    UiLifecycleFixture()
        : m_scene(SceneFlag), m_loading(LoadingWorld), m_savedPopup(g_pUIPopup),
          m_savedMessageVisible(g_MsgWin.IsVisible())
    {
        auto* system = CSystem::GetInstance();
        system->m_pNewUIMng = &registry;
        system->m_pNewSiegeWarfare = &siege;
        system->m_pNewChatInputBox = &chat;
        system->m_pNewQuickCommandWindow = &quick;
        system->m_pNewUIHotKey = &hotkey;
        registry.AddUIObj(INTERFACE_SIEGEWARFARE, &siege);
        registry.AddUIObj(INTERFACE_CHATINPUTBOX, &chat);
        registry.AddUIObj(INTERFACE_QUICK_COMMAND, &quick);
        registry.AddUIObj(INTERFACE_HOTKEY, &hotkey);
        // The chat window's own document, reduced to the two fields it owns.
        Rml::SetRenderInterface(&m_rmlRenderer);
        m_rmlInitialised = Rml::Initialise();
        m_rmlContext = m_rmlInitialised ? Rml::CreateContext("ui-lifecycle", {640, 480}) : nullptr;
        if (m_rmlContext != nullptr)
        {
            chat.m_pRmlDoc = m_rmlContext->LoadDocumentFromMemory(
                "<rml><body><input id='chat_field' type='text'/><input id='whisper_field' type='text'/></body></rml>");
            if (chat.m_pRmlDoc != nullptr)
                chat.m_pRmlDoc->Show();
            m_rmlContext->Update();
        }
        quick.SetID(L"Peer");
        quick.Show(false);
        SceneFlag = MAIN_SCENE;
        LoadingWorld = 0;
    }

    ~UiLifecycleFixture()
    {
        chat.m_pRmlDoc = nullptr; // Owned by the context removed below.
        if (m_rmlContext != nullptr)
            Rml::RemoveContext("ui-lifecycle");
        if (m_rmlInitialised)
            Rml::Shutdown();
        Rml::SetRenderInterface(nullptr);
        if (m_modalPrerequisites)
        {
            friends.Release();
            Core::Time::FrameTimerScheduler::Instance().Kill(CHATCONNECT_TIMER);
            CSystem::GetInstance()->m_pNewCryWolfInterface = nullptr;
        }
        auto* system = CSystem::GetInstance();
        system->m_pNewUIMng = nullptr;
        system->m_pNewSiegeWarfare = nullptr;
        system->m_pNewChatInputBox = nullptr;
        system->m_pNewQuickCommandWindow = nullptr;
        system->m_pNewUIHotKey = nullptr;
        registry.RemoveAllUIObjs();
        SceneFlag = m_scene;
        LoadingWorld = m_loading;
        g_pUIPopup = m_savedPopup;
        if (m_preparedPopup)
            g_MsgWin.CObject::Show(m_savedMessageVisible);
    }

    UiLifecycleFixture(const UiLifecycleFixture&) = delete;
    UiLifecycleFixture& operator=(const UiLifecycleFixture&) = delete;

    void PreparePopup()
    {
        g_pUIPopup = &popup;
        // CMsgWin has not run Create in this isolated executable. Initialize only
        // its inherited visibility; derived Show would read uninitialized widget state.
        m_preparedPopup = true;
        g_MsgWin.CObject::Show(false);
    }

    bool PrepareModalPrerequisites()
    {
        // Real empty owners for predicates preceding Siege/focus in modal policy.
        // No messagebox/scene asset startup and no Friends main-window request.
        m_modalPrerequisites = true;
        CSystem::GetInstance()->m_pNewCryWolfInterface = &cryWolf;
        registry.AddUIObj(INTERFACE_CRYWOLF, &cryWolf);
        registry.AddUIObj(INTERFACE_MESSAGEBOX, g_MessageBox);
        return friends.Create(&registry);
    }

    void PopulateSiege()
    {
        // The manager owns this actual child; ordinary InitMiniMapUI/Release
        // retires it. Deliberately bypass asset loading, not the observer.
        siege.m_pSiegeWarUI = new CSiegeWarObserver;
    }

    // Null when headless RmlUi could not start; callers REQUIRE it.
    Rml::Element* Input(bool whisper)
    {
        return chat.GetField(whisper ? "whisper_field" : "chat_field");
    }

    void Settle()
    {
        if (m_rmlContext != nullptr)
            m_rmlContext->Update();
    }

    CManager registry;
    CSiegeWarfare siege;
    CChatInputBox chat;
    CQuickCommandWindow quick;
    CHotKey hotkey;
    CCryWolf cryWolf;
    CFriendWindow friends;
    CUIPopup popup;

private:
    UiLifecycleNullRenderer m_rmlRenderer;
    bool m_rmlInitialised = false;
    Rml::Context* m_rmlContext = nullptr;
    bool m_modalPrerequisites = false;
    EGameScene m_scene;
    int m_loading;
    CUIPopup* m_savedPopup;
    bool m_savedMessageVisible;
    bool m_preparedPopup = false;
};
} // namespace mu::ui::window
