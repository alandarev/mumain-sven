#include "stdafx.h"
#include <doctest.h>
#include "UI/AuctionHouse/PriceInput.h"
#include "UI/AuctionHouse/WindowModel.h"
#include "UI/RmlBridge/RmlThemedView.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlNativeText.h"
#include <RmlUi/Core.h>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace
{
class Renderer : public Rml::RenderInterface
{
public:
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override
    {
        return 1;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i& size, const Rml::String&) override
    {
        size = {256, 256};
        return 1;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override
    {
        return 1;
    }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};

class Fixture
{
public:
    Fixture() : cwd(std::filesystem::current_path()), theme(UI::RmlBridge::GetActiveThemeName())
    {
        std::filesystem::current_path(MU_AUCTION_BIN);
        Rml::SetRenderInterface(&renderer);
        REQUIRE(Rml::Initialise());
        REQUIRE(Rml::LoadFontFace("fonts/DejaVuSans.ttf"));
        REQUIRE(Rml::LoadFontFace("fonts/LiberationSans-Regular.ttf"));
        context = Rml::CreateContext("auction-layout", {1024, 768});
        REQUIRE(context);
    }
    ~Fixture()
    {
        UI::RmlBridge::SetActiveThemeName(theme);
        Rml::Shutdown();
        Rml::SetRenderInterface(nullptr);
        std::filesystem::current_path(cwd);
    }
    Rml::Context* context = nullptr;

private:
    Renderer renderer;
    std::filesystem::path cwd;
    std::string theme;
};

void BindExcept(Rml::DataModelConstructor& c, UI::AuctionHouse::WindowModel& model,
                std::initializer_list<std::string_view> handled)
{
    UI::AuctionHouse::BindWindowModel(c, model);
    for (const char* name : {"close",          "tab",          "filter",         "reset",         "category",
                             "equipment",      "select",       "inventory",      "hover",         "inventory_hover",
                             "similar",        "leave",        "refresh",        "more",          "quote",
                             "sell_change",    "minus",        "plus",           "buy",           "sell",
                             "cancel_listing", "claim",        "dismiss",        "confirm",       "usable",
                             "luck",           "skill",        "open_filters",   "apply_filters", "cancel_filters",
                             "clear_filter",   "draft_luck",   "draft_skill",    "price_change",  "price_commit",
                             "price_key",      "price_wheel",  "select_content", "match_lowest",  "undercut",
                             "forget_price",   "max_quantity", "refresh_quote"})
        if (std::find(handled.begin(), handled.end(), name) == handled.end())
            c.BindEventCallback(name, [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
}

void Bind(Rml::DataModelConstructor& c, UI::AuctionHouse::WindowModel& model)
{
    BindExcept(c, model, {});
}
} // namespace

TEST_CASE("Auction native document stays bounded across tabs themes and scale [auction][rml]")
{
    Fixture fixture;
    for (const char* theme : {"legacy", "modern", "munolith"})
    {
        UI::RmlBridge::SetActiveThemeName(theme);
        UI::RmlBridge::ThemedView<UI::AuctionHouse::WindowModel> view(
            "auction_house", Bind,
            {{"Data/Interface/RmlUi/auction_house.rml", [&fixture] { return fixture.context; }}});
        REQUIRE(view.Ensure());
        auto* document = view.Document();
        REQUIRE(document);
        auto& model = view.GetModel();
        model.rows = {{"Excellent Dragon Gloves +13", "Defense 102\nDefense success rate +10%", "1,350kk", "Seller",
                       "excellent", "", 0, true, false}};
        model.inventory = model.rows;
        model.pending = 3;
        view.MarkDirty("pending");
        model.selected = true;
        model.selectedName = model.rows.front().name;
        model.selectedDetail = model.rows.front().detail;
        model.options = {{"0", "Any option"}, {"1", "Defense success rate +10%"}};
        model.equipment = "1";
        model.types = {{"0", "All types"}, {"1", "Swords"}};
        model.durations = {{"48", "48 h"}};
        model.duration = "48 h";
        model.sellGoods = true;
        model.maxQuantity = "Max (250)";
        model.marketLowest = "Lowest 1,990,000,000 · 2,000+ in stock";
        model.priceSource = "Current lowest";
        model.priceNote = "Rounded to 1,990,000,000";
        model.canForget = true;
        model.sellTiers.assign(8, {"1,990,000,000", "250"});
        // Results and inline validation at their longest.
        model.rows.front().selected = true;
        model.rows.front().breakdown = "Gross 2,000,000,000 Zen · Sale fee 200,000,000 Zen";
        model.status = "Bought 250 Jewel of Bless for 1,999,999,999 Zen · 248 in your bag, 2 in Collection";
        model.statusReceipt = true;
        model.statusLink = "collection";
        model.quantityHint = "Whole number 1–250";
        model.quantityHintError = true;
        model.listingCount = "Listings 20/20";
        model.listingCountFull = true;
        model.sellNote = "Posting fee: Need 100,000,000 Zen, you have 1,999,999,999.";
        model.buyHint = "Whole number 1–4,294,967,295";
        model.buyNote = "Need 1,999,999,999 Zen, you have 50,000";
        for (const char* field :
             {"sell_goods", "max_quantity", "market_lowest", "price_source", "price_note", "can_forget", "sell_tiers",
              "status", "status_receipt", "status_link", "quantity_hint", "quantity_hint_error", "listing_count",
              "listing_count_full", "sell_note", "buy_hint", "buy_note"})
            view.MarkDirty(field);
        for (int width : {640, 1024, 1920})
            for (float scale : {1.f, 1.6f, 2.f})
                for (const char* tab : {"browse", "sell", "own", "collection"})
                {
                    fixture.context->SetDimensions({width, width == 640 ? 480 : 768});
                    fixture.context->SetDensityIndependentPixelRatio(scale);
                    INFO(std::string(theme), " ", width, " scale ", scale, " tab ", std::string(tab));
                    model.tab = tab;
                    view.MarkDirty("tab");
                    view.MarkDirty("rows");
                    view.MarkDirty("inventory");
                    view.MarkDirty("selected");
                    view.MarkDirty("selected_name");
                    view.MarkDirty("selected_detail");
                    view.MarkDirty("options");
                    view.MarkDirty("types");
                    view.MarkDirty("durations");
                    view.MarkDirty("duration");
                    document->Show();
                    UI::RmlBridge::ApplyNativeTextSize(fixture.context);
                    fixture.context->Update();
                    fixture.context->Update();
                    auto* panel = document->GetElementById("panel");
                    REQUIRE(panel);
                    const auto size = panel->GetBox().GetSize(Rml::BoxArea::Border);
                    CHECK(size.x > 0);
                    CHECK(size.y > 0);
                    CHECK(size.x <= width);
                    CHECK(size.y <= fixture.context->GetDimensions().y);
                    {
                        // The receipt's Open link stays inside the panel.
                        Rml::ElementList links;
                        document->GetElementsByClassName(links, "status-link");
                        std::erase_if(links, [](Rml::Element* link) { return !link->IsVisible(true); });
                        REQUIRE(links.size() == 1);
                        const auto right = panel->GetAbsoluteOffset(Rml::BoxArea::Border).x + size.x;
                        CHECK(links.front()->GetBox().GetSize(Rml::BoxArea::Border).x > 0);
                        CHECK(links.front()->GetAbsoluteOffset(Rml::BoxArea::Border).x +
                                  links.front()->GetBox().GetSize(Rml::BoxArea::Border).x <=
                              right + 1);
                        Rml::ElementList notes;
                        document->GetElementsByClassName(notes, "field-note");
                        for (auto* note : notes)
                            if (note->IsVisible(true))
                                CHECK(note->GetAbsoluteOffset(Rml::BoxArea::Border).x +
                                          note->GetBox().GetSize(Rml::BoxArea::Border).x <=
                                      right + 1);
                        if (model.tab == "sell")
                            CHECK(std::count_if(notes.begin(), notes.end(),
                                                [](Rml::Element* note) { return note->IsVisible(true); }) == 3);
                    }
                    if (model.tab == "browse")
                    {
                        Rml::ElementList toggles;
                        document->GetElementsByClassName(toggles, "filter-toggle");
                        int visibleToggles = 0;
                        for (auto* toggle : toggles)
                        {
                            if (!toggle->IsVisible(true))
                                continue;
                            ++visibleToggles;
                            CHECK(toggle->GetBox().GetSize(Rml::BoxArea::Border).x > 0);
                            CHECK(toggle->GetBox().GetSize(Rml::BoxArea::Border).y > 0);
                            REQUIRE(toggle->GetNumChildren() == 2);
                            CHECK(toggle->GetChild(1)->GetBox().GetSize(Rml::BoxArea::Border).x > 0);
                        }
                        CHECK(visibleToggles == 1);
                        Rml::ElementList filterActions;
                        document->GetElementsByClassName(filterActions, "filter-actions");
                        REQUIRE(filterActions.size() == 1);
                        const auto right = panel->GetAbsoluteOffset(Rml::BoxArea::Border).x + size.x;
                        for (int i = 0; i < filterActions.front()->GetNumChildren(); ++i)
                        {
                            auto* action = filterActions.front()->GetChild(i);
                            CHECK(action->GetAbsoluteOffset(Rml::BoxArea::Border).x +
                                      action->GetBox().GetSize(Rml::BoxArea::Border).x <=
                                  right + 1);
                        }
                    }
                    auto* results = document->GetElementById("results");
                    Rml::ElementList actions;
                    document->GetElementsByClassName(actions, "primary");
                    for (auto* action : actions)
                    {
                        if (!action->IsVisible(true))
                            continue;
                        const auto parentWidth = action->GetParentNode()->GetBox().GetSize(Rml::BoxArea::Content).x;
                        INFO(action->GetParentNode()->GetTagName(), " display ",
                             action->GetParentNode()->GetProperty("display")->ToString());
                        CHECK(action->GetBox().GetSize(Rml::BoxArea::Border).x <= parentWidth + 1);
                    }
                    model.review = true;
                    view.MarkDirty("review");
                    model.confirmName = "Excellent Dragon Gloves +13";
                    view.MarkDirty("confirm_name");
                    model.confirmTotal = "4,294,967,295 Zen";
                    view.MarkDirty("confirm_total");
                    model.confirmDetail = "6 × 590,000 = 3,540,000\n2 × 595,000 = 1,190,000\n"
                                          "+20 at higher prices · 12,400,000 Zen";
                    view.MarkDirty("confirm_detail");
                    model.confirmWarning = "Cannot Equip with this character. You can still buy it.";
                    view.MarkDirty("confirm_warning");
                    model.confirmFee = "Posting fee 100,000,000 (5 %, charged now, not refunded)\n"
                                       "Sale fee 200,000,000 (10 %, when sold)\n"
                                       "You receive 1,800,000,000 if all sell\nDuration 48 h";
                    view.MarkDirty("confirm_fee");
                    fixture.context->Update();
                    Rml::ElementList cards;
                    document->GetElementsByClassName(cards, "review-card");
                    REQUIRE(cards.size() == 1);
                    CHECK(cards.front()->GetBox().GetSize(Rml::BoxArea::Border).x <= size.x);
                    CHECK(cards.front()->GetBox().GetSize(Rml::BoxArea::Border).y <= size.y);
                    model.review = false;
                    view.MarkDirty("review");
                    fixture.context->Update();
                    if (results)
                    {
                        CHECK(results->GetClientHeight() > 0);
                        CHECK(results->GetClientHeight() <= size.y);
                    }
                }
        view.Release();
    }
}

TEST_CASE("Auction option dropdown selects a parameter by mouse [auction][rml]")
{
    Fixture fixture;
    UI::RmlBridge::SetActiveThemeName("legacy");
    UI::RmlBridge::ThemedView<UI::AuctionHouse::WindowModel> view(
        "auction_house", Bind, {{"Data/Interface/RmlUi/auction_house.rml", [&fixture] { return fixture.context; }}});
    REQUIRE(view.Ensure());
    auto* document = view.Document();
    auto& model = view.GetModel();
    model.options = {{"0", "Any option"},         {"1", "Defense success rate +10%"},
                     {"2", "Reflect damage +5%"}, {"4", "Damage decrease +4%"},
                     {"8", "Maximum mana +4%"},   {"16", "Maximum HP +4%"},
                     {"32", "Zen drop +30%"},     {"64", "Excellent damage rate +10%"},
                     {"128", "Attack speed +7"},  {"256", "Damage increase +2%"}};
    view.MarkDirty("options");
    auto* panel = document->GetElementById("panel");
    panel->SetProperty("left", "20px");
    panel->SetProperty("top", "64px");
    document->Show();
    UI::RmlBridge::ApplyNativeTextSize(fixture.context);
    fixture.context->Update();
    fixture.context->Render();
    Rml::ElementList controls;
    document->GetElementsByClassName(controls, "option-control");
    REQUIRE(controls.size() == 1);
    auto* select = controls.front()->GetChild(1);
    REQUIRE(select);
    const auto position = select->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto size = select->GetBox().GetSize(Rml::BoxArea::Border);
    auto click = [&](int x, int y)
    {
        fixture.context->ProcessMouseMove(x, y, 0);
        fixture.context->ProcessMouseButtonDown(0, 0);
        fixture.context->Update();
        fixture.context->Render();
        fixture.context->ProcessMouseMove(x, y, 0);
        fixture.context->Update();
        fixture.context->Render();
        fixture.context->ProcessMouseMove(x, y, 0);
        fixture.context->ProcessMouseButtonUp(0, 0);
        fixture.context->Update();
        fixture.context->Render();
    };
    click(static_cast<int>(position.x + 10), static_cast<int>(position.y + 10));
    click(static_cast<int>(position.x + 30), static_cast<int>(position.y + size.y + 36));
    CHECK(model.option == "1");
    view.Release();
}

TEST_CASE("Auction goods price tiers keep three readable columns and the order footer [auction][rml]")
{
    Fixture fixture;
    for (const char* theme : {"legacy", "modern", "munolith"})
    {
        UI::RmlBridge::SetActiveThemeName(theme);
        UI::RmlBridge::ThemedView<UI::AuctionHouse::WindowModel> view(
            "auction_house", Bind,
            {{"Data/Interface/RmlUi/auction_house.rml", [&fixture] { return fixture.context; }}});
        REQUIRE(view.Ensure());
        auto* document = view.Document();
        auto& model = view.GetModel();
        model.goods = true;
        model.selected = true;
        model.selectedName = "Jewel of Bless";
        model.rows = {{"Jewel of Chaos", "", "150k Zen / each"},
                      {"Jewel of Bless", "", "591k Zen / each"},
                      {"Jewel of Soul", "", "1.2k Zen / each"},
                      {"Jewel of Life", "", "2.5kk Zen / each"}};
        for (int width : {640, 1024, 1920})
            for (float scale : {1.f, 1.6f})
                for (int tierCount : {3, 4, 8})
                {
                    INFO(std::string(theme), " width ", width, " scale ", scale, " tiers ", tierCount);
                    fixture.context->SetDimensions({width, width == 640 ? 480 : 768});
                    fixture.context->SetDensityIndependentPixelRatio(scale);
                    model.quoteTiers.assign(tierCount, {"591k Zen", "30", "–"});
                    model.quoteTiers.front() = {"590k Zen", "4 → 10", "10", "new", true};
                    model.quoteOrder = "10 for 5,900,000 Zen";
                    model.quoteBeyond = tierCount == 8 ? "+20 at higher prices · 12,400,000 Zen" : "";
                    model.quoteShort = "Only 30 available";
                    model.quoteTotal = "5,900,000 Zen";
                    model.quoteAge = "Updated 3 s ago";
                    model.buyQuantity = "10";
                    for (const char* field :
                         {"goods", "selected", "selected_name", "rows", "quote_tiers", "quote_order", "quote_beyond",
                          "quote_short", "quote_total", "quote_age", "buy_quantity"})
                        view.MarkDirty(field);
                    document->Show();
                    UI::RmlBridge::ApplyNativeTextSize(fixture.context);
                    fixture.context->Update();
                    fixture.context->Update();
                    Rml::ElementList tables;
                    document->GetElementsByClassName(tables, "depth");
                    REQUIRE(tables.size() == 1);
                    auto* table = tables.front();
                    const float tableWidth = table->GetBox().GetSize(Rml::BoxArea::Content).x;
                    REQUIRE(tableWidth > 0);
                    Rml::ElementList rows;
                    table->GetElementsByClassName(rows, "depth-row");
                    std::erase_if(rows, [](Rml::Element* row) { return !row->IsVisible(true); });
                    REQUIRE(rows.size() == tierCount + 1);
                    auto right = [](Rml::Element* element)
                    {
                        return element->GetAbsoluteOffset(Rml::BoxArea::Border).x +
                               element->GetBox().GetSize(Rml::BoxArea::Border).x;
                    };
                    const float tableRight = table->GetAbsoluteOffset(Rml::BoxArea::Content).x + tableWidth;
                    for (auto* row : rows)
                    {
                        CHECK(row->GetBox().GetSize(Rml::BoxArea::Content).x > tableWidth * 0.9f);
                        REQUIRE(row->GetNumChildren() == 3);
                        auto* price = row->GetChild(0);
                        auto* stock = row->GetChild(1);
                        auto* order = row->GetChild(2);
                        CHECK(price->GetAbsoluteOffset(Rml::BoxArea::Border).x <
                              stock->GetAbsoluteOffset(Rml::BoxArea::Border).x);
                        CHECK(stock->GetAbsoluteOffset(Rml::BoxArea::Border).x <
                              order->GetAbsoluteOffset(Rml::BoxArea::Border).x);
                        CHECK(right(order) <= tableRight + 1);
                    }
                    Rml::ElementList footers;
                    table->GetElementsByClassName(footers, "depth-footer");
                    std::erase_if(footers, [](Rml::Element* footer) { return !footer->IsVisible(true); });
                    CHECK(footers.size() == (tierCount == 8 ? 2u : 1u));
                    for (auto* footer : footers)
                        CHECK(right(footer) <= tableRight + 1);
                    // The freshness line and its Refresh control stay inside the details pane.
                    Rml::ElementList freshness;
                    document->GetElementsByClassName(freshness, "refresh-quote");
                    REQUIRE(freshness.size() == 1);
                    auto* refresh = freshness.front();
                    CHECK(refresh->IsVisible(true));
                    auto* details = refresh->GetParentNode();
                    while (details && !details->IsClassSet("details"))
                        details = details->GetParentNode();
                    REQUIRE(details);
                    CHECK(right(refresh) <= right(details) + 1);
                    CHECK(refresh->GetBox().GetSize(Rml::BoxArea::Border).x > 0);
                    if (tierCount == 8)
                    {
                        REQUIRE(table->GetScrollHeight() > table->GetClientHeight());
                        table->SetScrollTop(table->GetScrollHeight());
                        fixture.context->Update();
                        auto* last = rows.back();
                        CHECK(last->GetAbsoluteOffset(Rml::BoxArea::Border).y +
                                  last->GetBox().GetSize(Rml::BoxArea::Border).y <=
                              table->GetAbsoluteOffset(Rml::BoxArea::Content).y + table->GetClientHeight() + 1);
                    }
                }
        view.Release();
    }
}

TEST_CASE("Auction sell fields select on focus and route price keys and wheel [auction][rml]")
{
    Fixture fixture;
    for (const char* theme : {"legacy", "modern"})
    {
        UI::RmlBridge::SetActiveThemeName(theme);
        std::vector<UI::AuctionHouse::PriceStepInput> steps;
        Rml::String focused;
        auto bind = [&steps, &focused](Rml::DataModelConstructor& c, UI::AuctionHouse::WindowModel& model)
        {
            BindExcept(c, model, {"select_content", "price_key", "price_wheel"});
            c.BindEventCallback("select_content",
                                [&focused](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&)
                                { focused = event.GetCurrentElement()->GetId(); });
            auto route = [&steps](auto read)
            {
                return [&steps, read](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&)
                {
                    if (const auto step = read(event))
                    {
                        event.StopPropagation();
                        steps.push_back(*step);
                    }
                };
            };
            c.BindEventCallback("price_key", route(&UI::AuctionHouse::PriceStepFromKey));
            c.BindEventCallback("price_wheel", route(&UI::AuctionHouse::PriceStepFromWheel));
        };
        UI::RmlBridge::ThemedView<UI::AuctionHouse::WindowModel> view(
            "auction_house", bind,
            {{"Data/Interface/RmlUi/auction_house.rml", [&fixture] { return fixture.context; }}});
        REQUIRE(view.Ensure());
        auto* document = view.Document();
        auto& model = view.GetModel();
        fixture.context->SetDimensions({1024, 768});
        fixture.context->SetDensityIndependentPixelRatio(1.f);
        model.tab = "sell";
        model.selected = true;
        model.selectedName = "Jewel of Bless";
        model.sellGoods = true;
        model.maxQuantity = "Max (8)";
        model.marketLowest = "Lowest 595,000 · 10 in stock";
        model.priceSource = "Last price";
        model.price = "590,000";
        model.canForget = model.canMatch = model.canUndercut = true;
        model.sellTiers = {{"595,000", "4"}, {"600,000", "6"}};
        model.durations = {{"48", "48 h"}};
        model.duration = "48 h";
        for (const char* field :
             {"tab", "selected", "selected_name", "sell_goods", "max_quantity", "market_lowest", "price_source",
              "price", "can_forget", "can_match", "can_undercut", "sell_tiers", "durations", "duration"})
            view.MarkDirty(field);
        document->Show();
        UI::RmlBridge::ApplyNativeTextSize(fixture.context);
        fixture.context->Update();
        fixture.context->Update();
        INFO(std::string(theme));

        auto* panel = document->GetElementById("panel");
        auto* quantity = document->GetElementById("sell-quantity");
        auto* price = document->GetElementById("sell-price");
        REQUIRE(panel);
        REQUIRE(quantity);
        REQUIRE(price);
        const float panelRight =
            panel->GetAbsoluteOffset(Rml::BoxArea::Border).x + panel->GetBox().GetSize(Rml::BoxArea::Border).x;
        Rml::ElementList controls;
        document->GetElementsByClassName(controls, "market-actions");
        REQUIRE(controls.size() == 1);
        for (auto* control : {quantity, price, controls.front()})
        {
            CHECK(control->IsVisible(true));
            CHECK(control->GetAbsoluteOffset(Rml::BoxArea::Border).x +
                      control->GetBox().GetSize(Rml::BoxArea::Border).x <=
                  panelRight + 1);
        }
        CHECK(price->GetAttribute<Rml::String>("placeholder", "") == "Enter price");
        CHECK(price->GetBox().GetSize(Rml::BoxArea::Border).x > 80.f);
        CHECK(quantity->GetBox().GetSize(Rml::BoxArea::Border).x > 40.f);

        auto centre = [](Rml::Element* element)
        {
            const auto position = element->GetAbsoluteOffset(Rml::BoxArea::Border);
            const auto size = element->GetBox().GetSize(Rml::BoxArea::Border);
            return Rml::Vector2i{static_cast<int>(position.x + size.x / 2), static_cast<int>(position.y + size.y / 2)};
        };
        auto click = [&](Rml::Element* element)
        {
            const auto at = centre(element);
            fixture.context->ProcessMouseMove(at.x, at.y, 0);
            fixture.context->ProcessMouseButtonDown(0, 0);
            fixture.context->ProcessMouseButtonUp(0, 0);
            fixture.context->Update();
        };

        // Clicking into a quantity of 1 and typing 8 gives 8: the window selects after the click.
        click(quantity);
        CHECK(focused == "sell-quantity");
        UI::AuctionHouse::SelectContentIfFocused(document->GetElementById(focused));
        fixture.context->ProcessTextInput("8");
        fixture.context->Update();
        CHECK(model.quantity == "8");

        click(price);
        CHECK(focused == "sell-price");
        steps.clear();
        fixture.context->ProcessKeyDown(Rml::Input::KI_UP, 0);
        fixture.context->ProcessKeyDown(Rml::Input::KI_DOWN, Rml::Input::KM_SHIFT);
        fixture.context->ProcessKeyDown(Rml::Input::KI_LEFT, 0);
        REQUIRE(steps.size() == 2);
        CHECK(steps[0].up);
        CHECK(steps[0].steps == 1);
        CHECK_FALSE(steps[1].up);
        CHECK(steps[1].steps == 10);

        steps.clear();
        CHECK_FALSE(fixture.context->ProcessMouseWheel(Rml::Vector2f{0.f, -1.f}, 0)); // Consumed by the field.
        CHECK(fixture.context->ProcessMouseWheel(Rml::Vector2f{0.f, 1.f}, Rml::Input::KM_SHIFT) == false);
        REQUIRE(steps.size() == 2);
        CHECK(steps[0].up);
        CHECK_FALSE(steps[1].up);
        CHECK(steps[1].steps == 10);

        // Not hovered, or not focused: the wheel is left to the page.
        steps.clear();
        const auto away = centre(quantity);
        fixture.context->ProcessMouseMove(away.x, away.y, 0);
        fixture.context->ProcessMouseWheel(Rml::Vector2f{0.f, -1.f}, 0);
        click(quantity);
        const auto over = centre(price);
        fixture.context->ProcessMouseMove(over.x, over.y, 0);
        fixture.context->ProcessMouseWheel(Rml::Vector2f{0.f, -1.f}, 0);
        CHECK(steps.empty());
        view.Release();
    }
}
