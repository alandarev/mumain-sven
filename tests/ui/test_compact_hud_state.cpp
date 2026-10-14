#include "doctest.h"
#include "UI/HUD/ResourceMotion.h"
#include "UI/HUD/ExperienceProgress.h"
#include "UI/HUD/ActionBarProfile.h"
#include "UI/HUD/BuffPresentation.h"
#include "GameLogic/Buffs/EffectDuration.h"
#include "UI/HUD/Skills/SkillAvailability.h"
#include <limits>
#include <chrono>
#include <fstream>
using namespace UI::HUD;
TEST_CASE("skill feedback distinguishes two unavailable states and becomes ready at exact resource boundaries")
{
    using Availability = UI::Skills::SkillAvailability;
    using Reason = Availability::Reason;
    const Availability ready;
    const auto mana = ready.WithResources(21, 23, 22, 24, 0);
    CHECK(mana.reason == Reason::Mana);
    CHECK(mana.required == 22);
    CHECK(mana.current == 21);
    const auto ag = ready.WithResources(22, 23, 22, 24, 0);
    CHECK_FALSE(ag.Usable());
    CHECK(ag.reason == Reason::Ag);
    CHECK(mana != ag);
    CHECK(ready.WithResources(22, 24, 22, 24, 0).Usable());
    const auto cooling = ready.WithResources(22, 24, 22, 24, 1001);
    CHECK(cooling.reason == Reason::Cooldown);
    CHECK(cooling.Description("") == "Ready in 2s.");
    const Availability weapon{Reason::WeaponSkill};
    CHECK(weapon.WithResources(100, 100, 22, 24, 0) == weapon);
    CHECK(weapon.Description("Falling Slash") == "Equip a weapon with Falling Slash.");
}
TEST_CASE("server effect durations refresh independently and survive clock and integer boundaries")
{
    GameLogic::Buffs::EffectDuration life, defense;
    life.Refresh(62, 1000);
    defense.Refresh(20, 1000);
    CHECK(life.Remaining(2250) == 60750);
    CHECK(defense.Remaining(2250) == 18750);
    life.Refresh(62, 2250);
    CHECK(life.Remaining(2250) == 62000);
    CHECK(defense.Remaining(2250) == 18750);
    CHECK(life.Remaining(64250) == 0);
    CHECK(life.Remaining(65000) == 0);
    life.Refresh(0, 65000);
    CHECK(life.Remaining(65000) == 0);
    life.Refresh(std::numeric_limits<std::uint32_t>::max(), 65000);
    CHECK(life.Remaining(65000) == 0);
    life.Refresh(62, 5000000000ULL);
    CHECK(life.Remaining(5000001000ULL) == 61000);
    life.Refresh(5000000, 0);
    CHECK(life.Remaining(1) == std::numeric_limits<std::uint32_t>::max() - 1);
}
TEST_CASE("buff timers round partial seconds up and show complete minutes and hours")
{
    CHECK(CompactBuffDuration(0).empty());
    CHECK(CompactBuffDuration(std::numeric_limits<std::uint32_t>::max()).empty());
    CHECK(CompactBuffDuration(1) == "1s");
    CHECK(CompactBuffDuration(59001) == "1m");
    CHECK(CompactBuffDuration(59000) == "59s");
    CHECK(CompactBuffDuration(60000) == "1m");
    CHECK(CompactBuffDuration(61000) == "1m");
    CHECK(CompactBuffDuration(119000) == "1m");
    CHECK(CompactBuffDuration(120000) == "2m");
    CHECK(CompactBuffDuration(3600000) == "1h");
    CHECK(CompactBuffDuration(3601000) == "1h");
    CHECK_FALSE(BuffExpiresSoon(0));
    CHECK(BuffExpiresSoon(10000));
    CHECK_FALSE(BuffExpiresSoon(10001));
}
TEST_CASE("resource trails hold drain retarget and cancel on healing death or reset")
{
    ResourceMotion life(200.0, 900.0);
    life.Update(1.f, 0.0);
    auto hit = life.Update(.5f, 100.0);
    CHECK(hit.value == .5f);
    CHECK(hit.trail == 1.f);
    CHECK(life.Update(.5f, 299.0).trail == 1.f);
    CHECK(life.Update(.5f, 750.0).trail == doctest::Approx(.75f));
    CHECK(life.Update(.3f, 750.0).trail == doctest::Approx(.75f));
    auto heal = life.Update(.8f, 800.0);
    CHECK(heal.value == .8f);
    CHECK(heal.trail == .8f);
    CHECK(heal.sweepOpacity == 1.f);
    CHECK(life.Update(.8f, 1080.0).sweepOpacity == 0.f);
    CHECK(life.Update(0.f, 1100.0).trail == 0.f);
    CHECK(life.Update(.4f, 1200.0, true).trail == .4f);
    life.Reset();
    CHECK(life.Update(.2f, 1300.0).sweepOpacity == 0.f);
}
TEST_CASE("ability recovery grows from its visible value")
{
    ResourceMotion ag(100.0, 350.0, 450.0);
    ag.Update(.2f, 0.0);
    CHECK(ag.Update(.8f, 10.0).value == .2f);
    CHECK(ag.Update(.8f, 235.0).value == doctest::Approx(.5f));
    CHECK(ag.Update(.8f, 460.0).value == .8f);
}
TEST_CASE("whole-level experience guards normal and master boundaries")
{
    CHECK(ExperienceLevelStart(1, false) == 0);
    CHECK(ExperienceLevelStart(256, false) == 264 * 255 * 255 * 10);
    CHECK(ExperienceLevelStart(0, true) == 0);
    const auto start = ExperienceLevelStart(241, false);
    CHECK(NormalizeExperience(start + 5200, start + 10000, 241, false) == doctest::Approx(.52f));
    CHECK(NormalizeExperience(start - 1, start + 10000, 241, false) == 0.f);
    CHECK(NormalizeExperience(start + 10001, start + 10000, 241, false) == 1.f);
    CHECK(NormalizeExperience(start, start, 241, false) == 0.f);
    const auto master = ExperienceLevelStart(20, true);
    CHECK(NormalizeExperience(master + 100, master + 400, 20, true) == .25f);
}
TEST_CASE("experience shows completion carry loss and master transition")
{
    ExperienceProgress xp;
    xp.Update(.9f, 241, false, 0.0);
    CHECK(xp.Update(.2f, 242, false, 10.0).value == 1.f);
    CHECK(xp.Update(.2f, 242, false, 329.0).levelUp);
    CHECK(xp.Update(.2f, 242, false, 505.0).value == doctest::Approx(.1f));
    CHECK(xp.Update(.2f, 242, false, 680.0).value == .2f);
    CHECK(xp.Update(.1f, 242, false, 700.0).value == .1f);
    CHECK(xp.Update(.6f, 20, true, 710.0).value == .6f);
    CHECK_FALSE(xp.Update(.6f, 20, true, 710.0).levelUp);
    CHECK(xp.Update(.8f, 20, true, 720.0, true).value == .8f);
}
TEST_CASE("mixed action profiles retain keys and reject duplicate or corrupted input")
{
    ActionBarProfile profile;
    CHECK(profile.bindings[0].key == '1');
    CHECK(profile.bindings[9].key == '0');
    CHECK(profile.bindings[10].key == 'Q');
    CHECK(profile.bindings[11].key == 'W');
    CHECK(profile.bindings[12].key == 'E');
    std::string error;
    CHECK_FALSE(profile.Rebind(0, "Q", error));
    REQUIRE(profile.Rebind(0, "F4", error));
    CHECK_FALSE(profile.Rebind(1, "F4", error));
    CHECK_FALSE(profile.Rebind(-1, "A", error));
    CHECK_FALSE(profile.Rebind(0, "F13", error));
    CHECK_FALSE(profile.Rebind(0, "../a", error));
    CHECK(profile.Rebind(1, "None", error));
    CHECK(profile.Rebind(2, "None", error));
    profile.bindings[0].kind = ActionBarProfile::Kind::Item;
    profile.bindings[0].identity = 7168;
    profile.reducedMotion = true;
    const auto id = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto dir = std::filesystem::temp_directory_path() / ("compact-hud-profile-" + std::to_string(id));
    const auto path = dir / "character.profile";
    REQUIRE(profile.Save(path));
    ActionBarProfile loaded;
    REQUIRE(loaded.Load(path));
    CHECK(loaded.bindings[0].identity == 7168);
    CHECK(loaded.bindings[0].key == profile.bindings[0].key);
    CHECK(loaded.reducedMotion);
    REQUIRE(loaded.Rebind(0, "A", error));
    REQUIRE(loaded.Save(path));
    REQUIRE(profile.Load(path));
    CHECK(profile.bindings[0].key == 'A');
    std::ofstream(path, std::ios::trunc) << "1 0\n2 -1 0 Q\n";
    CHECK_FALSE(profile.Load(path));
    CHECK(profile.bindings[0].key == 'A');
    std::filesystem::remove_all(dir);
    profile.ResetKeys();
    CHECK(profile.bindings[0].key == '1');
}
