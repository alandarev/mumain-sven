#include "App/stdafx.h"

#include <doctest.h>

#include <algorithm>
#include <cstring>

#include "GameLogic/Pets/GIPetManager.h"

TEST_CASE("horse defense refresh replaces the first option without growing the item [pet]")
{
    CHARACTER_ATTRIBUTE attributes{};
    attributes.Dexterity = 200;
    auto* previous = CharacterAttribute;
    CharacterAttribute = &attributes;
    ITEM item{};
    item.Type = ITEM_DARK_HORSE_ITEM;
    item.SpecialNum = 1;
    item.Special[0] = AT_SET_OPTION_IMPROVE_DEFENCE;
    PET_INFO pet{};
    pet.m_wLevel = 3;

    for (int refresh = 0; refresh < 1000; ++refresh)
        giPetManager::SetPetItemConvert(&item, &pet);

    CHECK(item.SpecialNum == 1);
    CHECK(item.Special[0] == AT_SET_OPTION_IMPROVE_DEFENCE);
    CHECK(item.SpecialValue[0] == 21);
    CHECK(item.Special[1] == 0);
    CHECK(item.SpecialValue[1] == 0);
    CharacterAttribute = previous;
}

TEST_CASE("horse defense occupies one free option and preserves other options [pet]")
{
    CHARACTER_ATTRIBUTE attributes{};
    auto* previous = CharacterAttribute;
    CharacterAttribute = &attributes;
    ITEM item{};
    item.Type = ITEM_DARK_HORSE_ITEM;
    item.SpecialNum = 1;
    item.Special[0] = AT_SET_OPTION_IMPROVE_MAX_LIFE;
    item.SpecialValue[0] = 7;
    PET_INFO pet{};

    giPetManager::SetPetItemConvert(&item, &pet);
    giPetManager::SetPetItemConvert(&item, &pet);

    CHECK(item.SpecialNum == 2);
    CHECK(item.Special[0] == AT_SET_OPTION_IMPROVE_MAX_LIFE);
    CHECK(item.SpecialValue[0] == 7);
    CHECK(item.Special[1] == AT_SET_OPTION_IMPROVE_DEFENCE);
    CharacterAttribute = previous;
}

TEST_CASE("full horse options cannot overwrite adjacent item fields [pet]")
{
    ITEM item{};
    item.Type = ITEM_DARK_HORSE_ITEM;
    item.SpecialNum = MAX_ITEM_SPECIAL;
    item.Key = 0x12345678;
    std::fill_n(item.Special, MAX_ITEM_SPECIAL, AT_SET_OPTION_IMPROVE_MAX_LIFE);
    std::fill_n(item.SpecialValue, MAX_ITEM_SPECIAL, 7);
    const ITEM original = item;
    PET_INFO pet{};

    giPetManager::SetPetItemConvert(&item, &pet);

    CHECK(std::memcmp(&item, &original, sizeof(item)) == 0);
}
