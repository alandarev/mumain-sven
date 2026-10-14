#include "UI/RmlBridge/RmlPanelGeometry.h"
#include <doctest.h>
#include "UI/RmlBridge/ThemeFileInterface.h"
#include <RmlUi/Core.h>
#include "Guild/GuildMakeRmlModel.h"
#include "Guild/GuildInfoRmlModel.h"
#include "UI/Party/PartyListRmlModel.h"
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
struct Fixture
{
    NullRenderer renderer;
    UI::RmlBridge::ThemeFileInterface files;
    Rml::Context* context = nullptr;
    Rml::ElementDocument* doc = nullptr;
    float zero = 0, scale = 1, textPx = 9;
    Rml::String label = "Guild";
    Fixture()
    {
        Rml::SetFileInterface(&files);
        Rml::SetRenderInterface(&renderer);
        REQUIRE(Rml::Initialise());
        REQUIRE(Rml::LoadFontFace(std::string(MU_RMLUI_DIR) + "/../../../fonts/DejaVuSans.ttf"));
        context = Rml::CreateContext("social-layout", {1920, 1080});
    }
    ~Fixture()
    {
        Rml::Shutdown();
        Rml::SetRenderInterface(nullptr);
        Rml::SetFileInterface(nullptr);
    }
    void Roots(Rml::DataModelConstructor& c)
    {
        c.Bind("root_x", &zero);
        c.Bind("root_y", &zero);
        c.Bind("root_scale", &scale);
        c.Bind("text_px", &textPx);
    }
    void Load(const char* name)
    {
        doc = context->LoadDocument(std::string(MU_RMLUI_DIR) + "/themes/munolith/" + name);
        REQUIRE(doc);
        doc->Show();
        Update();
    }
    void Update()
    {
        context->Update();
        context->Update();
    }
    Rml::Element* Element(const std::string& id)
    {
        auto* element = doc->GetElementById(id);
        REQUIRE(element);
        REQUIRE(element->IsVisible());
        return element;
    }
};
} // namespace

TEST_CASE("Officer desk emblem editor exposes all native cells and palette colors [ui][social]")
{
    Fixture f;
    mu::ui::window::GuildMakeRmlModel state;
    state.page = 1;
    state.guildName = "Officers";
    state.cells.resize(64, {"#ff0000ff", false, "#ff0000ff"});
    state.palette.resize(16, {"#00ff00ff", false, "#00ff00ff"});
    state.markCells.resize(64, "#ff0000ff");
    auto c = f.context->CreateDataModel("guild_make");
    f.Roots(c);
    c.Bind("page", &state.page);
    c.Bind("guild_name", &state.guildName);
    for (const auto* name : {"title_text", "info_text", "make_text", "back_text", "next_text", "name_text",
                             "result_text", "palette_hint_1", "palette_hint_2"})
        c.Bind(name, &f.label);
    auto cell = c.RegisterStruct<mu::ui::window::GuildMakeCellEntry>();
    cell.RegisterMember("color", &mu::ui::window::GuildMakeCellEntry::color);
    cell.RegisterMember("empty", &mu::ui::window::GuildMakeCellEntry::empty);
    cell.RegisterMember("mark_color", &mu::ui::window::GuildMakeCellEntry::markColor);
    c.RegisterArray<std::vector<mu::ui::window::GuildMakeCellEntry>>();
    c.RegisterArray<std::vector<Rml::String>>();
    c.Bind("cells", &state.cells);
    c.Bind("palette", &state.palette);
    c.Bind("selected", &state.selected);
    c.Bind("mark_cells", &state.markCells);
    c.BindEventCallback("guild_make_button", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
    auto model = c.GetModelHandle();
    f.Load("guild_make.rml");
    auto* first = f.Element("guild_cell_0");
    auto* last = f.Element("guild_cell_63");
    CHECK(first->GetBox().GetSize(Rml::BoxArea::Border).x == doctest::Approx(20));
    CHECK(last->GetAbsoluteOffset().x - first->GetAbsoluteOffset().x == doctest::Approx(140));
    CHECK(last->GetAbsoluteOffset().y - first->GetAbsoluteOffset().y == doctest::Approx(140));
    for (int i = 0; i < 64; ++i)
        f.Element("guild_cell_" + std::to_string(i));
    for (int i = 0; i < 16; ++i)
        f.Element("guild_color_" + std::to_string(i));
    auto* color0 = f.Element("guild_color_0");
    auto* color3 = f.Element("guild_color_3");
    auto* color4 = f.Element("guild_color_4");
    CHECK(color3->GetAbsoluteOffset().y == color0->GetAbsoluteOffset().y);
    CHECK(color4->GetAbsoluteOffset().y > color0->GetAbsoluteOffset().y);
    state.page = 2;
    model.DirtyVariable("page");
    f.Update();
    CHECK_FALSE(f.doc->GetElementById("mark_editor")->GetParentNode()->IsVisible());
    CHECK(state.guildName == "Officers");
    f.Element("result_mark");
    state.page = 1;
    model.DirtyVariable("page");
    f.Update();
    CHECK(first->IsVisible());
    CHECK(state.cells.size() == 64);
}

TEST_CASE("Officer desk Party HUD uses two-column targeting bounds and native HP fractions [ui][social]")
{
    Fixture f;
    mu::ui::window::PartyListRmlModel state;
    for (int i = 0; i < 5; ++i)
    {
        mu::ui::window::PartyListCardEntry card;
        card.index = i;
        card.name = "Member";
        card.nameTextPx = 9;
        card.healthPercent = 50;
        card.showLeave = i == 0;
        state.cards.push_back(card);
    }
    auto c = f.context->CreateDataModel("party_list");
    f.Roots(c);
    auto card = c.RegisterStruct<mu::ui::window::PartyListCardEntry>();
    card.RegisterMember("index", &mu::ui::window::PartyListCardEntry::index);
    card.RegisterMember("name", &mu::ui::window::PartyListCardEntry::name);
    card.RegisterMember("name_text_px", &mu::ui::window::PartyListCardEntry::nameTextPx);
    card.RegisterMember("leader", &mu::ui::window::PartyListCardEntry::leader);
    card.RegisterMember("absent", &mu::ui::window::PartyListCardEntry::absent);
    card.RegisterMember("defense_buff", &mu::ui::window::PartyListCardEntry::defenseBuff);
    card.RegisterMember("selected", &mu::ui::window::PartyListCardEntry::selected);
    card.RegisterMember("show_leave", &mu::ui::window::PartyListCardEntry::showLeave);
    card.RegisterMember("health_percent", &mu::ui::window::PartyListCardEntry::healthPercent);
    c.RegisterArray<std::vector<mu::ui::window::PartyListCardEntry>>();
    c.Bind("cards", &state.cards);
    c.BindEventCallback("party_leave", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
    f.Load("party_list.rml");
    auto* first = f.Element("party_card_0");
    auto* second = f.Element("party_card_1");
    auto* third = f.Element("party_card_2");
    auto* last = f.Element("party_card_4");
    CHECK(first->GetAbsoluteOffset().y == second->GetAbsoluteOffset().y);
    CHECK(second->GetAbsoluteOffset().x > first->GetAbsoluteOffset().x);
    CHECK(third->GetAbsoluteOffset().y > first->GetAbsoluteOffset().y);
    CHECK(last->GetAbsoluteOffset().y + last->GetBox().GetSize(Rml::BoxArea::Border).y <= 96);
    auto* hp = first->QuerySelector(".card-health");
    REQUIRE(hp);
    CHECK(hp->GetBox().GetSize(Rml::BoxArea::Content).x == doctest::Approx(43));
    CHECK(first->QuerySelector(".card-leave")->IsVisible());
    CHECK_FALSE(second->QuerySelector(".card-leave")->IsVisible());
}

TEST_CASE("Officer desk keeps guild identity visible while long member lists scroll [ui][social]")
{
    Fixture f;
    using namespace mu::ui::window;
    GuildInfoRmlModel model;
    model.noGuild = false;
    model.tab = 1;
    model.guildIdentity = "Officers";
    model.textPx = 9;
    model.markCells.resize(64, "#00ff00ff");
    model.getPositionButton = {"Position", true};
    for (int i = 0; i < 30; ++i)
        model.memberRows.push_back({"Member" + std::to_string(i), "", 9, "1", false, false});
    auto c = f.context->CreateDataModel("guild_info");
    c.Bind("text_px", &model.textPx);
    c.Bind("no_guild", &model.noGuild);
    c.Bind("tab", &model.tab);
    c.Bind("union_shown", &model.unionShown);
    c.RegisterArray<std::vector<Rml::String>>();
    c.Bind("mark_cells", &model.markCells);

    auto lineType = c.RegisterStruct<GuildLine>();
    lineType.RegisterMember("text", &GuildLine::text);
    lineType.RegisterMember("text_px", &GuildLine::textPx);
    c.Bind("hint_title", &model.hintTitle);
    c.Bind("hint_line1", &model.hintLine1);
    c.Bind("hint_line2", &model.hintLine2);
    c.Bind("hint_line3", &model.hintLine3);
    c.Bind("title", &model.title);
    c.Bind("guild_name", &model.guildName);
    c.Bind("guild_identity", &model.guildIdentity);
    c.Bind("tab_info", &model.tabInfo);
    c.Bind("tab_members", &model.tabMembers);
    c.Bind("tab_union", &model.tabUnion);
    c.Bind("notice_label", &model.noticeLabel);
    c.Bind("created", &model.created);
    c.Bind("score", &model.score);
    c.Bind("member_count", &model.memberCount);
    c.Bind("rival", &model.rival);
    c.Bind("header_name", &model.headerName);
    c.Bind("header_position", &model.headerPosition);
    c.Bind("header_server", &model.headerServer);
    c.Bind("header_union_name", &model.headerUnionName);
    c.Bind("header_union_members", &model.headerUnionMembers);
    c.RegisterArray<std::vector<GuildLine>>();
    c.Bind("alliance_lines", &model.allianceLines);
    auto noticeRow = c.RegisterStruct<GuildNoticeRow>();
    noticeRow.RegisterMember("text", &GuildNoticeRow::text);
    c.RegisterArray<std::vector<GuildNoticeRow>>();
    c.Bind("notice_rows", &model.noticeRows);
    auto memberRow = c.RegisterStruct<GuildMemberRow>();
    memberRow.RegisterMember("name", &GuildMemberRow::name);
    memberRow.RegisterMember("role", &GuildMemberRow::role);
    memberRow.RegisterMember("role_text_px", &GuildMemberRow::roleTextPx);
    memberRow.RegisterMember("server", &GuildMemberRow::server);
    memberRow.RegisterMember("selected", &GuildMemberRow::selected);
    memberRow.RegisterMember("officer", &GuildMemberRow::officer);
    c.RegisterArray<std::vector<GuildMemberRow>>();
    c.Bind("member_rows", &model.memberRows);
    auto unionRow = c.RegisterStruct<GuildUnionRow>();
    unionRow.RegisterMember("name", &GuildUnionRow::name);
    unionRow.RegisterMember("member_count", &GuildUnionRow::memberCount);
    unionRow.RegisterMember("count_text_px", &GuildUnionRow::countTextPx);
    unionRow.RegisterMember("mark_cells", &GuildUnionRow::markCells);
    unionRow.RegisterMember("selected", &GuildUnionRow::selected);
    c.RegisterArray<std::vector<GuildUnionRow>>();
    c.Bind("union_rows", &model.unionRows);
    auto actionButton = c.RegisterStruct<GuildActionButton>();
    actionButton.RegisterMember("label", &GuildActionButton::label);
    actionButton.RegisterMember("shown", &GuildActionButton::shown);
    c.Bind("guild_out_button", &model.guildOutButton);
    c.Bind("get_position_button", &model.getPositionButton);
    c.Bind("free_position_button", &model.freePositionButton);
    c.Bind("get_out_button", &model.getOutButton);
    c.Bind("union_create_button", &model.unionCreateButton);
    c.Bind("union_out_button", &model.unionOutButton);
    c.Bind("exit_tooltip", &model.exitTooltip);
    c.Bind("label_line_px", &model.labelLinePx);

    int selected = -1;
    for (const auto* event : {"guild_info_button", "guild_info_tab", "guild_info_select_union"})
        c.BindEventCallback(event, [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
    c.BindEventCallback("guild_info_select_member",
                        [&selected](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        { selected = args.at(0).Get<int>(); });
    auto handle = c.GetModelHandle();
    f.Load("guild_info.rml");
    auto* mark = f.Element("guild_mark");
    auto* list = f.Element("gi_member_list");
    auto* action = f.Element("btn_get_position");
    CHECK(list->GetScrollHeight() > list->GetClientHeight());
    CHECK(list->GetAbsoluteOffset().y + list->GetClientHeight() < action->GetAbsoluteOffset().y);
    list->SetScrollTop(list->GetScrollHeight());
    f.Update();
    CHECK(list->GetScrollTop() > 0);
    Rml::ElementList rows;
    f.doc->QuerySelectorAll(rows, ".member-row");
    std::erase_if(rows, [](auto* row) { return !row->IsVisible(); });
    REQUIRE(rows.size() == 30);
    rows.back()->DispatchEvent("click", {});
    CHECK(selected == 29);
    model.getPositionButton.shown = false;
    handle.DirtyVariable("get_position_button");
    f.Update();
    CHECK_FALSE(action->IsVisible());
    model.tab = 0;
    handle.DirtyVariable("tab");
    f.Update();
    CHECK(mark->IsVisible());
    CHECK_FALSE(list->GetParentNode()->IsVisible());
    UI::RmlBridge::SlotPlacement{0.f, 0.f, 1.5f}.Apply(f.doc, "panel");
    f.Update();
    CHECK(mark->IsVisible());
    CHECK(mark->GetBox().GetSize(Rml::BoxArea::Border).x == doctest::Approx(72));
}
