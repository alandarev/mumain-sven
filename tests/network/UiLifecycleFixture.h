#pragma once

#include "UI/Core/WindowSystem.h"
#include "UI/Combat/SiegeWarObserver.h"
#include "Scenes/MainScene.h"
#include "Core/Time/FrameTimerScheduler.h"
#include "UI/Dialogs/MessageBox.h"
#include "UI/Core/UIManager.h"
#include "UI/Windows/MsgWin.h"
#include "UI/RmlBridge/RmlTheme.h"

#include <RmlUi/Core.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <memory>

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
        : m_scene(SceneFlag), m_loading(LoadingWorld),
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
        m_previousTheme = UI::RmlBridge::GetActiveThemeName();
        // The chat window's own document, reduced to the two fields it owns.
        Rml::SetRenderInterface(&m_rmlRenderer);
        m_rmlInitialised = Rml::Initialise();
        // Theme selection clears RmlUi caches: only select while RmlUi is alive.
        if (m_rmlInitialised)
            UI::RmlBridge::SetActiveThemeName("legacy");
        m_rmlContext = m_rmlInitialised ? Rml::CreateContext("ui-lifecycle", {640, 480}) : nullptr;
        if (m_rmlContext != nullptr)
        {
            m_rmlContext->SetSkipHiddenDocuments(true); // As RmlUiRuntime::Create() sets up the production context.
            m_fixtureFile = std::filesystem::temp_directory_path() /
                ("mu-ui-lifecycle-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".rml");
            std::ofstream(m_fixtureFile) << "<rml><body data-model='chat_fixture'><input id='chat_field' type='text'/><input id='whisper_field' type='text'/></body></rml>";
            UI::RmlBridge::SetContextResolver([this](std::string_view) { return m_rmlContext; });
            std::destroy_at(&chat.m_RmlView);
            std::construct_at(&chat.m_RmlView, "chat_fixture",
                [&chat = chat](Rml::DataModelConstructor& c, ChatInputRmlModel& model) { chat.BindRmlModel(c, model); },
                std::vector<UI::RmlBridge::ThemedDocumentSpec>{{m_fixtureFile.string()}});
            if (chat.m_RmlView.Ensure())
                chat.m_RmlView.Document()->Show();
            m_rmlContext->Update();
        }
        quick.SetID(L"Peer");
        quick.Show(false);
        SceneFlag = MAIN_SCENE;
        LoadingWorld = 0;
    }

    ~UiLifecycleFixture()
    {
        quick.m_RmlView.Release();
        chat.m_RmlView.Release();
        UI::RmlBridge::SetContextResolver({});
        std::error_code fileError;
        std::filesystem::remove(m_fixtureFile, fileError);
        std::filesystem::remove(m_quickFixtureFile, fileError);
        if (m_rmlContext != nullptr)
            Rml::RemoveContext("ui-lifecycle");
        if (m_rmlInitialised)
        {
            UI::RmlBridge::SetActiveThemeName(m_previousTheme);
            Rml::Shutdown();
        }
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
        if (m_preparedMessageWindow)
            g_MsgWin.CObject::Show(m_savedMessageVisible);
    }

    UiLifecycleFixture(const UiLifecycleFixture&) = delete;
    UiLifecycleFixture& operator=(const UiLifecycleFixture&) = delete;

    void PrepareMessageWindow()
    {
        // CMsgWin has not run Create in this isolated executable. Initialize only
        // its inherited visibility; derived Show would read uninitialized widget state.
        m_preparedMessageWindow = true;
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

    Rml::Element* UnrelatedInput()
    {
        if (m_rmlContext == nullptr) return nullptr;
        auto* document = m_rmlContext->LoadDocumentFromMemory("<rml><body><input id='unrelated' type='text'/></body></rml>");
        if (document == nullptr) return nullptr;
        document->Show();
        return document->GetElementById("unrelated");
    }

    Rml::ElementDocument* PrepareQuickMenu()
    {
        if (m_rmlContext == nullptr)
            return nullptr;
        m_quickFixtureFile = m_fixtureFile.string() + "-quick.rml";
        std::ofstream(m_quickFixtureFile) <<
            "<rml><head><style>body { pointer-events: none; }"
            "#panel { position: absolute; left: var(--slot-left, 0px); top: var(--slot-top, 0px); width: 112px; height: 140px; pointer-events: auto; }"
            "#rows { display: block; } #rows div { display: block; width: 112px; height: 20px; }</style></head>"
            "<body data-model='quick_fixture'><div id='panel'><div id='rows'><div/><div/></div></div></body></rml>";
        std::destroy_at(&quick.m_RmlView);
        std::construct_at(&quick.m_RmlView, "quick_fixture",
            [&quick = quick](Rml::DataModelConstructor& c, QuickCommandRmlModel& model) { quick.BindRmlModel(c, model); },
            std::vector<UI::RmlBridge::ThemedDocumentSpec>{{m_quickFixtureFile.string()}});
        if (!quick.m_RmlView.Ensure())
            return nullptr;
        quick.m_RmlView.Document()->Show();
        return quick.m_RmlView.Document();
    }

    void Hover(int x, int y)
    {
        if (m_rmlContext != nullptr)
            m_rmlContext->ProcessMouseMove(x, y, 0);
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

private:
    std::string m_previousTheme;
    std::filesystem::path m_fixtureFile;
    std::filesystem::path m_quickFixtureFile;
    UiLifecycleNullRenderer m_rmlRenderer;
    bool m_rmlInitialised = false;
    Rml::Context* m_rmlContext = nullptr;
    bool m_modalPrerequisites = false;
    EGameScene m_scene;
    int m_loading;
    bool m_savedMessageVisible;
    bool m_preparedMessageWindow = false;
};
} // namespace mu::ui::window
