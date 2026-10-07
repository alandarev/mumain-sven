#include "stdafx.h"
#include <doctest.h>
#include "UI/AuctionHouse/WindowModel.h"
#include "UI/RmlBridge/RmlThemedView.h"
#include "UI/RmlBridge/RmlTheme.h"
#include <RmlUi/Core.h>
#include <filesystem>

namespace
{
class Renderer : public Rml::RenderInterface
{
public:
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override
    {
        return 1;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i& size, const Rml::String&) override
    {
        size = {256, 256};
        return 1;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override
    {
        return 1;
    }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};

class Fixture
{
public:
    Fixture() : cwd(std::filesystem::current_path()), theme(UI::RmlBridge::GetActiveThemeName())
    {
        std::filesystem::current_path(MU_AUCTION_BIN);
        Rml::SetRenderInterface(&renderer);
        REQUIRE(Rml::Initialise());
        REQUIRE(Rml::LoadFontFace("fonts/DejaVuSans.ttf"));
        REQUIRE(Rml::LoadFontFace("fonts/LiberationSans-Regular.ttf"));
        context = Rml::CreateContext("auction-layout", {1024, 768});
        REQUIRE(context);
    }
    ~Fixture()
    {
        UI::RmlBridge::SetActiveThemeName(theme);
        Rml::Shutdown();
        Rml::SetRenderInterface(nullptr);
        std::filesystem::current_path(cwd);
    }
    Rml::Context* context = nullptr;

private:
    Renderer renderer;
    std::filesystem::path cwd;
    std::string theme;
};

void Bind(Rml::DataModelConstructor& c, UI::AuctionHouse::WindowModel& model)
{
    UI::AuctionHouse::BindWindowModel(c, model);
    for (const char* name : {"close",     "tab",     "filter",    "reset",  "category",
                             "equipment", "select",  "inventory", "hover",  "inventory_hover",
                             "leave",     "refresh", "more",      "quote",  "sell_change",
                             "minus",     "plus",    "buy",       "sell",   "cancel_listing",
                             "claim",     "dismiss", "confirm",   "usable", "luck",
                             "skill"})
        c.BindEventCallback(name, [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
}
} // namespace

TEST_CASE("Auction native document stays bounded across tabs themes and scale [auction][rml]")
{
    Fixture fixture;
    for (const char* theme : {"legacy", "modern", "munolith"})
    {
        UI::RmlBridge::SetActiveThemeName(theme);
        UI::RmlBridge::ThemedView<UI::AuctionHouse::WindowModel> view(
            "auction_house", Bind,
            {{"Data/Interface/RmlUi/auction_house.rml", [&fixture] { return fixture.context; }}});
        REQUIRE(view.Ensure());
        auto* document = view.Document();
        REQUIRE(document);
        auto& model = view.GetModel();
        model.rows = {{"Excellent Dragon Gloves +13", "Defense 102\nDefense success rate +10%", "1,350kk", "Seller",
                       "excellent", "", 0, true, false}};
        model.inventory = model.rows;
        model.pending = 3;
        view.MarkDirty("pending");
        model.selected = true;
        model.selectedName = model.rows.front().name;
        model.selectedDetail = model.rows.front().detail;
        model.options = {{"0", "Any option"}, {"1", "Defense success rate +10%"}};
        model.types = {{"0", "All types"}};
        model.durations = {{"48", "48 h"}};
        model.duration = "48 h";
        for (int width : {640, 1024, 1920})
            for (float scale : {1.f, 1.6f, 2.f})
                for (const char* tab : {"browse", "sell", "own", "collection"})
                {
                    fixture.context->SetDimensions({width, width == 640 ? 480 : 768});
                    fixture.context->SetDensityIndependentPixelRatio(scale);
                    INFO(std::string(theme), " ", width, " scale ", scale, " tab ", std::string(tab));
                    model.tab = tab;
                    view.MarkDirty("tab");
                    view.MarkDirty("rows");
                    view.MarkDirty("inventory");
                    view.MarkDirty("selected");
                    view.MarkDirty("selected_name");
                    view.MarkDirty("selected_detail");
                    view.MarkDirty("options");
                    view.MarkDirty("types");
                    view.MarkDirty("durations");
                    view.MarkDirty("duration");
                    document->Show();
                    fixture.context->Update();
                    fixture.context->Update();
                    auto* panel = document->GetElementById("panel");
                    REQUIRE(panel);
                    const auto size = panel->GetBox().GetSize(Rml::BoxArea::Border);
                    CHECK(size.x > 0);
                    CHECK(size.y > 0);
                    CHECK(size.x <= width);
                    CHECK(size.y <= fixture.context->GetDimensions().y);
                    if (model.tab == "browse")
                    {
                        Rml::ElementList toggles;
                        document->GetElementsByClassName(toggles, "filter-toggle");
                        REQUIRE(toggles.size() == 3);
                        for (auto* toggle : toggles)
                        {
                            CHECK(toggle->IsVisible(true));
                            CHECK(toggle->GetBox().GetSize(Rml::BoxArea::Border).x > 0);
                            CHECK(toggle->GetBox().GetSize(Rml::BoxArea::Border).y > 0);
                            REQUIRE(toggle->GetNumChildren() == 2);
                            CHECK(toggle->GetChild(1)->GetBox().GetSize(Rml::BoxArea::Border).x > 0);
                        }
                    }
                    auto* results = document->GetElementById("results");
                    Rml::ElementList actions;
                    document->GetElementsByClassName(actions, "primary");
                    for (auto* action : actions)
                    {
                        if (!action->IsVisible(true))
                            continue;
                        const auto parentWidth = action->GetParentNode()->GetBox().GetSize(Rml::BoxArea::Content).x;
                        INFO(action->GetParentNode()->GetTagName(), " display ",
                             action->GetParentNode()->GetProperty("display")->ToString());
                        CHECK(action->GetBox().GetSize(Rml::BoxArea::Border).x <= parentWidth + 1);
                    }
                    model.review = true;
                    view.MarkDirty("review");
                    model.confirmName = "Excellent Dragon Gloves +13";
                    view.MarkDirty("confirm_name");
                    model.confirmTotal = "4,294,967,295 Zen";
                    view.MarkDirty("confirm_total");
                    fixture.context->Update();
                    Rml::ElementList cards;
                    document->GetElementsByClassName(cards, "review-card");
                    REQUIRE(cards.size() == 1);
                    CHECK(cards.front()->GetBox().GetSize(Rml::BoxArea::Border).x <= size.x);
                    model.review = false;
                    view.MarkDirty("review");
                    fixture.context->Update();
                    if (results)
                    {
                        CHECK(results->GetClientHeight() > 0);
                        CHECK(results->GetClientHeight() <= size.y);
                    }
                }
        view.Release();
    }
}
