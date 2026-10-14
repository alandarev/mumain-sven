#include <doctest.h>
#include <RmlUi/Core.h>
#include <fstream>
#include <sstream>

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
struct Choice
{
    Rml::String name;
};
std::string Style()
{
    std::ifstream file(std::string(MU_RMLUI_DIR) + "/themes/munolith/main_frame.rcss");
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}
} // namespace
TEST_CASE("Munolith assignment tiles have two full hit areas and a scrollable list [ui][munolith]")
{
    NullRenderer renderer;
    Rml::SetRenderInterface(&renderer);
    REQUIRE(Rml::Initialise());
    REQUIRE(Rml::LoadFontFace(std::string(MU_RMLUI_DIR) + "/../../../fonts/DejaVuSans.ttf"));
    struct Viewport
    {
        int width, height;
        float unit;
        const char* classes;
        int listHeight;
    };
    for (const auto viewport : {Viewport{1920, 1080, 1.40625f, "", 240}, Viewport{1024, 768, 1.f, "", 240},
                                Viewport{800, 600, 1.5f, "compact-medium", 150},
                                Viewport{800, 600, 2.f, "compact-medium compact-narrow", 122}})
    {
        INFO("viewport ", viewport.width, " x ", viewport.height, " unit ", viewport.unit);
        auto* context = Rml::CreateContext("picker-layout", {viewport.width, viewport.height});
        REQUIRE(context != nullptr);
        auto model = context->CreateDataModel("picker-test");
        auto choice = model.RegisterStruct<Choice>();
        choice.RegisterMember("name", &Choice::name);
        model.RegisterArray<std::vector<Choice>>();
        std::vector<Choice> choices(20, {"Swell Life Proficiency"});
        model.Bind("choices", &choices);
        const std::string markup =
            "<rml><head><style>" + Style() + "</style></head><body data-model='picker-test' class='" +
            viewport.classes +
            "' style='display:block;position:absolute;left:0;top:0;width:100%;height:100%;font-size:" +
            std::to_string(viewport.unit) +
            "px;'>"
            "<div id='compact_picker' class='compact-popover'>"
            "<div class='popover-header'><span>Assign action 1 [1]</span><button>Close</button></div>"
            "<div class='popover-tabs'><button>Skills</button><button>Consumables</button></div>"
            "<div class='compact-choice-list' style='height:" +
            std::to_string(viewport.listHeight) +
            "rem;'>"
            "<div class='compact-choice' data-for='choice : choices'><div "
            "class='choice-icon'></div><span>{{choice.name}}</span></div>"
            "</div><div class='popover-footer'><button>Clear slot</button><button>Key "
            "bindings</button></div></div></body></rml>";
        auto* document = context->LoadDocumentFromMemory(markup);
        REQUIRE(document != nullptr);
        document->Show();
        context->Update();
        context->Update();
        Rml::ElementList rows;
        document->QuerySelectorAll(rows, ".compact-choice");
        REQUIRE(rows.size() >= choices.size());
        Rml::ElementList visible;
        for (auto* row : rows)
            if (row->IsVisible())
                visible.push_back(row);
        REQUIRE(visible.size() == choices.size());
        auto* list = visible[0]->GetParentNode();
        const auto width = list->GetBox().GetSize(Rml::BoxArea::Content).x;
        for (auto* row : visible)
        {
            const auto size = row->GetBox().GetSize(Rml::BoxArea::Border);
            INFO("list width=", width, " tile=", size.x, " x=", row->GetAbsoluteOffset().x,
                 " y=", row->GetAbsoluteOffset().y);
            CHECK(size.x > width * .40f);
            CHECK(size.x < width * .51f);
        }
        CHECK(visible[0]->GetAbsoluteOffset().y == doctest::Approx(visible[1]->GetAbsoluteOffset().y));
        CHECK(visible[1]->GetAbsoluteOffset().x > visible[0]->GetAbsoluteOffset().x);
        CHECK(list->GetScrollHeight() > list->GetClientHeight());
        auto* picker = document->GetElementById("compact_picker");
        CHECK(picker->GetAbsoluteOffset().y >= 0);
        CHECK(picker->GetAbsoluteOffset().y + picker->GetBox().GetSize(Rml::BoxArea::Border).y <= viewport.height);
        list->SetScrollTop(list->GetScrollHeight());
        context->Update();
        CHECK(list->GetScrollTop() > 0);
        context->UnloadDocument(document);
        Rml::RemoveContext("picker-layout");
    }
    Rml::Shutdown();
    Rml::SetRenderInterface(nullptr);
}
