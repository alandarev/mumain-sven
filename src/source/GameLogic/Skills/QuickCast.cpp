#include "stdafx.h"
#include "QuickCast.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Combat/SkillExecution.h"
#include "Engine/Object/ZzzInterface.h"
#include "GameLogic/Skills/SkillManager.h"

extern MovementSkill g_MovementSkill;
extern int TargetX, TargetY;

namespace GameLogic::Skills
{
bool IsSelfBuff(int skillType)
{
    return skillType == AT_SKILL_SWELL_LIFE || skillType == AT_SKILL_SWELL_LIFE_STR ||
           skillType == AT_SKILL_SWELL_LIFE_PROFICIENCY;
}

bool TryCastSelfBuff(int skillIndex)
{
    if (Hero == nullptr || Hero->Dead != 0 || skillIndex < 0 || skillIndex >= MAX_MAGIC)
        return false;
    const auto skill = CharacterAttribute->Skill[skillIndex];
    if (!IsSelfBuff(skill) || CharacterAttribute->SkillDelay[skillIndex] > 0 ||
        g_isCharacterBuff((&Hero->Object), eDeBuff_Stun) || g_isCharacterBuff((&Hero->Object), eDeBuff_Sleep))
        return false;
    int manaCost = 0, agCost = 0;
    gSkillManager.GetSkillInformation(skill, 1, nullptr, &manaCost, nullptr, &agCost);
    if (CharacterAttribute->Mana < manaCost || CharacterAttribute->SkillMana < agCost)
        return false;

    const auto priorSkill = Hero->CurrentSkill;
    const auto priorSelected = SelectedCharacter;
    const auto priorAttack = Attacking;
    const auto priorRightPress = MouseRButtonPress;
    const auto priorMovementSkill = g_MovementSkill;
    const auto priorTarget = Hero->TargetCharacter;
    const auto priorX = TargetX, priorY = TargetY;
    vec3_t priorPosition;
    VectorCopy(Hero->TargetPosition, priorPosition);
    SelectedCharacter = -1;
    TargetX = Hero->PositionX;
    TargetY = Hero->PositionY;
    VectorCopy(Hero->Object.Position, Hero->TargetPosition);
    const bool result = GameLogic::Combat::ExecuteSkill(Hero, skill, 0.f) > 0;
    Hero->CurrentSkill = priorSkill;
    SelectedCharacter = priorSelected;
    Attacking = priorAttack;
    MouseRButtonPress = priorRightPress;
    g_MovementSkill = priorMovementSkill;
    Hero->TargetCharacter = priorTarget;
    TargetX = priorX;
    TargetY = priorY;
    VectorCopy(priorPosition, Hero->TargetPosition);
    return result;
}
} // namespace GameLogic::Skills
