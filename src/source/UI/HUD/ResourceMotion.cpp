#include "ResourceMotion.h"
#include <algorithm>

namespace UI::HUD
{
namespace
{
constexpr double HealSweepMs = 280.0;
float Progress(double elapsed, double duration)
{
    if (duration <= 0.0)
        return 1.f;
    const float t = static_cast<float>(std::clamp(elapsed / duration, 0.0, 1.0));
    return t * t * (3.f - 2.f * t);
}
} // namespace

ResourceMotion::ResourceMotion(double lossHoldMs, double lossDrainMs, double recoveryMs)
    : m_HoldMs(lossHoldMs), m_DrainMs(lossDrainMs), m_RecoveryMs(recoveryMs)
{
}

void ResourceMotion::Reset()
{
    m_Initialized = false;
}

ResourceMotion::Frame ResourceMotion::Sample(double nowMs) const
{
    Frame frame;
    const double elapsed = std::max(0.0, nowMs - m_ChangedAt);
    frame.value = m_Healing ? m_Start + (m_Value - m_Start) * Progress(elapsed, m_RecoveryMs) : m_Value;
    frame.trail =
        m_Healing ? frame.value : m_TrailStart + (m_Value - m_TrailStart) * Progress(elapsed - m_HoldMs, m_DrainMs);
    if (m_Healing)
    {
        frame.sweep = Progress(elapsed, HealSweepMs);
        frame.sweepOpacity = 1.f - frame.sweep;
    }
    return frame;
}

ResourceMotion::Frame ResourceMotion::Update(float fraction, double nowMs, bool reducedMotion)
{
    fraction = std::clamp(fraction, 0.f, 1.f);
    if (!m_Initialized || reducedMotion || fraction == 0.f)
    {
        m_Value = m_Start = m_TrailStart = fraction;
        m_ChangedAt = nowMs;
        m_Healing = false;
        m_Initialized = true;
        return {fraction, fraction, 0.f, 0.f};
    }
    if (fraction != m_Value)
    {
        const Frame visible = Sample(nowMs);
        m_Healing = fraction > m_Value;
        m_Start = visible.value;
        m_TrailStart = m_Healing ? fraction : std::max(visible.trail, visible.value);
        m_Value = fraction;
        m_ChangedAt = nowMs;
    }
    return Sample(nowMs);
}
} // namespace UI::HUD
