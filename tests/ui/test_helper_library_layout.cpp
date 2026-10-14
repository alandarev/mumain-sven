#include "stdafx.h"
#include <doctest.h>
#include <RmlUi/Core.h>
#include "UI/MuHelper/MuHelperSkillPicker.h"
#include "UI/RmlBridge/ThemeFileInterface.h"
#include "UI/RmlBridge/RmlPanelGeometry.h"

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
TEST_CASE("Helper skill library scrolls all eligible choices and dispatches native identities [ui][helper]")
{
    using namespace mu::ui::window;
    NullRenderer renderer;
    UI::RmlBridge::ThemeFileInterface files;
    Rml::SetFileInterface(&files);
    Rml::SetRenderInterface(&renderer);
    REQUIRE(Rml::Initialise());
    REQUIRE(Rml::LoadFontFace(std::string(MU_RMLUI_DIR) + "/../../../fonts/DejaVuSans.ttf"));
    auto* context = Rml::CreateContext("helper-library", {1920, 1080});
    MuHelperSkillPickerRmlModel state;
    state.textPx = 9;
    for (int i = 0; i < 50; ++i)
    {
        MuHelperSkillPickerEntry entry;
        entry.skillType = 100 + i;
        entry.name = "A learned skill with a full name";
        entry.decorator = "none";
        state.entries.push_back(entry);
    }
    auto c = context->CreateDataModel("mu_helper_skill_picker");
    c.Bind("origin_x", &state.originX);
    c.Bind("origin_y", &state.originY);
    c.Bind("scale", &state.scale);
    c.Bind("attack", &state.attack);
    c.Bind("text_px", &state.textPx);
    auto entry = c.RegisterStruct<MuHelperSkillPickerEntry>();
    entry.RegisterMember("name", &MuHelperSkillPickerEntry::name);
    entry.RegisterMember("skill_type", &MuHelperSkillPickerEntry::skillType);
    entry.RegisterMember("decorator", &MuHelperSkillPickerEntry::decorator);
    c.RegisterArray<std::vector<MuHelperSkillPickerEntry>>();
    c.Bind("entries", &state.entries);
    int chosen = -1;
    c.BindEventCallback("muhelper_choose", [&chosen](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        { chosen = args.at(0).Get<int>(); });
    c.BindEventCallback("muhelper_picker_close", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
    auto* doc = context->LoadDocument(std::string(MU_RMLUI_DIR) + "/themes/munolith/mu_helper_skill_picker.rml");
    REQUIRE(doc);
    UI::RmlBridge::SlotPlacement{0.f, 0.f, 1.f}.Apply(doc, "panel");
    doc->Show();
    context->Update();
    context->Update();
    auto* pane = doc->GetElementById("skill_library");
    REQUIRE(pane);
    CHECK(pane->GetClientHeight() == doctest::Approx(338));
    CHECK(pane->GetScrollHeight() > pane->GetClientHeight());
    pane->SetScrollTop(pane->GetScrollHeight());
    context->Update();
    CHECK(pane->GetScrollTop() > 0);
    Rml::ElementList rows;
    doc->QuerySelectorAll(rows, ".library-skill");
    std::erase_if(rows, [](auto* row) { return !row->IsVisible(); });
    REQUIRE(rows.size() == 50);
    rows.back()->DispatchEvent("click", {});
    CHECK(chosen == 149);
    CHECK(pane->GetAbsoluteOffset().y + pane->GetClientHeight() < 400);
    Rml::Shutdown();
    Rml::SetRenderInterface(nullptr);
    Rml::SetFileInterface(nullptr);
}
