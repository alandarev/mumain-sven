#pragma once

#if MU_ENABLE_CONTROL_SOCKET
#include "UI/NewUI/Dialogs/NewUICommonMessageBox.h"
#include <string_view>

namespace SEASON3B
{
// A native presentation of the developer-only, callback-free comparison fixture.
// Only CreateOwned can grant identity. Every delete path invokes this destructor.
class ControlModalMessageBox final : public CNewUICommonMessageBox
{
public:
    ControlModalMessageBox() = default;
    ~ControlModalMessageBox() override;
    ControlModalMessageBox(const ControlModalMessageBox&) = delete;
    ControlModalMessageBox& operator=(const ControlModalMessageBox&) = delete;
    void Release() override;
    bool Create(int x, int y, int width, int height, float priority = 3.f) override;

    static std::string CreateOwned(CNewUIMessageBoxMng& manager, std::string_view nonce);
    static bool Owns(const CNewUIMessageBoxMng& manager, std::string_view token);
    static bool Retire(CNewUIMessageBoxMng& manager, std::string_view token);

private:
    friend class CNewUIMessageBoxMng;
    static void InvalidateOwner(const CNewUIMessageBoxMng& manager);
    void InvalidateIdentity();
    static ControlModalMessageBox* s_owned;
    std::string m_token;
};
} // namespace SEASON3B
#endif
