#include "stdafx.h"
#include <doctest.h>
#include "UI/RmlBridge/RmlElementBox.h"
#include <RmlUi/Core.h>
#include <cmath>
#include <limits>

namespace
{
class ProjectionFixture
{
    struct Renderer : Rml::RenderInterface
    {
        Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override
        {
            return 1;
        }
        void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override
        {
            ++draws;
        }
        void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
        Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override
        {
            return 0;
        }
        Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override
        {
            return 1;
        }
        void ReleaseTexture(Rml::TextureHandle) override {}
        void EnableScissorRegion(bool) override {}
        void SetScissorRegion(Rml::Rectanglei) override {}
        int draws = 0;
    } renderer;

public:
    ProjectionFixture()
    {
        Rml::SetRenderInterface(&renderer);
        REQUIRE(Rml::Initialise());
        context = Rml::CreateContext("forward-point", {12000, 6000});
        REQUIRE(context);
    }
    ~ProjectionFixture()
    {
        Rml::Shutdown();
        Rml::SetRenderInterface(nullptr);
    }
    Rml::ElementDocument* Load(const Rml::String& markup)
    {
        auto* doc = context->LoadDocumentFromMemory(markup);
        REQUIRE(doc);
        doc->Show();
        Prepare();
        return doc;
    }
    void Prepare()
    {
        context->Update();
        const auto before = renderer.draws;
        Rml::ElementUtilities::UpdateTransformCaches(context->GetRootElement());
        CHECK(renderer.draws == before);
    }
    Rml::Context* context = nullptr;
};
void Point(Rml::Element* e, Rml::Vector2f input, Rml::Vector2f expected)
{
    REQUIRE(Rml::ElementUtilities::TransformPoint(e, input));
    CAPTURE(e->GetId());
    CAPTURE(input.x);
    CAPTURE(input.y);
    CAPTURE(expected.x);
    CAPTURE(expected.y);
    CHECK(std::abs(input.x - expected.x) <= .002f);
    CHECK(std::abs(input.y - expected.y) <= .002f);
}
} // namespace

TEST_CASE("Engine forward points preserve full two-corner semantics at large moving origins [ui][projection]")
{
    ProjectionFixture f;
    auto* doc = f.Load("<rml><head><style>div{position:absolute;transform-origin:left top;}</style></head><body><div "
                       "id='root' style='width:128px;height:64px;background-color:#ffffff;'/></body></rml>");
    auto* e = doc->GetElementById("root");
    REQUIRE(e);
    Point(e, {1, 2}, {1, 2});
    for (float left : {0.f, 3200.f, 9500.f, 3211.125f})
    {
        e->SetProperty(Rml::PropertyId::Left, Rml::Property(left, Rml::Unit::PX));
        e->SetProperty("top", "800px");
        e->SetProperty("transform", "translate(11px,7px) scale(0.5)");
        f.Prepare();
        Point(e, {left, 800}, {left + 11, 807});
        Point(e, {left + 128, 864}, {left + 75, 839});
        Rml::Vector2f offset, size;
        REQUIRE(UI::RmlBridge::DrawnContentBox(*e, offset, size));
        CHECK(std::abs(offset.x - left - 11) <= .002f);
        CHECK(size == Rml::Vector2f(64, 32));
        Rml::Rectanglef engine;
        REQUIRE(Rml::ElementUtilities::GetBoundingBox(engine, e, Rml::BoxArea::Content));
        CHECK(std::abs(offset.x - engine.Left()) <= .002f);
        CHECK(std::abs(size.x - engine.Width()) <= .002f);
        f.context->Render();
        Rml::Vector2f renderedOffset, renderedSize;
        REQUIRE(UI::RmlBridge::DrawnContentBox(*e, renderedOffset, renderedSize));
        CHECK(renderedOffset == offset);
        CHECK(renderedSize == size);
        auto* hit = f.context->GetElementAtPoint({offset.x + 20, offset.y + 10});
        CHECK(hit == e); // Independent real engine inverse hit path.
    }
    e->SetProperty("left", "200px");
    e->SetProperty("top", "100px");
    e->SetProperty("width", "80px");
    e->SetProperty("height", "20px");
    e->SetProperty("transform", "rotate(30deg)");
    f.Prepare();
    Point(e, {200, 100}, {200, 100});
    Point(e, {280, 120},
          {200 + 80 * std::cos(.5235987756f) - 20 * std::sin(.5235987756f),
           100 + 80 * std::sin(.5235987756f) + 20 * std::cos(.5235987756f)});
    Rml::Vector2f offset, size;
    REQUIRE(UI::RmlBridge::DrawnBox(*e, Rml::BoxArea::Content, offset, size));
    CHECK(offset == Rml::Vector2f(200, 100));
    Rml::Rectanglef aabb;
    REQUIRE(Rml::ElementUtilities::GetBoundingBox(aabb, e, Rml::BoxArea::Content));
    CHECK(aabb.Left() < offset.x - 9.f);
    e->SetProperty("transform", "translate(3px,4px)");
    e->SetProperty("width", "0px");
    e->SetProperty("height", "0px");
    f.Prepare();
    REQUIRE(UI::RmlBridge::DrawnTopLeft(*e, offset));
    CHECK(offset == Rml::Vector2f(203, 104));
    offset = {17, 19};
    size = {23, 29};
    CHECK_FALSE(UI::RmlBridge::DrawnContentBox(*e, offset, size));
    CHECK(offset == Rml::Vector2f(17, 19));
    CHECK(size == Rml::Vector2f(23, 29));
    e->SetProperty("transform", "scale(0)");
    f.Prepare();
    Rml::Vector2f unchanged{200, 100};
    CHECK_FALSE(Rml::ElementUtilities::TransformPoint(e, unchanged));
    CHECK(unchanged == Rml::Vector2f(200, 100));
    CHECK_FALSE(UI::RmlBridge::DrawnTopLeft(*e, offset));
    CHECK(offset == Rml::Vector2f(17, 19));
    unchanged = {std::numeric_limits<float>::infinity(), 100};
    CHECK_FALSE(Rml::ElementUtilities::TransformPoint(e, unchanged));
    CHECK(std::isinf(unchanged.x));
    unchanged = {std::numeric_limits<float>::quiet_NaN(), 100};
    CHECK_FALSE(Rml::ElementUtilities::TransformPoint(e, unchanged));
    CHECK(std::isnan(unchanged.x));
}

TEST_CASE("Engine forward points retain ancestor counter-scale scroll and perspective semantics [ui][projection]")
{
    ProjectionFixture f;
    auto* doc = f.Load(
        "<rml><head><style>div{position:absolute;display:block;transform-origin:left top;}</style></head><body><div "
        "id='parent' style='left:3000px;top:200px;width:200px;height:100px;transform:scale(0.5);'><div id='child' "
        "style='left:100px;top:20px;width:40px;height:60px;transform:scale(2);background-color:#ffffff;'/></div></"
        "body></rml>");
    auto* e = doc->GetElementById("child");
    REQUIRE(e);
    Point(e, {3100, 220}, {3050, 210});
    Point(e, {3140, 280}, {3090, 270});
    auto* parent = doc->GetElementById("parent");
    parent->SetProperty("overflow", "auto");
    e->SetProperty("top", "200px");
    e->SetProperty("height", "400px");
    e->SetProperty("transform", "none");
    e->SetProperty("position", "relative");
    f.Prepare();
    parent->SetScrollTop(100);
    f.Prepare();
    CHECK(parent->GetScrollTop() == 100);
    const auto absolute = e->GetAbsoluteOffset(Rml::BoxArea::Content);
    Point(e, absolute, {3050, 250});
    doc->Close();
    f.Prepare();
    doc = f.Load(
        "<rml><head><style>div{position:absolute;display:block;transform-origin:left top;}</style></head><body><div "
        "style='left:100px;top:50px;width:200px;height:100px;perspective:500px;perspective-origin:left top;'><div "
        "id='child' "
        "style='left:10px;top:20px;width:40px;height:20px;transform:translateZ(100px);'/></div></body></rml>");
    e = doc->GetElementById("child");
    REQUIRE(e);
    Point(e, e->GetAbsoluteOffset(Rml::BoxArea::Content), {112.5f, 75.f});
    Rml::Vector2f offset, size;
    REQUIRE(UI::RmlBridge::DrawnContentBox(*e, offset, size));
    CHECK(std::abs(size.x - 50) <= .002f);
    CHECK(std::abs(size.y - 25) <= .002f);
    f.context->Render();
    Rml::Vector2f after, afterSize;
    REQUIRE(UI::RmlBridge::DrawnContentBox(*e, after, afterSize));
    CHECK(after == offset);
    CHECK(afterSize == size);
    e->SetProperty("transform", "translateZ(500px)");
    f.Prepare();
    auto point = e->GetAbsoluteOffset();
    const auto old = point;
    CHECK_FALSE(Rml::ElementUtilities::TransformPoint(e, point));
    CHECK(point == old);
    e->SetProperty("transform", "translateZ(600px)");
    f.Prepare();
    CHECK_FALSE(Rml::ElementUtilities::TransformPoint(e, point));
    CHECK(point == old);
}
