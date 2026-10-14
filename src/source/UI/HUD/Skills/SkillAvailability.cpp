#include "SkillAvailability.h"
#include <algorithm>

namespace UI::Skills
{
SkillAvailability SkillAvailability::WithResources(int mana, int ag, int manaCost, int agCost, int cooldownMs) const
{
    if (!Usable())
        return *this;
    if (cooldownMs > 0)
        return {Reason::Cooldown, cooldownMs};
    if (mana < manaCost)
        return {Reason::Mana, manaCost, mana};
    if (ag < agCost)
        return {Reason::Ag, agCost, ag};
    return {};
}

std::string SkillAvailability::Description(std::string_view weaponSkillName) const
{
    const auto amount = [this](std::string_view label)
    {
        return "Requires " + std::to_string(required) + " " + std::string(label) + " (you have " +
               std::to_string(current) + ").";
    };
    switch (reason)
    {
    case Reason::None:
        return {};
    case Reason::Level:
        return "Requires level " + std::to_string(required) + " (you are " + std::to_string(current) + ").";
    case Reason::Strength:
        return amount("Strength");
    case Reason::Dexterity:
        return amount("Agility");
    case Reason::Energy:
        return amount("Energy");
    case Reason::Charisma:
        return amount("Command");
    case Reason::Mana:
        return amount("mana");
    case Reason::Ag:
        return amount("AG");
    case Reason::Cooldown:
        return "Ready in " + std::to_string(std::max(1, required / 1000 + (required % 1000 != 0))) + "s.";
    case Reason::CastleSiege:
        return "Only available during an active Castle Siege.";
    case Reason::SkillBrand:
        return "Unavailable while the skill-brand buff is active.";
    case Reason::Mount:
        return "Mount a Uniria, Dinorant or Fenrir.";
    case Reason::Spear:
        return "Equip a spear.";
    case Reason::Dismount:
        return "Dismount to use this skill.";
    case Reason::Party:
        return "Join a party to use this skill.";
    case Reason::EventMap:
        return "Unavailable in this event map.";
    case Reason::DarkHorse:
        return "Equip a living Dark Horse.";
    case Reason::BuffActive:
        return "This buff is already active.";
    case Reason::Weapon:
        return "Requires a main-hand weapon; staves are incompatible.";
    case Reason::Gloves:
        return "Equip the gloves required by this skill.";
    case Reason::Transformation:
        return "Remove the transformation ring to use this skill.";
    case Reason::WeaponSkill:
        return "Equip a weapon with " + std::string(weaponSkillName) + ".";
    case Reason::Bow:
        return "Equip a bow or crossbow.";
    case Reason::Dead:
        return "Cannot use while dead.";
    case Reason::NotLearned:
        return "This skill is no longer learned.";
    case Reason::Pet:
        return "Equip a Dark Spirit to use this command.";
    case Reason::SafeZone:
        return "Leave the safe zone to cast this skill.";
    case Reason::Stun:
        return "Cannot cast while stunned.";
    case Reason::Sleep:
        return "Cannot cast while asleep.";
    case Reason::Invalid:
        return "Skill data is unavailable.";
    case Reason::Requirements:
        return "Skill requirements are not met.";
    }
    return {};
}
} // namespace UI::Skills
