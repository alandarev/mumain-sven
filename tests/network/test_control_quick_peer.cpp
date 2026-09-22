#include "stdafx.h"
#include "doctest.h"
#include "App/Control/ControlQuickPeer.h"
#include "App/Control/ControlUiReplay.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInventory.h"
#include "UI/Core/WindowCommon.h"
#include "UI/HUD/QuickCommandWindow.h"
#include "World/MapInfra/MapManager.h"
#include <limits>
#include "json.hpp"

#include <array>
#include <memory>

extern CHARACTER* CharacterMemoryDump;

TEST_CASE("Quick peer resolver binds current key name and bounded index [network][control-ui]")
{
    const int originalSelection = SelectedCharacter;
    auto storage = std::make_unique<CHARACTER[]>(4);
    const std::span<CHARACTER> view(storage.get(), 4);
    auto& peer = storage[1];
    peer.Object.Live = true;
    peer.Key = 123;
    wcscpy(peer.ID, L"Peer");
    const auto resolve = [&](int key, std::string_view id)
    { return App::Control::ResolveQuickPeer(view, storage.get(), key, id); };
    CHECK(resolve(123, "Peer") == 1);
    CHECK(resolve(124, "Peer") == -1);
    CHECK(resolve(123, "Other") == -1);
    CHECK(resolve(-1, "Peer") == -1);
    CHECK(resolve(32768, "Peer") == -1);
    CHECK(resolve(123, "") == -1);
    CHECK(App::Control::ResolveQuickPeer({}, storage.get(), 123, "Peer") == -1);
    CHARACTER outside;
    CHECK(App::Control::ResolveQuickPeer(view, &outside, 123, "Peer") == -1);
    CHECK(App::Control::ResolveQuickPeer(view, &peer, 123, "Peer") == -1);
    peer.Object.Live = false;
    CHECK(resolve(123, "Peer") == -1);
    peer.Object.Live = true;
    wcscpy(peer.ID, L"Reused");
    CHECK(resolve(123, "Peer") == -1);
    wcscpy(peer.ID, L"Peer");
    CHECK(resolve(123, "Peer") == 1); // Semantic identity, not incarnation continuity.
    storage[2].Object.Live = true;
    storage[2].Key = 123;
    wcscpy(storage[2].ID, L"Other");
    CHECK(resolve(123, "Peer") == -1);
    storage[2].Object.Live = false;
    std::fill(std::begin(peer.ID), std::end(peer.ID), L'x');
    CHECK(resolve(123, "Peer") == -1);
    peer.Object.Live = false;
    storage[2].Object.Live = true;
    wcscpy(storage[2].ID, L"Peer");
    CHECK(resolve(123, "Peer") == 2);
    CHECK(SelectedCharacter == originalSelection);
}

TEST_CASE("Quick peer world storage rejects singleton aliases and missing owner [network][control-ui]")
{
    auto storage = std::make_unique<CHARACTER[]>(MAX_CHARACTERS_CLIENT + 1 + 128);
    CHARACTER photo;
    auto* savedOwner = CharacterMemoryDump;
    auto* savedStorage = CharactersClient;
    CharacterMemoryDump = storage.get();
    CharactersClient = storage.get() + 127;
    CHECK(App::Control::WorldCharacterStorage().size() == MAX_CHARACTERS_CLIENT);
    CHECK(App::Control::WorldCharacterStorage().data() == CharactersClient);
    CharactersClient = &photo;
    CHECK(App::Control::WorldCharacterStorage().empty());
    CharactersClient = storage.get() + 128;
    CHECK(App::Control::WorldCharacterStorage().empty());
    CharacterMemoryDump = nullptr;
    CHECK(App::Control::WorldCharacterStorage().empty());
    CharacterMemoryDump = savedOwner;
    CharactersClient = savedStorage;
}

TEST_CASE("Quick peer natural prerequisites reject unsafe current candidates [network][control-ui]")
{
    CHARACTER hero;
    CHARACTER peer;
    hero.Object.Live = peer.Object.Live = true;
    hero.Object.Kind = peer.Object.Kind = KIND_PLAYER;
    hero.Object.Type = peer.Object.Type = MODEL_PLAYER;
    hero.Object.SubType = peer.Object.SubType = 0;
    Vector(0.f, 0.f, 0.f, hero.Object.Position);
    Vector(0.f, 0.f, 0.f, peer.Object.Position);
    const int savedWorld = gMapManager.WorldActive;
    gMapManager.WorldActive = WD_0LORENCIA;
    CHECK(App::Control::QuickPeerAllowed(hero, peer));
    peer.Object.Position[0] = 299.f;
    CHECK(App::Control::QuickPeerAllowed(hero, peer));
    peer.Object.Position[0] = 300.f;
    CHECK_FALSE(App::Control::QuickPeerAllowed(hero, peer));
    peer.Object.Position[0] = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(App::Control::QuickPeerAllowed(hero, peer));
    peer.Object.Position[0] = 0.f;
    peer.Object.Kind = KIND_NPC;
    CHECK_FALSE(App::Control::QuickPeerAllowed(hero, peer));
    peer.Object.Kind = KIND_PLAYER;
    peer.Object.Type = MODEL_SKELETON1;
    CHECK_FALSE(App::Control::QuickPeerAllowed(hero, peer));
    peer.Object.Type = MODEL_PLAYER;
    for (int subtype : {MODEL_XMAS_EVENT_CHA_DEER, MODEL_XMAS_EVENT_CHA_SNOWMAN, MODEL_XMAS_EVENT_CHA_SSANTA})
    {
        peer.Object.SubType = subtype;
        CHECK_FALSE(App::Control::QuickPeerAllowed(hero, peer));
    }
    peer.Object.SubType = 0;
    for (int world : {WD_18CHAOS_CASTLE, WD_45CURSEDTEMPLE_LV1})
    {
        gMapManager.WorldActive = world;
        CHECK_FALSE(App::Control::QuickPeerAllowed(hero, peer));
    }
    gMapManager.WorldActive = WD_63PK_FIELD;
    hero.m_byGensInfluence = 1;
    peer.m_byGensInfluence = 2;
    // The server's non-PvP mode can suppress Strife; compare to the actual predicate.
    CHECK(App::Control::QuickPeerAllowed(hero, peer) == !IsStrifeMap(gMapManager.WorldActive));
    peer.m_byGensInfluence = 1;
    CHECK(App::Control::QuickPeerAllowed(hero, peer));
    gMapManager.WorldActive = WD_0LORENCIA;
    hero.Object.m_BuffMap.RegisterBuff(eBuff_DuelWatch);
    CHECK_FALSE(App::Control::QuickPeerAllowed(hero, peer));
    hero.Object.m_BuffMap.UnRegisterBuff(eBuff_DuelWatch);
    peer.Object.Live = false;
    CHECK_FALSE(App::Control::QuickPeerAllowed(hero, peer));
    gMapManager.WorldActive = savedWorld;
}

TEST_CASE("Quick menu read-only identity follows real opening and closing resets [network][control-ui]")
{
    // Actual object lifecycle/accessors, not OpenQuickCommand integration: no texture or system init.
    mu::ui::window::CQuickCommandWindow menu;
    CHECK(menu.SelectedCharacterIndex() == -1);
    menu.SetID(L"Peer");
    menu.SetSelectedCharacterIndex(2);
    menu.SetPos(20, 40);
    CHECK(std::wstring(menu.TargetName()) == L"Peer");
    CHECK(menu.Position().x == 20);
    CHECK(menu.SelectedCharacterIndex() == 2);
    menu.ClosingProcess();
    CHECK(menu.SelectedCharacterIndex() == -1);
    CHECK(menu.SelectedCommandIndex() == -1);
    menu.SetSelectedCharacterIndex(2);
    menu.OpenningProcess();
    CHECK(menu.SelectedCharacterIndex() == -1);
}

TEST_CASE("Quick peer unknown producer refuses without action [network][control-ui]")
{
    REQUIRE(App::Control::WorldCharacterStorage().empty());
    const auto first = App::Control::QuickPeerObservation(123, "Peer");
    CHECK(nlohmann::json::parse(first)["ready"] == false);
    CHECK_FALSE(App::Control::QuickPeerRefusal(first, true).empty());
    CHECK_FALSE(App::Control::QuickPeerRefusal("null", false).empty());
    CHECK_FALSE(App::Control::ApplyQuickPeer(123, "Peer", true, first).empty());
    CHECK(App::Control::QuickPeerObservation(123, "Peer") == first);
}

TEST_CASE("Read-only quick input prerequisite includes unfocused release edges [network][control-ui]")
{
    auto* input = g_pNewKeyInput;
    REQUIRE_FALSE(input->HasPendingInput());
    for (auto state : {mu::ui::window::CNewKeyInput::KEY_PRESS, mu::ui::window::CNewKeyInput::KEY_REPEAT,
                       mu::ui::window::CNewKeyInput::KEY_RELEASE})
    {
        input->SetKeyState(VK_LBUTTON, state);
        CHECK(input->HasPendingInput());
        CHECK(input->HasPendingInput());
        input->SetKeyState(VK_LBUTTON, mu::ui::window::CNewKeyInput::KEY_NONE);
        CHECK_FALSE(input->HasPendingInput());
    }
}
