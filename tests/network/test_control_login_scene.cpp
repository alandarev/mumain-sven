#include "doctest.h"

#include "App/Control/ControlLoginScene.h"

#include "json.hpp"

using nlohmann::json;

namespace
{
App::Control::LoginSceneFacts ServerList()
{
    App::Control::LoginSceneFacts facts;
    facts.serverGroups = 1;
    facts.serverSelectVisible = true;
    facts.loginVisible = false;
    facts.messageVisible = false;
    facts.systemMenuVisible = false;
    facts.optionVisible = false;
    facts.creditVisible = false;
    facts.inputIdle = true;
    facts.windowPointer = std::array<float, 2>{512.0f, 192.0f};
    facts.logicalPointer = std::array<int, 2>{320, 120};
    return facts;
}

json Report(const App::Control::LoginSceneFacts& facts)
{
    return json::parse(App::Control::LoginSceneObject(facts));
}
} // namespace

TEST_CASE("Login scene report is versioned and names its scene [network][control-ui]")
{
    const json report = Report(ServerList());
    CHECK(report["version"] == App::Control::LoginSceneObservationVersion);
    CHECK(report["scene"] == "login");
    CHECK(report["server_groups"] == 1);
    CHECK(report["login_form_ready"] == false);
    CHECK(report["windows"]["server_select"] == true);
    CHECK(report["windows"]["login"] == false);
    CHECK(report["input_focused"] == false);
    CHECK(report["input_idle"] == true);
    CHECK(report["pointer"] == json::array({512.0, 192.0, 320, 120}));
}

TEST_CASE("Login scene report shows the account field, never the password [network][control-ui]")
{
    App::Control::LoginSceneFacts facts = ServerList();
    facts.loginFormReady = true;
    facts.loginVisible = true;
    facts.account = "test4";
    facts.passwordEmpty = true;
    facts.focusedField = "account";
    const json report = Report(facts);
    CHECK(report["login_form"]["account"] == "test4");
    CHECK(report["login_form"]["password_empty"] == true);
    CHECK(report["login_form"]["focused_field"] == "account");
    CHECK_FALSE(report["login_form"].contains("password"));
}

TEST_CASE("Login scene report keeps unavailable facts null, never negative [network][control-ui]")
{
    App::Control::LoginSceneFacts facts = ServerList();
    facts.genericConfirmVisible.reset();
    facts.inputIdle.reset();
    facts.windowPointer.reset();
    facts.focusedField = "something else";
    const json report = Report(facts);
    CHECK(report["windows"]["generic_confirm"].is_null());
    CHECK(report["input_idle"].is_null());
    CHECK(report["pointer"].is_null());
    CHECK(report["login_form"]["account"].is_null());
    CHECK(report["login_form"]["password_empty"].is_null());
    CHECK(report["login_form"]["focused_field"].is_null());
}
