#pragma once
#include <string>
#include <string_view>

namespace UI::Skills
{
struct SkillAvailability
{
    enum class Reason
    {
        None,
        Invalid,
        Requirements,
        Level,
        Strength,
        Dexterity,
        Energy,
        Charisma,
        CastleSiege,
        SkillBrand,
        Mount,
        Spear,
        Dismount,
        Party,
        EventMap,
        DarkHorse,
        BuffActive,
        Weapon,
        Gloves,
        Transformation,
        WeaponSkill,
        Bow,
        Dead,
        NotLearned,
        Pet,
        Mana,
        Ag,
        Cooldown,
        SafeZone,
        Stun,
        Sleep
    };
    Reason reason = Reason::None;
    int required = 0, current = 0;
    bool Usable() const
    {
        return reason == Reason::None;
    }
    bool operator==(const SkillAvailability&) const = default;
    SkillAvailability WithResources(int mana, int ag, int manaCost, int agCost, int cooldownMs) const;
    std::string Description(std::string_view weaponSkillName) const;
};

// The existing HUD equipment, attributes, mount, party, event and buff restrictions.
// Resource/cooldown feedback is added separately by the compact action presentation.
SkillAvailability ReadSkillRequirements(int skillType);
} // namespace UI::Skills
