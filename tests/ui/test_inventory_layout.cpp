#include "stdafx.h"
#include <doctest.h>
#include <RmlUi/Core.h>
#include "UI/Inventory/ItemGridModel.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "UI/RmlBridge/RmlPanelGeometry.h"
#include "UI/RmlBridge/ThemeFileInterface.h"
#include <algorithm>
#include <numeric>
#include <filesystem>

namespace
{
struct NullRenderer : Rml::RenderInterface
{
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override { return 1; }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override { return 0; }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override { return 1; }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};

class InventoryFixture
{
public:
    InventoryFixture()
    {
        Rml::SetFileInterface(&files);
        Rml::SetRenderInterface(&renderer);
        REQUIRE(Rml::Initialise());
        REQUIRE(Rml::LoadFontFace(std::string(MU_FONT_DIR) + "/DejaVuSans.ttf"));
        context = Rml::CreateContext("inventory-layout", {1920, 1080});
        auto c = context->CreateDataModel("my_inventory");
        Bind(c);
        model = c.GetModelHandle();
    }
    ~InventoryFixture()
    {
        Rml::Shutdown();
        Rml::SetRenderInterface(nullptr);
        Rml::SetFileInterface(nullptr);
    }
    void Load(const char* theme)
    {
        if (document != nullptr) document->Close();
        const std::string base = MU_RMLUI_DIR;
        const auto themed = base + "/themes/" + theme + "/my_inventory.rml";
        const auto path = std::filesystem::exists(themed) ? themed : base + "/my_inventory.rml";
        document = context->LoadDocument(path);
        REQUIRE(document != nullptr);
        document->Show();
    }
    void Refresh(float scale)
    {
        UI::RmlBridge::SlotPlacement{200.f, 80.f, scale}.Apply(document, "panel");
        model.DirtyAllVariables();
        context->Update();
        context->Update();
        context->Render();
    }
    Rml::Context* context = nullptr;
    Rml::ElementDocument* document = nullptr;
    Rml::DataModelHandle model;
    UI::Items::ItemGridCells cells = UI::Items::ItemGridCells(64);
    UI::Items::ItemGridCells extension1 = UI::Items::ItemGridCells(32), extension2 = UI::Items::ItemGridCells(32);
    std::vector<int> rows;
    int owned = 0;
    int setHover = 0, socketHover = 0, setLeave = 0, socketLeave = 0;

private:
    void Bind(Rml::DataModelConstructor& c)
    {
        c.Bind("text_px", &textPx);
        for (const char* field : {"title", "gold_text", "gold_tier", "capacity_text", "gold_label", "repair_tooltip",
             "myshop_tooltip", "exit_tooltip", "expand_tooltip", "set_option_label", "socket_option_label"})
            c.Bind(field, &label);
        for (const char* field : {"repair_visible", "myshop_visible", "myshop_mode_open", "set_option_active",
             "socket_option_active", "delivery_visible", "expansion_visible"}) c.Bind(field, &yes);
        for (const char* field : {"repair_active", "myshop_locked", "overflow_visible", "gold_gained"}) c.Bind(field, &no);
        UI::Items::RegisterItemGridCells(c);
        c.Bind("grid_cells", &cells);
        c.Bind("grid_cells_extension_1", &extension1);
        c.Bind("grid_cells_extension_2", &extension2);
        c.RegisterArray<std::vector<int>>();
        c.Bind("rows", &rows);
        c.Bind("owned_extensions", &owned);
        c.RegisterArray<std::vector<Rml::String>>();
        c.Bind("slot_states", &slots);
        c.Bind("delivery_column", &deliveryColumn);
        c.Bind("delivery_row", &deliveryRow);
        c.Bind("delivery_width", &deliveryWidth);
        c.Bind("delivery_height", &deliveryHeight);
        for (const char* name : {"window_close", "my_inventory_exit_click", "my_inventory_repair_click",
             "my_inventory_myshop_click", "my_inventory_expand_click", "my_inventory_overflow_click"})
            c.BindEventCallback(name, [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
        c.BindEventCallback("my_inventory_set_option_hover", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { ++setHover; });
        c.BindEventCallback("my_inventory_socket_option_hover", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { ++socketHover; });
        c.BindEventCallback("my_inventory_set_option_unhover", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { ++setLeave; });
        c.BindEventCallback("my_inventory_socket_option_unhover", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { ++socketLeave; });
    }
    NullRenderer renderer;
    UI::RmlBridge::ThemeFileInterface files;
    float textPx = 12.f;
    Rml::String label = "State";
    bool yes = true, no = false;
    int deliveryColumn = 2, deliveryRow = 10, deliveryWidth = 2, deliveryHeight = 2;
    std::vector<Rml::String> slots = std::vector<Rml::String>(16);
};
}

TEST_CASE("Embedded inventory preserves cells counts drop and merchant feedback across fitted placement [ui][inventory]")
{
    InventoryFixture f;
    f.cells[0].count = "3";
    f.cells[1].tint = "tint-durability-100";
    f.extension1[0].count = "7";
    f.extension1[2].drop = "drop-valid";
    f.extension2[3].drop = "drop-invalid";
    f.Load("munolith");
    for (int owned : {0, 1, 2, 0})
    {
        f.owned = owned;
        f.rows.resize(8 + 4 * owned);
        std::iota(f.rows.begin(), f.rows.end(), 0);
        for (float scale : {0.75f, 1.f, 1.5f})
        {
            f.Refresh(scale);
            Rml::ElementList cells;
            f.document->QuerySelectorAll(cells, ".item-cell");
            CHECK(cells.size() == 128); // Hidden extension DOM remains available for later unlocks.
            size_t visibleCells = 0;
            for (const char* id : {"item_grid", "item_grid_extension_1", "item_grid_extension_2"})
            {
                auto* grid = f.document->GetElementById(id);
                if (!grid->IsVisible()) continue;
                Rml::ElementList drawnCells;
                grid->QuerySelectorAll(drawnCells, ".item-cell");
                visibleCells += drawnCells.size();
            }
            CHECK(visibleCells == 64 + 32 * owned);
            REQUIRE(f.document->GetElementById("item_view"));
            CHECK(f.document->GetElementById("item_view")->IsVisible());
            CHECK(f.document->QuerySelector(".tint-durability-100") != nullptr);
            CHECK(f.document->GetElementById("item_grid_extension_1")->IsVisible() == (owned >= 1));
            CHECK(f.document->GetElementById("item_grid_extension_2")->IsVisible() == (owned >= 2));
            auto* count = f.document->GetElementById("item_grid_counts")->QuerySelector(".item-count-text");
            REQUIRE(count);
            CHECK(count->GetInnerRML() == "3");
            if (owned >= 1)
            {
                CHECK(f.document->QuerySelector(".drop-valid")->IsVisible());
                Rml::Vector2f offset, size;
                auto* feedback = f.document->QuerySelector(".merchant-delivery");
                REQUIRE(UI::RmlBridge::DrawnBox(*feedback, Rml::BoxArea::Border, offset, size));
                CHECK(offset.x == doctest::Approx(200.f + 55.f * scale));
                CHECK(offset.y == doctest::Approx(80.f + 245.f * scale));
                CHECK(size.x == doctest::Approx(40.f * scale));
                CHECK(size.y == doctest::Approx(40.f * scale));
            }
        }
    }
    Rml::Dictionary parameters;
    for (const char* id : {"set_option", "socket_option"})
    {
        auto* control = f.document->GetElementById(id);
        REQUIRE(control);
        CHECK(control->IsVisible());
        CHECK(control->IsClassSet("active"));
        control->DispatchEvent(Rml::EventId::Mouseover, parameters);
        control->DispatchEvent(Rml::EventId::Mouseout, parameters);
    }
    CHECK(f.setHover == 1);
    CHECK(f.socketHover == 1);
    CHECK(f.setLeave == 1);
    CHECK(f.socketLeave == 1);
    REQUIRE(f.document->GetElementById("option_tooltip_anchor"));
}

TEST_CASE("Lucky item compact threshold preserves native grid count target and readable text [ui][inventory][placement]")
{
    InventoryFixture f;
    auto c = f.context->CreateDataModel("lucky_item");
    float textPx = 9.f;
    bool mixVisible = true;
    Rml::String title = "Lucky item", hint = "Combine";
    UI::Items::ItemGridCells cells(32);
    cells[0].count = "5";
    struct RewardLine { Rml::String text, color; };
    std::vector<RewardLine> lines{{"Review reward before combining", "#ffffff"}};
    auto line = c.RegisterStruct<RewardLine>();
    line.RegisterMember("text", &RewardLine::text);
    line.RegisterMember("color", &RewardLine::color);
    c.RegisterArray<std::vector<RewardLine>>();
    c.Bind("text_lines", &lines);
    UI::Items::RegisterItemGridCells(c);
    c.Bind("grid_cells", &cells);
    c.Bind("text_px", &textPx);
    c.Bind("title", &title);
    c.Bind("mix_tooltip", &hint);
    c.Bind("mix_visible", &mixVisible);
    for (const char* name : {"window_close", "lucky_item_mix_click"})
        c.BindEventCallback(name, [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
    auto* doc = f.context->LoadDocument(std::string(MU_RMLUI_DIR) + "/themes/munolith/lucky_item.rml");
    REQUIRE(doc);
    doc->Show();
    for (float scale : {0.8f, 0.9f, 1.2f, 0.7f})
    {
        UI::RmlBridge::SlotPlacement{200.f, 80.f, scale}.Apply(doc, "panel");
        f.context->Update();
        f.context->Update();
        f.context->Render();
        const bool compact = scale < 0.9f;
        auto* panel = doc->GetElementById("panel");
        CHECK(panel->IsClassSet("compact-scale") == compact);
        Rml::Vector2f gridOffset, gridSize, countOffset, countSize, targetOffset, targetSize;
        REQUIRE(UI::RmlBridge::DrawnBox(*doc->GetElementById("item_grid"), Rml::BoxArea::Border, gridOffset, gridSize));
        REQUIRE(UI::RmlBridge::DrawnBox(*doc->GetElementById("item_grid_counts"), Rml::BoxArea::Border, countOffset, countSize));
        REQUIRE(UI::RmlBridge::DrawnBox(*doc->GetElementById("item_view"), Rml::BoxArea::Border, targetOffset, targetSize));
        CHECK(gridOffset.y == doctest::Approx(80.f + (compact ? 55.f : 38.f) * scale));
        CHECK(countOffset.x == doctest::Approx(gridOffset.x));
        CHECK(countOffset.y == doctest::Approx(gridOffset.y));
        CHECK(countSize.x == doctest::Approx(gridSize.x));
        CHECK(countSize.y == doctest::Approx(gridSize.y));
        CHECK(targetOffset.x < gridOffset.x);
        CHECK(targetOffset.y < gridOffset.y);
        CHECK(targetOffset.x + targetSize.x > gridOffset.x + gridSize.x);
        CHECK(targetOffset.y + targetSize.y > gridOffset.y + gridSize.y);
        auto* label = doc->QuerySelector("#title .native-text");
        REQUIRE(label);
        CHECK(label->GetComputedValues().font_size() == doctest::Approx(compact ? 11.f : 14.f));
        CHECK(doc->QuerySelector(".item-count-text")->GetInnerRML() == "5");
    }
}
