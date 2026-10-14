#include "UI/RmlBridge/ThemeFileInterface.h"
#include "UI/RmlBridge/RmlPanelGeometry.h"
#include "UI/Inventory/ItemGridModel.h"
#include <doctest.h>
#include <RmlUi/Core.h>
#include <vector>
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

struct Line
{
    Rml::String text, kind;
    int index = 0;
    bool selected = false;
};

struct ForgeFixture
{
    NullRenderer renderer;
    UI::RmlBridge::ThemeFileInterface files;
    UI::Items::ItemGridCells cells = UI::Items::ItemGridCells(32);
    Rml::Context* context = nullptr;
    Rml::ElementDocument* doc = nullptr;
    Rml::DataModelHandle model;
    float zero = 0, scale = 1, textPx = 9;
    bool yes = true, no = false, locked = false, visible = true;
    Rml::String label = "Combination", cost = "Required Zen: 2,000,000";
    std::vector<Line> empty, descriptions, sockets;
    int selected = -1;

    ForgeFixture()
    {
        Rml::SetFileInterface(&files);
        Rml::SetRenderInterface(&renderer);
        REQUIRE(Rml::Initialise());
        REQUIRE(Rml::LoadFontFace(std::string(MU_RMLUI_DIR) + "/../../../fonts/DejaVuSans.ttf"));
        context = Rml::CreateContext("crafting-layout", {1920, 1080});
        auto c = context->CreateDataModel("mix_inventory");
        UI::Items::RegisterItemGridCells(c);
        c.Bind("grid_cells", &cells);
        BindFacts(c);
        BindLines(c);
        c.BindEventCallback("window_close", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
        c.BindEventCallback("mix_inventory_mix_click",
                            [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
        c.BindEventCallback("mix_inventory_select_socket",
                            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                            { selected = args.at(0).Get<int>(-1); });
        model = c.GetModelHandle();
        doc = context->LoadDocument(std::string(MU_RMLUI_DIR) + "/themes/munolith/mix_inventory.rml");
        REQUIRE(doc != nullptr);
        UI::RmlBridge::SlotPlacement{0.f, 0.f, 1.f}.Apply(doc, "panel");
        doc->Show();
        Update();
    }

    ~ForgeFixture()
    {
        Rml::Shutdown();
        Rml::SetRenderInterface(nullptr);
        Rml::SetFileInterface(nullptr);
    }

    void BindFacts(Rml::DataModelConstructor& c)
    {
        c.Bind("root_x", &zero);
        c.Bind("root_y", &zero);
        c.Bind("root_scale", &scale);
        c.Bind("text_px", &textPx);
        c.Bind("mix_locked", &locked);
        c.Bind("mix_visible", &visible);
        for (const auto* name :
             {"show_recipe", "recipe_ready", "show_success_rate", "show_required_zen", "show_socket_list"})
            c.Bind(name, &yes);
        for (const auto* name :
             {"show_recipe_line2", "show_prediction", "success_boosted", "show_tax_rate", "show_socket_prompt"})
            c.Bind(name, &no);
        for (const auto* name : {"title", "recipe_line1", "recipe_line2", "prediction_text", "success_rate_text",
                                 "tax_rate_text", "socket_prompt_text", "mix_tooltip"})
            c.Bind(name, &label);
        c.Bind("required_zen_text", &cost);
    }

    void BindLines(Rml::DataModelConstructor& c)
    {
        auto line = c.RegisterStruct<Line>();
        line.RegisterMember("text", &Line::text);
        line.RegisterMember("kind", &Line::kind);
        line.RegisterMember("index", &Line::index);
        line.RegisterMember("selected", &Line::selected);
        c.RegisterArray<std::vector<Line>>();
        for (const auto* name : {"source_lines", "status_lines", "advice_lines"})
            c.Bind(name, &empty);
        for (int i = 0; i < 20; ++i)
            descriptions.push_back({"A long crafting warning that must remain reachable without overlapping the native "
                                    "material grid or the action row.",
                                    "warning"});
        for (int i = 0; i < 5; ++i)
            sockets.push_back({"Socket option " + std::to_string(i + 1), "", i});
        c.Bind("description_lines", &descriptions);
        c.Bind("socket_lines", &sockets);
    }

    void Update()
    {
        context->Update();
        context->Update();
    }
};
} // namespace

TEST_CASE("Compact forge bounds long recipe and socket flow independently of native cells and actions [ui][crafting]")
{
    ForgeFixture f;
    auto* grid = f.doc->GetElementById("item_grid");
    auto* pane = f.doc->GetElementById("forge_content");
    auto* mix = f.doc->GetElementById("btn_mix");
    REQUIRE(grid);
    REQUIRE(pane);
    REQUIRE(mix);
    CHECK(grid->GetBox().GetSize(Rml::BoxArea::Content).x == doctest::Approx(160));
    CHECK(grid->GetBox().GetSize(Rml::BoxArea::Content).y == doctest::Approx(80));
    CHECK(grid->GetAbsoluteOffset().y == doctest::Approx(44));
    CHECK(pane->GetAbsoluteOffset().y >= grid->GetAbsoluteOffset().y + 80);
    CHECK(pane->GetAbsoluteOffset().y + pane->GetClientHeight() < mix->GetAbsoluteOffset().y);
    CHECK(pane->GetScrollHeight() > pane->GetClientHeight());
    pane->SetScrollTop(pane->GetScrollHeight());
    f.Update();
    CHECK(pane->GetScrollTop() > 0);
    Rml::ElementList rows;
    f.doc->QuerySelectorAll(rows, ".socket-row");
    std::erase_if(rows, [](auto* row) { return !row->IsVisible(); });
    REQUIRE(rows.size() == 5);
    rows.back()->DispatchEvent("click", {});
    CHECK(f.selected == 4);
    f.locked = true;
    f.model.DirtyVariable("mix_locked");
    f.Update();
    CHECK(mix->HasAttribute("disabled"));
    for (auto* row : rows)
        CHECK(row->HasAttribute("disabled"));
    f.visible = false;
    f.model.DirtyVariable("mix_visible");
    f.Update();
    CHECK_FALSE(mix->IsVisible());
    CHECK(grid->IsVisible());
}
