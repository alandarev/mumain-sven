#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace UI::Character
{
struct ArmorySource
{
    int slot = -1;
    std::string name, operation;
    float value = 0.f;
    std::string stat, amount, group;
};
struct ArmoryAttribute
{
    int index = 0;
    std::string label;
    std::uint32_t base = 0, maximum = 0;
    float total = 0.f;
    std::vector<ArmorySource> sources;
};
struct ArmoryRow
{
    std::string id, category, label, value, explanation;
    std::vector<ArmorySource> sources;
};
struct ArmorySkill
{
    int number = 0;
    std::string name, output, kind, costs, restriction;
    std::vector<std::string> calculation;
};
struct ArmorySnapshot
{
    std::uint32_t requestId = 0, freePoints = 0;
    std::string character, status, target;
    std::vector<ArmoryAttribute> attributes;
    std::vector<ArmoryRow> rows;
    ArmorySkill skill;
};
struct ArmoryRequest
{
    std::uint32_t requestId = 0, expectedPoints = 0;
    std::uint16_t skill = 0, target = 0xffff;
    bool apply = false;
    std::array<std::uint16_t, 5> additions{};
    std::array<std::uint32_t, 5> bases{};
};
} // namespace UI::Character
