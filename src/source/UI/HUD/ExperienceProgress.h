#pragma once
#include <cstdint>

namespace UI::HUD
{
std::int64_t ExperienceLevelStart(int level, bool master);
float NormalizeExperience(std::int64_t current, std::int64_t next, int level, bool master);

class ExperienceProgress
{
public:
    struct Frame
    {
        float value = 0.f;
        float gainStart = 0.f;
        float gainFraction = 0.f;
        float gainOpacity = 0.f;
        bool levelUp = false;
    };
    Frame Update(float fraction, int level, bool master, double nowMs, bool reducedMotion = false);
    void Reset();

private:
    Frame Sample(double nowMs) const;
    float m_Target = 0.f;
    float m_Start = 0.f;
    int m_Level = 0;
    double m_ChangedAt = 0.0;
    bool m_Master = false;
    bool m_LevelUp = false;
    bool m_Initialized = false;
};
} // namespace UI::HUD
