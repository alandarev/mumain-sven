#pragma once

#include "UI/Core/WindowSystem.h"
#include "UI/Combat/SiegeWarObserver.h"
#include "UI/Widgets/UIControls.h"
#include "Scenes/MainScene.h"
#include "Core/Time/FrameTimerScheduler.h"

extern int LoadingWorld;

namespace mu::ui::window
{
// Test-only ownership bridge. No renderer/texture Create, gameplay input or
// network processing runs. The registered objects and observation methods are real.
class UiLifecycleFixture
{
public:
    UiLifecycleFixture() : m_scene(SceneFlag), m_loading(LoadingWorld)
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
        chat.m_pChatInputBox = new CUITextInputBox;
        chat.m_pWhsprIDInputBox = new CUITextInputBox;
        quick.SetID(L"Peer");
        quick.Show(false);
        SceneFlag = MAIN_SCENE;
        LoadingWorld = 0;
    }

    ~UiLifecycleFixture()
    {
        chat.m_pChatInputBox->SetState(UISTATE_HIDE);
        chat.m_pWhsprIDInputBox->SetState(UISTATE_HIDE);
        auto* system = CSystem::GetInstance();
        system->m_pNewUIMng = nullptr;
        system->m_pNewSiegeWarfare = nullptr;
        system->m_pNewChatInputBox = nullptr;
        system->m_pNewQuickCommandWindow = nullptr;
        system->m_pNewUIHotKey = nullptr;
        registry.RemoveAllUIObjs();
        SceneFlag = m_scene;
        LoadingWorld = m_loading;
    }

    UiLifecycleFixture(const UiLifecycleFixture&) = delete;
    UiLifecycleFixture& operator=(const UiLifecycleFixture&) = delete;

    void PopulateSiege()
    {
        // The manager owns this actual child; ordinary InitMiniMapUI/Release
        // retires it. Deliberately bypass asset loading, not the observer.
        siege.m_pSiegeWarUI = new CSiegeWarObserver;
    }

    CUITextInputBox& Input(bool whisper)
    {
        return *(whisper ? chat.m_pWhsprIDInputBox : chat.m_pChatInputBox);
    }

    CManager registry;
    CSiegeWarfare siege;
    CChatInputBox chat;
    CQuickCommandWindow quick;
    CHotKey hotkey;

private:
    int m_scene;
    int m_loading;
};
} // namespace mu::ui::window
