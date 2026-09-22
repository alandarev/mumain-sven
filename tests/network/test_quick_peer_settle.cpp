#include "doctest.h"
#include "App/Control/QuickPeerSettleAct.h"
#include "App/Control/ControlUiFrames.h"
#include "json.hpp"

namespace
{
void CompleteFrame()
{
    App::Control::BeginUiFrame();
    App::Control::ObserveUiUpdate();
    App::Control::ObserveUiRender();
    App::Control::CompleteUiFrame();
}
} // namespace

TEST_CASE("Quick settle cannot complete in dispatcher Handle and same server Poll ticks [network][control-ui]")
{
    int observations = 0;
    std::unique_ptr<App::Control::Act> act = std::make_unique<App::Control::QuickPeerSettleAct>(
        R"({"window":"quick_command"})", "unchanged", [] { return true; },
        [&]
        {
            ++observations;
            return "unchanged";
        });
    act->SetEncodedId("7");
    std::string response;
    // Exact production schedule: Handle installs the act and calls Tick, then
    // ControlServer::Poll calls Tick again. No fake dispatcher or frame sleeps.
    CHECK(act->Tick(response) == App::Control::Act::Status::Running);
    CHECK(act->Tick(response) == App::Control::Act::Status::Running);
    CHECK(response.empty()); // The old two-Tick implementation already answered here.
    for (int poll = 0; poll < 5; ++poll)
        CHECK(act->Tick(response) == App::Control::Act::Status::Running);
    CompleteFrame();
    CHECK(act->Tick(response) == App::Control::Act::Status::Running);
    CHECK(act->Tick(response) == App::Control::Act::Status::Running);
    CHECK(response.empty());
    CompleteFrame();
    CHECK(act->Tick(response) == App::Control::Act::Status::Finished);
    CHECK(observations == 10);
    const auto result = nlohmann::json::parse(response);
    CHECK(result["ok"] == true);
    CHECK(result["id"] == 7);
}

TEST_CASE("Quick settle counts a completed-frame jump as one observation [network][control-ui]")
{
    App::Control::QuickPeerSettleAct act("{}", "same", [] { return true; }, [] { return "same"; });
    std::string response;
    CHECK(act.Deadline() == std::chrono::seconds(5));
    CompleteFrame();
    CompleteFrame();
    CompleteFrame();
    CHECK(act.Tick(response) == App::Control::Act::Status::Running);
    CHECK(act.Tick(response) == App::Control::Act::Status::Running);
    CHECK(response.empty());
    // Skipped UI render cannot grant the second observation.
    App::Control::BeginUiFrame();
    App::Control::ObserveUiUpdate();
    App::Control::CompleteUiFrame();
    CHECK(act.Tick(response) == App::Control::Act::Status::Running);
    CompleteFrame();
    CHECK(act.Tick(response) == App::Control::Act::Status::Finished);
    CHECK(nlohmann::json::parse(response)["ok"] == true);
}

TEST_CASE("Only completed update then render frames advance quick settling [network][control-ui]")
{
    const auto before = App::Control::CompletedUiFrames();
    App::Control::BeginUiFrame();
    App::Control::CompleteUiFrame();
    CHECK(App::Control::CompletedUiFrames() == before);
    App::Control::BeginUiFrame();
    App::Control::ObserveUiRender();
    App::Control::ObserveUiUpdate();
    App::Control::CompleteUiFrame();
    CHECK(App::Control::CompletedUiFrames() == before);
    App::Control::BeginUiFrame();
    App::Control::ObserveUiUpdate();
    App::Control::CompleteUiFrame();
    CHECK(App::Control::CompletedUiFrames() == before);
    CompleteFrame();
    CHECK(App::Control::CompletedUiFrames() == before + 1);
    App::Control::CompleteUiFrame();
    CHECK(App::Control::CompletedUiFrames() == before + 1);
}

TEST_CASE("Quick deferred snapshot refuses target input menu and geometry drift without cleanup [network][control-ui]")
{
    const nlohmann::json baseline = {{"target", "Peer"},      {"index", 2},
                                     {"input_idle", true},    {"menu_visible", true},
                                     {"placement", {20, 40}}, {"modal", false}};
    for (const char* field : {"target", "index", "input_idle", "menu_visible", "placement", "modal"})
    {
        CAPTURE(field);
        auto current = baseline;
        App::Control::QuickPeerSettleAct act(
            "{}", baseline.dump(), [] { return true; }, [&] { return current.dump(); });
        std::string response;
        CHECK(act.Tick(response) == App::Control::Act::Status::Running);
        CHECK(act.Tick(response) == App::Control::Act::Status::Running);
        CompleteFrame();
        current[field] = nullptr; // Observation fixture, not a live producer manipulation.
        const auto beforeRefusal = current;
        CHECK(act.Tick(response) == App::Control::Act::Status::Finished);
        CHECK(nlohmann::json::parse(response)["error"] == "not_allowed");
        CHECK(current == beforeRefusal);
    }
}

TEST_CASE("Quick readiness loss refuses before deferred snapshot on any tick [network][control-ui]")
{
    for (bool advanceFrame : {false, true})
    {
        bool ready = true;
        int reads = 0;
        App::Control::QuickPeerSettleAct act(
            "{}", "same", [&] { return ready; },
            [&]
            {
                ++reads;
                return "same";
            });
        std::string response;
        CHECK(act.Tick(response) == App::Control::Act::Status::Running);
        if (advanceFrame)
            CompleteFrame();
        ready = false; // Models world/loading/registry/storage readiness loss.
        CHECK(act.Tick(response) == App::Control::Act::Status::Finished);
        CHECK(reads == 1);
        CHECK_FALSE(ready);
        CHECK(nlohmann::json::parse(response)["error"] == "not_allowed");
    }
}
