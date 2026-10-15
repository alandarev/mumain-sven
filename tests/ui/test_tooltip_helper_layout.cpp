#include "stdafx.h"
#include <doctest.h>
#include <RmlUi/Core.h>
#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/ElementUtilities.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "Data/GameConfig/GameConfig.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/HUD/MuHelperBar.h"
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
