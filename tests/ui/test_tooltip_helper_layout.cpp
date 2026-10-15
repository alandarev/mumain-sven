#include "stdafx.h"
#include "HeadlessCaptureRenderer.h"
#include "UI/RmlBridge/RmlRenderTarget.h"
#include "UI/RmlBridge/RmlRuntimeHooks.h"
#include "UI/RmlBridge/RmlPanelGeometry.h"
#include <RmlUi/Core/FontMetrics.h>
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
#include "UI/NPCs/NPCShop.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Events/DoppelGangerWindow.h"
#include "UI/Inventory/CursorItemLayer.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/ItemMng.h"
#include "UI/Character/CharacterInfoWindow.h"
#include "UI/Character/Armory/ArmoryController.h"
#include "UI/HUD/ChatLogWindow.h"
#include "App/Control/ControlCommands.h"
#include "json.hpp"
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

extern int LoadingWorld;

namespace
{
HeadlessCaptureRenderer captureRenderer;
int rmlDrawCalls = 0;
std::map<std::wstring, std::pair<int, int>> savedPositions;
namespace Tip = UI::RmlBridge::Tooltip;
struct NullRenderer : Rml::RenderInterface
{
    struct Mesh
    {
        std::vector<Rml::Vertex> vertices;
        std::vector<int> indices;
    };
    std::map<Rml::CompiledGeometryHandle, Mesh> meshes;
    std::vector<Mesh> paint;
    bool tracePaint = false;
    Rml::CompiledGeometryHandle nextMesh = 0;
    Rml::Matrix4f transform = Rml::Matrix4f::Identity();
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                Rml::Span<const int> indices) override
    {
        meshes[++nextMesh] = {{vertices.begin(), vertices.end()}, {indices.begin(), indices.end()}};
        return nextMesh;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle) override
    {
        ++rmlDrawCalls;
        if (!tracePaint)
            return;
        Mesh mesh = meshes.at(handle);
        for (auto& vertex : mesh.vertices)
        {
            const auto position = vertex.position + translation;
            const auto projected = transform * Rml::Vector4f(position.x, position.y, 0.f, 1.f);
            vertex.position = {projected.x / projected.w, projected.y / projected.w};
        }
        paint.push_back(std::move(mesh));
    }
    void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override
    {
        meshes.erase(handle);
    }
    void SetTransform(const Rml::Matrix4f* value) override
    {
        transform = value ? *value : Rml::Matrix4f::Identity();
    }
    // Observe submitted material triangles through the engine's actual render path, not panel boxes.
    int ChatBackdropPaints(Rml::Vector2f first, Rml::Vector2f second) const
    {
        const auto cross = [](Rml::Vector2f a, Rml::Vector2f b) { return a.x * b.y - a.y * b.x; };
        int count = 0;
        for (const auto& mesh : paint)
        {
            bool coversFirst = false, coversSecond = false;
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
            {
                const auto& a = mesh.vertices[mesh.indices[i]];
                const auto& b = mesh.vertices[mesh.indices[i + 1]];
                const auto& c = mesh.vertices[mesh.indices[i + 2]];
                if (a.colour.red != 16 * a.colour.alpha / 255 || a.colour.green != 25 * a.colour.alpha / 255 ||
                    a.colour.blue != 16 * a.colour.alpha / 255 || a.colour.alpha == 0)
                    continue;
                const auto covers = [&](Rml::Vector2f p)
                {
                    const float x = cross(b.position - a.position, p - a.position);
                    const float y = cross(c.position - b.position, p - b.position);
                    const float z = cross(a.position - c.position, p - c.position);
                    return !((x < 0.f || y < 0.f || z < 0.f) && (x > 0.f || y > 0.f || z > 0.f));
                };
                coversFirst |= covers(first);
                coversSecond |= covers(second);
            }
            if (coversFirst && coversSecond)
                ++count;
        }
        return count;
    }
    Rml::TextureHandle LoadTexture(Rml::Vector2i& size, const Rml::String& source) override
    {
        if (!UI::RmlBridge::RenderTarget::IsSource(source))
            return 0;
        void* handle = UI::RmlBridge::RenderTarget::Resolve(source, size.x, size.y);
        CHECK(handle != nullptr);
        return reinterpret_cast<Rml::TextureHandle>(handle);
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
    int elapsedReads = 0;
    double GetElapsedTime() override
    {
        ++elapsedReads;
        return Rml::SystemInterface::GetElapsedTime();
    }
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

extern "C" mu::IMuRenderer& __wrap__ZN2mu11GetRendererEv()
{
    return captureRenderer;
}

// Same native metric contract, backed by real bundled TTF faces without a GPU renderer.
// The production model, row classes, measuring/placement and hint mechanism remain unchanged.
// Isolate real drag/save callbacks from executable/live preference files.
extern "C" void __wrap__ZN10GameConfig17SetWindowPositionERKNSt7__cxx1112basic_stringIwSt11char_traitsIwESaIwEEEii(
    GameConfig*, const std::wstring& id, int x, int y)
{
    savedPositions[id] = {x, y};
}
extern "C" bool __wrap__ZNK10GameConfig17GetWindowPositionERKNSt7__cxx1112basic_stringIwSt11char_traitsIwESaIwEEERiS8_(
    const GameConfig*, const std::wstring& id, int& x, int& y)
{
    auto found = savedPositions.find(id);
    if (found == savedPositions.end())
        return false;
    x = found->second.first;
    y = found->second.second;
    return true;
}

extern "C" int __wrap__ZN19CUIRenderTextSDLTtf10LineHeightEN2UI7Scaling8FontRoleE(UI::Scaling::FontRole role)
{
    const auto transform = UI::Scaling::GetActiveTransform();
    TTF_Font* font = role == UI::Scaling::FontRole::Bold ? bold : normal;
    REQUIRE(font);
    REQUIRE(TTF_SetFontSize(font, UI::Scaling::NativeTextPixelSize(role, transform)));
    return static_cast<int>(std::lround(TTF_GetFontHeight(font) / transform.scaleY));
}

int cursorDrawCalls = 0;
extern "C" void __wrap__ZN2mu2ui6window11CPickedItem8Render3DEv(mu::ui::window::CPickedItem*)
{
    ++cursorDrawCalls; // GPU asset drawing only; real held ownership/camera/projection remain.
    REQUIRE_FALSE(captureRenderer.scales.empty());
    CHECK(captureRenderer.scales.front().first == doctest::Approx(1.f));
    CHECK(captureRenderer.scales.front().second == doctest::Approx(1.f));
}

extern "C" bool __wrap__Z10LoadBitmapPKwjjjbb(const wchar_t*, GLuint, GLuint, GLuint, bool, bool)
{
    return true; // Native assets need a GPU; actual view, input and grid geometry remain linked.
}

namespace
{
void CheckInventoryText(Rml::ElementDocument* doc);
}

namespace
{
class InputGeometryProbe : public mu::ui::window::CObject
{
public:
    int nativeSyncs = 0;
    void SyncNativeLayout() override
    {
        ++nativeSyncs;
    }
    std::function<void()> check;
    bool Render() override
    {
        return true;
    }
    bool UpdateKeyEvent() override
    {
        return true;
    }
    float GetLayerDepth() override
    {
        return 1000.f;
    }
    bool UpdateMouseEvent() override
    {
        if (check)
            check();
        return true;
    }
};
} // namespace

class RmlViewLayoutFixture
{
public:
    RmlViewLayoutFixture()
        : cwd(std::filesystem::current_path()), theme(UI::RmlBridge::GetActiveThemeName()), width(WindowWidth),
          height(WindowHeight), percent(GameConfig::GetInstance().GetUIScalePercent()), mouseX(g_fWindowMouseX),
          mouseY(g_fWindowMouseY)
    {
        REQUIRE(RmlUiRuntime::Instance().m_Context == nullptr);
        savedPositions.clear();
        oldManager = mu::ui::window::CSystem::GetInstance()->m_pNewUIMng;
        mu::ui::window::CSystem::GetInstance()->m_pNewUIMng = &manager;
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
        oldHooks = RmlUiRuntime::Instance().m_Hooks;
        RmlUiRuntimeHooks hooks;
        hooks.beforeUpdate = [this]
        {
            ++beforeHooks;
            UI::RmlBridge::SyncDocumentLayoutScales();
            Tip::ExpireUnrefreshed();
        };
        hooks.afterUpdate = [this](Rml::Context* c)
        {
            ++afterHooks;
            UI::Placement::AfterRmlLayout(c);
        };
        hooks.publicationRevision = [] { return UI::RmlBridge::RenderTarget::PublicationRevision(); };
        RmlUiRuntime::Instance().SetHooks(std::move(hooks));
    }
    ~RmlViewLayoutFixture()
    {
        UI::Items::ReleaseCursorItemLayer();
        inventory.reset();
        character.reset();
        helper.reset();
        UI::Placement::Release();
        mu::ui::window::CSystem::GetInstance()->m_pNewUIMng = oldManager;
        Tip::Hide();
        RmlUiRuntime::Instance().m_Context = nullptr;
        RmlUiRuntime::Instance().m_framePrepared = false;
        RmlUiRuntime::Instance().SetHooks(oldHooks);
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
        UI::Placement::Update();
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
        inventory->m_pNewUIMng = &manager;
        manager.AddUIObj(mu::ui::window::INTERFACE_INVENTORY, inventory.get());
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
        const bool bag = std::string_view(name) == "inventory";
        UI::Placement::RegisterWindow(
            bag ? mu::ui::window::INTERFACE_INVENTORY : mu::ui::window::INTERFACE_CHARACTER, name,
            [owner] { return owner; }, bag ? "my_inventory.rml" : "character_info.rml");
        UI::Placement::MakeGroupDraggable(name, owner->GetPlacedDocument());
    }
    nlohmann::json EquipmentQuery(int slot)
    {
        auto* system = mu::ui::window::CSystem::GetInstance();
        auto* oldInventory = system->m_pNewMyInventory;
        auto* oldCharacter = system->m_pNewCharacterInfoWindow;
        system->m_pNewMyInventory = inventory.get();
        system->m_pNewCharacterInfoWindow = character.get();
        const int before = beforeHooks, after = afterHooks, clock = log.elapsedReads, draws = rmlDrawCalls;
        const auto prefs = savedPositions;
        std::unique_ptr<App::Control::Act> act;
        const auto request = App::Control::Request::Parse(
            nlohmann::json{{"cmd", "slot-pixel"}, {"grid", "equipment"}, {"slot", slot}}.dump());
        const auto reply = App::Control::Commands::SlotPixel(request, act);
        system->m_pNewMyInventory = oldInventory;
        system->m_pNewCharacterInfoWindow = oldCharacter;
        CHECK(act == nullptr);
        CHECK(beforeHooks == before);
        CHECK(afterHooks == after);
        CHECK(log.elapsedReads == clock);
        CHECK(rmlDrawCalls == draws);
        CHECK(savedPositions == prefs);
        return nlohmann::json::parse(reply);
    }
    void ProductionFrame()
    {
        UI::Placement::Update();                 // Same pre-input barrier used by real CSystem.
        RmlUiRuntime::Instance().PrepareFrame(); // Actual offscreen seam, before native capture.
        UI::RmlBridge::RenderTarget::RenderAll();
        RmlUiRuntime::Instance().RenderFrame(); // Hook-free publication flush, then prepared render.
        context->Render();
    }
    Rml::ElementDocument* Inventory(int owned, int variant, bool arrange = true)
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
        if (arrange)
        {
            UI::Placement::Arrange(); // One ordinary show arrangement, not a fixture convergence loop.
            Refresh();
        }
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
        const auto b = Box(*doc->GetElementById("item_grid"));
        const auto& geometry = inventory->GetInventoryCtrl()->Geometry();
        CHECK(geometry.Left() == doctest::Approx(b.left));
        CHECK(geometry.Top() == doctest::Approx(b.top));
        CHECK(geometry.PitchX() * 8 == doctest::Approx(b.width));
        CHECK(geometry.PitchY() * 8 == doctest::Approx(b.height));
        const auto image = Box(*doc->GetElementById("item_view"));
        CHECK(inventory->m_ItemTarget.m_offset.x == doctest::Approx(image.left));
        CHECK(inventory->m_ItemTarget.m_offset.y == doctest::Approx(image.top));
        CHECK(inventory->m_ItemTarget.m_size.x == doctest::Approx(image.width));
        CHECK(inventory->m_ItemTarget.m_size.y == doctest::Approx(image.height));
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
    void CheckCoherent()
    {
        Rml::Element* safe = nullptr;
        for (int i = 0; i < context->GetNumDocuments(); ++i)
            if (auto* e = context->GetDocument(i)->GetElementById("safe_area"))
                safe = e;
        REQUIRE(safe);
        const auto bounds = Box(*safe);
        for (const auto& [name, owner] :
             {std::pair<const char*, mu::ui::window::CObject*>{"inventory", inventory.get()},
              {"character", character.get()}})
        {
            UI::Placement::PlacementParticipant::Box cached;
            if (!owner->IsVisible())
            {
                CHECK_FALSE(UI::Placement::SlotBox(name, cached));
                continue;
            }
            REQUIRE(UI::Placement::SlotBox(name, cached));
            Rml::Vector2f p, z;
            REQUIRE(UI::RmlBridge::DrawnBox(*owner->GetPlacedDocument()->GetElementById("panel"), Rml::BoxArea::Border,
                                            p, z));
            CAPTURE(name);
            CAPTURE(cached.scale);
            if (WindowWidth == 3840 && WindowHeight == 2160)
            {
                // Supplemental large-origin Project inverse-map cancellation, NOT a relative tolerance.
                constexpr float Supplemental4kResidualPhysicalPixels = .25f;
                const std::array delta{std::abs(p.x - cached.left), std::abs(p.y - cached.top),
                                       std::abs(z.x - cached.width), std::abs(z.y - cached.height)};
                for (float residual : delta)
                    CHECK(residual <= Supplemental4kResidualPhysicalPixels);
                std::fprintf(stderr, "4k residual %s left%.6f top%.6f width%.6f height%.6f\n", name, delta[0], delta[1],
                             delta[2], delta[3]);
            }
            else
            {
                CHECK(p.x == doctest::Approx(cached.left));
                CHECK(p.y == doctest::Approx(cached.top));
                CHECK(z.x == doctest::Approx(cached.width));
                CHECK(z.y == doctest::Approx(cached.height));
            }
            CHECK(p.x >= bounds.left - .25f);
            CHECK(p.y >= bounds.top - .25f);
            CHECK(p.x + z.x <= bounds.left + bounds.width + .25f);
            CHECK(p.y + z.y <= bounds.top + bounds.height + .25f);
        }
    }
    void HidePanes()
    {
        inventory->CObject::Show(false);
        character->CObject::Show(false);
        inventory->GetPlacedDocument()->Hide();
        character->GetPlacedDocument()->Hide();
        UI::Placement::Arrange();
    }
    void Reopen(bool linked)
    {
        inventory->CObject::Show(true);
        inventory->GetPlacedDocument()->Show();
        character->CObject::Show(linked);
        if (linked)
            character->GetPlacedDocument()->Show();
        else
            character->GetPlacedDocument()->Hide();
        UI::Placement::Arrange();
        Refresh(); // First paint must already agree with published caches.
    }
    void CheckBeforeRealInput()
    {
        const auto scene = SceneFlag;
        const auto loading = LoadingWorld;
        SceneFlag = MAIN_SCENE;
        LoadingWorld = 0;
        inventory->Enable(false);
        character->Enable(false);
        helper->Enable(false); // No gameplay/model window updates.
        InputGeometryProbe probe;
        int checked = 0;
        probe.check = [&]
        {
            ++checked;
            CheckCoherent();
            CheckNativeGrid(inventory->GetPlacedDocument());
        };
        constexpr DWORD ProbeId = 0x7fffffff;
        manager.AddUIObj(ProbeId, &probe);
        mu::ui::window::CSystem::GetInstance()->Update(); // REAL pre-input phase + native dispatcher.
        CHECK(checked == 1);
        manager.RemoveUIObj(ProbeId);
        inventory->Enable(true);
        character->Enable(true);
        helper->Enable(true);
        SceneFlag = scene;
        LoadingWorld = loading;
    }
    void MoveInternalAnchor()
    {
        auto* grid = inventory->GetPlacedDocument()->GetElementById("item_grid");
        const auto old = Box(*grid);
        grid->SetProperty("top", "210px");
        RmlUiRuntime::Instance().PrepareFrame();
        const auto moved = Box(*grid);
        CHECK(moved.top != doctest::Approx(old.top));
        CHECK_FALSE(UI::Placement::Update()); // Outer intrinsic size unchanged: native hook still runs.
        CheckNativeGrid(inventory->GetPlacedDocument());
        RmlUiRuntime::Instance().RenderFrame();
        grid->RemoveProperty("top");
        ProductionFrame();
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
        UI::Placement::AfterRmlLayout(context);
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
            ProductionFrame();
            const auto current = Box(*doc->GetElementById("panel"));
            CHECK(current.left == doctest::Approx(initial.left));
            CHECK(current.top == doctest::Approx(initial.top));
            CHECK(current.width == doctest::Approx(initial.width));
            CHECK(current.height == doctest::Approx(initial.height));
        }
    }

    void UnplacedIdle()
    {
        using namespace mu::ui::window;
        InputGeometryProbe friends, flyout;
        friends.Enable(false);
        flyout.Enable(false);
        manager.AddUIObj(INTERFACE_FRIEND, &friends);
        manager.AddUIObj(INTERFACE_MUHELPER_SKILL_LIST, &flyout);
        UI::Placement::RegisterName(INTERFACE_FRIEND, "friends");
        UI::Placement::RegisterWindow(
            INTERFACE_MUHELPER_SKILL_LIST, "mu_helper_skill_list", [&] { return &flyout; },
            "mu_helper_skill_picker.rml");
        ProductionFrame();
        for (int i = 0; i < 12; ++i)
            CHECK_FALSE(UI::Placement::Update());
        friends.Show(false);
        flyout.Show(false);
        CHECK(UI::Placement::Update());
        for (int i = 0; i < 4; ++i)
            CHECK_FALSE(UI::Placement::Update());
        friends.Show(true);
        flyout.Show(true);
        CHECK(UI::Placement::Update());
        for (int i = 0; i < 12; ++i)
            CHECK_FALSE(UI::Placement::Update());
        const auto scene = SceneFlag;
        const auto loading = LoadingWorld;
        SceneFlag = MAIN_SCENE;
        LoadingWorld = 0;
        const int before = log.elapsedReads;
        const auto saves = savedPositions;
        const int friendSyncs = friends.nativeSyncs, flyoutSyncs = flyout.nativeSyncs;
        for (int i = 0; i < 12; ++i)
            CSystem::GetInstance()->Update(); // Real dispatch; no model/gameplay windows enabled.
        CHECK(log.elapsedReads == before);
        CHECK(savedPositions == saves);
        CHECK(friends.nativeSyncs == friendSyncs + 12);
        CHECK(flyout.nativeSyncs ==
              flyoutSyncs + 12); // One geometry dispatch per barrier, not registry+manager doubles.
        SceneFlag = scene;
        LoadingWorld = loading;
        manager.RemoveUIObj(INTERFACE_FRIEND);
        manager.RemoveUIObj(INTERFACE_MUHELPER_SKILL_LIST);
    }
    void NpcTransition()
    {
        using namespace mu::ui::window;
        auto npc = std::make_unique<CNPCShop>();
        npc->m_pNewInventoryCtrl = new CInventoryCtrl;
        REQUIRE(npc->m_pNewInventoryCtrl->Create(STORAGE_TYPE::UNDEFINED, &items, npc.get(), 8, 15));
        npc->m_pNewUIMng = &manager;
        manager.AddUIObj(INTERFACE_NPCSHOP, npc.get());
        npc->BuildRmlUi();
        REQUIRE(npc->GetPlacedDocument());
        npc->CObject::Show(true);
        npc->GetPlacedDocument()->Show();
        UI::Placement::RegisterWindow(INTERFACE_NPCSHOP, "npc_shop", [&] { return npc.get(); }, "npc_shop.rml");
        auto* previous = ItemAttribute;
        ITEM_ATTRIBUTE attribute{};
        attribute.Width = attribute.Height = 1;
        ItemAttribute = &attribute;
        npc->m_ItemTarget.m_drawer = [](const auto&, const auto&) {}; // GPU asset boundary only.
        auto stock = [&](int row)
        {
            npc->m_pNewInventoryCtrl->RemoveAllItems();
            ITEM prototype{};
            prototype.Type = 0;
            prototype.Durability = 7;
            auto* item = items.DuplicateItem(&prototype);
            REQUIRE(item);
            REQUIRE(npc->m_pNewInventoryCtrl->AddItem(0, row, item));
            items.DeleteItem(item);
            npc->m_pNewInventoryCtrl->Render(); // Real control cell state publication, no tooltip/3D drawing here.
            npc->SyncRmlModel(); // Real owner model phase: cells, stock-derived rows, context and invalidation.
        };
        auto coherent = [&]
        {
            auto* doc = npc->GetPlacedDocument();
            const auto image = Box(*doc->GetElementById("item_view"));
            CHECK(npc->m_ItemTarget.m_offset.x == doctest::Approx(image.left));
            CHECK(npc->m_ItemTarget.m_offset.y == doctest::Approx(image.top));
            CHECK(npc->m_ItemTarget.m_size.x == doctest::Approx(image.width));
            CHECK(npc->m_ItemTarget.m_size.y == doctest::Approx(image.height));
            const auto grid = Box(*doc->GetElementById("item_grid"));
            const auto& native = npc->m_pNewInventoryCtrl->Geometry();
            CHECK(native.Left() == doctest::Approx(grid.left));
            CHECK(native.Top() == doctest::Approx(grid.top));
            CHECK(native.PitchX() * 8 == doctest::Approx(grid.width));
            CHECK(native.PitchY() * npc->m_RmlView.GetModel().rows.size() == doctest::Approx(grid.height));
        };
        npc->m_ItemTarget.m_drawer = [&](const auto& offset, const auto& size)
        {
            const auto image = Box(*npc->GetPlacedDocument()->GetElementById("item_view"));
            CHECK(offset.x == doctest::Approx(image.left));
            CHECK(size.x == doctest::Approx(image.width));
            REQUIRE_FALSE(captureRenderer.scales.empty());
            CHECK(captureRenderer.scales.front().first == doctest::Approx(WindowWidth / image.width));
            CHECK(captureRenderer.scales.front().second == doctest::Approx(WindowHeight / image.height));
        };
        auto capture = [&]
        {
            coherent();
            const auto desired = Box(*npc->GetPlacedDocument()->GetElementById("item_view"));
            captureRenderer.captureExtent = std::pair{static_cast<std::uint32_t>(std::lround(desired.width)),
                                                      static_cast<std::uint32_t>(std::lround(desired.height))};
            UI::RmlBridge::RenderTarget::RenderAll();
            captureRenderer.captureExtent.reset();
            int w = 0, h = 0;
            const auto source = npc->m_ItemTarget.m_target.Source();
            REQUIRE_FALSE(source.empty());
            REQUIRE(UI::RmlBridge::RenderTarget::Resolve(source, w, h));
            const auto image = Box(*npc->GetPlacedDocument()->GetElementById("item_view"));
            CHECK(w == std::lround(image.width));
            CHECK(h == std::lround(image.height));
            RmlUiRuntime::Instance().RenderFrame();
        };
        for (int row : {0, 14, 2})
        {
            stock(row);
            RmlUiRuntime::Instance().PrepareFrame();
            capture();
        }
        // Real native dispatcher sees final NPC geometry without running another owner model Update.
        const auto scene = SceneFlag;
        const auto loading = LoadingWorld;
        SceneFlag = MAIN_SCENE;
        LoadingWorld = 0;
        inventory->Enable(false);
        character->Enable(false);
        helper->Enable(false);
        npc->Enable(false);
        InputGeometryProbe probe;
        int inputs = 0;
        probe.check = [&]
        {
            ++inputs;
            coherent();
        };
        manager.AddUIObj(0x7fffffff, &probe);
        CSystem::GetInstance()->Update();
        CHECK(inputs == 1);
        manager.RemoveUIObj(0x7fffffff);
        SceneFlag = scene;
        LoadingWorld = loading;
        inventory->Enable(true);
        character->Enable(true);
        helper->Enable(true);
        // Unchanged-size group title drag: actual Rml events, then pre-input geometry barrier.
        inventory->CObject::Show(false);
        inventory->GetPlacedDocument()->Hide();
        character->CObject::Show(false);
        character->GetPlacedDocument()->Hide();
        UI::Placement::Arrange();
        auto* title = npc->GetPlacedDocument()->GetElementById("title");
        const auto t = Box(*title);
        context->ProcessMouseMove(static_cast<int>(t.left + 12), static_cast<int>(t.top + t.height / 2), 0);
        context->ProcessMouseButtonDown(0, 0);
        context->ProcessMouseMove(static_cast<int>(t.left - 48), static_cast<int>(t.top + t.height / 2 + 10), 0);
        UI::Placement::Update();
        coherent();
        context->ProcessMouseButtonUp(0, 0);
        RmlUiRuntime::Instance().PrepareFrame();
        capture();
        UI::RmlBridge::ReloadAllThemedDocuments();
        RmlUiRuntime::Instance().PrepareFrame();
        capture();
        npc->CObject::Show(false);
        npc->GetPlacedDocument()->Hide();
        RmlUiRuntime::Instance().PrepareFrame();
        CHECK(npc->m_RmlView.GetModel().rows.size() == 3);
        CHECK_FALSE(npc->IsTransactionPending());
        RmlUiRuntime::Instance().RenderFrame();
        ItemAttribute = previous;
    }

    void UnregisteredDialogAndSharedView()
    {
        using namespace mu::ui::window;
        auto* previous = ItemAttribute;
        ITEM_ATTRIBUTE attribute{};
        attribute.Width = attribute.Height = 1;
        ItemAttribute = &attribute;
        CGenericConfirmDialog dialog;
        dialog.Create(&manager);
        int actions = 0;
        GenericDialogConfig cfg;
        cfg.item3D = ITEM{};
        cfg.item3D->Type = 0;
        cfg.onPrimary = [&] { ++actions; };
        const auto id = dialog.Show(std::move(cfg));
        dialog.m_Item3DTarget.m_drawer = [](const auto&, const auto&) {};
        auto check = [&]
        {
            auto* doc = dialog.m_RmlView.Document();
            REQUIRE(doc);
            const auto image = Box(*doc->GetElementById("gcd_item3d"));
            CHECK(dialog.m_Item3DTarget.m_offset.x == doctest::Approx(image.left));
            CHECK(dialog.m_Item3DTarget.m_offset.y == doctest::Approx(image.top));
            CHECK(dialog.m_Item3DTarget.m_size.x == doctest::Approx(image.width));
            CHECK(dialog.IsActive(id));
            CHECK(actions == 0);
        };
        RmlUiRuntime::Instance().PrepareFrame();
        check();
        RmlUiRuntime::Instance().RenderFrame();
        auto* panel = dialog.m_RmlView.Document()->GetElementById("panel");
        const auto start = Box(*panel);
        context->ProcessMouseMove(static_cast<int>(start.left + 10), static_cast<int>(start.top + 5), 0);
        context->ProcessMouseButtonDown(0, 0);
        context->ProcessMouseMove(static_cast<int>(start.left - 10), static_cast<int>(start.top + 35), 0);
        context->ProcessMouseButtonUp(0, 0);
        RmlUiRuntime::Instance().PrepareFrame();
        check();
        RmlUiRuntime::Instance().RenderFrame();
        UI::RmlBridge::ReloadAllThemedDocuments();
        RmlUiRuntime::Instance().PrepareFrame();
        check();
        RmlUiRuntime::Instance().RenderFrame();
        dialog.Cancel(id);
        dialog.Release();
        auto* system = CSystem::GetInstance();
        auto* oldItems = system->m_pNewItemMng;
        system->m_pNewItemMng = &items;
        CDoppelGangerWindow event;
        REQUIRE(event.Create(&manager));
        event.CObject::Show(true);
        event.GetPlacedDocument()->Show();
        std::vector<mu::ui::window::EventItemEntryView::Text> texts(9);
        for (auto& text : texts)
            text.text = L"Event entry description";
        event.m_View.SetTexts(std::move(texts));
        event.m_View.Sync(true);
        RmlUiRuntime::Instance().PrepareFrame(); // Real shared-view delegation through manager, no event Update.
        REQUIRE(event.m_View.m_ItemTarget);
        const auto image = Box(*event.GetPlacedDocument()->GetElementById("entry_item"));
        CHECK(event.m_View.m_ItemTarget->m_size.x == doctest::Approx(image.width));
        CHECK(event.m_View.m_ItemTarget->m_offset.y == doctest::Approx(image.top));
        RmlUiRuntime::Instance().RenderFrame();
        event.Release();
        system->m_pNewItemMng = oldItems;
        ItemAttribute = previous;
        NoErrors();
    }
    void CursorBarrier()
    {
        using namespace mu::ui::window;
        const int documents = context->GetNumDocuments();
        UI::Items::SyncCursorItemLayout();
        CHECK(context->GetNumDocuments() == documents);
        auto* previous = ItemAttribute;
        ITEM_ATTRIBUTE attribute{};
        attribute.Width = attribute.Height = 1;
        ItemAttribute = &attribute;
        auto* system = CSystem::GetInstance();
        auto* old = system->m_pNewItemMng;
        system->m_pNewItemMng = &items;
        InputGeometryProbe owner;
        CInventoryCtrl control;
        REQUIRE(control.Create(STORAGE_TYPE::UNDEFINED, &items, &owner, 8, 8));
        ITEM item{};
        item.Type = 0;
        item.Durability = 7;
        REQUIRE(CInventoryCtrl::CreatePickedItem(&control, &item));
        auto* held = CInventoryCtrl::GetPickedItem();
        held->ShowPickedItem();
        UI::Items::SyncCursorItemLayer(); // Normal existing-layer lifecycle; NOT from geometry hook.
        for (const auto v : {Viewport{1024, 768, 100}, Viewport{800, 600, 150}})
        {
            WindowWidth = v.width;
            WindowHeight = v.height;
            context->SetDimensions({v.width, v.height});
            const int before = cursorDrawCalls;
            RmlUiRuntime::Instance().PrepareFrame();
            captureRenderer.allowed.insert(captureRenderer.lastCreated + 1);
            UI::RmlBridge::RenderTarget::RenderAll();
            CHECK(cursorDrawCalls == before + 1);
            CHECK(CInventoryCtrl::GetPickedItem() == held);
            RmlUiRuntime::Instance().RenderFrame();
        }
        held->HidePickedItem();
        RmlUiRuntime::Instance().PrepareFrame();
        const int hidden = cursorDrawCalls;
        UI::RmlBridge::RenderTarget::RenderAll();
        CHECK(cursorDrawCalls == hidden);
        RmlUiRuntime::Instance().RenderFrame();
        CHECK(CInventoryCtrl::GetPickedItem() == held);
        CInventoryCtrl::DeletePickedItem();
        UI::Items::ReleaseCursorItemLayer();
        system->m_pNewItemMng = old;
        ItemAttribute = previous;
    }

    void PopulateArmory(bool editing)
    {
        auto& a = *character->m_Armory;
        a.m_Title = "LongName123 · Grand Master · Level 400";
        a.m_Status = "Allocation confirmed. Character statistics and equipped item bonuses are current.";
        a.m_Points = "32767";
        a.m_Budget = "Allocating 32767 / 32767 points";
        a.m_Ready = a.m_Valid = a.m_Retry = a.m_Ratings = true;
        a.m_Editing = editing;
        a.m_SkillName = "Lightning Shock · Master Enhancement";
        a.m_SkillOutput = "1,234,567–9,876,543";
        a.m_SkillKind = "Magical damage · area attack";
        a.m_Costs = "Mana: 1234 / AG: 567 / Cooldown: 120 seconds";
        a.m_Restriction = "Requires a staff equipped and sufficient character attributes.";
        a.m_Target = "Target: Level 400 Grand Master · defence 123456";
        a.m_Resources = {"123456789 / 987654321", "123456789 / 987654321", "123456 / 987654"};
        a.m_ResourceDetails = {"Base + equipment + skill bonuses", "Maximum mana and regeneration details",
                               "Current AG and regeneration details"};
        a.m_ResourceExpanded = {true, true, true};
        a.m_Attributes.clear();
        int index = 0;
        for (const char* label : {"Strength", "Agility", "Vitality", "Energy", "Command"})
        {
            UI::Character::ArmoryController::AttributeView row;
            row.index = index++;
            row.label = label;
            row.total = "32,767 (+1234)";
            row.input = "32767";
            row.addition = 1234;
            row.proposed = true;
            row.note = "Equipment bonus +1,234 · allocated +12,345";
            row.explanation = "Base attribute plus equipped item bonus";
            a.m_Attributes.push_back(row);
        }
        a.m_Categories.clear();
        index = 0;
        for (const char* name : {"Attack", "Defence", "Ratings"})
        {
            UI::Character::ArmoryController::CategoryView category;
            category.name = name;
            for (const char* label :
                 {"Excellent damage rate", "Double damage chance", "Additional elemental damage", "Attack speed"})
            {
                UI::Character::ArmoryController::RowView row;
                row.index = index++;
                row.label = label;
                row.value = "123,456 (99.99%)";
                row.detail = "Base + equipment + excellent options + master skill bonuses";
                row.expanded = true;
                category.rows.push_back(row);
            }
            a.m_Categories.push_back(category);
        }
        a.m_Snapshot.rows.resize(index); // Real hover callbacks require view row indices backed by snapshot rows.
        a.m_Handle.DirtyAllVariables();
    }
    void CheckArmoryText()
    {
        auto* doc = character->GetPlacedDocument();
        const auto stripped = [](Rml::String text)
        {
            text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); }),
                       text.end());
            return text;
        };
        int lines = 0;
        const auto inspect = [&](auto&& self, Rml::Element* e) -> void
        {
            if (!e->IsVisible(true))
                return;
            if (auto* text = dynamic_cast<Rml::ElementText*>(e))
            {
                if (stripped(text->GetText()).empty())
                    return;
                auto* parent = text->GetParentNode();
                REQUIRE(parent);
                CAPTURE(parent->GetAddress());
                CAPTURE(text->GetText());
                Rml::Rectanglef limit;
                REQUIRE(Rml::ElementUtilities::GetBoundingBox(limit, parent, Rml::BoxArea::Border));
                const float scale = UI::RmlBridge::DrawnScale(*parent);
                const auto& metrics = Rml::GetFontEngineInterface()->GetFontMetrics(text->GetFontFaceHandle());
                CHECK(parent->GetComputedValues().font_size() * scale >= 10.f - .1f);
                Rml::Vector2f painted;
                REQUIRE(UI::RmlBridge::DrawnTopLeft(*parent, painted));
                const auto parentOffset = parent->GetAbsoluteOffset(Rml::BoxArea::Border);
                const auto offset = text->GetAbsoluteOffset(Rml::BoxArea::Content) - parentOffset;
                Rml::String rendered;
                for (const auto& line : text->GetLines())
                {
                    rendered += line.text;
                    ++lines;
                    const auto baseline = painted + (offset + line.position) * scale;
                    const float width = Rml::ElementUtilities::GetStringWidth(parent, line.text) * scale;
                    CHECK(baseline.x >= limit.Left() - .75f);
                    CHECK(baseline.x + width <= limit.Right() + .75f);
                    CHECK(baseline.y - metrics.ascent * scale >= limit.Top() - .75f);
                    CHECK(baseline.y + metrics.descent * scale <= limit.Bottom() + .75f);
                }
                CHECK(stripped(rendered) == stripped(text->GetText()));
            }
            for (int i = 0; i < e->GetNumChildren(); ++i)
                self(self, e->GetChild(i));
        };
        inspect(inspect, doc);
        CHECK(lines > 90);
        Rml::ElementList actions;
        doc->QuerySelectorAll(actions, "button, input, .gear-slot");
        for (auto* action : actions)
        {
            if (!action->IsVisible(true))
                continue;
            CAPTURE(action->GetAddress());
            action->ScrollIntoView();
            ProductionFrame();
            const auto b = Box(*action), panel = Box(*doc->GetElementById("panel"));
            CHECK(b.width > 0);
            CHECK(b.height >= action->GetComputedValues().font_size() - .25f);
            CHECK(b.left >= panel.left - .75f);
            CHECK(b.left + b.width <= panel.left + panel.width + .75f);
            CHECK(b.top >= panel.top - .75f);
            CHECK(b.top + b.height <= panel.top + panel.height + .75f);
            context->ProcessMouseMove(static_cast<int>(b.left + b.width / 2), static_cast<int>(b.top + b.height / 2),
                                      0);
            auto* hit = context->GetHoverElement();
            while (hit && hit != action)
                hit = hit->GetParentNode();
            CHECK(hit == action); // Real engine hit routing; no callbacks activated/clicked.
        }
        doc->QuerySelector(".armory-left")->SetScrollTop(0);
        doc->QuerySelector(".armory-right")->SetScrollTop(0);
        ProductionFrame();
    }

    void CheckArmoryGeometry()
    {
        auto* doc = character->GetPlacedDocument();
        auto* panel = doc->GetElementById("panel");
        auto* face = doc->QuerySelector(".armory-face");
        REQUIRE(panel);
        REQUIRE(face);
        const auto bounds = Box(*panel), actual = Box(*face);
        CHECK(actual.left == doctest::Approx(bounds.left));
        CHECK(actual.top == doctest::Approx(bounds.top));
        CHECK(actual.width == doctest::Approx(bounds.width));
        CHECK(actual.height == doctest::Approx(bounds.height));
        const auto* token = doc->GetLocalProperty("--root-scale");
        REQUIRE(token);
        CHECK(std::stof(token->ToString()) == doctest::Approx(UI::RmlBridge::DrawnScale(*panel)));
        for (const char* selector : {"#title", ".status", ".close", ".armory-body"})
        {
            auto* e = doc->QuerySelector(selector);
            REQUIRE(e);
            const auto b = Box(*e);
            CHECK(b.left >= bounds.left - .25f);
            CHECK(b.top >= bounds.top - .25f);
            CHECK(b.left + b.width <= bounds.left + bounds.width + .25f);
            CHECK(b.top + b.height <= bounds.top + bounds.height + .25f);
        }
        // Real rem/counter-scale authored camera images, full (unclipped) extent, not gear-mask viewport.
        using UI::RmlBridge::RenderTarget;
        for (const char* id : {"armory_preview", "armory_item_0", "armory_item_2", "armory_item_7"})
        {
            auto* image = doc->GetElementById(id);
            REQUIRE(image);
            const auto desired = image->GetBox().GetSize(Rml::BoxArea::Content);
            bool captured = false;
            RenderTarget target(
                [&](std::uint32_t w, std::uint32_t h)
                {
                    captured = true;
                    CHECK(w == std::lround(desired.x));
                    CHECK(h == std::lround(desired.y));
                });
            target.SetEnabled(true);
            target.BindImage(image);
            captureRenderer.allowed.insert(captureRenderer.lastCreated + 1);
            RenderTarget::RenderAll();
            REQUIRE(captured); // No manual consumer size or gameplay/viewer Sync.
            CHECK(image->GetAttribute<Rml::String>("src", "") == target.Source());
        }
        auto* title = inventory->GetPlacedDocument()->GetElementById("title");
        REQUIRE(title);
        const auto header = Box(*title);
        auto* hit = context->GetElementAtPoint({header.left + 12, header.top + header.height / 2});
        while (hit && hit != title)
            hit = hit->GetParentNode();
        CHECK(hit == title); // Actual title hit, not an alternate handle hiding Armory overflow.
        NoErrors();
    }

    void DragAndSave(bool linked)
    {
        HidePanes();
        Reopen(linked);
        CheckCoherent();
        auto* doc = inventory->GetPlacedDocument();
        CheckNativeGrid(doc);
        auto* title = doc->GetElementById("title");
        REQUIRE(title);
        const auto start = Box(*title);
        UI::Placement::PlacementParticipant::Box before;
        REQUIRE(UI::Placement::SlotBox("inventory", before));
        const int x = static_cast<int>(start.left + 12.f), y = static_cast<int>(start.top + start.height / 2.f);
        context->ProcessMouseMove(x, y, 0);
        context->ProcessMouseButtonDown(0, 0);
        context->ProcessMouseMove(x - 60, y + 80, 0);
        context->ProcessMouseMove(x - 90, y + 100, 0);
        UI::Placement::Update(); // Real pre-native-input barrier; no manual Arrange.
        CheckCoherent();
        CheckNativeGrid(doc);
        UI::Placement::PlacementParticipant::Box moved;
        REQUIRE(UI::Placement::SlotBox("inventory", moved));
        if (!linked)
            CHECK(moved.left !=
                  doctest::Approx(before.left)); // Linked group fills available width: clamped, not movable.
        else
            CHECK(moved.left == doctest::Approx(before.left));
        context->ProcessMouseButtonUp(0, 0);
        REQUIRE(savedPositions.contains(L"group.items"));
        const auto saved = savedPositions.at(L"group.items");
        ProductionFrame();
        CheckCoherent();
        CHECK(savedPositions.at(L"group.items") == saved);
        HidePanes();
        ProductionFrame();
        Reopen(linked);
        CheckCoherent();
        CheckNativeGrid(doc);
        CHECK(savedPositions.at(L"group.items") == saved);
    }

    void CheckChatComposition()
    {

        using namespace mu::ui::window;
        CreateInventoryGroup();
        CChatLogWindow chat;
        CSystemLogWindow notices;
        REQUIRE(chat.Create(&manager));
        REQUIRE(notices.Create(&manager));
        chat.Show(true);
        notices.Show(true);
        notices.AddText(L"Welcome", TYPE_SYSTEM_MESSAGE);
        chat.AddText(L"Peer", L"Native chat remains readable", TYPE_CHAT_MESSAGE);
        for (const auto v : {cases[0], cases[1], cases[2], Viewport{3840, 2160, 100}})
        {
            CAPTURE(v.width);
            Configure("munolith", v);
            Inventory(0, 1);
            PopulateArmory(false);
            chat.SyncDocVisibility(true);
            notices.SyncDocVisibility(true);
            for (const bool framed : {false, true, false})
            {
                CAPTURE(framed);
                framed ? chat.ShowFrame() : chat.HideFrame();
                REQUIRE(chat.Update());
                REQUIRE(notices.Update());
                ProductionFrame();
                Rml::ElementDocument* doc = nullptr;
                for (int i = 0; i < context->GetNumDocuments(); ++i)
                    if (context->GetDocument(i)->GetSourceURL().find("chat_log.rml") != Rml::String::npos)
                        doc = context->GetDocument(i);
                REQUIRE(doc);
                REQUIRE(doc->IsVisible());
                CHECK(doc->GetElementById("lines")->GetInnerRML().find("Peer") != Rml::String::npos);
                auto* panel = doc->GetElementById("panel");
                REQUIRE(panel);
                const auto b = Box(*panel);
                const Rml::Vector2f first(b.left + b.width / 2.f, b.top + b.height / 4.f);
                const Rml::Vector2f second(b.left + b.width / 2.f, b.top + b.height * .75f);
                renderer.tracePaint = true;
                renderer.paint.clear();
                context->Render();
                CHECK_FALSE(renderer.paint.empty()); // Actual peers and native-font messages are drawn together.
                CHECK(renderer.ChatBackdropPaints(first, second) == (framed ? 1 : 0));
                renderer.tracePaint = false;
                CHECK(chat.GetBackAlpha() == doctest::Approx(.6f));
                CHECK(chat.IsShowFrame() == framed);
                CHECK(notices.IsVisible());
                CHECK(inventory->IsVisible());
                CHECK(character->IsVisible());
                NoErrors();
            }
        }
    }
    void CheckEquipmentQuery()
    {

        CreateInventoryGroup();
        const char* legacySlots[] = {"slot_weapon_right", "slot_weapon_left", "slot_helm",       "slot_armor",
                                     "slot_pants",        "slot_gloves",      "slot_boots",      "slot_wing",
                                     "slot_helper",       "slot_amulet",      "slot_ring_right", "slot_ring_left"};
        for (const char* theme : {"legacy", "modern", "munolith"})
            for (const auto v : {cases[0], cases[1], cases[2], Viewport{3840, 2160, 100}})
            {
                CAPTURE(std::string(theme));
                CAPTURE(v.width);
                Configure(theme, v);
                inventory->CObject::Show(true);
                character->CObject::Show(true);
                inventory->GetPlacedDocument()->Show();
                character->GetPlacedDocument()->Show();
                Inventory(0, 1);
                ProductionFrame();
                const bool embedded = UI::RmlBridge::ThemeUsesEmbeddedInventory();
                auto* owner = embedded ? static_cast<mu::ui::window::CObject*>(character.get()) : inventory.get();
                auto* document = owner->GetPlacedDocument();
                REQUIRE(document);
                for (int slot = 0; slot < MAX_EQUIPMENT_INDEX; ++slot)
                {
                    CAPTURE(slot);
                    const auto id = embedded ? "armory_slot_" + std::to_string(slot) : legacySlots[slot];
                    auto* frame = document->GetElementById(id);
                    REQUIRE(frame);
                    const auto box = Box(*frame);
                    const int before = beforeHooks, after = afterHooks, clock = log.elapsedReads, draws = rmlDrawCalls;
                    const auto prefs = savedPositions;
                    const auto reply = EquipmentQuery(slot);
                    CHECK(beforeHooks == before);
                    CHECK(afterHooks == after);
                    CHECK(log.elapsedReads == clock);
                    CHECK(rmlDrawCalls == draws);
                    CHECK(savedPositions == prefs);
                    REQUIRE(reply["ok"] == true);
                    CHECK(reply["result"]["x"].get<float>() == doctest::Approx(box.left + box.width / 2.f));
                    CHECK(reply["result"]["y"].get<float>() == doctest::Approx(box.top + box.height / 2.f));
                    auto projected =
                        Rml::Vector2f(reply["result"]["x"].get<float>(), reply["result"]["y"].get<float>());
                    REQUIRE(frame->Project(projected));
                    const auto expected = frame->GetAbsoluteOffset(Rml::BoxArea::Border) +
                                          frame->GetBox().GetSize(Rml::BoxArea::Border) * .5f;
                    if (v.width == 3840)
                    {
                        CHECK(std::abs(projected.x - expected.x) <= .25f);
                        CHECK(std::abs(projected.y - expected.y) <= .25f);
                    }
                    else
                    {
                        CHECK(projected.x == doctest::Approx(expected.x));
                        CHECK(projected.y == doctest::Approx(expected.y));
                    }
                }
                CHECK(EquipmentQuery(-1)["error"] == "bad_request");
                CHECK(EquipmentQuery(MAX_EQUIPMENT_INDEX)["error"] == "bad_request");
                inventory->CObject::Show(!embedded);
                inventory->GetPlacedDocument()->SetProperty("visibility", embedded ? "hidden" : "visible");
                ProductionFrame();
                REQUIRE(EquipmentQuery(EQUIPMENT_HELM)["ok"] == true); // Character-only embedded ownership.
                owner->CObject::Show(false);
                owner->GetPlacedDocument()->Hide();
                CHECK(EquipmentQuery(EQUIPMENT_HELM)["error"] == "not_open");
                owner->CObject::Show(true);
                document->Show();
                if (!embedded)
                {
                    character->CObject::Show(false);
                    character->GetPlacedDocument()->Hide();
                }
                ProductionFrame();
                REQUIRE(EquipmentQuery(EQUIPMENT_HELM)["ok"] == true);
                const auto helmId = embedded ? "armory_slot_2" : "slot_helm";
                auto* helm = document->GetElementById(helmId);
                REQUIRE(helm);
                helm->SetProperty("display", "none");
                ProductionFrame();
                CHECK(EquipmentQuery(EQUIPMENT_HELM)["error"] == "bad_request");
                helm->RemoveProperty("display");
                ProductionFrame();
                REQUIRE(EquipmentQuery(EQUIPMENT_HELM)["ok"] == true);
                document->Hide();
                CHECK(EquipmentQuery(EQUIPMENT_HELM)["error"] == "bad_request");
                document->Show();
                ProductionFrame();
                REQUIRE(EquipmentQuery(EQUIPMENT_HELM)["ok"] == true);
                helm->GetParentNode()->RemoveChild(helm);
                ProductionFrame();
                CHECK(EquipmentQuery(EQUIPMENT_HELM)["error"] == "bad_request");
                // Real view lifecycle rebuild, not coordinates, a surrogate slot or old-cache fallback.
                if (embedded)
                    character->m_RmlView.Release();
                else
                    inventory->m_RmlView.Release();
                CHECK(EquipmentQuery(EQUIPMENT_HELM)["error"] == "bad_request");
                if (embedded)
                    character->m_RmlView.Ensure();
                else
                    inventory->m_RmlView.Ensure();
                owner->GetPlacedDocument()->Show();
                CHECK(EquipmentQuery(EQUIPMENT_HELM)["error"] == "bad_request");
                Inventory(0, 1);
                ProductionFrame();
                REQUIRE(EquipmentQuery(EQUIPMENT_HELM)["ok"] == true);
                NoErrors();
            }
    }

    int beforeHooks = 0, afterHooks = 0;
    Rml::Context* context = nullptr;

private:
    RmlUiRuntimeHooks oldHooks;
    std::filesystem::path cwd;
    std::string theme;
    int width, height, percent;
    float mouseX, mouseY;
    NullRenderer renderer;
    UI::RmlBridge::ThemeFileInterface files;
    ModelLog log;
    mu::ui::window::CManager manager;
    mu::ui::window::CManager* oldManager = nullptr;
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
                f.CheckCoherent();
                f.NoErrors();
            }
    }
}

TEST_CASE("Closed normal inventory reopens compact with coherent first-paint production placement "
          "[ui][placement][reconciliation]")
{
    RmlViewLayoutFixture f;
    f.CreateInventoryGroup();
    f.Configure("munolith", {1024, 768, 100});
    f.Inventory(0, 1);
    for (bool linked : {false, true})
        for (const auto v : {Viewport{800, 600, 150}, Viewport{1920, 1080, 75}, Viewport{1024, 768, 100}})
        {
            CAPTURE(linked);
            CAPTURE(v.width);
            f.HidePanes();
            f.Configure("munolith", v);
            f.ProductionFrame();
            f.ProductionFrame(); // Settle HUD while ordinary windows stay closed.
            f.Reopen(linked);
            f.CheckCoherent(); // Before any next Update/rearrange.
            for (int i = 0; i < 4; ++i)
            {
                f.ProductionFrame();
                f.CheckCoherent();
            }
        }
}

TEST_CASE("Production model capacity and unchanged-size anchors reconcile before native input and capture "
          "[ui][placement][barrier]")
{
    RmlViewLayoutFixture f;
    f.CreateInventoryGroup();
    f.Configure("munolith", {800, 600, 150});
    auto* doc = f.Inventory(0, 1);
    for (int owned : {1, 2, 0})
    {
        f.Inventory(owned, 2, false); // Dirty real model only: no Arrange, invalidation or fixture layout loop.
        const int before = f.beforeHooks, after = f.afterHooks;
        f.ProductionFrame();
        CHECK(f.beforeHooks == before + 1);
        CHECK(f.afterHooks == after + 1);
        f.CheckCoherent();
        f.CheckNativeGrid(doc);
        f.CheckBeforeRealInput();
        for (int idle = 0; idle < 8; ++idle)
        {
            CHECK_FALSE(UI::Placement::Update());
            f.ProductionFrame();
            CHECK_FALSE(UI::Placement::Update());
        }
    }
    f.MoveInternalAnchor();
    f.CheckBeforeRealInput();
}

TEST_CASE(
    "Bound capture sources publish on success and reload without repeated owner hooks [ui][render-target][publication]")
{
    using UI::RmlBridge::RenderTarget;
    RmlViewLayoutFixture f;
    // Six semantic consumer roles (Armory has character + equipment): all have authored extents.
    for (const char* id : {"item_view", "armory_preview", "armory_item_0", "hp_item_view", "preview", "photo_image"})
    {
        CAPTURE(std::string(id));
        auto* doc = f.context->LoadDocumentFromMemory(
            Rml::String("<rml><head><style>img {width:64px;height:32px;} .render-target-pending "
                        "{opacity:0;}</style></head><body><img id='") +
            id + "'/></body></rml>");
        REQUIRE(doc);
        doc->Show();
        f.Refresh();
        auto* image = doc->GetElementById(id);
        REQUIRE(image);
        RenderTarget target([](std::uint32_t, std::uint32_t) {});
        target.SetEnabled(true);
        const auto firstId = captureRenderer.lastCreated + 1;
        target.BindImage(image);
        CHECK(image->IsClassSet("render-target-pending"));
        CHECK(target.Source().empty());
        const auto before = Box(*image);
        const int hooks = f.beforeHooks;
        RmlUiRuntime::Instance().PrepareFrame();
        captureRenderer.allowed.insert(firstId);
        RenderTarget::RenderAll(); // Real capture/promotion/source registry, no GPU.
        const auto first = target.Source();
        REQUIRE_FALSE(first.empty());
        CHECK(image->GetAttribute<Rml::String>("src", "") == first);
        CHECK_FALSE(image->IsClassSet("render-target-pending"));
        RmlUiRuntime::Instance().RenderFrame();
        f.context->Render(); // Real engine samples the successfully promoted registry handle now.
        CHECK(f.beforeHooks == hooks + 1);
        const auto after = Box(*image);
        CHECK(before.width == after.width);
        CHECK(before.height == after.height);
        int w = 0, h = 0;
        CHECK(RenderTarget::Resolve(first, w, h) != nullptr);
        CHECK(w == 64);
        CHECK(h == 32);
        const auto revision = RenderTarget::PublicationRevision();
        target.BindImage(image);
        f.ProductionFrame(); // Idle draw/owner sync never republishes old/empty URI.
        CHECK(RenderTarget::PublicationRevision() == revision);
        CHECK(image->GetAttribute<Rml::String>("src", "") == first);
        image->SetProperty("width", "80px");
        image->SetProperty("height", "40px");
        RmlUiRuntime::Instance().PrepareFrame();
        RenderTarget::RenderAll(); // Pending capture denied: ready old source remains, no false promotion.
        CHECK(target.Source() == first);
        CHECK(image->GetAttribute<Rml::String>("src", "") == first);
        const auto secondId = captureRenderer.lastCreated;
        captureRenderer.allowed.insert(secondId);
        RenderTarget::RenderAll();
        const auto second = target.Source();
        CHECK(second != first);
        CHECK(image->GetAttribute<Rml::String>("src", "") == second);
        CHECK(RenderTarget::Resolve(first, w, h) != nullptr); // Retire lag unchanged, old handle still resolvable.
        for (int i = 0; i < 3; ++i)
            RenderTarget::RenderAll();
        CHECK(RenderTarget::Resolve(first, w, h) == nullptr);
        CHECK(RenderTarget::Resolve(second, w, h) != nullptr);
        CHECK(w == 80);
        CHECK(h == 40);
        CHECK_FALSE(image->IsClassSet("render-target-pending"));
        doc->Close();
        f.context->Update(); // Expired observer cannot reference next document generation.
        doc = f.context->LoadDocumentFromMemory(
            "<rml><head><style>img {width:64px;height:32px;visibility:hidden;}</style></head><body><img "
            "id='ready'/></body></rml>");
        doc->Show();
        image = doc->GetElementById("ready");
        target.BindImage(image);
        f.Refresh();
        CHECK(image->GetAttribute<Rml::String>("src", "") == second);
        CHECK_FALSE(image->IsVisible(true));
        {
            RenderTarget replacement([](std::uint32_t, std::uint32_t) {});
            replacement.BindImage(image); // Old owner cannot clobber replacement's readiness/source.
            target.BindImage(nullptr);
            CHECK(image->IsClassSet("render-target-pending"));
        }
        CHECK_FALSE(image->IsClassSet("render-target-pending"));
        doc->Close();
        f.context->Update();
        target.BindImage(nullptr);
    }
}

TEST_CASE("Placement bounds expose bounded safe and unsupported no-safe outcomes without idle retry "
          "[ui][placement][fallback]")
{
    RmlViewLayoutFixture f;
    f.Configure("munolith", {800, 600, 100});
    int places = 0;
    float natural = 100.f;
    UI::Placement::RegisterParticipant(
        "inventory", {[] { return true; }, [&] { return UI::Placement::PlacementParticipant::Size{100.f, natural}; },
                      [&](const UI::Placement::PlacementParticipant::Box* b)
                      {
                          if (b && ++places <= 8)
                              natural = places % 2 ? 110.f : 100.f;
                      }});
    UI::Placement::Arrange();
    CHECK(places <= 9);
    CHECK(UI::Placement::LastReconciliationResult() == UI::Placement::ReconciliationResult::MeasuredSafe);
    UI::Placement::PlacementParticipant::Box safe;
    REQUIRE(UI::Placement::SlotBox("inventory", safe));
    CHECK(safe.height == doctest::Approx(natural * safe.scale));
    const int finalPlaces = places;
    for (int i = 0; i < 8; ++i)
    {
        CHECK_FALSE(UI::Placement::Update());
        CHECK(places == finalPlaces);
    }
    // Pathological theme: a screen-sized minimum larger than its workspace at ANY applied scale.
    float scale = 1.f;
    UI::Placement::RegisterParticipant(
        "inventory",
        {[] { return true; }, [&] { return UI::Placement::PlacementParticipant::Size{1000.f / scale, 1000.f / scale}; },
         [&](const UI::Placement::PlacementParticipant::Box* b)
         {
             if (b)
                 scale = b->scale;
         }});
    UI::Placement::Arrange();
    CHECK(UI::Placement::LastReconciliationResult() == UI::Placement::ReconciliationResult::Unresolved);
    CHECK_FALSE(UI::Placement::SlotBox("inventory", safe));
    for (int i = 0; i < 8; ++i)
        CHECK_FALSE(UI::Placement::Update());
    // Unsupported no-safe does NOT hide/close/cancel anything or claim input quarantine.
    // Relevant corrected sizing recovers naturally, not on every idle or pointer tick.
    UI::Placement::RegisterParticipant(
        "inventory", {[] { return true; }, [] { return UI::Placement::PlacementParticipant::Size{100.f, 100.f}; },
                      [](const auto*) {}});
    REQUIRE(UI::Placement::Update());
    CHECK(UI::Placement::LastReconciliationResult() == UI::Placement::ReconciliationResult::Converged);
    CHECK(UI::Placement::SlotBox("inventory", safe));
}

TEST_CASE("Layout projection cache refresh uses real render transforms without drawing [ui][placement][transforms]")
{
    RmlViewLayoutFixture f;
    auto* doc = f.context->LoadDocumentFromMemory(R"(<rml><head><style>
        #outer {position:absolute;left:40px;top:50px;width:180px;height:120px;transform:translate(11px,7px) scale(0.7);transform-origin:left top;background-color:#ffffff;}
        #scroll {width:90px;height:50px;overflow:auto;}
        #child {display:block;width:130px;height:180px;transform:scale(1.4285714);transform-origin:left top;background-color:#ff0000;}
        #hidden {position:absolute;left:7px;top:9px;width:64px;height:32px;visibility:hidden;}
    </style></head><body><div id="outer"><div id="scroll"><div id="child"/></div><img id="hidden"/></div></body></rml>)");
    REQUIRE(doc);
    doc->Show();
    f.context->Update();
    auto extra = doc->CreateElement("div");
    auto* nonDom = extra.get();
    nonDom->SetProperty("position", "absolute");
    nonDom->SetProperty("width", "8px");
    nonDom->SetProperty("height", "9px");
    auto* parent = doc->GetElementById("outer");
    parent->AppendChild(std::move(extra), false);
    doc->UpdateDocument();
    nonDom->SetBox(Rml::Box({8.f, 9.f}));
    nonDom->SetOffset({3.f, 4.f}, parent);
    doc->GetElementById("scroll")->SetScrollTop(20.f);
    f.context->Update();
    const int draws = rmlDrawCalls;
    Rml::ElementUtilities::UpdateTransformCaches(doc); // Actual patched engine methods, parent-first.
    CHECK(rmlDrawCalls == draws);
    std::vector<std::pair<Rml::Element*, Rect>> prepared;
    for (const char* id : {"outer", "scroll", "child", "hidden"})
    {
        CAPTURE(std::string(id));
        auto* e = doc->GetElementById(id);
        REQUIRE(e);
        prepared.push_back({e, Box(*e)});
    }
    prepared.push_back({nonDom, Box(*nonDom)});
    f.context->Render();
    CHECK(rmlDrawCalls > draws);
    for (const auto& [e, before] : prepared)
    {
        const auto after = Box(*e);
        CHECK(after.left == doctest::Approx(before.left));
        CHECK(after.top == doctest::Approx(before.top));
        CHECK(after.width == doctest::Approx(before.width));
        CHECK(after.height == doctest::Approx(before.height));
    }
    doc->GetElementById("outer")->SetProperty("transform", "translate(20px,30px) scale(1.3)");
    f.context->Update();
    Rml::ElementUtilities::UpdateTransformCaches(doc->GetElementById("child")); // Ancestors too.
    const auto changed = Box(*doc->GetElementById("child"));
    CHECK(changed.width != doctest::Approx(prepared[2].second.width));
    f.context->Render();
    const auto painted = Box(*doc->GetElementById("child"));
    CHECK(painted.left == doctest::Approx(changed.left));
    CHECK(painted.width == doctest::Approx(changed.width));
    doc->Close();
    f.context->Update();
}

TEST_CASE("Real compact title drag clamps native geometry and saves settled group anchor [ui][placement][drag]")
{
    for (const bool linked : {false, true})
    {
        RmlViewLayoutFixture f;
        f.CreateInventoryGroup();
        f.Configure("munolith", {800, 600, 150});
        f.Inventory(0, 1);
        f.DragAndSave(linked);
    }
}

TEST_CASE(
    "Armory canonical geometry and real dense models need no gameplay sync before capture [ui][armory][placement]")
{
    RmlViewLayoutFixture f;
    f.CreateInventoryGroup();
    for (const auto v : {cases[0], cases[1], cases[2], Viewport{3840, 2160, 100}})
        for (bool editing : {false, true})
        {
            CAPTURE(v.width);
            CAPTURE(editing);
            f.Configure("munolith", v);
            f.Inventory(0, 1);
            f.PopulateArmory(editing);
            f.ProductionFrame();
            f.CheckArmoryGeometry();
            f.CheckArmoryText();
            f.CheckCoherent();
        }
}

TEST_CASE(
    "Document canonical token uses live root identity and cleans unplace replacement and reload [ui][placement][token]")
{
    RmlViewLayoutFixture f;
    auto load = [&]
    {
        auto* d = f.context->LoadDocumentFromMemory(
            "<rml><head><style>div {width:100px;height:50px;}</style></head><body><div id='one'/><div "
            "id='two'/></body></rml>");
        REQUIRE(d);
        d->Show();
        return d;
    };
    auto* doc = load();
    auto* other = load();
    auto* root = doc->GetElementById("one");
    UI::RmlBridge::ApplyLayoutScale(root, .75f);
    REQUIRE(doc->GetLocalProperty("--root-scale"));
    CHECK(doc->GetLocalProperty("--root-scale")->ToString() == "0.75");
    CHECK_FALSE(other->GetLocalProperty("--root-scale"));
    root->SetProperty("visibility", "hidden");
    UI::RmlBridge::SyncDocumentLayoutScales();
    CHECK(doc->GetLocalProperty("--root-scale")); // Still-placed hidden root retains its ownership.
    UI::RmlBridge::ApplyLayoutScale(doc->GetElementById("two"), 1.5f);
    CHECK(doc->GetLocalProperty("--root-scale")->ToString() == "0.75"); // Conflict never takes over another live root.
    auto retained = doc->RemoveChild(root);
    auto replacement = doc->CreateElement("div");
    replacement->SetId("one");
    auto* fresh = replacement.get();
    doc->AppendChild(std::move(replacement));
    UI::RmlBridge::SyncDocumentLayoutScales();
    CHECK_FALSE(doc->GetLocalProperty("--root-scale")); // Same ID != same identity.
    UI::RmlBridge::ApplyLayoutScale(fresh, .5f);
    CHECK(doc->GetLocalProperty("--root-scale"));
    UI::RmlBridge::ApplyLayoutScale(fresh, std::numeric_limits<float>::quiet_NaN());
    CHECK_FALSE(doc->GetLocalProperty("--root-scale"));
    UI::RmlBridge::SlotPlacement placement;
    placement.Set(10, 20, .8f);
    placement.Apply(doc, "one");
    REQUIRE(doc->GetLocalProperty("--root-scale"));
    placement.Set(0, 0, 0);
    placement.Apply(doc, "one");
    CHECK_FALSE(doc->GetLocalProperty("--root-scale"));
    placement.Set(10, 20, .8f);
    placement.Sync(doc, "one");
    CHECK(doc->GetLocalProperty("--root-scale"));
    fresh->SetAttribute("data-compact-scale", "invalid");
    UI::RmlBridge::ApplyLayoutScale(fresh, .8f, false);
    CHECK_FALSE(doc->GetLocalProperty("--root-scale"));
    UI::RmlBridge::ApplyLayoutScale(other->GetElementById("one"), 1.25f);
    doc->Close();
    f.context->Update();
    UI::RmlBridge::SyncDocumentLayoutScales();
    REQUIRE(other->GetLocalProperty("--root-scale"));
    CHECK(other->GetLocalProperty("--root-scale")->ToString() == "1.25");
    auto* reloaded = load();
    placement.Sync(reloaded, "one");
    CHECK(reloaded->GetLocalProperty("--root-scale"));
    retained.reset();
    reloaded->Close();
    other->Close();
    f.context->Update();
    UI::RmlBridge::SyncDocumentLayoutScales();
}

TEST_CASE("Bound capture extents use final content or full drawn box and defer invalid expired failed capture "
          "[ui][render-target][extent]")
{
    RmlViewLayoutFixture f;
    f.context->SetDensityIndependentPixelRatio(2.f);
    auto* doc = f.context->LoadDocumentFromMemory(
        "<rml><head><style>#crop "
        "{position:absolute;width:10px;height:10px;overflow:hidden;transform:scale(0.5);transform-origin:left top;} "
        "img{width:64dp;height:32dp;}.render-target-pending{opacity:0;}</style></head><body><div id='crop'><img "
        "id='image'/></div></body></rml>");
    REQUIRE(doc);
    doc->Show();
    f.Refresh();
    auto* image = doc->GetElementById("image");
    REQUIRE(image);
    using UI::RmlBridge::RenderTarget;
    for (auto extent : {RenderTarget::ImageExtent::Content, RenderTarget::ImageExtent::DrawnContent})
    {
        int captures = 0;
        std::uint32_t width = 0, height = 0;
        RenderTarget target(
            [&](auto w, auto h)
            {
                ++captures;
                width = w;
                height = h;
            });
        target.SetEnabled(true);
        target.BindImage(image, true, extent);
        const int hooks = f.beforeHooks;
        RmlUiRuntime::Instance().PrepareFrame();
        RmlUiRuntime::Instance().PrepareFrame(); // Prior preparation skipped submission.
        CHECK(f.beforeHooks == hooks + 2);
        captureRenderer.allowed.insert(captureRenderer.lastCreated + 1);
        RenderTarget::RenderAll();
        REQUIRE(captures == 1);
        CHECK(width == (extent == RenderTarget::ImageExtent::Content ? 128 : 64));
        CHECK(height == (extent == RenderTarget::ImageExtent::Content ? 64 : 32));
        const auto source = target.Source();
        RmlUiRuntime::Instance().RenderFrame();
        CHECK(f.beforeHooks == hooks + 2);
        image->SetProperty("visibility", "hidden");
        target.BindImage(image, true, extent);
        f.ProductionFrame();
        CHECK_FALSE(image->IsVisible(true));
        image->RemoveProperty("visibility");
        image->SetProperty("width", "0px");
        RmlUiRuntime::Instance().PrepareFrame();
        const int old = captures;
        RenderTarget::RenderAll();
        CHECK(captures == old);
        CHECK(target.Source() == source);
        image->SetProperty("width", "80dp");
        RmlUiRuntime::Instance().PrepareFrame();
        RenderTarget::RenderAll();
        CHECK(captures == old); // New-size capture denied.
        CHECK(target.Source() == source);
        captureRenderer.allowed.insert(captureRenderer.lastCreated);
        RenderTarget::RenderAll();
        CHECK(captures == old + 1);
        CHECK(target.Source() != source);
        auto removed = doc->GetElementById("crop")->RemoveChild(image);
        removed.reset();
        f.context->Update();
        const int alive = captures;
        RenderTarget::RenderAll();
        CHECK(captures == alive); // Expired observer never captures stale extent.
        auto replacement = doc->CreateElement("img");
        replacement->SetId("image");
        image = replacement.get();
        doc->GetElementById("crop")->AppendChild(std::move(replacement));
        f.Refresh();
    }
    doc->Close();
    f.context->Update();
}

TEST_CASE("Production first-open placement remains coherent across inherited themes [ui][placement][themes]")
{
    RmlViewLayoutFixture f;
    f.CreateInventoryGroup();
    for (const char* theme : {"legacy", "modern", "munolith"})
        for (const auto v : cases)
            for (bool linked : {false, true})
            {
                CAPTURE(std::string(theme));
                CAPTURE(v.width);
                CAPTURE(linked);
                f.Configure(theme, v);
                f.Inventory(0, 1);
                f.HidePanes();
                f.ProductionFrame();
                f.Reopen(linked);
                f.CheckCoherent();
                f.ProductionFrame();
                f.CheckCoherent();
                f.NoErrors();
            }
}

TEST_CASE("Visible actual name and unslotted registry entries do not arrange every idle tick [ui][placement][unplaced]")
{
    for (const char* theme : {"legacy", "modern", "munolith"})
    {
        RmlViewLayoutFixture f;
        f.Configure(theme, {1024, 768, 100});
        f.UnplacedIdle();
    }
}
TEST_CASE("Real NPC stock model reconciles camera and native grid before first capture [ui][npc][barrier]")
{
    for (const auto v : {cases[0], cases[1], cases[2], Viewport{3840, 2160, 100}})
    {
        RmlViewLayoutFixture f;
        f.CreateInventoryGroup();
        f.Configure("munolith", v);
        f.Inventory(0, 1);
        f.NpcTransition();
    }
}

TEST_CASE("Manager barrier includes actual unregistered dialog and shared event owner [ui][native-barrier][dialog]")
{
    RmlViewLayoutFixture f;
    f.Configure("munolith", {1024, 768, 100});
    f.UnregisteredDialogAndSharedView();
}
TEST_CASE("Existing held cursor layer refreshes camera before capture without recreating or changing held owner "
          "[ui][native-barrier][cursor]")
{
    RmlViewLayoutFixture f;
    f.Configure("munolith", {1024, 768, 100});
    f.CursorBarrier();
}

TEST_CASE("Actual chat frame material respects unframed composition with Armory inventory and system log "
          "[ui][chat][composition][nw3]")
{
    RmlViewLayoutFixture f;
    f.CheckChatComposition();
}

TEST_CASE("Equipment slot-pixel reads current visible owner DOM without layout or cached geometry "
          "[ui][equipment][control-query]")
{
    RmlViewLayoutFixture f;
    f.CheckEquipmentQuery();
}
