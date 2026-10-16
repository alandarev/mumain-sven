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
#include <RmlUi/Core/FontEngineInterface.h>
#include <RmlUi/Core/Mesh.h>
#include <RmlUi/Core/TextShapingContext.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "Data/GameConfig/GameConfig.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Render/RmlUi/RmlUiSystemInterface.h"
#include <RmlUi_Platform_SDL.h>
#include "UI/HUD/MuHelperBar.h"
#include "UI/Inventory/MyInventory.h"
#include "UI/Inventory/ItemHelpView.h"
#include "UI/NPCs/NPCShop.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Events/DoppelGangerWindow.h"
#include "UI/Inventory/CursorItemLayer.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/ItemMng.h"
#include "UI/Character/CharacterInfoWindow.h"
#include "UI/Character/Armory/ArmoryController.h"
#include "UI/Social/PhotoViewer.h"
#include "UI/HUD/ChatLogWindow.h"
#include "UI/Core/WindowCommon.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
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
#include "Render/Text/CUIRenderText.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/Scaling/UITransform.h"
#include <filesystem>
#include <ostream>

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
    std::pair<int, int> PaintBand(Rml::ColourbPremultiplied colour) const
    {
        std::pair<int, int> band{-1, -1};
        for (int i = 0; i < static_cast<int>(paint.size()); ++i)
            if (std::any_of(paint[i].vertices.begin(), paint[i].vertices.end(),
                            [colour](const auto& vertex) { return vertex.colour == colour; }))
            {
                if (band.first < 0)
                    band.first = i;
                band.second = i;
            }
        return band;
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
// The first and last history rows the clipped chat log actually shows.
struct PaintedRows
{
    int first = -1, last = -1;
    bool operator==(const PaintedRows&) const = default;
};
std::ostream& operator<<(std::ostream& os, const PaintedRows& rows)
{
    return os << rows.first << ".." << rows.last;
}
// How far right a laid-out line's advance (the sum of its glyph advances, not its drawn ink) may
// reach, in layout px. RmlUi's LineBox::AddBox() offers a wrapping inline fragment
// RoundUp(line box width - cursor) (Layout/LineBox.cpp), so a wrapped line's advance may end up to
// one pixel past a line box of fractional width; otherwise the existing half-pixel budget holds.
// The two are alternatives, never added together.
float LineAdvanceLimit(float lineBoxLeft, float lineBoxWidth, float cursor, float nominalRight, bool wraps)
{
    const float nominal = nominalRight + 0.5f;
    return wraps ? std::max(nominal, lineBoxLeft + cursor + std::ceil(lineBoxWidth - cursor)) : nominal;
}
// A history row's text outside the well: which bound (`box` -- the row's border box past the well's
// client width; `advance` -- a line's advance past LineAdvanceLimit(); `ink` -- a drawn glyph quad
// past the well's client edge, where the scrollbar and frame begin) and by how much, in layout px.
struct HistoryOverflow
{
    int row;
    const char* bound;
    float overflow;
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
            UI::RmlBridge::ApplySceneStackingDepths();
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

    Rml::ElementDocument* Document(const char* name)
    {
        for (int i = 0; i < context->GetNumDocuments(); ++i)
            if (context->GetDocument(i)->GetSourceURL().ends_with(name))
                return context->GetDocument(i);
        return nullptr;
    }
    void CheckPassivePaint(bool passive, mu::ui::window::CChatLogWindow& chat)
    {
        auto* history = Document("chat_log.rml");
        auto* notices = Document("system_log.rml");
        REQUIRE(history);
        REQUIRE(notices);
        CHECK(history->GetAttribute<Rml::String>("data-stacking-role", "") == (passive ? "passive-log" : ""));
        CHECK(notices->GetAttribute<Rml::String>("data-stacking-role", "") == (passive ? "passive-log" : ""));
        CHECK(Document("chat_input.rml")->GetAttribute<Rml::String>("data-stacking-role", "").empty());
        CHECK(history->IsVisible());
        CHECK(notices->IsVisible());
        CHECK(history->GetElementById("lines")->GetInnerRML().find("Peer") != Rml::String::npos);
        CHECK(notices->GetElementById("panel")->GetInnerRML().find("Welcome") != Rml::String::npos);
        if (passive)
        {
            renderer.tracePaint = true;
            renderer.paint.clear();
            context->Render();
            const auto logs = renderer.PaintBand({9, 15, 9, 153});  // Unmodified authored material at .6.
            const auto stone = renderer.PaintBand({9, 11, 9, 255}); // Real Armory stone background.
            REQUIRE(logs.first >= 0);
            REQUIRE(stone.first >= 0);
            CHECK(logs.second < stone.first); // Actual system rows and, when framed, chat backdrop.
            const auto messages = renderer.PaintBand(chat.IsShowFrame() ? Rml::ColourbPremultiplied{8, 14, 7, 170}
                                                                        : Rml::ColourbPremultiplied{9, 17, 8, 204});
            REQUIRE(messages.first >= 0);
            CHECK(messages.second < stone.first); // Chat messages themselves, including unframed history.
            renderer.tracePaint = false;
        }
        else
        {
            CHECK(history->GetZIndex() == doctest::Approx(6.1f));
            CHECK(notices->GetZIndex() == doctest::Approx(6.05f));
        }
        CHECK(chat.GetBackAlpha() == doctest::Approx(.6f));
    }
    void CheckCoveredHistory(mu::ui::window::CChatLogWindow& chat)
    {
        auto* title = inventory->GetPlacedDocument()->GetElementById("title");
        REQUIRE(title);
        const auto caption = Box(*title);
        const int tx = std::lround(caption.left + caption.width / 2.f);
        const int ty = std::lround(caption.top + caption.height / 2.f);
        context->ProcessMouseMove(tx, ty, 0);
        context->ProcessMouseButtonDown(0, 0);
        context->ProcessMouseMove(tx - 40, ty, 0);
        context->ProcessMouseMove(tx - WindowWidth, ty + WindowHeight / 2, 0);
        context->ProcessMouseButtonUp(0, 0);
        ProductionFrame(); // Actual title/group drag into overlap; no fixture coordinates/placement writes.
        auto* history = Document("chat_log.rml");
        const auto handle = Box(*history->GetElementById("resize_handle"));
        const auto panel = Box(*character->GetPlacedDocument()->GetElementById("panel"));
        const float left = std::max(handle.left, panel.left),
                    right = std::min(handle.left + handle.width, panel.left + panel.width);
        REQUIRE(left < right);
        const int x = std::lround((left + right) / 2.f), y = std::lround(handle.top + handle.height / 2.f);
        context->ProcessMouseMove(x, y, 0);
        auto* hit = context->GetElementAtPoint({static_cast<float>(x), static_cast<float>(y)});
        REQUIRE(hit);
        CHECK(hit->GetOwnerDocument() == character->GetPlacedDocument());
        // Do not activate a protected foreground action; the real engine resolves its hit owner.
        auto* lines = history->GetElementById("lines");
        const auto well = Box(*lines);
        for (int i = 0; i < lines->GetNumChildren(); ++i)
        {
            auto* row = lines->GetChild(i);
            Rml::Vector2f offset, size;
            if (!UI::RmlBridge::DrawnContentBox(*row, offset, size))
                continue;
            const Rect b{offset.x, offset.y, size.x, size.y};
            const Rml::Vector2f point(std::max(b.left, panel.left) + 2.f, b.top + b.height / 2.f);
            auto* owner = context->GetElementAtPoint(point);
            if (!owner || owner->GetOwnerDocument() != character->GetPlacedDocument() || point.x >= b.left + b.width ||
                point.y < well.top || point.y >= well.top + well.height)
                continue;
            context->ProcessMouseMove(std::lround(point.x), std::lround(point.y), 0);
            g_fWindowMouseX = std::lround(point.x);
            g_fWindowMouseY = std::lround(point.y);
            const float oldScroll = lines->GetScrollTop();
            MouseWheel = 1;
            CHECK(chat.UpdateMouseEvent());
            CHECK(MouseWheel == 1);
            MouseWheel = 0;
            chat.Update();
            context->Update();
            CHECK_FALSE(row->IsClassSet("chat-line--pointed"));
            CHECK(lines->GetScrollTop() == doctest::Approx(oldScroll));
            return;
        }
        FAIL("No actual history row overlaps the foreground Armory");
    }
    void CheckExposedHistory(mu::ui::window::CChatLogWindow& chat)
    {
        HidePanes();
        ProductionFrame();
        auto* lines = Document("chat_log.rml")->GetElementById("lines");
        const auto b = Box(*lines);
        const int x = std::lround(b.left + b.width / 2.f), y = std::lround(b.top + b.height / 2.f);
        context->ProcessMouseMove(x, y, 0);
        g_fWindowMouseX = x;
        g_fWindowMouseY = y;
        chat.AfterRmlLayout(context);
        const auto oldScroll = lines->GetScrollTop();
        REQUIRE(oldScroll > 0.f);
        MouseWheel = 1;
        CHECK_FALSE(chat.UpdateMouseEvent());
        CHECK(MouseWheel == 0);
        CHECK(lines->GetScrollTop() < oldScroll);
        chat.Update();
        context->Update();
        CHECK(Document("chat_log.rml")->QuerySelector(".chat-line--pointed") != nullptr);
    }
    void CheckHistoryNavigation(mu::ui::window::CChatInputBox& input, mu::ui::window::CChatLogWindow& chat)
    {
        using namespace mu::ui::window;
        const auto key = [&](Rml::Input::KeyIdentifier identifier)
        {
            context->ProcessKeyDown(identifier, 0);
            context->ProcessKeyUp(identifier, 0);
            input.Update();
            context->Update();
        };
        key(Rml::Input::KI_F2);
        CHECK(chat.GetCurrentMsgType() == TYPE_WHISPER_MESSAGE);
        key(Rml::Input::KI_F2);
        CHECK(chat.GetCurrentMsgType() == TYPE_ALL_MESSAGE);
        key(Rml::Input::KI_F3);
        CHECK(Document("chat_input.rml")->GetElementById("whisper_field")->HasAttribute("disabled"));
        key(Rml::Input::KI_F3);
        CHECK_FALSE(Document("chat_input.rml")->GetElementById("whisper_field")->HasAttribute("disabled"));
        context->ProcessKeyDown(Rml::Input::KI_F5, 0);
        context->ProcessKeyUp(Rml::Input::KI_F5, 0);
        REQUIRE(chat.IsShowFrame());
        const auto count = chat.GetNumberOfShowingLines();
        context->ProcessKeyDown(Rml::Input::KI_F4, 0);
        context->ProcessKeyUp(Rml::Input::KI_F4, 0);
        CHECK(chat.GetNumberOfShowingLines() != count);
        const auto end = chat.GetCurrentRenderEndLine();
        context->ProcessKeyDown(Rml::Input::KI_PRIOR, 0);
        context->ProcessKeyUp(Rml::Input::KI_PRIOR, 0);
        CHECK(chat.GetCurrentRenderEndLine() < end);
        context->ProcessKeyDown(Rml::Input::KI_NEXT, 0);
        context->ProcessKeyUp(Rml::Input::KI_NEXT, 0);
        CHECK(chat.GetCurrentRenderEndLine() == end);
    }
    void CheckChatKeys(mu::ui::window::CChatInputBox& input, mu::ui::window::CChatLogWindow& chat)
    {
        using namespace mu::ui::window;
        input.Show(true);
        input.Update();
        ProductionFrame();
        auto* field = Document("chat_input.rml")->GetElementById("chat_field");
        REQUIRE(field);
        REQUIRE(field->Focus());
        REQUIRE(input.OwnsFocusedInput());
        const auto fieldBox = Box(*field);
        auto* fieldHit =
            context->GetElementAtPoint({fieldBox.left + fieldBox.width / 2.f, fieldBox.top + fieldBox.height / 2.f});
        REQUIRE(fieldHit);
        CHECK(fieldHit->GetOwnerDocument() == Document("chat_input.rml"));
        if (field->GetAttribute<Rml::String>("value", "").empty())
            context->ProcessTextInput("unsent");
        context->Update();
        const auto draft = field->GetAttribute<Rml::String>("value", "");
        REQUIRE(draft == "unsent");
        chat.HideFrame();
        context->ProcessKeyDown(Rml::Input::KI_F5, 0);
        CHECK(chat.IsShowFrame());
        g_pNewKeyInput->SetKeyState(VK_F5, CNewKeyInput::KEY_PRESS);
        input.UpdateKeyEvent(); // Physical held key after focused callback must NOT run twice.
        g_pNewKeyInput->SetKeyState(VK_F5, CNewKeyInput::KEY_NONE);
        CHECK(chat.IsShowFrame());
        context->ProcessKeyDown(Rml::Input::KI_F5, 0);
        CHECK(chat.IsShowFrame()); // Repeat suppressed until keyup.
        context->ProcessKeyUp(Rml::Input::KI_F5, 0);
        context->ProcessKeyDown(Rml::Input::KI_F5, 0);
        CHECK_FALSE(chat.IsShowFrame());
        context->ProcessKeyUp(Rml::Input::KI_F5, 0);
        CheckHistoryNavigation(input, chat);
        chat.HideFrame();
        input.Show(false);
        field->Blur();
        g_pNewKeyInput->SetKeyState(VK_F5, CNewKeyInput::KEY_PRESS);
        CHECK_FALSE(input.UpdateKeyEvent()); // Hidden-input ACK is no frame change.
        g_pNewKeyInput->SetKeyState(VK_F5, CNewKeyInput::KEY_NONE);
        CHECK_FALSE(chat.IsShowFrame());
        CHECK(field->GetAttribute<Rml::String>("value", "") == draft);
    }
    void CheckReferencePriority(mu::ui::window::ItemHelpView& reference)
    {
        reference.Sync(true, {{"Reference", "white", false, ""}});
        ProductionFrame();
        auto* document = Document("item_explanation.rml");
        REQUIRE(document);
        if (document->IsClassSet("reference-foreground"))
            CHECK(document->GetZIndex() == doctest::Approx(UI::RmlBridge::ForegroundPanelLayerDepth));
        else
            CHECK(document->GetZIndex() == doctest::Approx(6.5f));
        reference.Sync(false, {});
    }
    void CheckPassiveLogs()
    {
        using namespace mu::ui::window;
        CreateInventoryGroup();
        CChatLogWindow chat;
        CSystemLogWindow notices;
        CChatInputBox input;
        ItemHelpView reference("item_explanation", "Data/Interface/RmlUi/item_explanation.rml");
        REQUIRE(chat.Create(&manager, 15));
        REQUIRE(notices.Create(&manager));
        REQUIRE(input.Create(&manager, &chat, &notices));
        chat.Show(true);
        notices.Show(true);
        notices.AddText(L"Welcome", TYPE_SYSTEM_MESSAGE);
        for (int i = 0; i < 60; ++i)
            chat.AddText(L"Peer", L"Line " + std::to_wstring(i), TYPE_CHAT_MESSAGE);
        for (const char* selected : {"munolith", "legacy", "modern", "munolith"})
            for (const auto v : {cases[0], cases[1], cases[2], Viewport{3840, 2160, 100}})
            {
                CAPTURE(std::string(selected));
                CAPTURE(v.width);
                Configure(selected, v);
                chat.SetNumberOfShowingLines(15);
                inventory->CObject::Show(true);
                character->CObject::Show(true);
                inventory->GetPlacedDocument()->Show();
                character->GetPlacedDocument()->Show();
                Inventory(0, 1);
                const bool passive = UI::RmlBridge::ThemeUsesEmbeddedInventory();
                if (passive)
                    PopulateArmory(false);
                chat.SyncDocVisibility(true);
                notices.SyncDocVisibility(true);
                input.Update();
                for (const bool frame : {false, true, false})
                {
                    frame ? chat.ShowFrame() : chat.HideFrame();
                    chat.Update();
                    notices.Update();
                    ProductionFrame();
                    chat.AfterRmlLayout(context);
                    CheckPassivePaint(passive, chat);
                }
                chat.ShowFrame();
                chat.Update();
                ProductionFrame();
                chat.AfterRmlLayout(context);
                if (passive)
                    CheckCoveredHistory(chat);
                CheckExposedHistory(chat);
                CheckChatKeys(input, chat);
                CheckReferencePriority(reference);
                NoErrors();
            }
    }

    void CheckFocusedControlReload()
    {
        using namespace mu::ui::window;
        auto& runtime = RmlUiRuntime::Instance();
        auto oldSystem = std::move(runtime.m_SystemInterface);
        auto oldIme = std::move(runtime.m_TextInputMethodEditor);
        runtime.m_SystemInterface = std::make_unique<RmlUiSystemInterface>(nullptr);
        runtime.m_TextInputMethodEditor = std::make_unique<TextInputMethodEditor_SDL>();
        Rml::SetSystemInterface(runtime.m_SystemInterface.get());
        Rml::SetTextInputHandler(runtime.m_TextInputMethodEditor.get());
        const auto restore = std::unique_ptr<RmlUiRuntime, std::function<void(RmlUiRuntime*)>>(&runtime,
            [&](auto*)
            {
                if (auto* focus = context->GetFocusElement()) focus->Blur();
                Rml::SetTextInputHandler(nullptr);
                Rml::SetSystemInterface(&log);
                runtime.m_SystemInterface = std::move(oldSystem);
                runtime.m_TextInputMethodEditor = std::move(oldIme);
            });
        CreateInventoryGroup();
        CChatLogWindow chat;
        CSystemLogWindow notices;
        CChatInputBox input;
        REQUIRE(chat.Create(&manager, 6));
        REQUIRE(notices.Create(&manager));
        REQUIRE(input.Create(&manager, &chat, &notices));
        Configure("munolith", {800, 600, 150});
        Inventory(0, 1);
        PopulateArmory(false);
        CUIPhotoViewer viewer;
        auto& photo = character->m_Armory->m_PhotoControl;
        photo.AttachOrbit(character->GetPlacedDocument(), viewer, "armory_model_controls", "armory_preview");
        const auto detachPhoto = std::unique_ptr<CUIPhotoViewer, std::function<void(CUIPhotoViewer*)>>(&viewer,
            [&](auto*) { photo.Detach(); });
        auto oldDocument = character->GetPlacedDocument()->GetObserverPtr();
        chat.Show(true);
        notices.Show(true);
        chat.SyncDocVisibility(true);
        notices.SyncDocVisibility(true);
        for (int i = 0; i < 34; ++i)
            chat.AddText(L"QA640", L"SEEDED UI640 row " + std::to_wstring(i), TYPE_CHAT_MESSAGE);
        chat.Update();
        notices.Update();
        chat.ShowFrame();
        input.Show(true);
        input.Update();
        ProductionFrame();
        chat.AfterRmlLayout(context);
        auto* field = Document("chat_input.rml")->GetElementById("chat_field");
        REQUIRE(field->Focus());
        context->ProcessTextInput("B640 neutral unsent");
        context->Update();
        REQUIRE(runtime.GetTypingDocument() == Document("chat_input.rml"));
        for (auto key : {Rml::Input::KI_PRIOR, Rml::Input::KI_NEXT, Rml::Input::KI_F2, Rml::Input::KI_F2,
                        Rml::Input::KI_F3, Rml::Input::KI_F3, Rml::Input::KI_F4, Rml::Input::KI_F4,
                        Rml::Input::KI_F4, Rml::Input::KI_F4, Rml::Input::KI_F4})
        {
            context->ProcessKeyDown(key, 0);
            context->ProcessKeyUp(key, 0);
            input.Update();
            chat.Update();
            ProductionFrame();
            chat.AfterRmlLayout(context);
        }
        REQUIRE(chat.GetNumberOfShowingLines() == 6);
        REQUIRE(runtime.GetTypingDocument() == Document("chat_input.rml"));
        REQUIRE(field->GetAttribute<Rml::String>("value", "") == "B640 neutral unsent");
        std::unique_ptr<App::Control::Act> act;
        const auto request = App::Control::Request::Parse(R"({"cmd":"ui","action":"theme","name":"munolith"})");
        const auto reply = App::Control::Commands::Ui(request, act);
        REQUIRE(reply.empty());
        REQUIRE(act);
        std::string answer;
        act->Tick(answer);
        ProductionFrame();
        CHECK_FALSE(oldDocument); // Actual engine retired the old document before the next native model sync.
        photo.AttachOrbit(character->GetPlacedDocument(), viewer, "armory_model_controls", "armory_preview");
        photo.Sync();
        input.Update();
        chat.Update();
        notices.Update();
        ProductionFrame();
        act->Tick(answer);
        CHECK_FALSE(answer.empty());
        CHECK(Document("chat_input.rml")->GetElementById("chat_field")->GetAttribute<Rml::String>("value", "") ==
              "B640 neutral unsent");
        CHECK(chat.GetNumberOfLines(TYPE_ALL_MESSAGE) == 34);
        ProductionFrame();

        // Second same-theme reload: the owner's own document handle (what ArmoryController::Sync()
        // records) is read by a hover before the next Sync(), and a window hidden across the switch
        // detaches (Suspend()) from the retired document before attaching the rebuilt one.
        auto& armory = *character->m_Armory;
        armory.m_Document = character->GetPlacedDocument()->GetObserverPtr();
        oldDocument = character->GetPlacedDocument()->GetObserverPtr();
        act.reset();
        REQUIRE(App::Control::Commands::Ui(request, act).empty());
        REQUIRE(act);
        answer.clear();
        CHECK(act->Tick(answer) == App::Control::Act::Status::Running);
        ProductionFrame();
        CHECK_FALSE(oldDocument);
        armory.HoverAttribute(0);
        photo.Detach();
        photo.AttachOrbit(character->GetPlacedDocument(), viewer, "armory_model_controls", "armory_preview");
        photo.Sync();
        input.Update();
        chat.Update();
        notices.Update();
        ProductionFrame();
        CHECK(act->Tick(answer) == App::Control::Act::Status::Finished);
        CHECK(Document("chat_input.rml")->GetElementById("chat_field")->GetAttribute<Rml::String>("value", "") ==
              "B640 neutral unsent");
        CHECK(chat.GetNumberOfLines(TYPE_ALL_MESSAGE) == 34);
        armory.HoverAttribute(-1);
        ProductionFrame();
    }
    // First and last history rows (message indices) whose drawn box lies inside the well's drawn
    // content box -- what the clipped log actually shows.
    PaintedRows PaintedHistoryRows()
    {
        auto* lines = Document("chat_log.rml")->GetElementById("lines");
        Rml::Rectanglef well;
        REQUIRE(Rml::ElementUtilities::GetBoundingBox(well, lines, Rml::BoxArea::Content));
        PaintedRows rows;
        for (int i = 0; i < lines->GetNumChildren(); ++i)
        {
            Rml::Rectanglef row;
            if (!Rml::ElementUtilities::GetBoundingBox(row, lines->GetChild(i), Rml::BoxArea::Border))
                continue;
            const float middle = (row.Top() + row.Bottom()) / 2.f;
            if (middle < well.Top() || middle >= well.Bottom())
                continue;
            // DOM order is the message vector's order; a seeded row carries its own index.
            const auto text = lines->GetChild(i)->GetInnerRML();
            const auto at = text.rfind("SEEDED UI640 row ");
            if (at != Rml::String::npos)
                CHECK(std::stoi(text.substr(at + 17)) == i);
            rows.first = rows.first < 0 ? i : rows.first;
            rows.last = i;
        }
        return rows;
    }
    // History rows -- painted or scrolled out of view, a row's width does not depend on that --
    // whose text leaves the well's visible width (its client width: inside the padding and left of
    // the scrollbar). Layout px: the chat log carries no transform, so they are also screen px.
    // Each laid-out line is checked twice, independently: its advance against the engine's own line
    // envelope (LineAdvanceLimit(), from the row's content box and the line's cursor), and its ink --
    // the glyph quads the row's font engine generates for that line at that position, with the row's
    // own face and font effects, exactly as ElementText draws it -- against the well's client edge.
    std::vector<HistoryOverflow> ClippedHistoryRows(bool report = false)
    {
        auto* lines = Document("chat_log.rml")->GetElementById("lines");
        const float wellRight = lines->GetAbsoluteLeft() + lines->GetClientLeft() + lines->GetClientWidth();
        auto* fonts = Rml::GetFontEngineInterface();
        std::vector<HistoryOverflow> overflows;
        for (int i = 0; i < lines->GetNumChildren(); ++i)
        {
            Rml::Element* row = lines->GetChild(i);
            if (!row->IsVisible() || row->GetInnerRML().empty()) // data-for's own template stays hidden
                continue;
            const float boxRight = row->GetAbsoluteLeft() + row->GetOffsetWidth();
            if (boxRight > wellRight + 0.5f)
                overflows.push_back({i, "box", boxRight - wellRight});
            const float lineBoxLeft = row->GetAbsoluteOffset(Rml::BoxArea::Content).x;
            const float lineBoxWidth = row->GetBox().GetSize(Rml::BoxArea::Content).x;
            const auto whiteSpace = row->GetComputedValues().white_space();
            const bool wraps = whiteSpace == Rml::Style::WhiteSpace::Normal ||
                               whiteSpace == Rml::Style::WhiteSpace::Prewrap ||
                               whiteSpace == Rml::Style::WhiteSpace::Preline;
            float advanceRight = 0.f, advanceLimit = 0.f, inkRight = 0.f;
            for (int c = 0; c < row->GetNumChildren(); ++c)
            {
                auto* text = dynamic_cast<Rml::ElementText*>(row->GetChild(c));
                if (!text)
                    continue;
                const auto face = text->GetFontFaceHandle();
                REQUIRE(face);
                const Rml::FontEffectList none;
                const Rml::FontEffectList* effects = &none;
                if (text->GetComputedValues().has_font_effect())
                    if (const Rml::Property* property = text->GetProperty(Rml::PropertyId::FontEffect))
                        if (Rml::FontEffectsPtr list = property->Get<Rml::FontEffectsPtr>())
                            effects = &list->list;
                const auto effectsHandle = fonts->PrepareFontEffects(face, *effects);
                const auto& computed = text->GetComputedValues();
                const Rml::TextShapingContext shaping{computed.language(), computed.direction(),
                                                      computed.font_kerning(), computed.letter_spacing()};
                const Rml::Vector2f origin = text->GetAbsoluteOffset();
                for (const auto& line : text->GetLines())
                {
                    const float lineLeft = origin.x + line.position.x;
                    const float cursor = std::max(0.f, lineLeft - lineBoxLeft);
                    const float limit = LineAdvanceLimit(lineBoxLeft, lineBoxWidth, cursor, wellRight, wraps);
                    // A wrapped line keeps the space it broke at; the engine does not draw it.
                    Rml::String drawn = line.text;
                    while (!drawn.empty() && std::isspace(static_cast<unsigned char>(drawn.back())))
                        drawn.pop_back();
                    const float drawnAdvance =
                        lineLeft + static_cast<float>(fonts->GetStringWidth(face, drawn, shaping));
                    if (drawnAdvance > limit)
                        overflows.push_back({i, "advance", drawnAdvance - limit});
                    Rml::TexturedMeshList meshes;
                    fonts->GenerateString(*row->GetRenderManager(), face, effectsHandle, line.text, line.position,
                                          Rml::ColourbPremultiplied(255, 255, 255, 255), 1.f, shaping, meshes);
                    float lineInk = 0.f;
                    for (const auto& mesh : meshes)
                        for (const auto& vertex : mesh.mesh.vertices)
                            lineInk = std::max(lineInk, origin.x + vertex.position.x);
                    if (lineInk > wellRight)
                        overflows.push_back({i, "ink", lineInk - wellRight});
                    if (drawnAdvance > advanceRight)
                    {
                        advanceRight = drawnAdvance;
                        advanceLimit = limit;
                    }
                    inkRight = std::max(inkRight, lineInk);
                }
            }
            if (report && advanceRight > wellRight + 0.5f)
                MESSAGE("row " << i << " advance " << advanceRight << " beyond the well's " << wellRight
                               << " + 0.5 within the engine's wrap envelope " << advanceLimit << "; ink " << inkRight
                               << std::string(inkRight <= wellRight ? " inside" : " OUTSIDE") << " the well");
        }
        return overflows;
    }
    // The history's visible anchor, in the logical history contract (CChatLogWindow's end line): the
    // last message whose rendered row is at least half inside the well, by message index and text,
    // and how far that row's bottom (its margin box: a Legacy row's bottom margin is its pitch to
    // the next row) lies below the well's client bottom, in layout px (0 = the row ends exactly at
    // the well's bottom; positive = partly cut off).
    struct HistoryAnchor
    {
        int index = -1;
        Rml::String text;
        float belowBottom = 0.f;
        float height = 0.f;
    };
    HistoryAnchor VisibleHistoryAnchor(int messages)
    {
        auto* lines = Document("chat_log.rml")->GetElementById("lines");
        const float wellBottom = lines->GetAbsoluteTop() + lines->GetClientTop() + lines->GetClientHeight();
        HistoryAnchor anchor;
        for (int i = 0; i < std::min(messages, lines->GetNumChildren()); ++i)
        {
            Rml::Element* row = lines->GetChild(i);
            const float top = row->GetAbsoluteTop(), height = row->GetOffsetHeight();
            const float pitch = row->GetBox().GetEdge(Rml::BoxArea::Margin, Rml::BoxEdge::Bottom);
            if (top + height / 2.f <= wellBottom)
                anchor = {i, row->GetInnerRML(), top + height + pitch - wellBottom, height};
        }
        return anchor;
    }
    // Rendered heights of the history rows from `first` to `last`.
    std::vector<float> HistoryRowHeights(int first, int last)
    {
        auto* lines = Document("chat_log.rml")->GetElementById("lines");
        std::vector<float> heights;
        for (int i = std::max(first, 0); i <= last && i < lines->GetNumChildren(); ++i)
            heights.push_back(lines->GetChild(i)->GetOffsetHeight());
        return heights;
    }
    // Real native text measuring (CUIRenderTextSDLTtf over bundled faces opened at their cached
    // point size, as the renderer does) for the scope of one test: CChatLogWindow::SeparateText()
    // splits a long message with it.
    struct NativeTextMeasuring
    {
        NativeTextMeasuring()
        {
            captureRenderer.textEngine = TTF_CreateSurfaceTextEngine();
            REQUIRE(captureRenderer.textEngine);
            captureRenderer.textFont =
                TTF_OpenFont((std::string(MU_FONT_DIR) + "/DejaVuSans.ttf").c_str(),
                             static_cast<float>(UI::Scaling::CachedFontPointSize(UI::Scaling::FontRole::Normal)));
            captureRenderer.textFontBold =
                TTF_OpenFont((std::string(MU_FONT_DIR) + "/DejaVuSans-Bold.ttf").c_str(),
                             static_cast<float>(UI::Scaling::CachedFontPointSize(UI::Scaling::FontRole::Bold)));
            REQUIRE(captureRenderer.textFont);
            REQUIRE(captureRenderer.textFontBold);
            REQUIRE(g_pRenderText->Create(nullptr));
            const UI::Scaling::ScopedActiveTransform units(UI::Scaling::TypographyUnitsTransform(1024, 768));
            REQUIRE(g_pRenderText->MeasureText(L"W", 1).cx > 0);
        }
        ~NativeTextMeasuring()
        {
            g_pRenderText->Release();
            TTF_CloseFont(captureRenderer.textFont);
            TTF_CloseFont(captureRenderer.textFontBold);
            TTF_DestroySurfaceTextEngine(captureRenderer.textEngine);
            captureRenderer.textFont = captureRenderer.textFontBold = nullptr;
            captureRenderer.textEngine = nullptr;
        }
    };
    // F1 (native 1fd5): framed history at its tail painted the OLDEST rows after a same-theme
    // control reload, and PageDown/wheel then moved from there. Production frame order: window
    // Update() (SyncRmlModel), then the context update whose afterUpdate hook runs the chat's
    // AfterRmlLayout() (RmlRuntimeHooks.cpp), then render.
    void CheckFramedHistoryReload()
    {
        using namespace mu::ui::window;
        auto& runtime = RmlUiRuntime::Instance();
        auto oldSystem = std::move(runtime.m_SystemInterface);
        auto oldIme = std::move(runtime.m_TextInputMethodEditor);
        runtime.m_SystemInterface = std::make_unique<RmlUiSystemInterface>(nullptr);
        runtime.m_TextInputMethodEditor = std::make_unique<TextInputMethodEditor_SDL>();
        Rml::SetSystemInterface(runtime.m_SystemInterface.get());
        Rml::SetTextInputHandler(runtime.m_TextInputMethodEditor.get());
        const auto fixtureHooks = runtime.m_Hooks;
        CChatLogWindow chat;
        const auto restore = std::unique_ptr<RmlUiRuntime, std::function<void(RmlUiRuntime*)>>(
            &runtime,
            [&](auto*)
            {
                if (auto* focus = context->GetFocusElement())
                    focus->Blur();
                runtime.SetHooks(fixtureHooks);
                Rml::SetTextInputHandler(nullptr);
                Rml::SetSystemInterface(&log);
                runtime.m_SystemInterface = std::move(oldSystem);
                runtime.m_TextInputMethodEditor = std::move(oldIme);
            });
        auto hooks = fixtureHooks;
        hooks.afterUpdate = [&, fixture = fixtureHooks.afterUpdate](Rml::Context* c)
        {
            fixture(c);
            chat.AfterRmlLayout(c);
        };
        runtime.SetHooks(std::move(hooks));
        CreateInventoryGroup();
        CSystemLogWindow notices;
        CChatInputBox input;
        REQUIRE(chat.Create(&manager, 6));
        REQUIRE(notices.Create(&manager));
        REQUIRE(input.Create(&manager, &chat, &notices));
        Configure("munolith", {800, 600, 150});
        Inventory(0, 1);
        PopulateArmory(false);
        CUIPhotoViewer viewer;
        auto& photo = character->m_Armory->m_PhotoControl;
        photo.AttachOrbit(character->GetPlacedDocument(), viewer, "armory_model_controls", "armory_preview");
        const auto detachPhoto = std::unique_ptr<CUIPhotoViewer, std::function<void(CUIPhotoViewer*)>>(
            &viewer, [&](auto*) { photo.Detach(); });
        const auto frame = [&]
        {
            input.Update();
            chat.Update();
            notices.Update();
            ProductionFrame();
        };
        const auto key = [&](Rml::Input::KeyIdentifier identifier)
        {
            context->ProcessKeyDown(identifier, 0);
            context->ProcessKeyUp(identifier, 0);
            frame();
        };
        chat.Show(true);
        notices.Show(true);
        chat.SyncDocVisibility(true);
        notices.SyncDocVisibility(true);
        for (int i = 0; i < 34; ++i)
            chat.AddText(L"QA640", L"SEEDED UI640 row " + std::to_wstring(i), TYPE_CHAT_MESSAGE);
        input.Show(true);
        frame();
        auto* field = Document("chat_input.rml")->GetElementById("chat_field");
        REQUIRE(field->Focus());
        context->ProcessTextInput("B640 neutral unsent");
        frame();
        REQUIRE_FALSE(chat.IsShowFrame());
        key(Rml::Input::KI_F5);
        REQUIRE(chat.IsShowFrame());
        for (auto k : {Rml::Input::KI_PRIOR, Rml::Input::KI_NEXT, Rml::Input::KI_F2, Rml::Input::KI_F2,
                       Rml::Input::KI_F3, Rml::Input::KI_F3, Rml::Input::KI_F4, Rml::Input::KI_F4, Rml::Input::KI_F4,
                       Rml::Input::KI_F4, Rml::Input::KI_F4})
            key(k);
        REQUIRE(chat.GetNumberOfShowingLines() == 6);
        frame();
        const auto tail = PaintedHistoryRows();
        REQUIRE(tail.last == 33);
        REQUIRE(tail.first > 0);
        REQUIRE(chat.GetCurrentRenderEndLine() == 33);
        const int visibleRows = tail.last - tail.first + 1;

        const auto reload = [&](const char* theme)
        {
            std::unique_ptr<App::Control::Act> act;
            const auto request =
                App::Control::Request::Parse(std::string(R"({"cmd":"ui","action":"theme","name":")") + theme + "\"}");
            REQUIRE(App::Control::Commands::Ui(request, act).empty());
            REQUIRE(act);
            std::string answer;
            CHECK(act->Tick(answer) == App::Control::Act::Status::Running);
            frame();
            const auto firstObservable = PaintedHistoryRows();
            photo.AttachOrbit(character->GetPlacedDocument(), viewer, "armory_model_controls", "armory_preview");
            photo.Sync();
            frame();
            CHECK(act->Tick(answer) == App::Control::Act::Status::Finished);
            frame();
            CHECK(PaintedHistoryRows() == firstObservable);
            CHECK(Document("chat_input.rml")->GetElementById("chat_field")->GetAttribute<Rml::String>("value", "") ==
                  "B640 neutral unsent");
            return firstObservable;
        };

        // Following the tail: the reload keeps the tail, and the next message is still followed.
        CHECK(reload("munolith") == tail);
        CHECK(chat.GetCurrentRenderEndLine() == 33);
        CHECK(reload("munolith") == tail);
        chat.AddText(L"QA640", L"SEEDED UI640 row 34", TYPE_CHAT_MESSAGE);
        frame();
        frame();
        CHECK(PaintedHistoryRows().last == 34);

        // Scrolled away: the reload keeps that viewport; PageUp/PageDown continue from it.
        field = Document("chat_input.rml")->GetElementById("chat_field");
        REQUIRE(field->Focus());
        key(Rml::Input::KI_PRIOR);
        frame();
        const auto away = PaintedHistoryRows();
        REQUIRE(away.last < 34);
        REQUIRE(away.first > 0);
        CHECK(reload("munolith") == away);
        chat.AddText(L"QA640", L"SEEDED UI640 row 35", TYPE_CHAT_MESSAGE);
        frame();
        frame();
        CHECK(PaintedHistoryRows() == away); // A new line does not pull a scrolled-away view.
        field = Document("chat_input.rml")->GetElementById("chat_field");
        REQUIRE(field->Focus());
        key(Rml::Input::KI_PRIOR);
        frame();
        CHECK(PaintedHistoryRows().last < away.last);
        for (int i = 0; i < 4; ++i)
            key(Rml::Input::KI_NEXT);
        frame();
        CHECK(PaintedHistoryRows().last == 35);

        // Default-theme roundtrip at the tail, then back.
        for (const char* theme : {"legacy", "modern", "munolith"})
        {
            CAPTURE(std::string(theme));
            CHECK(reload(theme).last == 35);
        }
        const auto settled = PaintedHistoryRows();
        CHECK(settled.last - settled.first + 1 == visibleRows);

        // A log the scene hides across the reload (SyncMainSceneHudVisibility()) restores its
        // scrolled-away view when shown again. ShowChatLog() is not used: it returns to the tail.
        field = Document("chat_input.rml")->GetElementById("chat_field");
        REQUIRE(field->Focus());
        key(Rml::Input::KI_PRIOR);
        frame();
        const auto hiddenAway = PaintedHistoryRows();
        REQUIRE(hiddenAway.last < 35);
        chat.SyncDocVisibility(false);
        frame();
        reload("munolith");
        chat.SyncDocVisibility(true);
        frame();
        frame();
        CHECK(PaintedHistoryRows() == hiddenAway);

        // The exposed wheel scrolls from the restored viewport, not from the top.
        CheckExposedHistory(chat);
    }
    // Native d2bf profiles: history seeded at Munolith 1024x768@100 (wrapped rows split by native
    // SeparateText()), framed at its wrapped tail at 1920x1080@75, then the window resized to
    // 3840x2160 (dp unchanged) and the UI scale set to 100 (dp changes), then a Munolith reload,
    // painted older rows -- already before the reload. Viewport-only, scale-only, theme-only and
    // combined are separated here. Headless the following tail holds in every step (the native tail
    // loss is not reproduced); a scrolled-away view's logical end line drifted on a density change.
    // A resize makes Winmain.cpp's HandleWindowResize() steps that reach the UI (it is file-local
    // there): window size, the active screen-overlay transform the next frame's native measuring
    // runs under, and RmlUiRuntime::OnResize() with the production scale hooks -- what `window
    // resize` and `ui scale` reach through MuApplyWindowResolution().
    void CheckFramedHistoryResize()
    {
        using namespace mu::ui::window;
        auto& runtime = RmlUiRuntime::Instance();
        const auto startTransform = UI::Scaling::GetActiveTransform();
        const int startWidth = static_cast<int>(WindowWidth), startHeight = static_cast<int>(WindowHeight);
        const int startPercent = GameConfig::GetInstance().GetUIScalePercent();
        auto oldSystem = std::move(runtime.m_SystemInterface);
        auto oldIme = std::move(runtime.m_TextInputMethodEditor);
        runtime.m_SystemInterface = std::make_unique<RmlUiSystemInterface>(nullptr);
        runtime.m_TextInputMethodEditor = std::make_unique<TextInputMethodEditor_SDL>();
        Rml::SetSystemInterface(runtime.m_SystemInterface.get());
        Rml::SetTextInputHandler(runtime.m_TextInputMethodEditor.get());
        const auto fixtureHooks = runtime.m_Hooks;
        UI::RmlBridge::InstallRuntimeHooks();
        const auto productionHooks = runtime.m_Hooks;
        const auto applyWindowSize = [&](int width, int height)
        {
            WindowWidth = width;
            WindowHeight = height;
            UI::Scaling::SetActiveTransform(UI::Scaling::ScreenOverlayTransform(width, height));
            runtime.OnResize(width, height);
        };
        CChatLogWindow chat;
        const auto restore = std::unique_ptr<RmlUiRuntime, std::function<void(RmlUiRuntime*)>>(
            &runtime,
            [&](auto*)
            {
                if (auto* focus = context->GetFocusElement())
                    focus->Blur();
                GameConfig::GetInstance().SetUIScalePercent(startPercent);
                applyWindowSize(startWidth, startHeight);
                UI::Scaling::SetActiveTransform(startTransform);
                runtime.SetHooks(fixtureHooks);
                Rml::SetTextInputHandler(nullptr);
                Rml::SetSystemInterface(&log);
                runtime.m_SystemInterface = std::move(oldSystem);
                runtime.m_TextInputMethodEditor = std::move(oldIme);
            });
        const NativeTextMeasuring measuring;
        auto hooks = fixtureHooks;
        hooks.dpRatio = productionHooks.dpRatio;
        hooks.afterScale = productionHooks.afterScale;
        hooks.afterUpdate = [&, fixture = fixtureHooks.afterUpdate](Rml::Context* c)
        {
            fixture(c);
            chat.AfterRmlLayout(c);
        };
        runtime.SetHooks(std::move(hooks));
        CreateInventoryGroup();
        CSystemLogWindow notices;
        CChatInputBox input;
        REQUIRE(chat.Create(&manager, 6));
        REQUIRE(notices.Create(&manager));
        REQUIRE(input.Create(&manager, &chat, &notices));
        Configure("munolith", {1024, 768, 100});
        Inventory(0, 1);
        PopulateArmory(false);
        CUIPhotoViewer viewer;
        auto& photo = character->m_Armory->m_PhotoControl;
        photo.AttachOrbit(character->GetPlacedDocument(), viewer, "armory_model_controls", "armory_preview");
        const auto detachPhoto = std::unique_ptr<CUIPhotoViewer, std::function<void(CUIPhotoViewer*)>>(
            &viewer, [&](auto*) { photo.Detach(); });
        const auto frame = [&]
        {
            input.Update();
            chat.Update();
            notices.Update();
            ProductionFrame();
        };
        const auto key = [&](Rml::Input::KeyIdentifier identifier)
        {
            REQUIRE(Document("chat_input.rml")->GetElementById("chat_field")->Focus());
            context->ProcessKeyDown(identifier, 0);
            context->ProcessKeyUp(identifier, 0);
            frame();
        };
        const auto resize = [&](int width, int height, int percent)
        {
            GameConfig::GetInstance().SetUIScalePercent(percent);
            applyWindowSize(width, height);
            frame();
            frame();
        };
        const auto reload = [&](const char* theme)
        {
            std::unique_ptr<App::Control::Act> act;
            const auto request =
                App::Control::Request::Parse(std::string(R"({"cmd":"ui","action":"theme","name":")") + theme + "\"}");
            REQUIRE(App::Control::Commands::Ui(request, act).empty());
            REQUIRE(act);
            std::string answer;
            CHECK(act->Tick(answer) == App::Control::Act::Status::Running);
            frame();
            photo.AttachOrbit(character->GetPlacedDocument(), viewer, "armory_model_controls", "armory_preview");
            photo.Sync();
            frame();
            CHECK(act->Tick(answer) == App::Control::Act::Status::Finished);
            frame();
        };
        chat.Show(true);
        notices.Show(true);
        chat.SyncDocVisibility(true);
        notices.SyncDocVisibility(true);
        resize(1024, 768, 100);
        // The native fixture's rows (chat_fixture.py), arriving at Munolith 1024x768@100: 34 short
        // rows, then four long ones that native SeparateText() splits into two lines each.
        for (int i = 0; i < 34; ++i)
            chat.AddText(L"QA640", (i < 10 ? L"SEEDED UI640 row 0" : L"SEEDED UI640 row ") + std::to_wstring(i),
                         TYPE_CHAT_MESSAGE);
        for (int i = 40; i < 44; ++i)
            chat.AddText(L"QA640",
                         L"WRAPSTART" + std::to_wstring(i) +
                             L" neutral wrapped history row for reflow and anchor check WRAPEND" + std::to_wstring(i),
                         TYPE_CHAT_MESSAGE);
        const int tailRow = static_cast<int>(chat.GetNumberOfLines(TYPE_ALL_MESSAGE)) - 1;
        CHECK(tailRow == 41); // Each wrapped message is two native lines.
        input.Show(true);
        frame();
        REQUIRE(Document("chat_input.rml")->GetElementById("chat_field")->Focus());
        context->ProcessTextInput("B640 neutral unsent");
        frame();
        key(Rml::Input::KI_F5);
        REQUIRE(chat.IsShowFrame());
        const auto draft = [&]
        { return Document("chat_input.rml")->GetElementById("chat_field")->GetAttribute<Rml::String>("value", ""); };

        for (const char* theme : {"legacy", "modern", "munolith"})
        {
            CAPTURE(std::string(theme));
            reload(theme);
            resize(1920, 1080, 75);
            for (int i = 0; i < 8; ++i)
                key(Rml::Input::KI_NEXT);
            REQUIRE(PaintedHistoryRows().last == tailRow);
            REQUIRE(chat.GetCurrentRenderEndLine() == tailRow);
            CHECK(ClippedHistoryRows().empty());

            // Resize only, following the tail: the tail stays painted at every step and back. The
            // first step changes the viewport only (dp unchanged), the second the UI scale (dp).
            resize(3840, 2160, 75);
            CHECK(PaintedHistoryRows().last == tailRow);
            resize(3840, 2160, 100);
            CHECK(PaintedHistoryRows().last == tailRow);
            CHECK(chat.GetCurrentRenderEndLine() == tailRow);
            CHECK(ClippedHistoryRows().empty());
            resize(1920, 1080, 75);
            CHECK(PaintedHistoryRows().last == tailRow);

            // Resize only, scrolled away: the logical end line survives, the resized view is the one a
            // reload at that size shows for it, and resizing back paints the original rows again.
            key(Rml::Input::KI_PRIOR);
            const auto away = PaintedHistoryRows();
            const int awayEnd = chat.GetCurrentRenderEndLine();
            REQUIRE(away.last < tailRow);
            REQUIRE(away.first > 0);
            resize(3840, 2160, 75);
            resize(3840, 2160, 100);
            CHECK(chat.GetCurrentRenderEndLine() == awayEnd);
            const auto awayLarge = PaintedHistoryRows();
            CHECK(awayLarge.last < tailRow);
            reload(theme);
            CHECK(PaintedHistoryRows() == awayLarge);
            resize(1920, 1080, 75);
            CHECK(chat.GetCurrentRenderEndLine() == awayEnd);
            CHECK(PaintedHistoryRows() == away);

            // Theme only, at the same size.
            reload(theme);
            CHECK(PaintedHistoryRows() == away);

            // Combined, in the native order: two resizes, then a reload of another theme.
            for (int i = 0; i < 8; ++i)
                key(Rml::Input::KI_NEXT);
            REQUIRE(PaintedHistoryRows().last == tailRow);
            resize(3840, 2160, 75);
            resize(3840, 2160, 100);
            reload("munolith");
            CHECK(PaintedHistoryRows().last == tailRow);
            reload(theme);
            resize(1920, 1080, 75);
            CHECK(PaintedHistoryRows().last == tailRow);
            key(Rml::Input::KI_PRIOR);
            CHECK(chat.GetCurrentRenderEndLine() == awayEnd);
            resize(3840, 2160, 100);
            reload(theme);
            resize(1920, 1080, 75);
            CHECK(chat.GetCurrentRenderEndLine() == awayEnd);
            CHECK(PaintedHistoryRows() == away);

            // A log the scene hides across a resize restores its view when shown again.
            chat.SyncDocVisibility(false);
            frame();
            resize(3840, 2160, 100);
            resize(1920, 1080, 75);
            chat.SyncDocVisibility(true);
            frame();
            frame();
            CHECK(chat.GetCurrentRenderEndLine() == awayEnd);
            CHECK(PaintedHistoryRows() == away);

            // Navigation continues from the restored view; the tail then follows an append.
            key(Rml::Input::KI_PRIOR);
            CHECK(PaintedHistoryRows().last < away.last);
            for (int i = 0; i < 8; ++i)
                key(Rml::Input::KI_NEXT);
            CHECK(PaintedHistoryRows().last == tailRow);
            CHECK(draft() == "B640 neutral unsent");

            // The exposed wheel and the sender hit test on rows of this theme's (wrapped) heights.
            CheckExposedHistory(chat);
            for (int i = 0; i < 8; ++i)
                key(Rml::Input::KI_NEXT);
            CHECK(PaintedHistoryRows().last == tailRow);
        }

        // An empty filtered view across a resize; restoring the filter returns to the tail, which
        // follows an append made at the new size.
        key(Rml::Input::KI_F2);
        REQUIRE(chat.GetCurrentMsgType() == TYPE_WHISPER_MESSAGE);
        resize(3840, 2160, 100);
        key(Rml::Input::KI_F2);
        CHECK(PaintedHistoryRows().last == tailRow);
        chat.AddText(L"QA640", L"APPENDED UI640 tail row", TYPE_CHAT_MESSAGE);
        frame();
        frame();
        CHECK(PaintedHistoryRows().last == tailRow + 1);
        resize(1920, 1080, 75);
        CHECK(PaintedHistoryRows().last == tailRow + 1);
        CHECK(draft() == "B640 neutral unsent");

        // Histories whose stored line count is at, below or just above the window's nominal count
        // (6) while their wrapped rows overflow the well: whether the view scrolls is the laid-out
        // overflow, not the count. The tail stays at the bottom through PageDown, reload, resize, a
        // hidden reflow and an append; PageUp reaches the top and PageDown returns. Short rows that
        // fit, and an empty history, stay put at the top.
        REQUIRE(chat.GetNumberOfShowingLines() == 6);
        const std::wstring widest(89, L'W');
        struct ShortHistory
        {
            const char* name;
            int widest, shorts;
            bool overflows;
        };
        constexpr ShortHistory shortHistories[]{{"below", 2, 0, true},
                                                {"equal", 3, 0, true},
                                                {"above", 3, 1, true},
                                                {"short rows", 0, 3, false},
                                                {"empty", 0, 0, false}};
        for (const char* theme : {"legacy", "modern", "munolith"})
            for (const auto& history : shortHistories)
            {
                CAPTURE(std::string(theme));
                CAPTURE(std::string(history.name));
                reload(theme);
                resize(1920, 1080, 75);
                chat.ClearAll();
                frame();
                for (int i = 0; i < history.widest; ++i)
                    chat.AddText(L"QA640", widest, TYPE_CHAT_MESSAGE);
                for (int i = 0; i < history.shorts; ++i)
                    chat.AddText(L"QA640", L"SHORT row " + std::to_wstring(i), TYPE_CHAT_MESSAGE);
                frame();
                frame();
                const int total = static_cast<int>(chat.GetNumberOfLines(TYPE_ALL_MESSAGE));
                CHECK(total == history.widest * 2 + history.shorts); // Native splits each W message once.
                // A reload rebuilds the document: look #lines up again every time.
                const auto lines = [&] { return Document("chat_log.rml")->GetElementById("lines"); };
                const auto range = [&] { return lines()->GetScrollHeight() - lines()->GetClientHeight(); };
                REQUIRE((range() > 1.f) == history.overflows);
                if (history.overflows)
                {
                    // The overflow comes from wrapped rows: at least one row laid out on several lines.
                    int wrapped = 0;
                    for (int i = 0; i < total; ++i)
                        for (int c = 0; c < lines()->GetChild(i)->GetNumChildren(); ++c)
                            if (auto* text = dynamic_cast<Rml::ElementText*>(lines()->GetChild(i)->GetChild(c)))
                                wrapped += text->GetLines().size() > 1 ? 1 : 0;
                    CHECK(wrapped > 0);
                }
                const auto atTail = [&](const std::string& when)
                {
                    CAPTURE(when);
                    if (total == 0)
                    {
                        CHECK(chat.GetCurrentRenderEndLine() == -1);
                        CHECK(lines()->GetScrollTop() == 0.f);
                        return;
                    }
                    CHECK(chat.GetCurrentRenderEndLine() == total - 1);
                    const auto anchor = VisibleHistoryAnchor(total);
                    CHECK(anchor.index == total - 1);
                    if (history.overflows)
                    {
                        CHECK(std::abs(lines()->GetScrollTop() - range()) <= 0.5f);
                        CHECK(std::abs(anchor.belowBottom) <= 0.5f);
                    }
                    else
                        CHECK(lines()->GetScrollTop() == 0.f);
                };
                atTail("arrival");
                key(Rml::Input::KI_NEXT);
                atTail("PageDown");
                reload(theme);
                atTail("same-theme reload");
                resize(3840, 2160, 100);
                atTail("resized");
                resize(1920, 1080, 75);
                atTail("resized back");
                chat.SyncDocVisibility(false);
                frame();
                resize(800, 600, 150);
                resize(1920, 1080, 75);
                chat.SyncDocVisibility(true);
                frame();
                frame();
                atTail("hidden reflow");
                for (int i = 0; i < 4; ++i)
                    key(Rml::Input::KI_PRIOR);
                CHECK(lines()->GetScrollTop() == 0.f);
                if (total > 0)
                {
                    // The first row is in view; the end line reads back as the last row half inside.
                    CHECK(VisibleHistoryAnchor(total).index == chat.GetCurrentRenderEndLine());
                    CHECK(chat.GetCurrentRenderEndLine() >= 0);
                }
                for (int i = 0; i < 4; ++i)
                    key(Rml::Input::KI_NEXT);
                atTail("PageUp to the top, PageDown back");
                chat.AddText(L"QA640", L"SHORT appended", TYPE_CHAT_MESSAGE);
                frame();
                frame();
                CHECK(chat.GetCurrentRenderEndLine() == total);
                CHECK(VisibleHistoryAnchor(total + 1).index == total);
                CHECK(draft() == "B640 neutral unsent");
            }

        // A scrolled-away view over genuinely mixed row heights keeps its visible anchor -- the same
        // message ending at the well's bottom -- through viewport-only, user-scale, combined,
        // theme, reload and hidden transitions and an append, and Page/wheel continue from it.
        const auto seedMixed = [&]
        {
            chat.ClearAll();
            for (int i = 0; i < 30; ++i)
                chat.AddText(L"QA640",
                             i % 4 == 1 ? L"MIXEDLONG" + std::to_wstring(i) +
                                              L" a much longer neutral history message whose remainder alone needs "
                                              L"several wrapped lines in the well at every profile MIXEDEND"
                                        : L"MIXED short row " + std::to_wstring(i),
                             TYPE_CHAT_MESSAGE);
            frame();
            frame();
        };
        for (const char* theme : {"legacy", "modern", "munolith"})
        {
            CAPTURE(std::string(theme));
            reload(theme);
            resize(1920, 1080, 75);
            seedMixed();
            const int messages = static_cast<int>(chat.GetNumberOfLines(TYPE_ALL_MESSAGE));
            for (int i = 0; i < 8; ++i)
                key(Rml::Input::KI_NEXT);
            REQUIRE(VisibleHistoryAnchor(messages).index == messages - 1);
            key(Rml::Input::KI_PRIOR);
            key(Rml::Input::KI_PRIOR);
            const auto before = VisibleHistoryAnchor(messages);
            CAPTURE(before.index);
            CAPTURE(before.text);
            REQUIRE(before.index > 0);
            REQUIRE(before.index < messages - 1);
            // Genuinely mixed rendered heights around the anchor, before any reflow.
            const auto heights = HistoryRowHeights(before.index - 4, before.index + 4);
            REQUIRE(heights.size() >= 5);
            CHECK(*std::max_element(heights.begin(), heights.end()) >
                  1.5f * *std::min_element(heights.begin(), heights.end()));
            // PageUp lands the logical end line exactly at the well's bottom.
            CHECK(chat.GetCurrentRenderEndLine() == before.index);
            CHECK(std::abs(before.belowBottom) <= 0.5f);
            const auto same = [&](const char* when)
            {
                CAPTURE(when);
                const auto now = VisibleHistoryAnchor(messages);
                CHECK(now.index == before.index);
                CHECK(now.text == before.text);
                CHECK(std::abs(now.belowBottom) <= 0.5f);
                CHECK(chat.GetCurrentRenderEndLine() == before.index);
            };
            resize(3840, 2160, 75);
            same("viewport only");
            resize(3840, 2160, 100);
            same("user scale");
            resize(1920, 1080, 75);
            same("back");
            reload(theme);
            same("same-theme reload");
            resize(3840, 2160, 100);
            reload(std::string(theme) == "munolith" ? "legacy" : "munolith");
            same("combined resize, scale and theme");
            reload(theme);
            resize(1920, 1080, 75);
            same("theme back");
            chat.SyncDocVisibility(false);
            frame();
            resize(800, 600, 150);
            chat.SyncDocVisibility(true);
            frame();
            frame();
            same("hidden across a resize");
            resize(1920, 1080, 75);
            same("hidden back");
            chat.AddText(L"QA640", L"MIXED appended away", TYPE_CHAT_MESSAGE);
            frame();
            frame();
            same("away append");
            // Page continuity from the anchor, both ways.
            key(Rml::Input::KI_NEXT);
            CHECK(VisibleHistoryAnchor(messages).index > before.index);
            key(Rml::Input::KI_PRIOR);
            same("PageDown then PageUp");
            // The wheel scrolls from the anchor and leaves any offset; the sender hit test names the
            // row under the pointer (only a message's first line carries its sender; a wrapped
            // remainder is its own row without one); a resize keeps the message the wheel left at
            // the bottom.
            {
                HidePanes();
                ProductionFrame();
                auto* lines = Document("chat_log.rml")->GetElementById("lines");
                const auto b = Box(*lines);
                const int x = std::lround(b.left + b.width / 2.f), y = std::lround(b.top + b.height / 2.f);
                context->ProcessMouseMove(x, y, 0);
                g_fWindowMouseX = x;
                g_fWindowMouseY = y;
                const auto oldScroll = lines->GetScrollTop();
                MouseWheel = 1;
                CHECK_FALSE(chat.UpdateMouseEvent());
                CHECK(MouseWheel == 0);
                CHECK(lines->GetScrollTop() < oldScroll);
                frame();
                frame();
                Rml::Element* under = nullptr;
                for (int i = 0; i < lines->GetNumChildren(); ++i)
                {
                    Rml::Element* row = lines->GetChild(i);
                    const float top = row->GetAbsoluteTop();
                    const float pitch = row->GetBox().GetEdge(Rml::BoxArea::Margin, Rml::BoxEdge::Bottom);
                    if (y >= top && y < top + row->GetOffsetHeight() + pitch)
                        under = row;
                }
                REQUIRE(under);
                const bool hasSender = under->GetInnerRML().rfind("QA640 : ", 0) == 0;
                CHECK(under->IsClassSet("chat-line--pointed") == hasSender);
                Rml::ElementList pointed;
                Document("chat_log.rml")->QuerySelectorAll(pointed, ".chat-line--pointed");
                CHECK(pointed.size() == (hasSender ? 1u : 0u));
            }
            const auto wheeled = VisibleHistoryAnchor(messages);
            REQUIRE(wheeled.index >= 0);
            resize(3840, 2160, 100);
            CHECK(VisibleHistoryAnchor(messages).index == wheeled.index);
            CHECK(std::abs(VisibleHistoryAnchor(messages).belowBottom) <= 0.5f);
            resize(1920, 1080, 75);
            CHECK(VisibleHistoryAnchor(messages).index == wheeled.index);
            CHECK(draft() == "B640 neutral unsent");
        }

        // The split is measured in the window's own units wherever the message arrives: the same
        // two lines per long message at every profile. Rows wrap inside the well in every theme
        // (Legacy and Modern since the operator-approved wrapping; Munolith before), each theme in
        // its own unchanged font, so no text runs past the well -- where the message arrived, nor
        // after a resize to any other profile. The stored history is never re-split: count, order
        // and text stay as they arrived.
        constexpr Viewport profiles[]{{1024, 768, 100}, {800, 600, 150}, {1920, 1080, 75}, {3840, 2160, 100}};
        const auto arrive = [&](const char* theme, Viewport arrival)
        {
            resize(arrival.width, arrival.height, arrival.percent);
            reload(theme);
            chat.ClearAll();
            chat.AddText(L"QA640", L"SEEDED UI640 short row", TYPE_CHAT_MESSAGE);
            for (int i = 40; i < 44; ++i)
                chat.AddText(L"QA640",
                             L"WRAPSTART" + std::to_wstring(i) +
                                 L" neutral wrapped history row for reflow and anchor check WRAPEND" +
                                 std::to_wstring(i),
                             TYPE_CHAT_MESSAGE);
            chat.AddText(L"QA640",
                         L"LONGSTART a much longer neutral history message whose remainder alone needs several "
                         L"wrapped lines in the well at every profile LONGEND",
                         TYPE_CHAT_MESSAGE);
            // Unbroken text is legal input: the chat box takes up to 90 characters (89 W, the widest
            // ASCII run), and an arriving message is variable-length (200 X).
            chat.AddText(L"QA640", std::wstring(89, L'W'), TYPE_CHAT_MESSAGE);
            chat.AddText(L"QA640", L"UNBROKEN" + std::wstring(200, L'X'), TYPE_CHAT_MESSAGE);
            chat.AddText(L"QA640", L"SEEDED UI640 last short row", TYPE_CHAT_MESSAGE);
            frame();
            frame();
        };
        const auto rowTexts = [&]
        {
            std::vector<Rml::String> texts;
            auto* lines = Document("chat_log.rml")->GetElementById("lines");
            for (int i = 0; i < lines->GetNumChildren(); ++i)
                texts.push_back(lines->GetChild(i)->GetInnerRML());
            return texts;
        };
        // Each theme's own font, unchanged by wrapping: Legacy and Munolith rows draw the native
        // text size (chat_log.rml's text_px binding), Modern its 11dp body text.
        const auto checkFonts = [&](const char* theme)
        {
            auto* lines = Document("chat_log.rml")->GetElementById("lines");
            const bool modern = std::string(theme) == "modern";
            const float expectedPx = modern ? 11.f * context->GetDensityIndependentPixelRatio()
                                            : UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Normal);
            for (int i = 0; i < lines->GetNumChildren(); ++i)
            {
                Rml::Element* row = lines->GetChild(i);
                if (!row->IsVisible())
                    continue;
                CAPTURE(i);
                CHECK(row->GetComputedValues().font_size() == doctest::Approx(expectedPx));
                const auto* family = row->GetProperty("font-family");
                REQUIRE(family);
                CHECK(family->ToString() == (modern ? "Liberation Sans" : "DejaVu Sans"));
            }
        };
        const auto checkFits = [&](const std::string& when)
        {
            for (const auto& [row, bound, overflow] : ClippedHistoryRows(true))
            {
                CAPTURE(row);
                FAIL_CHECK(when << " row " << std::string(bound) << " overflows the well by " << overflow << "px");
            }
        };
        for (const char* theme : {"legacy", "modern", "munolith"})
            for (const auto arrival : profiles)
            {
                CAPTURE(std::string(theme));
                CAPTURE(arrival.width);
                CAPTURE(arrival.percent);
                arrive(theme, arrival);
                CHECK(chat.GetNumberOfLines(TYPE_ALL_MESSAGE) == 16); // Each long message is two lines.
                const auto arrived = rowTexts();
                checkFonts(theme);
                checkFits("arrival");
                for (const auto later : profiles)
                {
                    CAPTURE(later.width);
                    CAPTURE(later.percent);
                    resize(later.width, later.height, later.percent);
                    CHECK(rowTexts() == arrived);
                    checkFonts(theme);
                    checkFits("resized");
                }
            }

        // Negative controls through the same checks, on an explicitly malformed fixture: the
        // unbroken rows forced back to one unbreakable line (nowrap, no word breaking) as test-only
        // inline style. Their glyphs then really leave the well -- ink and advance are both caught --
        // and removing the fixture style makes them fit again.
        for (const char* theme : {"legacy", "modern", "munolith"})
        {
            CAPTURE(std::string(theme));
            arrive(theme, {1920, 1080, 75});
            REQUIRE(ClippedHistoryRows().empty());
            auto* lines = Document("chat_log.rml")->GetElementById("lines");
            std::vector<Rml::Element*> malformed;
            for (int i = 0; i < lines->GetNumChildren(); ++i)
                if (lines->GetChild(i)->GetInnerRML().find("WWWWWWWWWW") != Rml::String::npos ||
                    lines->GetChild(i)->GetInnerRML().find("XXXXXXXXXX") != Rml::String::npos)
                    malformed.push_back(lines->GetChild(i));
            REQUIRE(malformed.size() == 4);
            for (auto* row : malformed)
            {
                row->SetProperty("white-space", "nowrap");
                row->SetProperty("word-break", "normal");
            }
            frame();
            frame();
            const auto caught = ClippedHistoryRows();
            const auto has = [&](std::string_view bound)
            {
                return std::any_of(caught.begin(), caught.end(), [&](const HistoryOverflow& overflow)
                                   { return overflow.bound == bound && overflow.overflow > 1.f; });
            };
            CHECK(has("ink"));
            CHECK(has("advance"));
            for (auto* row : malformed)
            {
                row->RemoveProperty("white-space");
                row->RemoveProperty("word-break");
            }
            frame();
            frame();
            CHECK(ClippedHistoryRows().empty());
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

TEST_CASE("Declarative passive logs preserve actual paint hit ownership exposed history and focused draft "
          "[ui][chat][passive-logs]")
{
    RmlViewLayoutFixture f;
    f.CheckPassiveLogs();
}

TEST_CASE("Focused visible native-input history survives first same-theme control reload [ui][reload][640]")
{
    RmlViewLayoutFixture f;
    f.CheckFocusedControlReload();
}

TEST_CASE("Framed chat history keeps its viewport across theme control reloads [ui][reload][history]")
{
    RmlViewLayoutFixture f;
    f.CheckFramedHistoryReload();
}

TEST_CASE("Framed chat history keeps its viewport across window resizes and theme reloads [ui][resize][history]")
{
    RmlViewLayoutFixture f;
    f.CheckFramedHistoryResize();
}

TEST_CASE("Chat history line advance limit models RmlUi's rounded-up wrap width [ui][history]")
{
    // Layout px. The 3840x2160@100 Munolith case: line box at 10.7331, 711.07 wide, no cursor.
    CHECK(LineAdvanceLimit(10.7331f, 711.07f, 0.f, 721.8031f, true) == doctest::Approx(722.7331f));
    CHECK(722.733f <= LineAdvanceLimit(10.7331f, 711.07f, 0.f, 721.8031f, true));
    CHECK(722.75f > LineAdvanceLimit(10.7331f, 711.07f, 0.f, 721.8031f, true));
    // An integral width keeps the half-pixel budget only: rounding and budget are never stacked.
    CHECK(LineAdvanceLimit(10.f, 711.f, 0.f, 721.f, true) == doctest::Approx(721.5f));
    // A fraction of a half pixel or more: the budget already covers the rounding.
    CHECK(LineAdvanceLimit(0.f, 100.75f, 0.f, 100.75f, true) == doctest::Approx(101.25f));
    // A line that starts after a cursor: RoundUp(width - cursor) from the cursor.
    CHECK(LineAdvanceLimit(50.f, 100.9f, 0.6f, 150.9f, true) == doctest::Approx(151.6f));
    CHECK(LineAdvanceLimit(50.f, 100.9f, 3.f, 150.9f, true) == doctest::Approx(151.4f));
    // A line box narrower than the well (row padding): the well's budget bounds it.
    CHECK(LineAdvanceLimit(14.f, 300.2f, 0.f, 330.f, true) == doctest::Approx(330.5f));
    // No wrapping: the engine offers no rounded-up width.
    CHECK(LineAdvanceLimit(10.7331f, 711.07f, 0.f, 721.8031f, false) == doctest::Approx(722.3031f));
}
