#include <doctest.h>

#include "UI/RmlBridge/RmlDraggable.h"

#include <RmlUi/Core.h>

namespace
{
struct NullRenderer : Rml::RenderInterface
{
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override
    {
        return 0;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override
    {
        return 0;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override
    {
        return 0;
    }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};

int s_ends = 0;
} // namespace

// The drag listener is attached for three events; unloading its document detaches all three
// (a theme switch does this to the inventory). It used to delete itself on the first detach.
TEST_CASE("A draggable panel's document unloads without touching a freed listener [ui][rml-draggable]")
{
    NullRenderer renderer;
    Rml::SetRenderInterface(&renderer);
    REQUIRE(Rml::Initialise());
    auto* context = Rml::CreateContext("rml-draggable", {320, 240});
    REQUIRE(context != nullptr);

    for (int round = 0; round < 3; ++round)
    {
        auto* document = context->LoadDocumentFromMemory(
            "<rml><head></head><body><div id='panel' style='position:absolute;left:0;top:0;width:100px;height:100px'>"
            "<div id='title' style='display:block;height:20px'></div></div></body></rml>");
        REQUIRE(document != nullptr);
        UI::RmlBridge::MakeDraggable(document->GetElementById("title"), document->GetElementById("panel"), nullptr,
                                     [] { ++s_ends; });
        document->Show();
        context->Update();
        context->UnloadDocument(document);
        context->Update();
    }

    // The listener still serves its events, and its document still unloads cleanly after use.
    auto* document = context->LoadDocumentFromMemory(
        "<rml><head></head><body><div id='panel' style='position:absolute;left:0;top:0;width:100px;height:100px'>"
        "<div id='title' style='display:block;height:20px'></div></div></body></rml>");
    REQUIRE(document != nullptr);
    Rml::Element* title = document->GetElementById("title");
    UI::RmlBridge::MakeDraggable(title, document->GetElementById("panel"), nullptr, [] { ++s_ends; });
    Rml::Dictionary parameters;
    title->DispatchEvent(Rml::EventId::Dragend, parameters);
    CHECK(s_ends == 1);
    context->UnloadDocument(document);
    context->Update();

    Rml::RemoveContext("rml-draggable");
    Rml::Shutdown();
    Rml::SetRenderInterface(nullptr);
}

TEST_CASE("A denied title drag neither moves a panel nor saves its position [ui][rml-draggable]")
{
    NullRenderer renderer;
    Rml::SetRenderInterface(&renderer);
    REQUIRE(Rml::Initialise());
    auto* context = Rml::CreateContext("rml-denied-drag", {320, 240});
    REQUIRE(context != nullptr);
    auto* document = context->LoadDocumentFromMemory(
        "<rml><body><div id='panel' style='position:absolute;left:10px;top:15px;width:100px;height:100px'>"
        "<div id='title' style='height:20px'></div></div></body></rml>");
    REQUIRE(document != nullptr);
    auto* panel = document->GetElementById("panel");
    auto* title = document->GetElementById("title");
    int moves = 0;
    int saves = 0;
    UI::RmlBridge::MakeDraggable(title, panel, [&](float, float) { ++moves; }, [&] { ++saves; }, [] { return false; });
    document->Show();
    context->Update();
    Rml::Dictionary start;
    start["mouse_x"] = 20;
    start["mouse_y"] = 20;
    Rml::Dictionary end;
    end["mouse_x"] = 100;
    end["mouse_y"] = 100;
    title->DispatchEvent(Rml::EventId::Dragstart, start);
    title->DispatchEvent(Rml::EventId::Drag, end);
    title->DispatchEvent(Rml::EventId::Dragend, end);
    CHECK(moves == 0);
    CHECK(saves == 0);
    CHECK(panel->GetOffsetLeft() == doctest::Approx(10.f));
    CHECK(panel->GetOffsetTop() == doctest::Approx(15.f));
    context->UnloadDocument(document);
    context->Update();
    Rml::RemoveContext("rml-denied-drag");
    Rml::Shutdown();
    Rml::SetRenderInterface(nullptr);
}

TEST_CASE("An external drag reports screen motion through a scaled panel [ui][rml-draggable]")
{
    NullRenderer renderer;
    Rml::SetRenderInterface(&renderer);
    REQUIRE(Rml::Initialise());
    auto* context = Rml::CreateContext("rml-external-drag", {320, 240});
    REQUIRE(context != nullptr);
    auto* document = context->LoadDocumentFromMemory(
        "<rml><body><div id='panel' style='position:absolute;left:50px;top:30px;width:200px;height:100px;"
        "transform:scale(0.5);transform-origin:0 0'>"
        "<div id='title' style='display:block;width:200px;height:20px'></div></div></body></rml>");
    REQUIRE(document != nullptr);
    auto* panel = document->GetElementById("panel");
    float left = 0.f;
    float top = 0.f;
    int saves = 0;
    UI::RmlBridge::MakeExternalDraggable(
        document->GetElementById("title"), panel,
        [&](float x, float y)
        {
            left = x;
            top = y;
        },
        [&] { ++saves; });
    document->Show();
    context->Update();
    context->ProcessMouseMove(65, 35, 0);
    context->ProcessMouseButtonDown(0, 0);
    context->ProcessMouseMove(105, 65, 0);
    context->ProcessMouseMove(145, 95, 0);
    context->ProcessMouseButtonUp(0, 0);
    CHECK(left == doctest::Approx(130.f));
    CHECK(top == doctest::Approx(90.f));
    CHECK(saves == 1);
    CHECK(panel->GetOffsetLeft() == doctest::Approx(50.f));
    CHECK(panel->GetOffsetTop() == doctest::Approx(30.f));
    context->UnloadDocument(document);
    context->Update();
    Rml::RemoveContext("rml-external-drag");
    Rml::Shutdown();
    Rml::SetRenderInterface(nullptr);
}
