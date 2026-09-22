#include "stdafx.h"
#include "UI/NewUI/Dialogs/ControlModalMessageBox.h"

#if MU_ENABLE_CONTROL_SOCKET
#include <cstdint>
#include <limits>

namespace SEASON3B
{
ControlModalMessageBox* ControlModalMessageBox::s_owned = nullptr;

ControlModalMessageBox::~ControlModalMessageBox()
{
    Release();
}

void ControlModalMessageBox::Release()
{
    m_token.clear();
    if (s_owned == this)
        s_owned = nullptr;
    CNewUIMessageBoxBase::Release();
}

bool ControlModalMessageBox::Create(int x, int y, int width, int height, float priority)
{
    Release();
    return CNewUIMessageBoxBase::Create(x, y, width, height, priority);
}

std::string ControlModalMessageBox::CreateOwned(CNewUIMessageBoxMng& manager, std::string_view nonce)
{
    // This sequence counts fixture creations only, never ordinary UI lifetimes.
    static std::uint64_t sequence = 0;
    if (s_owned != nullptr || !manager.HasMessageBoxStorage() || !manager.IsEmpty() || manager.HasPendingEvents() ||
        manager.HasPendingPointerPress() || nonce.empty() || sequence == std::numeric_limits<std::uint64_t>::max())
        return {};
    auto* box = manager.NewMessageBox(MSGBOX_CLASS(ControlModalMessageBox));
    if (box == nullptr)
        return {};
    box->CNewUICommonMessageBox::Create(MSGBOX_COMMON_TYPE_OK,
                                        L"UI comparison fixture. Local observation test. No game action.");
    // Fixed native presentation only. No invitation/transaction or queued Close callback.
    box->RemoveAllCallbackFuncs();
    box->LockOkButton();
    box->m_token = std::string(nonce) + ":" + std::to_string(++sequence);
    s_owned = box;
    return box->m_token;
}

bool ControlModalMessageBox::Owns(const CNewUIMessageBoxMng& manager, std::string_view token)
{
    const auto& boxes = manager.GetMessageBoxes();
    // Membership is checked before dereferencing; a destructor clears s_owned.
    // A new object at the same address starts without identity and gets a new token.
    return s_owned != nullptr && boxes.size() == 1 && boxes.front() == s_owned && !token.empty() &&
           s_owned->m_token == token;
}

bool ControlModalMessageBox::Retire(CNewUIMessageBoxMng& manager, std::string_view token)
{
    if (!Owns(manager, token) || manager.HasPendingEvents() || manager.HasPendingPointerPress())
        return false;
    manager.DeleteMessageBox(s_owned); // Synchronous exact deletion: never DESTROY/PopAllEvents.
    return true;
}
} // namespace SEASON3B
#endif
