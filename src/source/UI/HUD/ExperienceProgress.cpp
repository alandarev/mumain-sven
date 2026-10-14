#include "ExperienceProgress.h"
#include <algorithm>

namespace UI::HUD
{
namespace
{
constexpr std::int64_t HighLevelBoundary = 255;
constexpr std::int64_t MasterBaseLevel = 400;
constexpr std::int64_t MasterOffset = 3892250000;
constexpr int MaximumWireLevel = 65535;
constexpr double LevelHoldMs = 320.0;
constexpr double GainMs = 350.0;
constexpr double HighlightMs = 900.0;
std::int64_t Curve(std::int64_t level)
{
    const auto ordinary = (9 + level) * level * level * 10;
    const auto over = std::max<std::int64_t>(0, level - HighLevelBoundary);
    return ordinary + (9 + over) * over * over * 1000;
}
} // namespace

std::int64_t ExperienceLevelStart(int level, bool master)
{
    if (level < 0 || level > MaximumWireLevel)
        return 0;
    if (master)
        return (Curve(level + MasterBaseLevel) - MasterOffset) / 2;
    return Curve(std::max(0, level - 1));
}

float NormalizeExperience(std::int64_t current, std::int64_t next, int level, bool master)
{
    const auto start = ExperienceLevelStart(level, master);
    if (next <= start)
        return 0.f;
    const double progress = (static_cast<double>(current) - static_cast<double>(start)) /
                            (static_cast<double>(next) - static_cast<double>(start));
    return static_cast<float>(std::clamp(progress, 0.0, 1.0));
}

void ExperienceProgress::Reset()
{
    m_Initialized = false;
}

ExperienceProgress::Frame ExperienceProgress::Sample(double nowMs) const
{
    const double elapsed = std::max(0.0, nowMs - m_ChangedAt);
    const double hold = m_LevelUp ? LevelHoldMs : 0.0;
    if (elapsed < hold)
        return {1.f, 0.f, 1.f, 1.f, true};
    const float t = static_cast<float>(std::clamp((elapsed - hold) / GainMs, 0.0, 1.0));
    const float start = m_LevelUp ? 0.f : m_Start;
    const float value = start + (m_Target - start) * t;
    const float opacity = static_cast<float>(std::clamp(1.0 - (elapsed - hold) / HighlightMs, 0.0, 1.0));
    return {value, start, std::max(0.f, value - start), opacity, m_LevelUp && elapsed < hold + GainMs};
}

ExperienceProgress::Frame ExperienceProgress::Update(float fraction, int level, bool master, double nowMs,
                                                     bool reducedMotion)
{
    fraction = std::clamp(fraction, 0.f, 1.f);
    if (!m_Initialized || master != m_Master || level < m_Level || (level == m_Level && fraction < m_Target) ||
        reducedMotion)
    {
        m_Target = m_Start = fraction;
        m_Level = level;
        m_Master = master;
        m_LevelUp = false;
        m_Initialized = true;
        m_ChangedAt = nowMs - HighlightMs;
        return {fraction, fraction, 0.f, 0.f, false};
    }
    if (level != m_Level || fraction != m_Target)
    {
        m_Start = Sample(nowMs).value;
        m_LevelUp = level > m_Level;
        m_Level = level;
        m_Target = fraction;
        m_ChangedAt = nowMs;
    }
    return Sample(nowMs);
}
} // namespace UI::HUD
