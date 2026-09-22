#include "stdafx.h"
#include "doctest.h"
#include "UI/NewUI/Dialogs/NewUICustomMessageBox.h"
#include "UI/NewUI/Dialogs/NewUICommonMessageBox.h"

namespace SEASON3B
{
// No renderer or world is initialized. Only storage is installed; all stack,
// queue, callback dispatch and cancellation operations below are production code.
struct MessageBoxLifecycleFixture
{
    CNewUIMessageBoxMng& manager = *g_MessageBox;
    MessageBoxLifecycleFixture()
    {
        REQUIRE(manager.m_pMsgBoxFactory == nullptr);
        REQUIRE(manager.IsEmpty());
        REQUIRE_FALSE(manager.HasPendingEvents());
        manager.m_pMsgBoxFactory = new CNewUIMessageBoxFactory;
    }
    ~MessageBoxLifecycleFixture()
    {
        manager.PopAllEvents();
        manager.PopAllMessageBoxes();
        delete manager.m_pMsgBoxFactory;
        manager.m_pMsgBoxFactory = nullptr;
    }
};
}

TEST_CASE("Native messagebox stack and queued cancellation preserve system menu identity [ui][control-ui]")
{
    using namespace SEASON3B;
    MessageBoxLifecycleFixture fixture;
    auto& manager = fixture.manager;
    CHECK(manager.IsEmpty());
    CHECK(manager.GetMessageBoxes().empty());
    CHECK_FALSE(manager.IsOnlySystemMenu());
    auto* unrelated = manager.NewMessageBox(MSGBOX_CLASS(CNewUICommonMessageBox));
    CHECK_FALSE(manager.IsOnlySystemMenu());
    CHECK_FALSE(manager.IsEmpty());
    manager.DeleteMessageBox(unrelated);
    auto* menu = manager.NewMessageBox(MSGBOX_CLASS(CSystemMenuMsgBox));
    REQUIRE(menu != nullptr);
    REQUIRE(menu->Create()); // Installs the original callbacks, no textures loaded.
    CHECK_FALSE(manager.IsEmpty());
    REQUIRE(manager.GetMessageBoxes().size() == 1);
    CHECK(dynamic_cast<const CSystemMenuMsgBox*>(manager.GetMessageBoxes().front()) == menu);
    CHECK_FALSE(manager.HasPendingEvents());
    CHECK(manager.IsOnlySystemMenu());
    CHECK(menu->GetCallbackFunc(MSGBOX_EVENT_PRESSKEY_ESC) == CSystemMenuMsgBox::CancelBtnDown);
    manager.SendEvent(menu, MSGBOX_EVENT_PRESSKEY_ESC);
    CHECK(manager.HasPendingEvents());
    CHECK_FALSE(manager.IsOnlySystemMenu());
    manager.Update(); // Cancel queues DESTROY; it does not synchronously pop the menu.
    CHECK_FALSE(manager.IsEmpty());
    CHECK(manager.HasPendingEvents());
    CHECK_FALSE(manager.IsOnlySystemMenu());
    manager.Update();
    CHECK(manager.IsEmpty());
    CHECK_FALSE(manager.HasPendingEvents());

    menu = manager.NewMessageBox(MSGBOX_CLASS(CSystemMenuMsgBox));
    REQUIRE(menu->Create());
    auto* other = manager.NewMessageBox(MSGBOX_CLASS(CSystemMenuMsgBox));
    REQUIRE(other->Create());
    CHECK_FALSE(manager.IsOnlySystemMenu());
    CHECK(manager.GetMessageBoxes().size() == 2); // Never the sole expected menu.
    manager.DeleteMessageBox(other);
    CHECK(manager.GetMessageBoxes().size() == 1);
    CSystemMenuMsgBox::CancelBtnDown(menu, leaf::xstreambuf());
    CHECK(manager.HasPendingEvents());
    CHECK_FALSE(manager.IsOnlySystemMenu());
    manager.Update();
    CHECK(manager.IsEmpty());
    CHECK_FALSE(manager.HasPendingEvents());
}
