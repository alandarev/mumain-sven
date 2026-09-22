#include "App/Control/QuickPeerSettleAct.h"
#include "App/Control/ControlUiFrames.h"
#include <utility>

namespace App::Control
{
QuickPeerSettleAct::QuickPeerSettleAct(std::string result, std::string expected, std::function<bool()> ready,
                                       std::function<std::string()> snapshot)
    : m_result(std::move(result)), m_expected(std::move(expected)), m_ready(std::move(ready)),
      m_snapshot(std::move(snapshot)), m_lastFrame(CompletedUiFrames())
{
}
std::string_view QuickPeerSettleAct::Name() const
{
    return "ui";
}
std::chrono::milliseconds QuickPeerSettleAct::Deadline() const
{
    constexpr std::chrono::seconds SettleDeadline{5};
    return SettleDeadline;
}
Act::Status QuickPeerSettleAct::Tick(std::string& response)
{
    // Check readiness before touching deferred window/hero producers, even in the same poll.
    if (!m_ready() || m_snapshot() != m_expected || CompletedUiFrames() < m_lastFrame)
    {
        response = EncodeError(EncodedId(), ErrorCode::NotAllowed, "quick peer UI changed while settling");
        return Status::Finished;
    }
    const auto frame = CompletedUiFrames();
    if (frame == m_lastFrame)
        return Status::Running;
    m_lastFrame = frame;
    if (--m_remaining != 0)
        return Status::Running;
    response = EncodeResult(EncodedId(), m_result);
    return Status::Finished;
}
} // namespace App::Control
