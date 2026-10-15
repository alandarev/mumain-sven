#include "stdafx.h"
#include <doctest.h>
#include <RmlUi/Core.h>
#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/ElementUtilities.h>
#include <RmlUi/Core/ElementText.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "Data/GameConfig/GameConfig.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/HUD/MuHelperBar.h"
#include "UI/Inventory/MyInventory.h"
#include "UI/Inventory/ItemMng.h"
#include "UI/Character/CharacterInfoWindow.h"
#include <numeric>
#include "UI/Placement/WindowPlacement.h"
#include "UI/RmlBridge/RmlDocumentHints.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "UI/RmlBridge/RmlNativeText.h"
#include "UI/RmlBridge/RmlScaleInputs.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlTooltip.h"
#include "UI/RmlBridge/ThemeFileInterface.h"
#include "UI/Scaling/UITransform.h"
#include <filesystem>

namespace
{
namespace Tip = UI::RmlBridge::Tooltip;
struct NullRenderer : Rml::RenderInterface
{
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override
    {
        return 1;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
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
};
struct ModelLog : Rml::SystemInterface
{
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override
    {
        if (type <= Rml::Log::LT_WARNING)
            std::fprintf(stderr, "RML: %s\n", message.c_str());
        // Schema failures, not the upstream data-for shrink-order value warnings. Those remain
        // visible in stderr; rendered row identities/counts/content below independently verify shrink.
        if (type <= Rml::Log::LT_WARNING &&
            (message.find("variable name") != std::string::npos ||
             message.find("Could not find") != std::string::npos || message.find("expression") != std::string::npos ||
             message.find("member") != std::string::npos))
            errors.push_back(message);
        return true;
    }
    std::vector<std::string> errors;
};
TTF_Font* normal = nullptr;
TTF_Font* bold = nullptr;
struct Viewport
{
    int width, height, percent;
};
constexpr Viewport cases[]{{1024, 768, 100}, {800, 600, 150}, {1920, 1080, 75}};
struct Rect
{
    float left, top, width, height;
};
Rect Box(Rml::Element& e)
{
    Rml::Vector2f pos, size;
    REQUIRE(UI::RmlBridge::DrawnContentBox(e, pos, size));
    return {pos.x, pos.y, size.x, size.y};
}
const char* bless = "Jewel of Bless";
const char* description = "It is used to increase your item level up to 6";
const char* configHint = "Official MU Helper Setting";
const char* startHint = "Start Official MU Helper";
const char* stopHint = "Stop Official MU Helper";
} // namespace

// Same native metric contract, backed by real bundled TTF faces without a GPU renderer.
// The production model, row classes, measuring/placement and hint mechanism remain unchanged.
extern "C" int __wrap__ZN19CUIRenderTextSDLTtf10LineHeightEN2UI7Scaling8FontRoleE(UI::Scaling::FontRole role)
{
    const auto transform = UI::Scaling::GetActiveTransform();
    TTF_Font* font = role == UI::Scaling::FontRole::Bold ? bold : normal;
    REQUIRE(font);
    REQUIRE(TTF_SetFontSize(font, UI::Scaling::NativeTextPixelSize(role, transform)));
    return static_cast<int>(std::lround(TTF_GetFontHeight(font) / transform.scaleY));
}

extern "C" bool __wrap__Z10LoadBitmapPKwjjjbb(const wchar_t*, GLuint, GLuint, GLuint, bool, bool)
{
    return true; // Native assets need a GPU; actual view, input and grid geometry remain linked.
}

namespace
{
void CheckInventoryText(Rml::ElementDocument* doc);
}

class RmlViewLayoutFixture
{
public:
    RmlViewLayoutFixture()
        : cwd(std::filesystem::current_path()), theme(UI::RmlBridge::GetActiveThemeName()), width(WindowWidth),
          height(WindowHeight), percent(GameConfig::GetInstance().GetUIScalePercent()), mouseX(g_fWindowMouseX),
          mouseY(g_fWindowMouseY)
    {
        REQUIRE(RmlUiRuntime::Instance().m_Context == nullptr);
        std::filesystem::current_path(std::string(MU_RMLUI_DIR) + "/../../..");
        REQUIRE(TTF_Init());
        normal = TTF_OpenFont((std::string(MU_FONT_DIR) + "/DejaVuSans.ttf").c_str(), 14.f);
        bold = TTF_OpenFont((std::string(MU_FONT_DIR) + "/DejaVuSans-Bold.ttf").c_str(), 14.f);
        REQUIRE(normal);
        REQUIRE(bold);
        Rml::SetRenderInterface(&renderer);
        Rml::SetFileInterface(&files);
        Rml::SetSystemInterface(&log);
        REQUIRE(Rml::Initialise());
        REQUIRE(Rml::LoadFontFace(std::string(MU_FONT_DIR) + "/DejaVuSans.ttf"));
        REQUIRE(Rml::LoadFontFace(std::string(MU_FONT_DIR) + "/DejaVuSans-Bold.ttf"));
        REQUIRE(Rml::LoadFontFace(std::string(MU_FONT_DIR) + "/LiberationSans-Regular.ttf", true));
        context = Rml::CreateContext("native-view-layout", {1024, 768});
        REQUIRE(context);
        RmlUiRuntime::Instance().m_Context = context;
        UI::RmlBridge::SetActiveThemeName("munolith");
    }
    ~RmlViewLayoutFixture()
    {
        inventory.reset();
        character.reset();
        helper.reset();
        UI::Placement::Release();
        Tip::Hide();
        RmlUiRuntime::Instance().m_Context = nullptr;
        UI::RmlBridge::ReloadAllThemedDocuments(); // Release static shared views before owned context dies.
        UI::RmlBridge::SetActiveThemeName(theme);
        Rml::Shutdown();
        Rml::SetSystemInterface(nullptr);
        Rml::SetFileInterface(nullptr);
        Rml::SetRenderInterface(nullptr);
        TTF_CloseFont(normal);
        TTF_CloseFont(bold);
        normal = bold = nullptr;
        TTF_Quit();
        GameConfig::GetInstance().SetUIScalePercent(percent);
        WindowWidth = width;
        WindowHeight = height;
        g_fWindowMouseX = mouseX;
        g_fWindowMouseY = mouseY;
        std::filesystem::current_path(cwd);
    }
    void Configure(const char* selected, Viewport v)
    {
        Tip::Hide();
        UI::RmlBridge::SetActiveThemeName(selected);
        WindowWidth = v.width;
        WindowHeight = v.height;
        GameConfig::GetInstance().SetUIScalePercent(v.percent);
        context->SetDimensions({v.width, v.height});
        context->SetDensityIndependentPixelRatio(UI::Scaling::CompanionRatio(v.width, v.height));
        UI::RmlBridge::ApplyScaleInputs(context);
        UI::RmlBridge::ReloadAllThemedDocuments();
        Refresh();
    }
    void Refresh()
    {
        context->Update();
        context->Update();
        context->Render();
    }
    Rml::Element* TooltipPanel()
    {
        for (int i = 0; i < context->GetNumDocuments(); ++i)
            if (auto* panel = context->GetDocument(i)->GetElementById("tooltip_panel"))
                return panel;
        return nullptr;
    }
    void NoErrors()
    {
        for (const auto& e : log.errors)
            CAPTURE(e);
        CHECK(log.errors.empty());
    }
    void CreateHelper()
    {
        REQUIRE(RmlUiRuntime::Instance().IsCreated());
        helper = std::make_unique<mu::ui::window::CMuHelperBar>();
        REQUIRE(helper->Create(&manager));
        REQUIRE(RmlUiRuntime::Instance().IsCreated());
        helper->BuildRmlUi();
        REQUIRE(helper->m_RmlView.Document());
    }
    Rml::ElementDocument* Helper(bool active)
    {
        auto& m = helper->m_RmlView.GetModel();
        m.mapName = "Lorencia";
        m.coordinateText = "(135, 137)";
        m.positionText = "Lorencia (135, 137)";
        m.muHelperActive = active;
        m.configHint = configHint;
        m.startHint = startHint;
        m.stopHint = stopHint;
        for (const char* field : {"map_name", "coordinate_text", "position_text", "mu_helper_active", "config_hint",
                                  "start_hint", "stop_hint"})
            helper->m_RmlView.MarkDirty(field);
        helper->SyncDocVisibility(true);
        Refresh();
        UI::Placement::Arrange();
        Refresh();
        return helper->m_RmlView.Document();
    }
    void Move(float x, float y)
    {
        g_fWindowMouseX = x;
        g_fWindowMouseY = y;
        context->ProcessMouseMove(static_cast<int>(x), static_cast<int>(y), 0);
        Refresh();
        UI::RmlBridge::DocumentHints::Update(*context);
        Refresh();
    }
    void Click(Rml::Element& element, bool config)
    {
        helper->m_bRmlConfigClicked = helper->m_bRmlToggleClicked = false;
        const auto r = Box(element);
        Move(r.left + r.width / 2, r.top + r.height / 2);
        context->ProcessMouseButtonDown(0, 0);
        context->ProcessMouseButtonUp(0, 0);
        CHECK(helper->m_bRmlConfigClicked == config);
        CHECK(helper->m_bRmlToggleClicked == !config);
        helper->m_bRmlConfigClicked = helper->m_bRmlToggleClicked = false;
        // Do not call native Update(): queued game actions are deliberately not executed.
    }
    void CreateInventoryGroup()
    {
        using namespace mu::ui::window;
        CreateHelper();
        Helper(false); // Real reserved top HUD participates in the same safe-area fit.
        inventory = std::make_unique<CMyInventory>();
        inventory->m_pNewInventoryCtrl = new CInventoryCtrl;
        REQUIRE(inventory->m_pNewInventoryCtrl->Create(STORAGE_TYPE::INVENTORY, &items, inventory.get(), 8, 8,
                                                       MAX_EQUIPMENT_INDEX));
        inventory->BuildRmlUi();
        REQUIRE(inventory->GetPlacedDocument());
        inventory->CObject::Show(true);
        inventory->GetPlacedDocument()->Show();
        character = std::make_unique<CCharacterInfoWindow>();
        REQUIRE(character->Create(&manager));
        REQUIRE(character->GetPlacedDocument());
        character->CObject::Show(true);
        character->GetPlacedDocument()->Show();
        RegisterPane("character", character.get());
        RegisterPane("inventory", inventory.get());
    }
    void RegisterPane(const char* name, mu::ui::window::CObject* owner)
    {
        UI::Placement::RegisterParticipant(
            name, {[] { return true; },
                   [owner]
                   {
                       auto* doc = owner->GetPlacedDocument();
                       auto* root = doc ? doc->GetElementById("panel") : nullptr;
                       const auto size = root ? root->GetBox().GetSize(Rml::BoxArea::Border) : Rml::Vector2f{};
                       return UI::Placement::PlacementParticipant::Size{size.x, size.y};
                   },
                   [owner](const UI::Placement::PlacementParticipant::Box* b)
                   {
                       if (b)
                           owner->PlaceInWorkspace(b->left, b->top, b->scale);
                       else
                           owner->LeaveWorkspace();
                       owner->SyncSlotPlacement(); // Production CSystem reattaches equal boxes after document reload.
                   }});
    }
    Rml::ElementDocument* Inventory(int owned, int variant)
    {
        auto& m = inventory->m_RmlView.GetModel();
        m.textPx = UI::Scaling::NativeTextPixelSize(UI::Scaling::FontRole::Normal,
                                                    UI::Scaling::TypographyUnitsTransform(WindowWidth, WindowHeight));
        m.title = "Inventory";
        m.setOptionLabel = "[Set option]";
        m.socketOptionLabel = "[Socket option]";
        m.goldText = variant == 2 ? "2,000,000,000" : "10,000,000";
        m.goldLabel = "Zen";
        m.capacityText = "Free space 24 / " + std::to_string(64 + 32 * owned);
        m.ownedExtensions = owned;
        m.rows.resize(8 + 4 * owned);
        std::iota(m.rows.begin(), m.rows.end(), 0);
        m.repairVisible = variant != 0;
        m.repairActive = variant == 2;
        m.myShopVisible = variant != 0;
        m.myShopLocked = variant == 2;
        m.expansionVisible = owned < 2;
        m.overflowVisible = owned == 2;
        m.gridCells.resize(64);
        m.gridCellsExtension1.resize(32);
        m.gridCellsExtension2.resize(32);
        m.gridCells[0].count = "3";
        m.gridCellsExtension1[0].count = "5";
        m.gridCellsExtension2[0].count = "7";
        m.gridCells[0].tint = "tint-durability-100";
        m.deliveryVisible = owned > 0;
        m.deliveryColumn = 2;
        m.deliveryRow = 9;
        m.deliveryWidth = m.deliveryHeight = 2;
        for (const char* name : {"text_px",
                                 "title",
                                 "set_option_label",
                                 "socket_option_label",
                                 "gold_text",
                                 "gold_label",
                                 "capacity_text",
                                 "owned_extensions",
                                 "rows",
                                 "repair_visible",
                                 "repair_active",
                                 "myshop_visible",
                                 "myshop_locked",
                                 "expansion_visible",
                                 "overflow_visible",
                                 "grid_cells",
                                 "grid_cells_extension_1",
                                 "grid_cells_extension_2",
                                 "delivery_visible",
                                 "delivery_column",
                                 "delivery_row",
                                 "delivery_width",
                                 "delivery_height"})
            inventory->m_RmlView.MarkDirty(name);
        inventory->GetPlacedDocument()->Show();
        character->GetPlacedDocument()->Show();
        Helper(false);
        Refresh();
        for (int i = 0; i < 6; ++i)
        {
            UI::Placement::Arrange();
            Refresh();
        }
        inventory->SyncNativeLayout();
        UI::Placement::PlacementParticipant::Box assignedBox;
        const bool placed = UI::Placement::SlotBox("inventory", assignedBox);
        std::fprintf(stderr, "Inventory layout %u %u %d placed%d %.2f %.2f %.2f %.2f scale%.4f class%d\n", WindowWidth,
                     WindowHeight, owned, placed, assignedBox.left, assignedBox.top, assignedBox.width,
                     assignedBox.height, assignedBox.scale,
                     inventory->GetPlacedDocument()->GetElementById("panel")->IsClassSet("slot-placed"));
        return inventory->GetPlacedDocument();
    }
    void CheckNativeGrid(Rml::ElementDocument* doc)
    {
        inventory->SyncNativeLayout();
        const auto b = Box(*doc->GetElementById("item_grid"));
        const auto& geometry = inventory->GetInventoryCtrl()->Geometry();
        CHECK(geometry.Left() == doctest::Approx(b.left));
        CHECK(geometry.Top() == doctest::Approx(b.top));
        CHECK(geometry.PitchX() * 8 == doctest::Approx(b.width));
        CHECK(geometry.PitchY() * 8 == doctest::Approx(b.height));
    }

    void CheckCells(Rml::ElementDocument* doc)
    {
        Rml::ElementList background;
        doc->QuerySelectorAll(background, ".grid .cell");
        Rml::ElementList cells;
        doc->QuerySelectorAll(cells, ".item-cell");
        std::erase_if(background, [](auto* e) { return !e->IsVisible(true); });
        std::erase_if(cells, [](auto* e) { return !e->IsVisible(true); });
        CHECK(cells.size() == 64 + 32 * inventory->m_RmlView.GetModel().ownedExtensions);
        CHECK(background.size() == cells.size());
        for (size_t i = 0; i < std::min(background.size(), cells.size()); ++i)
        {
            Rml::Vector2f p, z, q, w;
            REQUIRE(UI::RmlBridge::DrawnBox(*background[i], Rml::BoxArea::Border, p, z));
            REQUIRE(UI::RmlBridge::DrawnBox(*cells[i], Rml::BoxArea::Border, q, w));
            CHECK(p.x == doctest::Approx(q.x));
            CHECK(p.y == doctest::Approx(q.y));
            CHECK(z.x == doctest::Approx(w.x));
            CHECK(z.y == doctest::Approx(w.y));
        }
        for (const char* id : {"item_grid", "item_grid_extension_1", "item_grid_extension_2"})
            CheckExtension(doc, id);
    }
    void CheckExtension(Rml::ElementDocument* doc, const char* id)
    {
        auto* grid = doc->GetElementById(id);
        if (!grid->IsVisible(true))
            return;
        mu::ui::window::CInventoryCtrl control;
        const int rows = std::string_view(id) == "item_grid" ? 8 : 4;
        REQUIRE(control.Create(STORAGE_TYPE::INVENTORY, &items, inventory.get(), 8, rows));
        control.FollowGridPx(doc, id);
        const auto rect = Box(*grid);
        const auto& native = control.Geometry();
        CHECK(native.Left() == doctest::Approx(rect.left));
        CHECK(native.Top() == doctest::Approx(rect.top));
        CHECK(native.PitchX() * 8 == doctest::Approx(rect.width));
        CHECK(native.PitchY() * rows == doctest::Approx(rect.height));
        auto* counts = doc->GetElementById(std::string(id) + "_counts");
        REQUIRE(counts);
        const auto countRect = Box(*counts);
        CHECK(countRect.left == doctest::Approx(rect.left));
        CHECK(countRect.top == doctest::Approx(rect.top));
        CHECK(countRect.width == doctest::Approx(rect.width));
        CHECK(countRect.height == doctest::Approx(rect.height));
        auto* count = counts->QuerySelector(".item-count-text");
        REQUIRE(count);
        CHECK(count->IsVisible(true));
        CHECK(!count->GetInnerRML().empty());
    }
    void CheckSmallInventory(Rml::ElementDocument* doc)
    {
        UI::Placement::PlacementParticipant::Box assigned;
        REQUIRE(UI::Placement::SlotBox("inventory", assigned));
        // A smaller post-fit root than the native800 capture: text must keep its native floor.
        inventory->PlaceDocument(0.f, 0.f, 0.75f);
        Refresh();
        CheckInventoryText(doc);
        inventory->PlaceDocument(assigned.left, assigned.top, assigned.scale);
        Refresh();
    }
    void CheckStable(Rml::ElementDocument* doc)
    {
        const auto initial = Box(*doc->GetElementById("panel"));
        const auto image = Box(*doc->GetElementById("item_view"));
        CHECK(image.left < initial.left);
        CHECK(image.top < initial.top);
        CHECK(image.left + image.width > initial.left + initial.width);
        CHECK(image.top + image.height > initial.top + initial.height);
        for (int i = 0; i < 12; ++i)
        {
            UI::Placement::Update();
            UI::Placement::Arrange();
            Refresh();
            const auto current = Box(*doc->GetElementById("panel"));
            CHECK(current.left == doctest::Approx(initial.left));
            CHECK(current.top == doctest::Approx(initial.top));
            CHECK(current.width == doctest::Approx(initial.width));
            CHECK(current.height == doctest::Approx(initial.height));
        }
    }

    Rml::Context* context = nullptr;

private:
    std::filesystem::path cwd;
    std::string theme;
    int width, height, percent;
    float mouseX, mouseY;
    NullRenderer renderer;
    UI::RmlBridge::ThemeFileInterface files;
    ModelLog log;
    mu::ui::window::CManager manager;
    mu::ui::window::CItemMng items;
    std::unique_ptr<mu::ui::window::CMyInventory> inventory;
    std::unique_ptr<mu::ui::window::CCharacterInfoWindow> character;
    std::unique_ptr<mu::ui::window::CMuHelperBar> helper;
};

TEST_CASE("Actual Bless tooltip preserves native row metrics content colors and viewport fit [ui][tooltip][layout]")
{
    RmlViewLayoutFixture f;
    for (const char* theme : {"munolith", "legacy", "modern"})
        for (const auto v : cases)
        {
            CAPTURE(std::string(theme));
            CAPTURE(v.width);
            CAPTURE(v.height);
            CAPTURE(v.percent);
            f.Configure(theme, v);
            Tip::Config config;
            config.lines = {{bless, Tip::LineColor::Yellow, true}, {description}};
            config.textAlign = Tip::Config::TextAlign::Center;
            config.anchorX = v.width - 2.f;
            config.anchorY = v.height - 5.f;
            config.flipAnchorY = v.height - 10.f;
            config.centerHorizontally = true;
            Tip::Show(config);
            f.Refresh();
            auto* panel = f.TooltipPanel();
            REQUIRE(panel);
            REQUIRE(panel->GetOwnerDocument()->IsVisible());
            Rml::ElementList rows;
            for (int i = 0; i < panel->GetNumChildren(); ++i)
                if (panel->GetChild(i)->IsVisible(true))
                    rows.push_back(panel->GetChild(i));
            REQUIRE(rows.size() == 2);
            auto* title = rows[0];
            auto* body = rows[1];
            CHECK(title->GetInnerRML() == bless);
            CHECK(body->GetInnerRML() == description);
            const auto titleBox = Box(*title), bodyBox = Box(*body), panelBox = Box(*panel);
            const float textPx = UI::Scaling::NativeTextPixelSize(
                UI::Scaling::FontRole::Normal, UI::Scaling::TypographyUnitsTransform(v.width, v.height));
            // Modern intentionally authors 11dp instead of using the native-font-size binding.
            const float authoredPx =
                std::string_view(theme) == "modern" ? 11.f * f.context->GetDensityIndependentPixelRatio() : textPx;
            CHECK(title->GetComputedValues().font_size() == doctest::Approx(authoredPx));
            CHECK(body->GetComputedValues().font_size() == doctest::Approx(authoredPx));
            CHECK(titleBox.height >= textPx);
            CHECK(bodyBox.height >= textPx);
            CHECK(bodyBox.top >= titleBox.top + titleBox.height - .1f);
            CHECK(panelBox.height >= titleBox.height + bodyBox.height);
            CHECK(Rml::ElementUtilities::GetStringWidth(title, Rml::String(bless)) <= titleBox.width + 1.f);
            CHECK(Rml::ElementUtilities::GetStringWidth(body, Rml::String(description)) <= bodyBox.width + 1.f);
            CHECK(authoredPx >= 11.f);
            CHECK(panelBox.left >= -.1f);
            CHECK(panelBox.top >= -.1f);
            CHECK(panelBox.left + panelBox.width <= v.width + .1f);
            CHECK(panelBox.top + panelBox.height <= v.height + .1f);
            CHECK(panel->GetComputedValues().background_color().alpha >= 204);
            CHECK(title->GetComputedValues().color() == Rml::Colourb(255, 204, 26));
            CHECK(body->GetComputedValues().color() == Rml::Colourb(255, 255, 255));
            f.NoErrors();
        }
}

TEST_CASE("Actual helper idle and hover use one guarded shared hint across themes and reloads [ui][helper][layout]")
{
    RmlViewLayoutFixture f;
    f.CreateHelper();
    for (const char* theme : {"munolith", "legacy", "modern"})
        for (const auto v : cases)
        {
            CAPTURE(std::string(theme));
            CAPTURE(v.width);
            CAPTURE(v.height);
            CAPTURE(v.percent);
            f.Configure(theme, v);
            for (bool active : {false, true})
            {
                auto* doc = f.Helper(active);
                auto* panel = doc->GetElementById("panel");
                REQUIRE(panel);
                auto* config = doc->GetElementById("btn_config");
                auto* play = doc->GetElementById(active ? "btn_stop" : "btn_start");
                auto* hidden = doc->GetElementById(active ? "btn_start" : "btn_stop");
                REQUIRE(config);
                REQUIRE(play);
                REQUIRE(hidden);
                CHECK(play->IsVisible(true));
                CHECK_FALSE(hidden->IsVisible(true));
                f.Move(v.width - 1.f, v.height / 2.f); // Inventory-side pointer, not helper.
                auto* tip = f.TooltipPanel();
                CHECK((tip == nullptr || !tip->GetOwnerDocument()->IsVisible()));
                Rml::ElementList oldHints;
                doc->GetElementsByClassName(oldHints, "tooltip");
                for (auto* hint : oldHints)
                    CHECK_FALSE(hint->IsVisible(true));
                const auto readout = Box(*doc->GetElementById("text"));
                const auto control = Box(*config), toggle = Box(*play);
                CHECK(readout.left + readout.width <= control.left + .1f);
                CHECK(control.left + control.width <= toggle.left + .1f);
                for (auto* button : {config, play})
                {
                    const char* text = button == config ? configHint : (active ? stopHint : startHint);
                    CHECK(button->GetAttribute<Rml::String>("data-hint", "") == text);
                    const auto r = Box(*button);
                    f.Move(r.left + r.width / 2, r.top + r.height / 2);
                    tip = f.TooltipPanel();
                    REQUIRE(tip);
                    REQUIRE(tip->GetOwnerDocument()->IsVisible());
                    Rml::ElementList rows;
                    for (int i = 0; i < tip->GetNumChildren(); ++i)
                        if (tip->GetChild(i)->IsVisible(true))
                            rows.push_back(tip->GetChild(i));
                    REQUIRE(rows.size() == 1);
                    CHECK(rows[0]->GetInnerRML() == text);
                    const auto hintBox = Box(*tip);
                    // Shared hints prefer above the actual trigger, or flip below at the top edge.
                    CHECK((hintBox.top + hintBox.height <= r.top + .1f || hintBox.top >= r.top + r.height - .1f));
                    f.Click(*button, button == config);
                    f.Move(v.width - 1.f, v.height / 2.f);
                    CHECK_FALSE(tip->GetOwnerDocument()->IsVisible());
                }
            }
            f.NoErrors();
        }
}

namespace
{
void CheckInventoryText(Rml::ElementDocument* doc)
{
    auto* panel = doc->GetElementById("panel");
    REQUIRE(panel);
    Rml::ElementList labels;
    doc->QuerySelectorAll(labels, "#title .native-text, .option-label, .capacity, #btn_myshop .native-text, .money "
                                  ".native-text, .expand .native-text");
    std::vector<Rect> inks;
    const auto bounds = Box(*panel);
    for (auto* label : labels)
    {
        if (!label->IsVisible(true))
            continue;
        const auto box = Box(*label);
        const float layoutWidth = label->GetBox().GetSize(Rml::BoxArea::Content).x;
        const float scale = layoutWidth > 0 ? box.width / layoutWidth : 1.f;
        const float font = label->GetComputedValues().font_size() * scale;
        CHECK(font >= 11.f - .1f);
        const auto text = label->GetInnerRML();
        float longest = 0.f;
        int lineCount = 0;
        Rml::String rendered;
        for (int i = 0; i < label->GetNumChildren(); ++i)
            if (auto* node = dynamic_cast<Rml::ElementText*>(label->GetChild(i)))
                for (const auto& line : node->GetLines())
                {
                    longest =
                        std::max(longest, static_cast<float>(Rml::ElementUtilities::GetStringWidth(label, line.text)));
                    rendered += line.text;
                    ++lineCount;
                }
        REQUIRE(lineCount > 0);
        const auto withoutSpaces = [](Rml::String value)
        {
            std::erase_if(value, [](char c) { return std::isspace(static_cast<unsigned char>(c)); });
            return value;
        };
        CHECK(withoutSpaces(rendered) == withoutSpaces(text)); // Wrap, never truncate/drop suffixes.
        const Rect ink{box.left, box.top, longest * scale, font * 1.2f * lineCount};
        if (auto* button = label->GetParentNode(); button && button->GetTagName() == "button")
        {
            const auto target = Box(*button);
            CHECK(ink.left >= target.left - .1f);
            CHECK(ink.top >= target.top - .1f);
            CHECK(ink.left + ink.width <= target.left + target.width + .1f);
            CHECK(ink.top + ink.height <= target.top + target.height + .1f);
        }
        CAPTURE(text);
        CHECK(ink.left >= bounds.left - .1f);
        CHECK(ink.left + ink.width <= bounds.left + bounds.width + .1f);
        CHECK(ink.top >= bounds.top - .1f);
        CHECK(ink.top + ink.height <= bounds.top + bounds.height + .1f);
        for (const auto& other : inks)
            CHECK((ink.left + ink.width <= other.left + .1f || other.left + other.width <= ink.left + .1f ||
                   ink.top + ink.height <= other.top + .1f || other.top + other.height <= ink.top + .1f));
        inks.push_back(ink);
    }
}
} // namespace

TEST_CASE("Actual fitted inventory preserves readable header footer and native grid through capacity changes "
          "[ui][inventory][layout]")
{
    RmlViewLayoutFixture f;
    f.CreateInventoryGroup();
    for (const auto v : cases)
    {
        f.Configure("munolith", v);
        for (int owned : {0, 1, 2, 0})
            for (int variant : {0, 1, 2})
            {
                CAPTURE(v.width);
                CAPTURE(v.height);
                CAPTURE(v.percent);
                CAPTURE(owned);
                CAPTURE(variant);
                auto* doc = f.Inventory(owned, variant);
                auto* panel = doc->GetElementById("panel");
                REQUIRE(panel);
                for (const char* selector : {"#title .native-text", "#set_option", "#socket_option", ".capacity",
                                             ".money .amount", ".money .zen"})
                {
                    auto* text = doc->QuerySelector(selector);
                    REQUIRE(text);
                    CHECK(text->IsVisible(true));
                }
                CHECK(doc->GetElementById("btn_repair")->IsVisible(true) == (variant != 0));
                CHECK(doc->GetElementById("btn_myshop")->IsVisible(true) == (variant != 0));
                Rml::ElementList choices;
                doc->GetElementsByClassName(choices, "expand");
                CHECK(std::count_if(choices.begin(), choices.end(), [](auto* e) { return e->IsVisible(true); }) == 1);
                CheckInventoryText(doc);
                f.CheckSmallInventory(doc);
                f.CheckNativeGrid(doc);
                f.CheckCells(doc);
                f.CheckStable(doc);
                f.NoErrors();
            }
    }
}
