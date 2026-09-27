#include "doctest.h"

#include "App/Control/ControlCharacterScene.h"

#include "json.hpp"

using nlohmann::json;

namespace
{
App::Control::CharacterSceneFacts QuietScene()
{
    App::Control::CharacterSceneFacts facts;
    facts.rosterReceived = true;
    facts.selectedHero = 0;
    facts.characterSlots = 5;
    facts.characterSelectVisible = true;
    facts.characterMakeVisible = false;
    facts.messageVisible = false;
    facts.systemMenuVisible = false;
    facts.optionVisible = false;
    facts.inputIdle = true;
    facts.windowPointer = std::array<float, 2>{512.0f, 192.0f};
    facts.logicalPointer = std::array<int, 2>{320, 120};
    facts.serverMessageLines = 0;
    return facts;
}

json Report(const App::Control::CharacterSceneFacts& facts)
{
    return json::parse(App::Control::CharacterSceneObject(facts));
}
} // namespace

TEST_CASE("Character scene report is versioned and names its scene [network][control-ui]")
{
    const json report = Report(QuietScene());
    CHECK(report["version"] == App::Control::CharacterSceneObservationVersion);
    CHECK(report["scene"] == "character_list");
    CHECK(report["roster_received"] == true);
    CHECK(report["selected_slot"] == 1);
    CHECK(report["input_focused"] == false);
    CHECK(report["input_idle"] == true);
    CHECK(report["pointer"] == json::array({512.0, 192.0, 320, 120}));
    CHECK(report["windows"]["character_select"] == true);
    CHECK(report["windows"]["message"] == false);
    CHECK(report["server_message"]["visible"] == false);
    CHECK(report["server_message"]["lines"] == 0);
    CHECK(report["server_message"]["texts"] == json::array());
}

TEST_CASE("Character scene report keeps unavailable facts null, never negative [network][control-ui]")
{
    App::Control::CharacterSceneFacts facts = QuietScene();
    facts.genericConfirmVisible.reset();
    facts.optionVisible.reset();
    facts.inputIdle.reset();
    facts.windowPointer.reset();
    const json report = Report(facts);
    CHECK(report["windows"]["generic_confirm"].is_null());
    CHECK(report["windows"]["option"].is_null());
    CHECK(report["input_idle"].is_null());
    CHECK(report["pointer"].is_null());
}

TEST_CASE("Character scene report states no selection and a bad index as null [network][control-ui]")
{
    App::Control::CharacterSceneFacts facts = QuietScene();
    facts.selectedHero = -1;
    CHECK(Report(facts)["selected_slot"].is_null());
    facts.selectedHero = 5;
    CHECK(Report(facts)["selected_slot"].is_null());
    facts.selectedHero = 4;
    CHECK(Report(facts)["selected_slot"] == 5);
}

TEST_CASE("Character scene report lists exactly the stored server message lines [network][control-ui]")
{
    App::Control::CharacterSceneFacts facts = QuietScene();
    facts.serverMessageVisible = true;
    facts.serverMessageLines = 2;
    facts.serverMessageTexts = {"First notice", "Second notice"};
    const json report = Report(facts);
    CHECK(report["server_message"]["visible"] == true);
    CHECK(report["server_message"]["lines"] == 2);
    CHECK(report["server_message"]["texts"] == json::array({"First notice", "Second notice"}));
}

TEST_CASE("Character scene report refuses an impossible server message line count [network][control-ui]")
{
    App::Control::CharacterSceneFacts facts = QuietScene();
    facts.serverMessageLines = App::Control::CharacterSceneServerMessageLines + 1;
    facts.serverMessageTexts = {"a", "b", "c", "d", "e", "f"};
    CHECK(Report(facts)["server_message"]["lines"].is_null());
    CHECK(Report(facts)["server_message"]["texts"].is_null());

    facts.serverMessageLines = -1;
    CHECK(Report(facts)["server_message"]["lines"].is_null());

    facts.serverMessageLines = 3;
    facts.serverMessageTexts = {"only one"};
    CHECK(Report(facts)["server_message"]["lines"].is_null());
}
