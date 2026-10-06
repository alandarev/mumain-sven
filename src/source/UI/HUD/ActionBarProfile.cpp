#include "ActionBarProfile.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace UI::HUD
{
namespace
{
constexpr int FirstFunctionKey = 112;
constexpr int FunctionKeyCount = 12;
constexpr int MaximumIdentity = 8191;
constexpr int MaximumItemLevel = 15;
} // namespace
ActionBarProfile::ActionBarProfile()
{
    ResetKeys();
}
void ActionBarProfile::ResetKeys()
{
    for (int i = 0; i < ActionCount; ++i)
        bindings[i].key = i == ActionCount - 1 ? '0' : '1' + i;
    bindings[10].key = 'Q';
    bindings[11].key = 'W';
    bindings[12].key = 'E';
}
int ActionBarProfile::ParseKey(std::string key)
{
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::toupper(c); });
    if (key == "NONE")
        return 0;
    if (key.size() == 1 && ((key[0] >= 'A' && key[0] <= 'Z') || (key[0] >= '0' && key[0] <= '9')))
        return key[0];
    if (key.size() >= 2 && key.size() <= 3 && key[0] == 'F')
    {
        const auto number = key.substr(1);
        for (int i = 1; i <= FunctionKeyCount; ++i)
            if (number == std::to_string(i))
                return FirstFunctionKey + i - 1;
    }
    return -1;
}
std::string ActionBarProfile::KeyName(int key)
{
    if (key == 0)
        return "None";
    if (key >= FirstFunctionKey && key < FirstFunctionKey + FunctionKeyCount)
        return "F" + std::to_string(key - FirstFunctionKey + 1);
    return std::string(1, static_cast<char>(key));
}
bool ActionBarProfile::Rebind(int slot, const std::string& key, std::string& error)
{
    const int parsed = ParseKey(key);
    if (slot < 0 || slot >= BindingCount || parsed < 0)
    {
        error = "Use a letter, digit, F1-F12, or None.";
        return false;
    }
    for (int i = 0; i < BindingCount; ++i)
        if (i != slot && parsed != 0 && bindings[i].key == parsed)
        {
            error = "That key belongs to another action. Unbind it first.";
            return false;
        }
    bindings[slot].key = parsed;
    error.clear();
    return true;
}
bool ActionBarProfile::Load(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file)
        return false;
    ActionBarProfile loaded;
    int version = 0;
    int motion = 0;
    if (!(file >> version >> motion) || version != 1 || (motion != 0 && motion != 1))
        return false;
    loaded.reducedMotion = motion != 0;
    std::set<int> keys;
    for (auto& binding : loaded.bindings)
    {
        int kind = 0;
        std::string key;
        if (!(file >> kind >> binding.identity >> binding.itemLevel >> key) || kind < 0 ||
            kind > static_cast<int>(Kind::Pet) || binding.identity < -1 || binding.identity > MaximumIdentity ||
            binding.itemLevel < 0 || binding.itemLevel > MaximumItemLevel)
            return false;
        binding.kind = static_cast<Kind>(kind);
        binding.key = ParseKey(key);
        if (binding.key < 0 || (binding.key != 0 && !keys.insert(binding.key).second))
            return false;
    }
    std::string extra;
    if (file >> extra)
        return false;
    *this = loaded;
    return true;
}
bool ActionBarProfile::Save(const std::filesystem::path& path) const
{
    std::error_code error;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), error);
    if (error)
        return false;
    auto temporary = path;
    temporary += ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    file << 1 << ' ' << reducedMotion << '\n';
    for (const auto& binding : bindings)
        file << static_cast<int>(binding.kind) << ' ' << binding.identity << ' ' << binding.itemLevel << ' '
             << KeyName(binding.key) << '\n';
    file.close();
    if (!file)
        return false;
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::filesystem::rename(temporary, path, error);
    return !error;
#endif
}
} // namespace UI::HUD
