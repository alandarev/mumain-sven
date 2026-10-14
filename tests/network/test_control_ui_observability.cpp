#include "doctest.h"

#include "App/Control/ControlUiObservation.h"

#include <vector>

TEST_CASE("Control UI activity preserves unavailable state [network][control-ui]")
{
    std::vector<int> dialogs;
    int reads = 0;
    const auto query = [&reads](const auto& stack)
    {
        ++reads;
        return !stack.empty();
    };

    const std::vector<int>* missing = nullptr;
    CHECK_FALSE(App::Control::ObserveUiActivity(missing, true, query).has_value());
    CHECK_FALSE(App::Control::ObserveUiActivity(&dialogs, false, query).has_value());
    CHECK(reads == 0);

    dialogs.push_back(1);
    CHECK_FALSE(App::Control::ObserveUiActivity(&dialogs, false, query).has_value());
    CHECK(reads == 0);
}

TEST_CASE("Control UI activity reads empty and nonempty state without consuming it [network][control-ui]")
{
    std::vector<int> dialogs;
    const auto query = [](const auto& stack) { return !stack.empty(); };
    const auto empty = App::Control::ObserveUiActivity(&dialogs, true, query);
    REQUIRE(empty.has_value());
    CHECK_FALSE(*empty);
    CHECK(dialogs.empty());

    dialogs.push_back(1);
    dialogs.push_back(2);
    const auto before = dialogs;
    for (int repetition = 0; repetition < 2; ++repetition)
    {
        const auto active = App::Control::ObserveUiActivity(&dialogs, true, query);
        REQUIRE(active.has_value());
        CHECK(*active);
        CHECK(dialogs == before);
    }
}
