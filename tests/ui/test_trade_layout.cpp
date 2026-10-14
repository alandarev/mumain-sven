#include "UI/RmlBridge/ThemeFileInterface.h"
#include "UI/RmlBridge/RmlPanelGeometry.h"
#include "UI/Inventory/ItemGridModel.h"
#include <doctest.h>
#include <RmlUi/Core.h>
#include <filesystem>
#include <vector>

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
} // namespace

TEST_CASE("stacked Trade keeps full native offers and symmetric money through readiness changes [ui][trade]")
{
    NullRenderer renderer;
    UI::RmlBridge::ThemeFileInterface files;
    UI::Items::ItemGridCells cells = UI::Items::ItemGridCells(32);
    Rml::SetFileInterface(&files);
    Rml::SetRenderInterface(&renderer);
    REQUIRE(Rml::Initialise());
    REQUIRE(Rml::LoadFontFace(std::string(MU_RMLUI_DIR) + "/../../../fonts/DejaVuSans.ttf"));
    auto* context = Rml::CreateContext("trade-layout", {1920, 1080});
    auto constructor = context->CreateDataModel("trade");
    UI::Items::RegisterItemGridCells(constructor);
    constructor.Bind("grid_cells", &cells);
    constructor.Bind("partner_cells", &cells);
    float text = 9;
    constructor.Bind("text_px", &text);
    bool partnerReady = false, localReady = false, waiting = false, guildVisible = true;
    constructor.Bind("your_confirm_checked", &partnerReady);
    constructor.Bind("my_confirm_checked", &localReady);
    constructor.Bind("my_confirm_waiting", &waiting);
    constructor.Bind("your_guild_visible", &guildVisible);
    Rml::String title = "Trade", name = "TenLetters", level = "Lv. 400+", zen = "999,999,999";
    constructor.Bind("title", &title);
    for (const auto* field : {"your_id_text", "my_id_text", "your_guild_name", "item_warning_text"})
        constructor.Bind(field, &name);
    constructor.Bind("your_level_text", &level);
    constructor.Bind("your_gold_text", &zen);
    constructor.Bind("my_gold_text", &zen);
    struct Badge
    {
        float x, y, width;
    };
    auto badge = constructor.RegisterStruct<Badge>();
    badge.RegisterMember("x", &Badge::x);
    badge.RegisterMember("y", &Badge::y);
    badge.RegisterMember("width", &Badge::width);
    constructor.RegisterArray<std::vector<Badge>>();
    std::vector<Badge> badges;
    constructor.Bind("item_warning_badges", &badges);
    int cancelCalls = 0, moneyCalls = 0, readyCalls = 0;
    constructor.BindEventCallback("trade_exit_click", [&cancelCalls](Rml::DataModelHandle, Rml::Event&,
                                                                     const Rml::VariantList&) { ++cancelCalls; });
    constructor.BindEventCallback(
        "trade_zen_click", [&moneyCalls](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { ++moneyCalls; });
    constructor.BindEventCallback("trade_my_confirm_click", [&readyCalls](Rml::DataModelHandle, Rml::Event&,
                                                                          const Rml::VariantList&) { ++readyCalls; });
    auto model = constructor.GetModelHandle();
    const auto themePath = std::string(MU_RMLUI_DIR) + "/themes/munolith/";
    auto* foreground = context->LoadDocument(themePath + "trade.rml");
    REQUIRE(foreground);
    UI::RmlBridge::SlotPlacement{0.f, 0.f, 1.f}.Apply(foreground, "panel");
    foreground->Show();
    for (int state = 0; state < 4; ++state)
    {
        partnerReady = state == 1 || state == 2;
        localReady = state == 2;
        waiting = state == 3;
        model.DirtyAllVariables();
        context->Update();
        context->Update();
        auto* panel = foreground->GetElementById("panel");
        CHECK(panel->GetBox().GetSize(Rml::BoxArea::Border) == Rml::Vector2f(190, 350));
        Rml::ElementList grids;
        foreground->QuerySelectorAll(grids, ".item-grid");
        REQUIRE(grids.size() == 2);
        const char* anchors[] = {"partner_grid", "item_grid"};
        for (size_t i = 0; i < grids.size(); ++i)
        {
            auto* anchor = foreground->GetElementById(anchors[i]);
            REQUIRE(anchor);
            CHECK(anchor->GetAbsoluteOffset() == grids[i]->GetAbsoluteOffset());
            CHECK(anchor->GetBox().GetSize(Rml::BoxArea::Content) == Rml::Vector2f(160, 80));
            Rml::ElementList cells;
            grids[i]->QuerySelectorAll(cells, ".item-cell");
            REQUIRE(cells.size() == 32);
            for (auto* cell : cells)
                CHECK(cell->GetBox().GetSize(Rml::BoxArea::Border) == Rml::Vector2f(20, 20));
        }
        auto* partnerZen = foreground->GetElementById("your_gold_bg");
        auto* localZen = foreground->GetElementById("my_gold_bg");
        CHECK(partnerZen->GetBox().GetSize(Rml::BoxArea::Border) == localZen->GetBox().GetSize(Rml::BoxArea::Border));
        CHECK(partnerZen->GetAbsoluteOffset().x == localZen->GetAbsoluteOffset().x);
        CHECK(partnerZen->IsClassSet("ready") == partnerReady);
        CHECK(localZen->IsClassSet("ready") == localReady);
        auto* partnerOverlay = foreground->GetElementById("partner_ready_overlay");
        auto* localOverlay = foreground->GetElementById("my_ready_overlay");
        CHECK(partnerOverlay->IsVisible() == partnerReady);
        CHECK(localOverlay->IsVisible() == localReady);
        for (auto* overlay : {partnerOverlay, localOverlay})
            CHECK(overlay->GetProperty<Rml::Style::PointerEvents>("pointer-events") == Rml::Style::PointerEvents::None);
        auto* action = foreground->GetElementById("my_confirm");
        CHECK(action->IsClassSet("waiting") == waiting);
        Rml::ElementList labels;
        action->QuerySelectorAll(labels, ".native-text > span");
        REQUIRE(labels.size() == 3);
        CHECK(labels[0]->IsVisible() == waiting);
        CHECK(labels[1]->IsVisible() == (!waiting && localReady));
        CHECK(labels[2]->IsVisible() == (!waiting && !localReady));
        CHECK(foreground->GetElementById("notice_line1") == nullptr);
        for (const auto* id : {"btn_exit", "my_confirm", "btn_corner_close"})
        {
            auto* control = foreground->GetElementById(id);
            REQUIRE(control);
            CHECK(control->IsVisible());
            CHECK(control->GetAbsoluteOffset().y + control->GetBox().GetSize(Rml::BoxArea::Border).y <= 350);
        }
    }
    Rml::Dictionary parameters;
    foreground->GetElementById("my_gold_bg")->DispatchEvent("click", parameters);
    foreground->GetElementById("my_confirm")->DispatchEvent("click", parameters);
    foreground->GetElementById("btn_exit")->DispatchEvent("click", parameters);
    foreground->GetElementById("btn_corner_close")->DispatchEvent("click", parameters);
    CHECK(moneyCalls == 1);
    CHECK(readyCalls == 1);
    CHECK(cancelCalls == 2);
    context->UnloadAllDocuments();
    Rml::RemoveContext("trade-layout");
    Rml::Shutdown();
    Rml::SetRenderInterface(nullptr);
    Rml::SetFileInterface(nullptr);
}
