#include "stdafx.h"
#include "UI/AuctionHouse/ItemPresentation.h"
#include "UI/Core/UIManager.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/CSItemOption.h"
#include "GameLogic/Items/CComGem.h"
#include "Network/Server/SocketSystem.h"
#include "Core/Utilities/StringUtils.h"
#include "Character/CharacterManager.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/AuctionHouse/AuctionHouse.h"

namespace UI::AuctionHouse
{
namespace
{
void Line(RowModel& row, const std::string& text)
{
    if (text.empty())
        return;
    if (!row.detail.empty())
        row.detail += "\n";
    row.detail += text;
}
void Stats(RowModel& row, const ITEM* item)
{
    if (item->DamageMax)
        Line(row, "Damage " + std::to_string(item->DamageMin) + "–" + std::to_string(item->DamageMax));
    if (item->Defense)
        Line(row, "Defense " + std::to_string(item->Defense));
    if (item->SuccessfulBlocking)
        Line(row, "Defense success rate " + std::to_string(item->SuccessfulBlocking));
    if (item->MagicPower)
        Line(row, "Magic power " + std::to_string(item->MagicPower) + "%");
    if (item->WeaponSpeed)
        Line(row, "Attack speed " + std::to_string(item->WeaponSpeed));
}

bool IsAdditional(WORD option)
{
    return option == AT_IMPROVE_DAMAGE || option == AT_IMPROVE_MAGIC || option == AT_IMPROVE_CURSE ||
           option == AT_IMPROVE_BLOCKING || option == AT_IMPROVE_DEFENSE || option == AT_LIFE_REGENERATION;
}

bool IsExcellent(WORD option)
{
    return option == AT_IMPROVE_LIFE || option == AT_IMPROVE_MANA || option == AT_DECREASE_DAMAGE ||
           option == AT_REFLECTION_DAMAGE || option == AT_IMPROVE_BLOCKING_PERCENT || option == AT_IMPROVE_GAIN_GOLD ||
           option == AT_EXCELLENT_DAMAGE || option == AT_IMPROVE_DAMAGE_LEVEL || option == AT_IMPROVE_DAMAGE_PERCENT ||
           option == AT_IMPROVE_MAGIC_LEVEL || option == AT_IMPROVE_MAGIC_PERCENT ||
           option == AT_IMPROVE_ATTACK_SPEED || option == AT_IMPROVE_GAIN_LIFE || option == AT_IMPROVE_GAIN_MANA;
}

void Append(Rml::String& target, const std::string& text, const char* separator = "\n")
{
    if (text.empty())
        return;
    if (!target.empty())
        target += separator;
    target += text;
}

void Special(RowModel& row, WORD option, const std::string& text)
{
    Line(row, text);
    if (IsAdditional(option))
    {
        Append(row.stats, text);
        Append(row.summary, text, " · ");
    }
    else
        Append(row.options, text);
    if (IsExcellent(option))
        Append(row.matched, text, " · ");
}

void Options(RowModel& row, const ITEM* item)
{
    wchar_t text[256]{};
    if (item->HasSkill)
        row.summary = "Skill";
    if (item->HasLuck)
        Append(row.summary, "Luck", " · ");
    if (item->AncientDiscriminator && g_csItemOption.GetSetItemName(text, item->Type, item->AncientDiscriminator))
        row.name = StringUtils::WideToNarrow(text) + row.name;
    for (int i = 0; i < std::min<int>(item->SpecialNum, MAX_ITEM_SPECIAL); ++i)
    {
        if (item->Special[i] == AT_LUCK)
            continue;
        text[0] = 0;
        GetSpecialOptionText(item->Type, text, item->Special[i], item->SpecialValue[i], 0);
        if (text[0])
            Special(row, item->Special[i], StringUtils::WideToNarrow(text));
    }
    if (item->HasLuck)
    {
        Line(row, "Luck");
        Append(row.stats, "Luck");
    }
    text[0] = 0;
    if (g_csItemOption.GetDefaultOptionText(item, text))
    {
        Line(row, StringUtils::WideToNarrow(text));
        Append(row.options, StringUtils::WideToNarrow(text));
    }
}

void Sockets(RowModel& row, const ITEM* item)
{
    wchar_t text[256]{};
    for (int i = 0; i < std::min<int>(item->SocketCount, MAX_SOCKETS); ++i)
    {
        if (item->SocketSeedID[i] == SOCKET_EMPTY)
            Line(row, "Empty socket");
        else if (item->SocketSeedID[i] < SEASON4A::MAX_SOCKET_OPTION && item->SocketSphereLv[i] > 0 &&
                 item->SocketSphereLv[i] <= 5)
        {
            text[0] = 0;
            g_SocketItemMgr.CreateSocketOptionText(text, item->SocketSeedID[i], item->SocketSphereLv[i]);
            Line(row, StringUtils::WideToNarrow(text));
        }
    }
}

void Harmony(RowModel& row, const ITEM* item)
{
    if (g_pUIJewelHarmonyinfo && item->Jewel_Of_Harmony_Option > 0 &&
        item->Jewel_Of_Harmony_Option <= MAXHARMONYJEWELOPTIONINDEX && item->Jewel_Of_Harmony_OptionLevel < 14)
    {
        auto* harmony = g_pUIJewelHarmonyinfo;
        const auto type = harmony->GetItemType(item->Type);
        if (harmony->IsHarmonyJewelOption(type, item->Jewel_Of_Harmony_Option))
        {
            const auto& option = harmony->GetHarmonyJewelOptionInfo(type, item->Jewel_Of_Harmony_Option);
            Line(row, StringUtils::WideToNarrow(option.Name) + " +" +
                          std::to_string(option.HarmonyJewelLevel[item->Jewel_Of_Harmony_OptionLevel]));
        }
    }
}

// Strength and agility the item's harmony and socket options take off its requirements.
std::pair<int, int> RequirementReductions(const ITEM& item)
{
    constexpr BYTE StrengthReductionSeed = 38, AgilityReductionSeed = 39;
    int strength = 0, agility = 0;
    if (g_pUIJewelHarmonyinfo && item.Level >= item.Jewel_Of_Harmony_OptionLevel)
    {
        StrengthenCapability capability{};
        g_pUIJewelHarmonyinfo->GetStrengthenCapability(&capability, &item, 0);
        if (capability.SI_isNB)
        {
            strength = capability.SI_NB.SI_force;
            agility = capability.SI_NB.SI_activity;
        }
    }
    for (int i = 0; i < std::min<int>(item.SocketCount, MAX_SOCKETS); ++i)
        if (item.SocketSeedID[i] == StrengthReductionSeed)
            strength += g_SocketItemMgr.GetSocketOptionValue(&item, i);
        else if (item.SocketSeedID[i] == AgilityReductionSeed)
            agility += g_SocketItemMgr.GetSocketOptionValue(&item, i);
    return {strength, agility};
}
// The requirements the details show: the same effective numbers Cannot Equip uses, with the base value
// when harmony or socket options reduce it (UX3-04).
void Requirements(RowModel& row, const ITEM* item)
{
    const auto [strengthDown, agilityDown] = RequirementReductions(*item);
    if (item->RequireStrength)
        Line(row, RequirementText("Strength", item->RequireStrength, strengthDown));
    if (item->RequireDexterity)
        Line(row, RequirementText("Agility", item->RequireDexterity, agilityDown));
    if (item->RequireEnergy)
        Line(row, "Energy " + std::to_string(item->RequireEnergy));
    if (item->RequireVitality)
        Line(row, "Vitality " + std::to_string(item->RequireVitality));
    if (item->RequireCharisma)
        Line(row, "Leadership " + std::to_string(item->RequireCharisma));
    if (item->RequireLevel)
        Line(row, "Level " + std::to_string(item->RequireLevel));
    const auto maxDurability = CalcMaxDurability(item, &ItemAttribute[item->Type], item->Level);
    if (item->Type / MAX_ITEM_INDEX < 12 && item->Durability < maxDurability)
        Line(row, "Durability " + std::to_string(item->Durability) + "/" + std::to_string(maxDurability));
}

} // namespace

RowModel DescribeItem(const ITEM* item, int index)
{
    RowModel row;
    row.index = index;
    if (!item || item->Type < 0 || item->Type >= MAX_ITEM || !ItemAttribute)
    {
        row.name = "Unknown item";
        return row;
    }
    wchar_t name[256]{};
    GetItemName(item->Type, item->Level, name);
    row.name = StringUtils::WideToNarrow(name);
    if (COMGEM::isCompiledGem(item))
        row.name = StringUtils::WideToNarrow(ItemAttribute[item->Type].Name) + " ×" +
                   std::to_string(static_cast<int>(COMGEM::CalcCompiledCount(item)));
    row.tone = item->AncientDiscriminator ? "ancient"
               : item->ExcellentFlags     ? "excellent"
               : item->SocketCount        ? "socket"
                                          : "normal";
    Stats(row, item);
    row.stats = row.detail;
    Options(row, item);
    const auto optionsStart = row.detail.size();
    Sockets(row, item);
    Harmony(row, item);
    if (item->option_380 && g_pItemAddOptioninfo)
    {
        const auto& guardian = g_pItemAddOptioninfo->GetItemAddOtioninfo(item->Type);
        if (guardian.m_byOption1 >= 1 && guardian.m_byOption1 <= 8 && guardian.m_byOption2 >= 1 &&
            guardian.m_byOption2 <= 8)
        {
            std::vector<std::wstring> text;
            g_pItemAddOptioninfo->GetItemAddOtioninfoText(text, item->Type);
            for (const auto& line : text)
                Line(row, StringUtils::WideToNarrow(line.c_str()));
        }
        else
            Line(row, "Guardian option");
    }
    auto extraOptions = row.detail.substr(optionsStart);
    if (!extraOptions.empty() && extraOptions.front() == '\n')
        extraOptions.erase(0, 1);
    Append(row.options, extraOptions);
    const auto requirementsStart = row.detail.size();
    Requirements(row, item);
    row.requirements = row.detail.substr(requirementsStart);
    if (!row.requirements.empty() && row.requirements.front() == '\n')
        row.requirements.erase(0, 1);
    if (row.tone == "excellent")
        row.name = "Excellent " + row.name;
    row.excellent = ExcellentMarks(GameLogic::AuctionHouse::ItemSlotMask(item->Type), item->ExcellentFlags);
    return row;
}

std::optional<EquipFacts> CharacterEquipFacts(const ITEM& item)
{
    if (!Hero || !CharacterAttribute || !ItemAttribute || item.Type < 0 || item.Type >= MAX_ITEM)
        return std::nullopt;
    EquipFacts facts;
    const auto& attribute = ItemAttribute[item.Type];
    std::copy_n(std::begin(attribute.RequireClass), facts.requireClass.size(), facts.requireClass.begin());
    facts.baseClass = static_cast<std::uint8_t>(gCharacterManager.GetBaseClass(Hero->Class));
    facts.classStep = gCharacterManager.GetStepClass(Hero->Class);
    if (Check_LuckyItem(item.Type))
        return facts; // Lucky items have no stat requirements.
    const auto [strengthDown, agilityDown] = RequirementReductions(item);
    const auto& character = *CharacterAttribute;
    facts.stats = {
        {"Strength", EffectiveRequirement(item.RequireStrength, strengthDown),
         character.Strength + character.AddStrength},
        {"Agility", EffectiveRequirement(item.RequireDexterity, agilityDown),
         character.Dexterity + character.AddDexterity},
        {"Energy", item.RequireEnergy, character.Energy + character.AddEnergy},
        {"Vitality", item.RequireVitality, character.Vitality + character.AddVitality},
        {"Leadership", item.RequireCharisma, character.Charisma + character.AddCharisma},
        {"Level", item.RequireLevel, character.Level},
    };
    return facts;
}

void ExplainCannotEquip(RowModel& row, const ITEM& item)
{
    const auto facts = CharacterEquipFacts(item);
    const auto reasons = facts ? CannotEquipReasons(*facts) : std::vector<std::string>{};
    row.equipReason.clear();
    row.equipReasons.clear();
    if (reasons.empty())
        return;
    row.equipReason = reasons.front();
    if (reasons.size() > 1)
        row.equipReason += " (+" + std::to_string(reasons.size() - 1) + " more)";
    for (const auto& reason : reasons)
        Append(row.equipReasons, reason);
}

bool SameInventoryItem(const ITEM* item, const ITEM* saved, std::uint32_t sourceKey)
{
    if (!item || !saved)
        return false;
    // Preview duplicates receive their own key; retain the inventory identity separately.
    return item->Key == sourceKey && item->Type == saved->Type && item->Level == saved->Level &&
           item->Durability == saved->Durability && item->OptionLevel == saved->OptionLevel &&
           item->OptionType == saved->OptionType && item->ExcellentFlags == saved->ExcellentFlags &&
           item->AncientDiscriminator == saved->AncientDiscriminator &&
           item->AncientBonusOption == saved->AncientBonusOption && item->HasSkill == saved->HasSkill &&
           item->HasLuck == saved->HasLuck && item->option_380 == saved->option_380 &&
           item->Jewel_Of_Harmony_Option == saved->Jewel_Of_Harmony_Option &&
           item->Jewel_Of_Harmony_OptionLevel == saved->Jewel_Of_Harmony_OptionLevel &&
           item->SocketCount == saved->SocketCount &&
           std::equal(std::begin(item->bySocketOption), std::end(item->bySocketOption),
                      std::begin(saved->bySocketOption));
}

GameLogic::AuctionHouse::PricingIdentity EquipmentPricingIdentity(const ITEM& item)
{
    GameLogic::AuctionHouse::PricingIdentity identity;
    identity.type = static_cast<std::uint16_t>(item.Type);
    identity.level = static_cast<std::uint8_t>(item.Level);
    identity.excellentFlags = item.ExcellentFlags;
    identity.luck = item.HasLuck;
    identity.skill = item.HasSkill;
    identity.optionType = item.OptionType;
    identity.optionLevel = item.OptionLevel;
    identity.ancientDiscriminator = item.AncientDiscriminator;
    identity.ancientBonus = item.AncientBonusOption;
    identity.socketCount = item.SocketCount;
    static_assert(std::tuple_size_v<decltype(identity.sockets)> == MAX_SOCKETS);
    std::copy(std::begin(item.bySocketOption), std::end(item.bySocketOption), identity.sockets.begin());
    identity.harmonyOption = item.Jewel_Of_Harmony_Option;
    identity.harmonyLevel = item.Jewel_Of_Harmony_OptionLevel;
    identity.option380 = item.option_380;
    return identity;
}
} // namespace UI::AuctionHouse
