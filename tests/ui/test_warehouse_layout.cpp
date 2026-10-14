#include <doctest.h>
#include <RmlUi/Core.h>
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlPanelGeometry.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "UI/RmlBridge/ThemeFileInterface.h"
#include "UI/Inventory/ItemGridModel.h"
#include <filesystem>
#include <numeric>
#include <algorithm>

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

TEST_CASE("combined vault capability defaults off and reloads by declaration [ui][vault]")
{
    NullRenderer renderer;
    UI::RmlBridge::ThemeFileInterface files;
    Rml::SetFileInterface(&files);
    Rml::SetRenderInterface(&renderer);
    REQUIRE(Rml::Initialise());
    const auto cwd = std::filesystem::current_path();
    std::filesystem::current_path(std::string(MU_RMLUI_DIR) + "/../../..");
    UI::RmlBridge::SetActiveThemeName("legacy");
    CHECK_FALSE(UI::RmlBridge::ThemeUsesEmbeddedVault());
    UI::RmlBridge::SetActiveThemeName("munolith");
    CHECK(UI::RmlBridge::ThemeUsesEmbeddedVault());
    UI::RmlBridge::SetActiveThemeName("modern");
    CHECK_FALSE(UI::RmlBridge::ThemeUsesEmbeddedVault());
    std::filesystem::current_path(cwd);
    Rml::Shutdown();
    Rml::SetRenderInterface(nullptr);
    Rml::SetFileInterface(nullptr);
}

TEST_CASE("Warehouse owns its full grids, cells, counts and item target in one placed document [ui][vault]")
{
    NullRenderer renderer;
    UI::RmlBridge::ThemeFileInterface files;
    Rml::SetFileInterface(&files);
    Rml::SetRenderInterface(&renderer);
    REQUIRE(Rml::Initialise());
    REQUIRE(Rml::LoadFontFace(std::string(MU_RMLUI_DIR) + "/../../../fonts/DejaVuSans.ttf"));
    auto* context = Rml::CreateContext("vault-layout", {1920, 1080});
    auto constructor = context->CreateDataModel("storage");
    float text = 9;
    constructor.Bind("text_px", &text);
    bool extra = false, locked = false, expand = false;
    constructor.Bind("extra_visible", &extra);
    constructor.Bind("storage_locked", &locked);
    constructor.Bind("expand_visible", &expand);
    Rml::String label = "Storage", zen = "999,999,999";
    for (const auto* field : {"title", "extra_title", "fee_label", "fee_value", "insert_tooltip", "take_tooltip", "expand_tooltip"})
        constructor.Bind(field, &label);
    constructor.Bind("zen_text", &zen);
    UI::Items::RegisterItemGridCells(constructor);
    UI::Items::ItemGridCells baseCells(120), extraCells(120);
    baseCells[0].count = "12";
    baseCells[0].tint = "tint-durability-100";
    extraCells[0].drop = "drop-valid";
    constructor.Bind("grid_cells", &baseCells);
    constructor.Bind("grid_cells_extra", &extraCells);
    for (const auto* event :
         {"window_close", "storage_insert_click", "storage_take_click", "storage_lock_click", "storage_expand_click"})
        constructor.BindEventCallback(event, [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
    auto foregroundModel = constructor.GetModelHandle();
    const auto themePath = std::string(MU_RMLUI_DIR) + "/themes/munolith/";
    auto* foreground = context->LoadDocument(themePath + "storage.rml");
    REQUIRE(foreground);
    UI::RmlBridge::SlotPlacement{0.f, 0.f, 1.f}.Apply(foreground, "panel");
    foreground->Show();
    for (const bool owned : {false, true, false})
    {
        extra = owned;
        foregroundModel.DirtyVariable("extra_visible");
        context->Update();
        context->Update();
        auto* panel = foreground->GetElementById("panel");
        auto* base = foreground->GetElementById("item_grid");
        CHECK(panel->GetBox().GetSize(Rml::BoxArea::Border).x == doctest::Approx(owned ? 360 : 190));
        CHECK(base->GetAbsoluteOffset().x == doctest::Approx(15));
        CHECK(base->GetAbsoluteOffset().y == doctest::Approx(45));
        CHECK(base->GetBox().GetSize(Rml::BoxArea::Content) == Rml::Vector2f(160, 300));
        Rml::ElementList grids;
        foreground->QuerySelectorAll(grids, ".item-grid");
        REQUIRE(grids.size() == 2);
        CHECK(grids[0]->IsVisible());
        CHECK(grids[1]->IsVisible() == owned);
        for (auto* grid : grids)
        {
            if (!grid->IsVisible())
                continue;
            Rml::ElementList cells;
            grid->QuerySelectorAll(cells, ".item-cell");
            REQUIRE(cells.size() == 120);
            CHECK(grid->GetBox().GetSize(Rml::BoxArea::Content) == Rml::Vector2f(160, 300));
            for (auto* cell : cells)
                CHECK(cell->GetBox().GetSize(Rml::BoxArea::Border) == Rml::Vector2f(20, 20));
        }
        if (owned)
        {
            auto* extraGrid = foreground->GetElementById("item_grid_extra");
            REQUIRE(extraGrid);
            CHECK(extraGrid->GetAbsoluteOffset() == grids[1]->GetAbsoluteOffset());
            CHECK(extraGrid->GetAbsoluteOffset().x == doctest::Approx(185));
        }
        for (const auto* id : {"btn_insert", "btn_take", "btn_lock", "vault_close"})
        {
            auto* control = foreground->GetElementById(id);
            REQUIRE(control);
            CHECK(control->IsVisible());
            CHECK(control->GetAbsoluteOffset().x >= 0);
            CHECK(control->GetAbsoluteOffset().x + control->GetBox().GetSize(Rml::BoxArea::Border).x <=
                  panel->GetBox().GetSize(Rml::BoxArea::Border).x);
        }
    }
    auto* target = foreground->GetElementById("item_view");
    REQUIRE(target);
    CHECK(target->IsVisible());
    CHECK(target->GetBox().GetSize(Rml::BoxArea::Content) == Rml::Vector2f(230, 465));
    Rml::ElementList counts;
    foreground->GetElementById("item_grid_counts")->QuerySelectorAll(counts, ".item-count-text");
    std::erase_if(counts, [](const auto* span) { return !span->IsVisible(); });
    REQUIRE(counts.size() == 1);
    CHECK(counts[0]->GetInnerRML() == "12");
    CHECK(foreground->QuerySelector(".tint-durability-100") != nullptr);
    UI::RmlBridge::SlotPlacement{200.f, 80.f, 1.5f}.Apply(foreground, "panel");
    context->Update();
    context->Update();
    // RmlUi updates paint transforms during render; Project()/DrawnBox() read those transforms.
    context->Render();
    Rml::Vector2f offset, size;
    auto* base = foreground->GetElementById("item_grid");
    REQUIRE(UI::RmlBridge::DrawnBox(*base, Rml::BoxArea::Border, offset, size));
    CHECK(offset.x == doctest::Approx(222.5f));
    CHECK(offset.y == doctest::Approx(147.5f));
    CHECK(size.x == doctest::Approx(240.f));
    CHECK(size.y == doctest::Approx(450.f));
    context->UnloadAllDocuments();
    Rml::RemoveContext("vault-layout");
    Rml::Shutdown();
    Rml::SetRenderInterface(nullptr);
    Rml::SetFileInterface(nullptr);
}
