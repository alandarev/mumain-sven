#include "UI/RmlBridge/ThemeFileInterface.h"
#include "UI/RmlBridge/RmlPanelGeometry.h"
#include "UI/Inventory/ItemGridModel.h"
#include <doctest.h>
#include <RmlUi/Core.h>
#include "UI/RmlBridge/RmlTheme.h"
#include <filesystem>
#include <numeric>

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
}

TEST_CASE("compact merchant capability defaults off and reloads by declaration [ui][npc]")
{
    NullRenderer renderer;
    UI::RmlBridge::ThemeFileInterface files;
    UI::Items::ItemGridCells cells = UI::Items::ItemGridCells(120);
    Rml::SetFileInterface(&files);
    Rml::SetRenderInterface(&renderer);
    REQUIRE(Rml::Initialise());
    const auto cwd = std::filesystem::current_path();
    std::filesystem::current_path(std::string(MU_RMLUI_DIR) + "/../../..");
    UI::RmlBridge::SetActiveThemeName("legacy");
    CHECK_FALSE(UI::RmlBridge::ThemeUsesCompactNpcShop());
    UI::RmlBridge::SetActiveThemeName("munolith");
    CHECK(UI::RmlBridge::ThemeUsesCompactNpcShop());
    UI::RmlBridge::SetActiveThemeName("modern");
    CHECK_FALSE(UI::RmlBridge::ThemeUsesCompactNpcShop());
    std::filesystem::current_path(cwd);
    Rml::Shutdown();
    Rml::SetRenderInterface(nullptr);
    Rml::SetFileInterface(nullptr);
}

TEST_CASE("counter flow fits complete stock and exposes full quantity controls [ui][npc]")
{
    NullRenderer renderer;
    UI::RmlBridge::ThemeFileInterface files;
    UI::Items::ItemGridCells cells = UI::Items::ItemGridCells(120);
    Rml::SetFileInterface(&files);
    Rml::SetRenderInterface(&renderer);
    REQUIRE(Rml::Initialise());
    REQUIRE(Rml::LoadFontFace(std::string(MU_RMLUI_DIR) + "/../../../fonts/DejaVuSans.ttf"));
    auto* context = Rml::CreateContext("counter-layout", {1920, 1080});
    auto constructor = context->CreateDataModel("npc_shop");
    UI::Items::RegisterItemGridCells(constructor);
    constructor.Bind("grid_cells", &cells);
    constructor.RegisterArray<std::vector<int>>();
    std::vector<int> rows;
    constructor.Bind("rows", &rows);
    float x = 0, y = 0, scale = 1, text = 9;
    constructor.Bind("root_x", &x); constructor.Bind("root_y", &y);
    constructor.Bind("root_scale", &scale); constructor.Bind("text_px", &text);
    Rml::String label = "Small Healing Potion", total = "12 items · Est. 1,200 Zen";
    for (const auto* field : {"title", "context_name", "context_price", "context_hint", "quantity_name"})
        constructor.Bind(field, &label);
    constructor.Bind("quantity_total", &total);
    bool valid = true;
    constructor.Bind("quantity_valid", &valid);
    bool repair = false, no = false, quantity = false;
    constructor.Bind("repair_visible", &repair);
    constructor.Bind("repair_active", &no); constructor.Bind("buy_pending", &no);
    constructor.Bind("highlight_visible", &no); constructor.Bind("quantity_open", &quantity);
    int one = 1, zero = 0;
    Rml::String countText = "4";
    constructor.Bind("quantity", &countText);
    constructor.Bind("highlight_column", &zero); constructor.Bind("highlight_row", &zero);
    constructor.Bind("highlight_width", &one); constructor.Bind("highlight_height", &one);
    for (const auto* event : {"counter_close", "npc_shop_repair_click", "npc_shop_repair_all_click",
                             "quantity_less", "quantity_more", "quantity_cancel", "quantity_buy", "quantity_key"})
        constructor.BindEventCallback(event, [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
    auto model = constructor.GetModelHandle();
    auto* doc = context->LoadDocument(std::string(MU_RMLUI_DIR) + "/themes/munolith/npc_shop.rml");
    REQUIRE(doc != nullptr);
    UI::RmlBridge::SlotPlacement{0.f, 0.f, 1.f}.Apply(doc, "panel");
    doc->Show();
    for (const int number : {1, 5, 15})
    {
        rows.resize(number); std::iota(rows.begin(), rows.end(), 0);
        cells.resize(number * 8);
        model.DirtyVariable("grid_cells");
        model.DirtyVariable("rows"); context->Update(); context->Update();
        auto* panel = doc->GetElementById("panel");
        auto* grid = doc->GetElementById("item_grid");
        CHECK(panel->GetBox().GetSize(Rml::BoxArea::Border).x == doctest::Approx(190));
        CHECK(grid->GetBox().GetSize(Rml::BoxArea::Content).y == doctest::Approx(number * 20));
        CHECK(grid->GetAbsoluteOffset().y == doctest::Approx(34));
        CHECK(panel->GetBox().GetSize(Rml::BoxArea::Border).y <= 429);
    }
    repair = true; quantity = true;
    model.DirtyVariable("repair_visible"); model.DirtyVariable("quantity_open");
    context->Update(); context->Update();
    auto* picker = doc->GetElementById("quantity_picker");
    REQUIRE(picker->IsVisible());
    CHECK(picker->GetAbsoluteOffset().x >= 0);
    CHECK(picker->GetAbsoluteOffset().x + picker->GetBox().GetSize(Rml::BoxArea::Border).x <= 190);
    Rml::ElementList buttons; picker->QuerySelectorAll(buttons, "button");
    REQUIRE(buttons.size() == 4);
    for (auto* button : buttons) CHECK(button->GetBox().GetSize(Rml::BoxArea::Border).y >= 17);
    context->UnloadDocument(doc);
    Rml::RemoveContext("counter-layout");
    Rml::Shutdown(); Rml::SetRenderInterface(nullptr);
}
