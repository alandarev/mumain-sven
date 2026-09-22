#pragma once
#include "App/Control/ControlDispatcher.h"
#include <functional>

namespace App::Control
{
// Used only by guarded quick operations. Callbacks observe; never clean up UI.
class QuickPeerSettleAct final : public Act
{
public:
    QuickPeerSettleAct(std::string result, std::string expected, std::function<bool()> ready,
                       std::function<std::string()> snapshot);
    std::string_view Name() const override;
    std::chrono::milliseconds Deadline() const override;
    Status Tick(std::string& response) override;

private:
    std::string m_result;
    std::string m_expected;
    std::function<bool()> m_ready;
    std::function<std::string()> m_snapshot;
    std::uint64_t m_lastFrame;
    static constexpr int RequiredCompletedFrames = 2;
    int m_remaining = RequiredCompletedFrames;
};
} // namespace App::Control
