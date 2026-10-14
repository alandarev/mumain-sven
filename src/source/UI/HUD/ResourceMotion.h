#pragma once

namespace UI::HUD
{
class ResourceMotion
{
public:
    struct Frame
    {
        float value = 0.f;
        float trail = 0.f;
        float sweep = 0.f;
        float sweepOpacity = 0.f;
    };

    ResourceMotion(double lossHoldMs, double lossDrainMs, double recoveryMs = 0.0);
    Frame Update(float fraction, double nowMs, bool reducedMotion = false);
    void Reset();

private:
    Frame Sample(double nowMs) const;
    double m_HoldMs;
    double m_DrainMs;
    double m_RecoveryMs;
    double m_ChangedAt = 0.0;
    float m_Value = 0.f;
    float m_Start = 0.f;
    float m_TrailStart = 0.f;
    bool m_Initialized = false;
    bool m_Healing = false;
};
} // namespace UI::HUD
