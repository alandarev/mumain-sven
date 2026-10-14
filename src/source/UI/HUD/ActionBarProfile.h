#pragma once
#include <array>
#include <filesystem>
#include <string>

namespace UI::HUD
{
class ActionBarProfile
{
public:
    static constexpr int ActionCount = 10;
    static constexpr int BindingCount = 13;
    enum class Kind
    {
        Empty,
        Skill,
        Item,
        Pet
    };
    struct Binding
    {
        Kind kind = Kind::Empty;
        int identity = -1;
        int itemLevel = 0;
        int key = 0;
    };
    ActionBarProfile();
    void ResetKeys();
    bool Rebind(int slot, const std::string& key, std::string& error);
    bool Load(const std::filesystem::path& path);
    bool Save(const std::filesystem::path& path) const;
    static int ParseKey(std::string key);
    static std::string KeyName(int key);
    std::array<Binding, BindingCount> bindings;
    bool reducedMotion = false;
};
} // namespace UI::HUD
