#include "stdafx.h"
#include "GameLogic/Items/ItemCategories.h"
#include "SkillAvailability.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/CSItemOption.h"
#include "GameLogic/Skills/SkillManager.h"
#include "GameLogic/Social/MonkSystem.h"
#include "GameLogic/Pets/GIPetManager.h"
#include "World/MapInfra/MapManager.h"
#include "Character/CharacterManager.h"

namespace UI::Skills
{
namespace
{
using Reason = SkillAvailability::Reason;
bool HasOneHandedOrMeleeWeapon()
{
    const int iTypeL = CharacterMachine->Equipment[EQUIPMENT_WEAPON_LEFT].Type;
    const int iTypeR = CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type;
    return iTypeR != -1 && (iTypeR < ITEM_STAFF || iTypeR >= ITEM_STAFF + MAX_ITEM_INDEX) &&
           (iTypeL < ITEM_STAFF || iTypeL >= ITEM_STAFF + MAX_ITEM_INDEX);
}

SkillAvailability AttributeFailure(ActionSkillType type)
{
    const auto base = gSkillManager.MasterSkillToBaseSkillIndex(type);
    if (gMapManager.IsEmpireGuardian() && (base == AT_SKILL_TELEPORT || base == AT_SKILL_TELEPORT_ALLY))
        return {Reason::EventMap};
    const auto& hero = CharacterMachine->Character;
    const auto& skill = SkillAttribute[base];
    int energy = 0;
    gSkillManager.GetSkillInformation_Energy(base, &energy);
    const std::array<SkillAvailability, 5> requirements{
        {{Reason::Level, skill.Level, hero.Level},
         {Reason::Strength, skill.Strength, hero.Strength + hero.AddStrength},
         {Reason::Dexterity, skill.Dexterity, hero.Dexterity + hero.AddDexterity},
         {Reason::Energy, static_cast<WORD>(energy), hero.Energy + hero.AddEnergy},
         {Reason::Charisma, skill.Charisma, hero.Charisma + hero.AddCharisma}}};
    for (const auto& requirement : requirements)
        if (requirement.current < requirement.required)
            return requirement;
    return {Reason::Requirements};
}
constexpr int DarkSpiritInfoIndex = 421 - PET_TYPE_DARK_SPIRIT;
constexpr int DarkSpiritBaseCommand = 185;
constexpr int DarkSpiritCommandPerLevel = 15;
SkillAvailability MountedRequirements(ActionSkillType bySkillType)
{
    auto isSittingOnPet = GameLogic::Items::IsHornMountModel(Hero->Helper.Type);
    if (bySkillType == AT_SKILL_IMPALE && !isSittingOnPet)
    {
        return {Reason::Mount};
    }

    if (bySkillType == AT_SKILL_IMPALE && isSittingOnPet)
    {
        int iTypeL = CharacterMachine->Equipment[EQUIPMENT_WEAPON_LEFT].Type;
        int iTypeR = CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type;
        if ((iTypeL < ITEM_SPEAR || iTypeL >= ITEM_BOW) && (iTypeR < ITEM_SPEAR || iTypeR >= ITEM_BOW))
        {
            return {Reason::Spear};
        }
    }

    if (isSittingOnPet && ((bySkillType >= AT_SKILL_BLOCKING && bySkillType <= AT_SKILL_SLASH) ||
                           bySkillType == AT_SKILL_FALLING_SLASH_STR || bySkillType == AT_SKILL_LUNGE_STR ||
                           bySkillType == AT_SKILL_CYCLONE_STR || bySkillType == AT_SKILL_CYCLONE_STR_MG ||
                           bySkillType == AT_SKILL_SLASH_STR))
    {
        return {Reason::Dismount};
    }

    if ((bySkillType == AT_SKILL_POWER_SLASH || bySkillType == AT_SKILL_POWER_SLASH_STR) && isSittingOnPet)
    {
        return {Reason::Dismount};
    }

    return {};
}
SkillAvailability PartyPetRequirements(ActionSkillType bySkillType)
{
    if (bySkillType == AT_SKILL_PARTY_TELEPORT && PartyNumber <= 0)
    {
        return {Reason::Party};
    }

    if (bySkillType == AT_SKILL_PARTY_TELEPORT &&
        (IsDoppelGanger1() || IsDoppelGanger2() || IsDoppelGanger3() || IsDoppelGanger4()))
    {
        return {Reason::EventMap};
    }

    if (bySkillType == AT_SKILL_EARTHSHAKE || bySkillType == AT_SKILL_EARTHSHAKE_STR ||
        bySkillType == AT_SKILL_EARTHSHAKE_MASTERY)
    {
        BYTE byDarkHorseLife = 0;
        byDarkHorseLife = CharacterMachine->Equipment[EQUIPMENT_HELPER].Durability;
        if (byDarkHorseLife == 0 || Hero->Helper.Type != MODEL_DARK_HORSE_ITEM)
        {
            return {Reason::DarkHorse};
        }
    }
#ifdef PJH_FIX_SPRIT
    /*박종훈*/
    if (bySkillType >= AT_PET_COMMAND_DEFAULT && bySkillType < AT_PET_COMMAND_END)
    {
        int iCharisma = CharacterAttribute->Charisma + CharacterAttribute->AddCharisma;
        PET_INFO PetInfo;
        giPetManager::GetPetInfo(PetInfo, DarkSpiritInfoIndex);
        int RequireCharisma = (DarkSpiritBaseCommand + (PetInfo.m_wLevel * DarkSpiritCommandPerLevel));
        if (RequireCharisma > iCharisma)
        {
            return {Reason::Charisma, RequireCharisma, iCharisma};
        }
    }
#endif // PJH_FIX_SPRIT
    return {};
}
SkillAvailability ExistingBuffRequirements(ActionSkillType bySkillType)
{
    if ((bySkillType == AT_SKILL_INFINITY_ARROW) || (bySkillType == AT_SKILL_INFINITY_ARROW_STR) ||
        (bySkillType == AT_SKILL_EXPANSION_OF_WIZARDRY) || (bySkillType == AT_SKILL_EXPANSION_OF_WIZARDRY_STR) ||
        (bySkillType == AT_SKILL_EXPANSION_OF_WIZARDRY_MASTERY))
    {
        if ((g_isCharacterBuff((&Hero->Object), eBuff_InfinityArrow)) ||
            (g_isCharacterBuff((&Hero->Object), eBuff_SwellOfMagicPower)))
        {
            return {Reason::BuffActive};
        }
    }

    return {};
}
SkillAvailability SpecialStatRequirements(ActionSkillType bySkillType)
{
    if (bySkillType == AT_SKILL_FIRE_SLASH || bySkillType == AT_SKILL_FIRE_SLASH_STR)
    {
        WORD Strength;
        const WORD wRequireStrength = 596;
        Strength = CharacterAttribute->Strength + CharacterAttribute->AddStrength;
        if (Strength < wRequireStrength)
        {
            return {Reason::Strength, wRequireStrength, Strength};
        }
        int iTypeL = CharacterMachine->Equipment[EQUIPMENT_WEAPON_LEFT].Type;
        int iTypeR = CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type;

        if (!(iTypeR != -1 && (iTypeR < ITEM_STAFF || iTypeR >= ITEM_STAFF + MAX_ITEM_INDEX) &&
              (iTypeL < ITEM_STAFF || iTypeL >= ITEM_STAFF + MAX_ITEM_INDEX)))
        {
            return {Reason::Weapon};
        }
    }

    switch (bySkillType)
    {
        // case AT_SKILL_PIERCING:
    case AT_SKILL_ICE_ARROW:
    case AT_SKILL_ICE_ARROW_STR:
    {
        WORD Dexterity;
        const WORD wRequireDexterity = 646;
        Dexterity = CharacterAttribute->Dexterity + CharacterAttribute->AddDexterity;
        if (Dexterity < wRequireDexterity)
        {
            return {Reason::Dexterity, wRequireDexterity, Dexterity};
        }
    }
    break;
    }

    return {};
}
SkillAvailability MeleeRequirements(ActionSkillType bySkillType)
{
    if (bySkillType == AT_SKILL_TWISTING_SLASH || bySkillType == AT_SKILL_TWISTING_SLASH_STR ||
        bySkillType == AT_SKILL_TWISTING_SLASH_STR_MG || bySkillType == AT_SKILL_TWISTING_SLASH_MASTERY ||
        bySkillType == AT_SKILL_RAGEFUL_BLOW || bySkillType == AT_SKILL_RAGEFUL_BLOW_STR ||
        bySkillType == AT_SKILL_RAGEFUL_BLOW_MASTERY || bySkillType == AT_SKILL_DEATHSTAB ||
        bySkillType == AT_SKILL_DEATHSTAB_STR)
    {
        int iTypeL = CharacterMachine->Equipment[EQUIPMENT_WEAPON_LEFT].Type;
        int iTypeR = CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type;

        if (!(iTypeR != -1 && (iTypeR < ITEM_STAFF || iTypeR >= ITEM_STAFF + MAX_ITEM_INDEX) &&
              (iTypeL < ITEM_STAFF || iTypeL >= ITEM_STAFF + MAX_ITEM_INDEX)))
        {
            return {Reason::Weapon};
        }
    }

    return {};
}
SkillAvailability EventRequirements(ActionSkillType bySkillType)
{
    if (gMapManager.InChaosCastle() == true)
    {
        if (bySkillType == AT_SKILL_EARTHSHAKE || bySkillType == AT_SKILL_EARTHSHAKE_STR ||
            bySkillType == AT_SKILL_EARTHSHAKE_MASTERY || bySkillType == AT_SKILL_RIDER ||
            (static_cast<int>(bySkillType) >= static_cast<int>(AT_PET_COMMAND_DEFAULT) &&
             static_cast<int>(bySkillType) <= static_cast<int>(AT_PET_COMMAND_TARGET)))
        {
            return {Reason::EventMap};
        }
    }
    else
    {
        if (bySkillType == AT_SKILL_EARTHSHAKE || bySkillType == AT_SKILL_EARTHSHAKE_STR ||
            bySkillType == AT_SKILL_EARTHSHAKE_MASTERY)
        {
            BYTE byDarkHorseLife = 0;
            byDarkHorseLife = CharacterMachine->Equipment[EQUIPMENT_HELPER].Durability;
            if (byDarkHorseLife == 0)
            {
                return {Reason::DarkHorse};
            }
        }
    }

    return {};
}
SkillAvailability TransformationRequirements(ActionSkillType bySkillType)
{
    if (!g_CMonkSystem.IsSwordformGlovesUseSkill(bySkillType))
    {
        return {Reason::Gloves};
    }
    if (g_CMonkSystem.IsRideNotUseSkill(bySkillType, Hero->Helper.Type))
    {
        return {Reason::Dismount};
    }

    ITEM* pLeftRing = &CharacterMachine->Equipment[EQUIPMENT_RING_LEFT];
    ITEM* pRightRing = &CharacterMachine->Equipment[EQUIPMENT_RING_RIGHT];

    if (g_CMonkSystem.IsChangeringNotUseSkill(pLeftRing->Type, pRightRing->Type, pLeftRing->Level, pRightRing->Level) &&
        (gCharacterManager.GetBaseClass(Hero->Class) == CLASS_RAGEFIGHTER))
    {
        return {Reason::Transformation};
    }

    return {};
}
SkillAvailability EquippedSkillRequirements(ActionSkillType bySkillType)
{
    if (!g_csItemOption.IsNonWeaponSkillOrIsSkillEquipped(bySkillType))
    {
        return {Reason::WeaponSkill};
    }

    if (bySkillType == AT_SKILL_MULTI_SHOT && gCharacterManager.GetEquipedBowType_Skill() == BOWTYPE_NONE)
    {
        return {Reason::Bow};
    }

    if (bySkillType == AT_SKILL_FLAME_STRIKE && !HasOneHandedOrMeleeWeapon())
    {
        return {Reason::Weapon};
    }

    return {};
}
} // namespace
SkillAvailability ReadSkillRequirements(int skillType)
{
    if (!Hero || !CharacterAttribute || !CharacterMachine || skillType <= 0 || skillType >= MAX_SKILLS)
        return {Reason::Invalid};
    const auto bySkillType = static_cast<ActionSkillType>(skillType);

    const BYTE bySkillUseType = SkillAttribute[bySkillType].SkillUseType;

    if (!gSkillManager.AreSkillAttributeRequirementsMet(bySkillType))
    {
        return AttributeFailure(bySkillType);
    }

    if (IsCanBCSkill(bySkillType) == false)
    {
        return {Reason::CastleSiege};
    }
    if (g_isCharacterBuff((&Hero->Object), eBuff_AddSkill) && bySkillUseType == SKILL_USE_TYPE_BRAND)
    {
        return {Reason::SkillBrand};
    }
    using Check = SkillAvailability (*)(ActionSkillType);
    constexpr std::array<Check, 8> checks{
        MountedRequirements, PartyPetRequirements, ExistingBuffRequirements,   SpecialStatRequirements,
        MeleeRequirements,   EventRequirements,    TransformationRequirements, EquippedSkillRequirements};
    for (const auto check : checks)
    {
        const auto failure = check(bySkillType);
        if (!failure.Usable())
            return failure;
    }
    return {};
}
} // namespace UI::Skills
