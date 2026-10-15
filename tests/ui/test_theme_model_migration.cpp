#include "stdafx.h"
#include <doctest.h>
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include "UI/Dialogs/ChatCommandRmlModel.h"
#include "UI/Inventory/ItemHelpRmlModel.h"
#include "UI/Inventory/ItemHelpView.h"
#include "UI/RmlBridge/RmlThemedView.h"
#include "UI/RmlBridge/RmlTheme.h"
#include <filesystem>
#include "UI/RmlBridge/RmlPanelGeometry.h"
#include "UI/RmlBridge/ThemeFileInterface.h"
#include <algorithm>

namespace
{
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
    bool LogMessage(Rml::Log::Type, const Rml::String& message) override
    {
        if (message.find("variable name") != std::string::npos || message.find("data variable") != std::string::npos ||
            message.find("data expression") != std::string::npos)
            errors.push_back(message);
        return true;
    }
    std::vector<std::string> errors;
};

struct Fixture
{
    Fixture()
    {
        Rml::SetRenderInterface(&renderer);
        Rml::SetFileInterface(&files);
        Rml::SetSystemInterface(&log);
        REQUIRE(Rml::Initialise());
        REQUIRE(Rml::LoadFontFace(std::string(MU_FONT_DIR) + "/DejaVuSans.ttf"));
        REQUIRE(Rml::LoadFontFace(std::string(MU_FONT_DIR) + "/DejaVuSans-Bold.ttf"));
        context = Rml::CreateContext("theme-model-migration", {1280, 960});
        context->GetRootElement()->SetProperty("--ui-scale", "1");
    }
    ~Fixture()
    {
        Rml::Shutdown();
        Rml::SetSystemInterface(nullptr);
        Rml::SetFileInterface(nullptr);
        Rml::SetRenderInterface(nullptr);
    }
    Rml::ElementDocument* Load(const std::string& name)
    {
        auto* doc = context->LoadDocument(std::string(MU_RMLUI_DIR) + "/themes/munolith/" + name + ".rml");
        REQUIRE(doc);
        doc->GetElementById("panel")->SetProperty("font-family", "DejaVu Sans");
        doc->Show();
        return doc;
    }
    void Refresh()
    {
        context->Update();
        context->Update();
        context->Render();
    }
    void NoModelErrors()
    {
        for (const auto& error : log.errors)
            CAPTURE(error);
        CHECK(log.errors.empty());
    }
    mu::ui::window::ChatCommandRmlModel chat;
    mu::ui::window::ItemHelpRmlModel help[2];
    Rml::DataTypeRegister helpTypes[2];
    NullRenderer renderer;
    UI::RmlBridge::ThemeFileInterface files;
    ModelLog log;
    Rml::Context* context = nullptr;
};

void BindChat(Rml::DataModelConstructor& c, mu::ui::window::ChatCommandRmlModel& model)
{
    using namespace mu::ui::window;
    auto line = c.RegisterStruct<ChatCommandLine>();
    line.RegisterMember("text", &ChatCommandLine::text);
    line.RegisterMember("text_px", &ChatCommandLine::textPx);
    line.RegisterMember("index", &ChatCommandLine::index);
    c.RegisterArray<std::vector<ChatCommandLine>>();
    auto row = c.RegisterStruct<ChatCommandRow>();
    row.RegisterMember("text", &ChatCommandRow::text);
    row.RegisterMember("text_px", &ChatCommandRow::textPx);
    row.RegisterMember("index", &ChatCommandRow::index);
    row.RegisterMember("favourite", &ChatCommandRow::favourite);
    c.RegisterArray<std::vector<ChatCommandRow>>();
    auto parameter = c.RegisterStruct<ChatCommandParameterRow>();
    parameter.RegisterMember("label", &ChatCommandParameterRow::label);
    parameter.RegisterMember("label_text_px", &ChatCommandParameterRow::labelTextPx);
    parameter.RegisterMember("value", &ChatCommandParameterRow::value);
    parameter.RegisterMember("value_text_px", &ChatCommandParameterRow::valueTextPx);
    parameter.RegisterMember("missing", &ChatCommandParameterRow::missing);
    parameter.RegisterMember("placeholder", &ChatCommandParameterRow::placeholder);
    parameter.RegisterMember("edited", &ChatCommandParameterRow::edited);
    c.RegisterArray<std::vector<ChatCommandParameterRow>>();
    c.Bind("text_px", &model.textPx);
    c.Bind("page", &model.page);
    c.Bind("title", &model.title);
    c.Bind("empty_message", &model.emptyMessage);
    c.Bind("all_command_rows", &model.allCommandRows);
    c.Bind("all_template_rows", &model.allTemplateRows);
    c.Bind("parameters", &model.parameters);
    c.Bind("description_text", &model.descriptionText);
    c.Bind("edited_parameter_label", &model.editedParameterLabel);
    c.Bind("edit_value", &model.editValue);
    c.Bind("favourite_action", &model.favouriteAction);
    c.Bind("save_action", &model.saveAction);
    c.Bind("has_left_button", &model.hasLeftButton);
    c.Bind("has_right_button", &model.hasRightButton);
    c.Bind("left_text", &model.leftText);
    c.Bind("right_text", &model.rightText);
    c.Bind("exit_tooltip", &model.exitTooltip);
    for (const char* event : {"chat_command_exit", "chat_command_scroll", "chat_command_hit", "chat_command_left",
                              "chat_command_right", "chat_command_edit_key"})
        c.BindEventCallback(event, [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
}

void BindHelp(Rml::DataModelConstructor& c, mu::ui::window::ItemHelpRmlModel& model)
{
    using namespace mu::ui::window;
    if (auto line = c.RegisterStruct<ItemHelpLineEntry>())
    {
        line.RegisterMember("text", &ItemHelpLineEntry::text);
        line.RegisterMember("color", &ItemHelpLineEntry::color);
        line.RegisterMember("bold", &ItemHelpLineEntry::bold);
        line.RegisterMember("spacer", &ItemHelpLineEntry::spacer);
    }
    c.RegisterArray<std::vector<ItemHelpLineEntry>>();
    if (auto cell = c.RegisterStruct<ItemHelpCellEntry>())
    {
        cell.RegisterMember("text", &ItemHelpCellEntry::text);
        cell.RegisterMember("can_equip", &ItemHelpCellEntry::canEquip);
    }
    c.RegisterArray<std::vector<ItemHelpCellEntry>>();
    if (auto column = c.RegisterStruct<ItemHelpColumnEntry>())
    {
        column.RegisterMember("heading", &ItemHelpColumnEntry::heading);
        column.RegisterMember("cells", &ItemHelpColumnEntry::cells);
    }
    c.RegisterArray<std::vector<ItemHelpColumnEntry>>();
    c.Bind("text_px", &model.textPx);
    c.Bind("bold_text_px", &model.boldTextPx);
    c.Bind("line_height", &model.lineHeight);
    c.Bind("bold_line_height", &model.boldLineHeight);
    c.Bind("lines", &model.lines);
    c.Bind("columns", &model.columns);
    c.Bind("tail_lines", &model.tailLines);
}
} // namespace

TEST_CASE("Command workbench exposes the controller's per-parameter field and two-way value [ui][migration]")
{
    Fixture f;
    auto c = f.context->CreateDataModel("chat_command");
    auto& model = f.chat;
    model.textPx = 12.f;
    model.page = 1;
    model.title.text = "Command workbench";
    model.descriptionText = "Choose parameters before sending";
    model.parameters = {{"Count", 12.f, false, "2", 12.f, false, false},
                        {"Target", 12.f, false, "Player", 12.f, false, true}};
    model.editedParameterLabel = "Target";
    model.editValue = "Player";
    BindChat(c, model);
    auto handle = c.GetModelHandle();
    auto* doc = f.Load("chat_command");
    UI::RmlBridge::SlotPlacement{100.f, 50.f, 1.2f}.Apply(doc, "panel");
    f.Refresh();
    Rml::ElementList rows;
    doc->QuerySelectorAll(rows, ".cc-parameter");
    REQUIRE(rows.size() >= 2);
    CHECK_FALSE(rows[0]->QuerySelector("input")->IsVisible());
    auto* field = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(rows[1]->QuerySelector("input"));
    REQUIRE(field);
    CHECK(field->IsVisible());
    CHECK(field->Focus());
    CHECK(f.context->GetFocusElement() == field);
    field->SetValue("OtherPlayer");
    Rml::Dictionary parameters;
    parameters["value"] = Rml::String("OtherPlayer");
    field->DispatchEvent(Rml::EventId::Change, parameters);
    CHECK(model.editValue == "OtherPlayer");
    model.parameters[1].edited = false;
    handle.DirtyVariable("parameters");
    f.Refresh();
    CHECK_FALSE(field->IsVisible());
    CHECK(doc->GetElementById("value_field") == nullptr);
    f.NoModelErrors();
}

TEST_CASE("Item and set help render current content columns rather than removed recorded boxes [ui][migration]")
{
    Fixture f;
    int index = 0;
    for (const char* name : {"item_explanation", "set_item_explanation"})
    {
        auto& model = f.help[index++];
        model.textPx = 12.f;
        model.boldTextPx = 14.f;
        model.lineHeight = 14.f;
        model.boldLineHeight = 16.f;
        model.lines = {{"Guarded sword", "yellow", true, ""}};
        const bool hasTable = std::string(name) == "item_explanation";
        if (hasTable)
            model.columns = {{"Level", {{"1", true}, {"2", true}}}, {"Damage", {{"12", true}, {"24", false}}}};
        model.tailLines = {{"Knight can equip", "green", false, ""}};
        // Match production RmlModelBinder: type definitions belong to each model, not the
        // context's default register. Both item-help views can then use the same C++ row types.
        auto c = f.context->CreateDataModel(name, &f.helpTypes[index - 1]);
        BindHelp(c, model);
        auto* doc = f.Load(name);
        f.Refresh();
        auto* box = doc->GetElementById("tip_box");
        REQUIRE(box);
        CHECK(box->IsVisible());
        CHECK(box->GetInnerRML().find("Guarded sword") != std::string::npos);
        CHECK(box->GetInnerRML().find("Knight can equip") != std::string::npos);
        Rml::ElementList columns;
        box->QuerySelectorAll(columns, ".tip-column");
        std::erase_if(columns, [](const auto* e) { return !e->IsVisible(); });
        CHECK(columns.size() == (hasTable ? 2 : 0));
        if (hasTable)
        {
            auto* blocked = box->QuerySelector(".cannot-equip");
            REQUIRE(blocked);
            CHECK(blocked->GetInnerRML() == "24");
        }
        CHECK(doc->GetElementById("table_stage") == nullptr);
    }
    f.NoModelErrors();
}

TEST_CASE("Actual item and set help views isolate their type definitions in one context [ui][migration][lifecycle]")
{
    Fixture f;
    const auto cwd = std::filesystem::current_path();
    const auto previousTheme = UI::RmlBridge::GetActiveThemeName();
    std::filesystem::current_path(std::string(MU_RMLUI_DIR) + "/../../..");
    UI::RmlBridge::SetActiveThemeName("munolith");
    UI::RmlBridge::SetContextResolver([&f](std::string_view) { return f.context; });
    {
        mu::ui::window::ItemHelpView item("item_explanation", "Data/Interface/RmlUi/item_explanation.rml");
        mu::ui::window::ItemHelpView set("set_item_explanation", "Data/Interface/RmlUi/set_item_explanation.rml");
        item.Build();
        set.Build();
        CHECK(f.context->GetNumDocuments() == 2);
        item.Release();
        item.Build(); // A view reload must reuse existing registrations too.
        CHECK(f.context->GetNumDocuments() >= 2);
        f.NoModelErrors();
    }
    UI::RmlBridge::SetContextResolver({});
    UI::RmlBridge::SetActiveThemeName(previousTheme);
    std::filesystem::current_path(cwd);
}
