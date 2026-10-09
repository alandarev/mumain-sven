#include "stdafx.h"
#include <doctest.h>
#include "UI/AuctionHouse/PriceInput.h"
#include "UI/AuctionHouse/ReviewGate.h"
#include "UI/AuctionHouse/RowEvents.h"
#include "UI/AuctionHouse/WindowModel.h"
#include "UI/RmlBridge/RmlThemedView.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlNativeText.h"
#include <RmlUi/Core.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

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
                             "compare_market", "leave",        "refresh",        "more",          "quote",
                             "sell_change",    "minus",        "plus",           "buy",           "sell",
                             "cancel_listing", "claim",        "dismiss",        "confirm",       "usable",
                             "luck",           "skill",        "open_filters",   "apply_filters", "cancel_filters",
                             "clear_filter",   "draft_luck",   "draft_skill",    "price_change",  "price_commit",
                             "price_key",      "price_wheel",  "select_content", "match_lowest",  "undercut",
                             "forget_price",   "max_quantity", "refresh_quote",  "clear_name",    "back_to_selling",
                             "refresh_hover",  "draft_family", "draft_class",  "dismiss_status", "dismiss_notice",
                             "toggle_depth"})
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
        // The longest character name in the Can equip label.
        model.usableLabel = "Abcdefghij can equip";
        view.MarkDirty("usable_label");
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
                                       "Receive if all sell at least 1,799,999,999\nDuration 48 h";
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
        model.rows = {{"Jewel of Chaos", "", "150k Zen each"},
                      {"Jewel of Bless", "", "591k Zen each"},
                      {"Jewel of Soul", "", "1.2k Zen each"},
                      {"Jewel of Life", "", "2.5kk Zen each"}};
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

TEST_CASE("Auction sell fields select on focus; keys step, the wheel only over − and + [auction][rml]")
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
        model.stepDown = model.stepUp = true;
        model.sellTiers = {{"595,000", "4"}, {"600,000", "6"}};
        model.durations = {{"48", "48 h"}};
        model.duration = "48 h";
        for (const char* field :
             {"tab", "selected", "selected_name", "sell_goods", "max_quantity", "market_lowest", "price_source",
              "price", "can_forget", "can_match", "can_undercut", "step_down", "step_up", "sell_tiers", "durations",
              "duration"})
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

        // The wheel over the focused price field is left to the page: scrolling never changes a price.
        steps.clear();
        const auto over = centre(price);
        fixture.context->ProcessMouseMove(over.x, over.y, 0);
        fixture.context->ProcessMouseWheel(Rml::Vector2f{0.f, -1.f}, 0);
        CHECK(steps.empty());

        // Over − or +: one valid price per notch, ten with Shift, wheel up raises; the wheel is consumed there,
        // also when the field does not have the focus.
        auto* down = document->GetElementById("price-down");
        auto* up = document->GetElementById("price-up");
        REQUIRE(down);
        REQUIRE(up);
        click(quantity);
        const auto plus = centre(up);
        fixture.context->ProcessMouseMove(plus.x, plus.y, 0);
        CHECK_FALSE(fixture.context->ProcessMouseWheel(Rml::Vector2f{0.f, -1.f}, 0));
        const auto minus = centre(down);
        fixture.context->ProcessMouseMove(minus.x, minus.y, 0);
        CHECK_FALSE(fixture.context->ProcessMouseWheel(Rml::Vector2f{0.f, 1.f}, Rml::Input::KM_SHIFT));
        REQUIRE(steps.size() == 2);
        CHECK(steps[0].up);
        CHECK(steps[0].steps == 1);
        CHECK_FALSE(steps[1].up);
        CHECK(steps[1].steps == 10);

        // A disabled stepper (at a limit, or no price yet) takes no wheel: the page scrolls instead.
        steps.clear();
        model.stepUp = false;
        view.MarkDirty("step_up");
        fixture.context->Update();
        fixture.context->ProcessMouseMove(plus.x, plus.y, 0);
        fixture.context->ProcessMouseWheel(Rml::Vector2f{0.f, -1.f}, 0);
        CHECK(steps.empty());

        // Elsewhere in the panel: left to the page.
        steps.clear();
        const auto away = centre(quantity);
        fixture.context->ProcessMouseMove(away.x, away.y, 0);
        fixture.context->ProcessMouseWheel(Rml::Vector2f{0.f, -1.f}, 0);
        CHECK(steps.empty());
        view.Release();
    }
}

namespace
{
float Right(Rml::Element* element)
{
    return element->GetAbsoluteOffset(Rml::BoxArea::Border).x + element->GetBox().GetSize(Rml::BoxArea::Border).x;
}

float Bottom(Rml::Element* element)
{
    return element->GetAbsoluteOffset(Rml::BoxArea::Border).y + element->GetBox().GetSize(Rml::BoxArea::Border).y;
}

std::vector<Rml::Element*> Visible(Rml::ElementDocument* document, const char* className)
{
    Rml::ElementList elements;
    document->GetElementsByClassName(elements, className);
    std::erase_if(elements, [](Rml::Element* element) { return !element->IsVisible(true); });
    return {elements.begin(), elements.end()};
}

// The element is drawn and does not reach past the container's right edge.
void CheckWithin(Rml::Element* element, Rml::Element* container)
{
    REQUIRE(element);
    REQUIRE(container);
    CHECK(element->GetBox().GetSize(Rml::BoxArea::Border).x > 0);
    CHECK(element->GetBox().GetSize(Rml::BoxArea::Border).y > 0);
    CHECK(element->GetAbsoluteOffset(Rml::BoxArea::Border).x >= container->GetAbsoluteOffset(Rml::BoxArea::Border).x - 1);
    CHECK(Right(element) <= Right(container) + 1);
}
} // namespace

namespace
{
// The window's EditPrice and StepPrice, queued as the window queues them for its next update.
struct PriceFieldDriver
{
    UI::AuctionHouse::PriceEntry entry;
    UI::AuctionHouse::Market::Rules rules;
    UI::AuctionHouse::WindowModel* model = nullptr;
    UI::RmlBridge::ThemedView<UI::AuctionHouse::WindowModel>* view = nullptr;
    std::vector<std::function<void()>> queued;
    int commits = 0;

    UI::AuctionHouse::PriceFieldHandlers Handlers()
    {
        return {.edit = [this](bool commit) { queued.push_back([this, commit] { Edit(commit); }); },
                .step = [this](UI::AuctionHouse::PriceStepInput input)
                { queued.push_back([this, input] { Step(input); }); }};
    }
    void Edit(bool commit)
    {
        entry.Edit(model->price);
        if (!commit)
            return;
        ++commits;
        entry.Commit(rules);
        Show();
    }
    void Step(UI::AuctionHouse::PriceStepInput input)
    {
        entry.Edit(model->price);
        if (entry.Step(rules, input, 0))
            Show();
    }
    void Show()
    {
        model->price = entry.Text();
        view->MarkDirty("price");
    }
    void Run()
    {
        for (auto events = std::exchange(queued, {}); auto& event : events)
            event();
    }
};
} // namespace

TEST_CASE("Auction − and + right after typing step once from the typed price, like the keys and the wheel [auction][rml]")
{
    Fixture fixture;
    UI::RmlBridge::SetActiveThemeName("modern");
    PriceFieldDriver driver;
    driver.rules.priceStepPrecision = 2;
    driver.rules.maxPrice = 2000000000;
    auto bind = [&driver](Rml::DataModelConstructor& c, UI::AuctionHouse::WindowModel& model)
    {
        driver.model = &model;
        BindExcept(c, model, {"price_change", "price_commit", "price_key", "price_wheel", "minus", "plus"});
        UI::AuctionHouse::BindPriceFieldEvents(c, driver.Handlers());
    };
    UI::RmlBridge::ThemedView<UI::AuctionHouse::WindowModel> view(
        "auction_house", bind, {{"Data/Interface/RmlUi/auction_house.rml", [&fixture] { return fixture.context; }}});
    driver.view = &view;
    REQUIRE(view.Ensure());
    auto* document = view.Document();
    auto& model = view.GetModel();
    fixture.context->SetDimensions({1024, 768});
    fixture.context->SetDensityIndependentPixelRatio(1.f);
    model.tab = "sell";
    model.selected = true;
    model.selectedName = "Jewel of Bless";
    model.sellGoods = true;
    model.stepDown = model.stepUp = true;
    model.durations = {{"48", "48 h"}};
    for (const char* field : {"tab", "selected", "selected_name", "sell_goods", "step_down", "step_up", "durations"})
        view.MarkDirty(field);
    document->Show();
    UI::RmlBridge::ApplyNativeTextSize(fixture.context);
    fixture.context->Update();
    fixture.context->Update();

    auto* price = document->GetElementById("sell-price");
    auto* down = document->GetElementById("price-down");
    auto* up = document->GetElementById("price-up");
    auto* quantity = document->GetElementById("sell-quantity");
    REQUIRE(price);
    REQUIRE(down);
    REQUIRE(up);
    REQUIRE(quantity);
    auto moveTo = [&fixture](Rml::Element* element)
    {
        const auto position = element->GetAbsoluteOffset(Rml::BoxArea::Border);
        const auto size = element->GetBox().GetSize(Rml::BoxArea::Border);
        fixture.context->ProcessMouseMove(static_cast<int>(position.x + size.x / 2),
                                          static_cast<int>(position.y + size.y / 2), 0);
    };
    auto settle = [&]
    {
        fixture.context->Update();
        driver.Run();
        fixture.context->Update();
    };
    auto press = [&](Rml::Element* element, int modifiers)
    {
        moveTo(element);
        fixture.context->ProcessMouseButtonDown(0, modifiers);
        fixture.context->ProcessMouseButtonUp(0, modifiers);
        settle();
    };
    auto type = [&](const char* text)
    {
        press(price, 0);
        REQUIRE(fixture.context->GetFocusElement() == price);
        UI::AuctionHouse::SelectContentIfFocused(price);
        fixture.context->ProcessTextInput(text);
        settle();
        REQUIRE(model.price == text);
    };

    struct Vector
    {
        bool up;
        int modifiers;
        const char* expected;
    };
    // D3: the typed off-step price is the start, so the first step down lands on the round-down value.
    const Vector vectors[] = {{false, 0, "93,000,000"},
                              {true, 0, "93,500,000"},
                              {false, Rml::Input::KM_SHIFT, "88,500,000"},
                              {true, Rml::Input::KM_SHIFT, "98,000,000"}};
    for (const auto& vector : vectors)
    {
        INFO(std::string(vector.expected));
        type("93,240,000");
        driver.commits = 0;
        press(vector.up ? up : down, vector.modifiers);
        CHECK(model.price == vector.expected);
        CHECK(driver.commits == 0);

        type("93,240,000");
        fixture.context->ProcessKeyDown(vector.up ? Rml::Input::KI_UP : Rml::Input::KI_DOWN, vector.modifiers);
        settle();
        CHECK(model.price == vector.expected);

        type("93,240,000");
        moveTo(vector.up ? up : down);
        fixture.context->ProcessMouseWheel(Rml::Vector2f{0.f, vector.up ? -1.f : 1.f}, vector.modifiers);
        settle();
        CHECK(model.price == vector.expected);
    }

    // Leaving the field for anything else commits and rounds; the next − steps from the rounded price.
    type("93,240,000");
    driver.commits = 0;
    press(quantity, 0);
    CHECK(driver.commits == 1);
    CHECK(model.price == "93,000,000");
    CHECK(driver.entry.Note() == "Rounded to 93,000,000");
    press(down, 0);
    CHECK(model.price == "92,500,000");

    // A disabled button does nothing, so leaving the field for it still commits.
    model.stepDown = false;
    view.MarkDirty("step_down");
    fixture.context->Update();
    type("93,240,000");
    driver.commits = 0;
    press(down, 0);
    CHECK(driver.commits == 1);
    view.Release();
}

TEST_CASE("Auction Cancel and Collect inside a row do not select it, so a press during a read still acts [auction][rml]")
{
    // RR-01: with a read in flight the press waits in the ReviewGate; a row selection drops what
    // waits there (Window::SelectRow). The handlers mirror the window's, through the production bindings.
    Fixture fixture;
    UI::RmlBridge::SetActiveThemeName("modern");
    UI::AuctionHouse::ReviewGate gate;
    bool readInFlight = false;
    std::vector<int> selected;
    std::vector<std::string> acted;
    auto bind = [&](Rml::DataModelConstructor& c, UI::AuctionHouse::WindowModel& model)
    {
        BindExcept(c, model, {"select", "cancel_listing", "claim"});
        UI::AuctionHouse::BindRowEvents(
            c, {.select =
                    [&](int row)
                {
                    gate.Cancel();
                    selected.push_back(row);
                },
                .cancel = [&](int row)
                { gate.Start(readInFlight, [&acted, row] { acted.push_back("cancel " + std::to_string(row)); }); },
                .claim = [&](int row)
                { gate.Start(readInFlight, [&acted, row] { acted.push_back("claim " + std::to_string(row)); }); }});
    };
    UI::RmlBridge::ThemedView<UI::AuctionHouse::WindowModel> view(
        "auction_house", bind, {{"Data/Interface/RmlUi/auction_house.rml", [&fixture] { return fixture.context; }}});
    REQUIRE(view.Ensure());
    auto* document = view.Document();
    auto& model = view.GetModel();
    fixture.context->SetDimensions({1024, 768});
    fixture.context->SetDensityIndependentPixelRatio(1.f);
    document->Show();
    UI::RmlBridge::ApplyNativeTextSize(fixture.context);

    auto show = [&](const char* tab)
    {
        model = {};
        model.tab = tab;
        model.rows.assign(2, {"Jewel of Bless", "", "", "6 remaining of 8", ""});
        for (int i = 0; i < 2; ++i)
            model.rows[i].index = i;
        view.MarkDirty("tab");
        view.MarkDirty("rows");
        fixture.context->Update();
        fixture.context->Update();
    };
    auto click = [&](Rml::Element* element)
    {
        const auto position = element->GetAbsoluteOffset(Rml::BoxArea::Border);
        const auto size = element->GetBox().GetSize(Rml::BoxArea::Border);
        fixture.context->ProcessMouseMove(static_cast<int>(position.x + size.x / 2),
                                          static_cast<int>(position.y + size.y / 2), 0);
        fixture.context->ProcessMouseButtonDown(0, 0);
        fixture.context->ProcessMouseButtonUp(0, 0);
        fixture.context->Update();
    };
    // The shown rows; the data-for template itself stays in the document, hidden.
    auto listingRows = [&]
    {
        Rml::ElementList rows;
        document->GetElementsByClassName(rows, "listing");
        std::erase_if(rows, [](Rml::Element* row) { return !row->IsVisible(true); });
        return rows;
    };
    auto rowButton = [&](int row) -> Rml::Element*
    {
        const auto rows = listingRows();
        REQUIRE(rows.size() == 2);
        Rml::ElementList buttons;
        rows[row]->GetElementsByTagName(buttons, "button");
        // data-if hides the other tab's button.
        std::erase_if(buttons, [](Rml::Element* button) { return !button->IsVisible(true); });
        REQUIRE(buttons.size() == 1);
        return buttons.front();
    };

    for (const char* tab : {"own", "collection"})
    {
        INFO(std::string(tab));
        const std::string action = std::string(tab) == "own" ? "cancel 1" : "claim 1";
        show(tab);
        selected.clear();
        acted.clear();
        readInFlight = true;
        click(rowButton(1));
        CHECK(selected.empty());
        CHECK(acted.empty());
        REQUIRE(gate.Waiting());
        // The answer arrives: the waiting action runs.
        readInFlight = false;
        auto start = gate.TakeWaiting(readInFlight);
        REQUIRE(start);
        start();
        REQUIRE(acted.size() == 1);
        CHECK(acted.front() == action);

        // No read in flight: it acts at once, still without selecting.
        acted.clear();
        click(rowButton(0));
        CHECK(selected.empty());
        REQUIRE(acted.size() == 1);

        // The row itself still selects.
        const auto rows = listingRows();
        REQUIRE(rows.size() == 2);
        Rml::ElementList names;
        rows[1]->GetElementsByClassName(names, "item-name");
        REQUIRE_FALSE(names.empty());
        click(names.front());
        REQUIRE(selected.size() == 1);
        CHECK(selected.front() == 1);
    }
    view.Release();
}

TEST_CASE("Auction comparison banner, empty state, option marks and equip reasons stay inside the panel [auction][rml]")
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
        UI::AuctionHouse::RowModel row{"Excellent Great Dragon Armor +15", "", "1,350kk", "48h", "excellent"};
        row.cannotEquip = true;
        row.equipReason = "For Dark Knight, Magic Gladiator only (+2 more)";
        row.excellent = {{"Defense success rate +10%", 0x02, true}, {"Reflect damage +5%", 0x04},
                         {"Damage decrease +4%", 0x08, true},       {"Maximum mana +4%", 0x10},
                         {"Maximum HP +4%", 0x20},                  {"Zen drop +30%", 0x01}};
        const std::vector<UI::AuctionHouse::RowModel> rows(3, row);
        model.rows = rows;
        model.inventory = rows;
        model.comparing = "Excellent Great Dragon Armor +15";
        model.usableLabel = "Abcdefghij can equip";
        model.equipment = "2";
        model.types = {{"0", "All types"}, {"8", "Armor"}};
        model.options = {{"0", "Any option"}, {"1", "Defense success rate +10%"}};
        model.selected = true;
        model.selectedName = row.name;
        model.cannotEquip = true;
        model.selectedEquipReasons =
            "For Dark Knight, Magic Gladiator only\nStrength 1,200 · you have 300\nAgility 1,000 · you have 25";
        model.price = "12,500,000";
        model.priceSource = "Last price";
        model.canForget = model.canCompare = model.stepDown = model.stepUp = true;
        model.durations = {{"48", "48 h"}};
        model.duration = "48 h";
        model.filterError = "Normal items have no excellent options. Clear the Option choice or pick another family.";
        model.skillApplies = false;
        for (const auto& [width, height] :
             std::initializer_list<std::pair<int, int>>{{640, 480}, {1024, 768}, {1280, 960}, {1920, 1080}})
            for (float scale : {1.f, 1.6f, 2.f})
            {
                INFO(std::string(theme), " ", width, "x", height, " scale ", scale);
                fixture.context->SetDimensions({width, height});
                fixture.context->SetDensityIndependentPixelRatio(scale);
                auto update = [&]
                {
                    for (const char* field :
                         {"tab", "rows", "inventory", "comparing", "usable_label", "equipment", "types", "options",
                          "selected", "selected_name", "cannot_equip", "selected_equip_reasons", "price",
                          "price_source", "can_forget", "can_compare", "step_down", "step_up", "durations",
                          "duration", "filter_error", "skill_applies", "filters_open", "empty_text",
                          "can_clear_name", "can_reset_filters", "sell_goods"})
                        view.MarkDirty(field);
                    document->Show();
                    UI::RmlBridge::ApplyNativeTextSize(fixture.context);
                    fixture.context->Update();
                    fixture.context->Update();
                };
                model.tab = "browse";
                model.rows = rows;
                update();
                auto* panel = document->GetElementById("panel");
                auto* results = document->GetElementById("results");
                REQUIRE(panel);
                REQUIRE(results);
                CHECK(panel->GetBox().GetSize(Rml::BoxArea::Border).y <= height + 1);
                const auto banners = Visible(document, "compare-banner");
                REQUIRE(banners.size() == 1);
                CheckWithin(banners.front(), panel);
                const auto back = Visible(document, "back-to-selling");
                REQUIRE(back.size() == 1);
                CheckWithin(back.front(), banners.front());
                CHECK(Bottom(back.front()) <= Bottom(banners.front()) + 1);
                const auto marks = Visible(document, "excellent-option");
                CHECK(marks.size() == 18);
                for (auto* mark : marks)
                    CheckWithin(mark, results);
                CHECK(Visible(document, "matched").empty());
                for (auto* reason : Visible(document, "cannot-equip"))
                    CheckWithin(reason, results);
                const auto details = Visible(document, "details");
                REQUIRE(details.size() == 1);
                const auto reasons = Visible(document, "equip-reasons");
                REQUIRE(reasons.size() == 1);
                CheckWithin(reasons.front(), details.front());
                for (auto* action : Visible(document, "filter-actions"))
                    for (int i = 0; i < action->GetNumChildren(); ++i)
                        CHECK(Right(action->GetChild(i)) <= Right(panel) + 1);

                // An empty list names the search and offers Clear name beside Reset filters.
                model.rows.clear();
                model.emptyText = "No equipment matches \"Excellent Great Dragon Armor of the Fallen\"";
                model.canClearName = model.canResetFilters = true;
                update();
                for (const char* name : {"clear-name", "reset-filters"})
                {
                    const auto buttons = Visible(document, name);
                    REQUIRE(buttons.size() == 1);
                    CheckWithin(buttons.front(), results);
                }
                model.canResetFilters = false;
                update();
                CHECK(Visible(document, "reset-filters").empty());
                CHECK(Visible(document, "clear-name").size() == 1);

                // Filters: the reason stays inside the dialog and Skill hides where it cannot apply.
                model.filtersOpen = true;
                update();
                const auto cards = Visible(document, "filter-card");
                REQUIRE(cards.size() == 1);
                const auto errors = Visible(document, "filter-error");
                REQUIRE(errors.size() == 1);
                CheckWithin(errors.front(), cards.front());
                CHECK(cards.front()->GetBox().GetSize(Rml::BoxArea::Border).y <=
                      panel->GetBox().GetSize(Rml::BoxArea::Border).y + 1);
                CHECK(Visible(document, "filter-toggle").size() == 2); // Can equip behind the shade, Luck
                model.filtersOpen = false;

                // Sell: Compare market beside the price field and the Refresh button keep inside the panel.
                model.tab = "sell";
                update();
                auto* price = document->GetElementById("sell-price");
                REQUIRE(price);
                const auto compare = Visible(document, "compare-market");
                REQUIRE(compare.size() == 1);
                CheckWithin(compare.front(), panel);
                CHECK(std::abs(compare.front()->GetAbsoluteOffset(Rml::BoxArea::Border).y -
                               price->GetAbsoluteOffset(Rml::BoxArea::Border).y) < 8.f * scale);
                CHECK(price->GetBox().GetSize(Rml::BoxArea::Border).x > 60.f);
                const auto refresh = Visible(document, "refresh-inventory");
                REQUIRE(refresh.size() == 1);
                CheckWithin(refresh.front(), panel);
                const auto inventory = Visible(document, "inventory-pane");
                REQUIRE(inventory.size() == 1);
                CHECK(Right(refresh.front()) <= Right(inventory.front()) + 1);
            }
        view.Release();
    }
}

namespace
{
constexpr const char* AllFields[] = {
    "filters_open", "active_filters", "draft_family", "draft_ancient_set", "draft_class_line", "draft_option",
    "draft_upgrade_min", "draft_upgrade_max", "draft_min_excellent", "draft_min_sockets", "draft_max_price",
    "draft_luck", "draft_skill", "filter_error", "skill_applies", "usable_label", "empty_text", "can_clear_name",
    "can_reset_filters", "comparing", "can_compare", "selected_equip_reasons", "depth_open", "depth_label",
    "price_examples", "price_grid", "price_range", "buy_blocked", "quote_move", "notices", "ancient_set", "sets",
    "pending", "confirm_balance", "step_down", "step_up", "tab", "category", "equipment", "item_type", "search",
    "option", "option2", "family", "class_line", "sort", "upgrade_min", "upgrade_max", "min_excellent", "min_sockets",
    "max_price", "usable", "luck", "skill", "wallet", "status", "status_error", "count", "duration", "sale_total",
    "posting_fee", "sale_fee", "proceeds", "price", "quantity", "buy_quantity", "selected_name", "selected_detail",
    "selected_options", "selected_requirements", "selected_seller", "selected_price", "selected_icon",
    "selected_tone", "quote_total", "quote_tiers", "quote_order", "quote_beyond", "quote_short", "quote_age", "stock",
    "duration_value", "confirm_title", "confirm_name", "confirm_total", "confirm_fee", "confirm_detail",
    "confirm_warning", "status_receipt", "status_link", "quantity_hint", "quantity_hint_error", "listing_count",
    "listing_count_full", "sell_note", "buy_hint", "buy_hint_error", "buy_note", "confirm_action", "loading", "more",
    "selected", "cannot_equip", "goods", "sell_goods", "sell_ready", "busy", "review", "rows", "inventory", "options",
    "types", "durations", "price_source", "price_note", "market_lowest", "max_quantity", "can_match", "can_undercut",
    "can_forget", "sell_tiers"};

// The resolutions the window must fit without scrolling to its actions.
constexpr std::pair<int, int> PlayerResolutions[] = {{1024, 768}, {1280, 960}};

// The longest option block of a sellable item: skill, luck, the additional option, six excellent
// options, harmony and the level-380 option.
constexpr const char* LongestOptions = "Skill: Lightning Slash\nLuck (success rate of Jewel of Soul +25%)\n"
                                       "Luck (critical damage rate +5%)\nAdditional defense +16\n"
                                       "Increase Max HP +4%\nIncrease Max Mana +4%\nDamage decrease +4%\n"
                                       "Reflect damage +5%\nDefense success rate +10%\n"
                                       "Increase Zen after hunt +30%\nHarmony: Defense +60\n"
                                       "Level 380: PvP defense success rate +10%";

float Top(Rml::Element* element)
{
    return element->GetAbsoluteOffset(Rml::BoxArea::Border).y;
}

float Left(Rml::Element* element)
{
    return element->GetAbsoluteOffset(Rml::BoxArea::Border).x;
}

void Refresh(UI::RmlBridge::ThemedView<UI::AuctionHouse::WindowModel>& view, Rml::Context* context)
{
    for (const char* field : AllFields)
        view.MarkDirty(field);
    view.Document()->Show();
    UI::RmlBridge::ApplyNativeTextSize(context);
    context->Update();
    context->Update();
}

Rml::Element* Single(Rml::ElementDocument* document, const char* className)
{
    const auto elements = Visible(document, className);
    REQUIRE(elements.size() == 1);
    return elements.front();
}

// The element lies inside the scroll pane's visible part: no scrolling is needed to reach it.
void CheckInViewport(Rml::Element* element, Rml::Element* pane)
{
    REQUIRE(element);
    REQUIRE(pane);
    const float top = pane->GetAbsoluteOffset(Rml::BoxArea::Padding).y;
    CHECK(Top(element) >= top - 1);
    CHECK(Bottom(element) <= top + pane->GetClientHeight() + 1);
}

void FillSellGoods(UI::AuctionHouse::WindowModel& model, int tierCount)
{
    model = {};
    model.tab = "sell";
    UI::AuctionHouse::RowModel jewel{"Jewel of Bless", "", "", "", ""};
    jewel.units = 250;
    model.inventory.assign(12, jewel);
    for (int i = 0; i < 12; ++i)
        model.inventory[i].index = 12 + i;
    model.selected = true;
    model.selectedName = "Jewel of Bless";
    model.sellGoods = true;
    model.quantity = "250";
    model.maxQuantity = "Max (250)";
    model.quantityHint = "250 available";
    model.price = "1,990,000,000";
    model.priceSource = "Current lowest";
    model.priceNote = "Rounded to 1,990,000,000";
    model.priceGrid = "Prices round down to ~1 % steps · ↑/↓ one step · Shift ten.";
    model.priceRange = "1 – 2,000,000,000 Zen";
    model.canForget = model.canMatch = model.canUndercut = model.stepDown = model.stepUp = true;
    model.marketLowest = "Lowest 1,990,000,000 · 2,000+ in stock · 16,000 total";
    model.sellTiers.assign(tierCount, {"1,990,000,000", "2,000"});
    model.depthLabel = "Market depth (" + std::to_string(tierCount) + " prices)";
    model.durations = {{"24", "24 h"}, {"48", "48 h"}};
    model.durationValue = "48";
    model.saleTotal = "1,990,000,000 Zen";
    model.postingFee = "99,500,000 Zen";
    model.saleFee = "199,000,000 Zen";
    model.proceeds = "at least 1,791,000,000 Zen";
    model.listingCount = "Listings 19/20";
    model.sellNote = "Posting fee: Need 99,500,000 Zen, you have 50,000.";
    model.sellReady = false;
}

void FillSellEquipment(UI::AuctionHouse::WindowModel& model)
{
    model = {};
    model.tab = "sell";
    UI::AuctionHouse::RowModel armour{"Excellent Great Dragon Armor +15", "", "", "", "excellent"};
    model.inventory.assign(12, armour);
    for (int i = 0; i < 12; ++i)
        model.inventory[i].index = 12 + i;
    model.selected = true;
    model.selectedName = armour.name;
    model.selectedTone = "excellent";
    model.selectedOptions = LongestOptions;
    model.price = "1,990,000,000";
    model.priceSource = "Last price";
    model.priceNote = "Rounded to 1,990,000,000";
    model.priceGrid = "Prices round down to ~1 % steps · ↑/↓ one step · Shift ten.";
    model.priceRange = "1 – 2,000,000,000 Zen";
    model.canForget = model.canCompare = model.stepDown = model.stepUp = true;
    model.durations = {{"24", "24 h"}, {"48", "48 h"}};
    model.durationValue = "48";
    model.saleTotal = "1,990,000,000 Zen";
    model.postingFee = "99,500,000 Zen";
    model.saleFee = "199,000,000 Zen";
    model.proceeds = "at least 1,791,000,000 Zen";
    model.listingCount = "Listings 20/20";
    model.listingCountFull = true;
    model.sellNote = "Listing limit reached (Listings 20/20).";
}

void AddLongestFeedback(UI::AuctionHouse::WindowModel& model, std::size_t notices, bool announcement = false)
{
    model.status = "Outcome unknown. Reopen Zyro and check your bag, Collection and My auctions before retrying.";
    model.statusError = true;
    if (announcement)
        model.notices.push_back(
            {"[Notice] The server restarts for maintenance in 5 minutes. Please log out in a safe place.", ""});
    for (std::size_t i = 0; i < notices; ++i)
        model.notices.push_back({"Sold 250 × Jewel of Bless · 1,791,000,000 Zen waiting in your Collection",
                                 "collection"});
}
} // namespace

TEST_CASE("Auction Sell summary and Review listing stay in view without scrolling [auction][rml]")
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
        for (const auto& [width, height] : PlayerResolutions)
            for (float scale : {1.f, 2.f})
                for (const char* variant : {"goods", "goods-depth-open", "goods-notice", "goods-feedback", "equipment",
                                            "equipment-notice", "equipment-feedback"})
                {
                    INFO(std::string(theme), " ", width, "x", height, " scale ", scale, " ", std::string(variant));
                    fixture.context->SetDimensions({width, height});
                    fixture.context->SetDensityIndependentPixelRatio(scale);
                    const std::string_view kind = variant;
                    const bool goods = kind.starts_with("goods");
                    if (goods)
                        FillSellGoods(model, 8);
                    else
                        FillSellEquipment(model);
                    model.depthOpen = kind == "goods-depth-open";
                    // A receipt with one notice, or the most feedback there can be: an announcement, three
                    // sale notices and a two-line error.
                    const bool mostFeedback = kind.ends_with("feedback");
                    if (mostFeedback)
                        AddLongestFeedback(model, 3, true);
                    if (kind.ends_with("notice"))
                    {
                        AddLongestFeedback(model, 1);
                        model.status = "Listed 250 × Jewel of Bless at 1,990,000,000 · fee 99,500,000";
                        model.statusError = false;
                        model.statusReceipt = true;
                        model.statusLink = "own";
                    }
                    Refresh(view, fixture.context);

                    auto* panel = document->GetElementById("panel");
                    REQUIRE(panel);
                    CHECK(panel->GetBox().GetSize(Rml::BoxArea::Border).y <= height + 1);
                    auto* workspace = Single(document, "workspace");
                    auto* scroll = Single(document, "detail-scroll");
                    auto* summary = Single(document, "sell-summary");
                    auto* review = document->GetElementById("review-listing");
                    REQUIRE(review);
                    CHECK(review->IsVisible(true));
                    CHECK(scroll->GetScrollTop() == 0.f);
                    // The pinned summary is outside the scrolling details and inside the drawn panel.
                    CHECK(Top(summary) >= Bottom(scroll) - 1);
                    CHECK(Bottom(summary) <= Bottom(workspace) + 1);
                    CHECK(Bottom(workspace) <= Bottom(panel) + 1);
                    CHECK(Top(review) >= Top(summary) - 1);
                    CHECK(Bottom(review) <= Bottom(summary) + 1);
                    CheckWithin(review, summary);
                    Rml::ElementList rows;
                    summary->GetElementsByClassName(rows, "fee-row");
                    REQUIRE(rows.size() == 4);
                    for (auto* row : rows)
                    {
                        CHECK(row->GetBox().GetSize(Rml::BoxArea::Border).y > 0);
                        CHECK(Bottom(row) <= Bottom(summary) + 1);
                        CheckWithin(row, summary);
                    }
                    CHECK(Single(document, "listing-count")->GetBox().GetSize(Rml::BoxArea::Border).x > 0);
                    CheckWithin(Single(document, "sell-note"), summary);
                    // The price field is reachable without scrolling, also below the longest option block.
                    // The most feedback may push it down; the summary and Review listing stay pinned.
                    if (!mostFeedback)
                        CheckInViewport(document->GetElementById("sell-price"), scroll);
                    if (goods && !model.depthOpen && !mostFeedback)
                    {
                        // The lowest-price line, Match lowest / Undercut and the collapsed depth stay in view.
                        CheckInViewport(Single(document, "market-actions"), scroll);
                        auto* toggle = Single(document, "depth-toggle");
                        CHECK(toggle->IsClassSet("open") == false);
                        CHECK(Visible(document, "sell-depth").empty());
                        if (kind == "goods")
                            CheckInViewport(toggle, scroll);
                    }
                    if (model.depthOpen)
                        CHECK(Visible(document, "sell-depth").size() == 1);
                    for (auto* link : Visible(document, "status-close"))
                        CheckWithin(link, panel);
                    for (auto* link : Visible(document, "status-link"))
                        CheckWithin(link, panel);
                }
        view.Release();
    }
}

TEST_CASE("Auction panel keeps one rectangle across tabs and categories [auction][rml]")
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
        const UI::AuctionHouse::RowModel armour{"Excellent Dragon Armor +9", "", "12.5kk Zen", "48h", "excellent"};
        const UI::AuctionHouse::RowModel jewel{"Jewel of Bless", "", "590k Zen each", "20 available"};
        const std::vector<std::pair<const char*, std::function<void()>>> states{
            {"browse equipment",
             [&]
             {
                 model = {};
                 model.rows.assign(6, armour);
             }},
            {"browse equipment selected",
             [&]
             {
                 model = {};
                 model.rows.assign(6, armour);
                 model.selected = true;
                 model.selectedName = armour.name;
                 model.selectedOptions = LongestOptions;
             }},
            {"browse jewels",
             [&]
             {
                 model = {};
                 model.category = "1";
                 model.goods = true;
                 model.rows.assign(4, jewel);
                 model.selected = true;
                 model.selectedName = jewel.name;
                 model.quoteTiers.assign(8, {"591k", "30", "–"});
                 model.quoteMove = "Lowest fell 600k → 590k";
             }},
            {"browse materials empty",
             [&]
             {
                 model = {};
                 model.category = "2";
                 model.goods = true;
                 model.emptyText = "No materials named “xyz”";
             }},
            {"sell nothing selected",
             [&]
             {
                 FillSellGoods(model, 0);
                 model.selected = false;
             }},
            {"sell goods", [&] { FillSellGoods(model, 2); }},
            {"sell equipment", [&] { FillSellEquipment(model); }},
            {"own empty",
             [&]
             {
                 model = {};
                 model.tab = "own";
             }},
            {"own", [&]
             {
                 model = {};
                 model.tab = "own";
                 model.rows.assign(3, jewel);
             }},
            {"collection",
             [&]
             {
                 model = {};
                 model.tab = "collection";
                 model.rows.assign(2, jewel);
                 AddLongestFeedback(model, 3, true);
             }}};
        for (const auto& [width, height] : PlayerResolutions)
        {
            fixture.context->SetDimensions({width, height});
            fixture.context->SetDensityIndependentPixelRatio(1.f);
            std::optional<Rml::Vector2f> offset, size;
            for (const auto& [name, apply] : states)
            {
                INFO(std::string(theme), " ", width, "x", height, " ", std::string(name));
                apply();
                Refresh(view, fixture.context);
                auto* panel = document->GetElementById("panel");
                REQUIRE(panel);
                const auto panelOffset = panel->GetAbsoluteOffset(Rml::BoxArea::Border);
                const auto panelSize = panel->GetBox().GetSize(Rml::BoxArea::Border);
                CHECK(panelSize.y <= height);
                if (!offset)
                {
                    offset = panelOffset;
                    size = panelSize;
                    continue;
                }
                CHECK(std::abs(panelOffset.x - offset->x) < 0.5f);
                CHECK(std::abs(panelOffset.y - offset->y) < 0.5f);
                CHECK(std::abs(panelSize.x - size->x) < 0.5f);
                CHECK(std::abs(panelSize.y - size->y) < 0.5f);
            }
        }
        view.Release();
    }
}

TEST_CASE("Auction Sell inventory column starts at the list edge and the detail pane takes the rest [auction][rml]")
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
        for (const auto& [width, height] : PlayerResolutions)
        {
            INFO(std::string(theme), " ", width, "x", height);
            fixture.context->SetDimensions({width, height});
            fixture.context->SetDensityIndependentPixelRatio(1.f);
            model = {};
            model.rows.assign(3, {"Excellent Dragon Armor +9", "", "12.5kk Zen", "48h", "excellent"});
            model.selected = true;
            model.selectedName = "Excellent Dragon Armor +9";
            Refresh(view, fixture.context);
            auto* results = document->GetElementById("results");
            REQUIRE(results);
            const float listLeft = Left(results);
            const float detailsRight = Right(Single(document, "details"));

            FillSellGoods(model, 2);
            Refresh(view, fixture.context);
            auto* panel = document->GetElementById("panel");
            auto* inventory = Single(document, "inventory-pane");
            auto* details = Single(document, "details");
            // No strip left of the inventory: it starts where the Browse list starts.
            CHECK(std::abs(Left(inventory) - listLeft) < 1.f);
            CHECK(Left(inventory) - Left(panel) < 0.05f * panel->GetBox().GetSize(Rml::BoxArea::Border).x);
            // The detail pane ends where the Browse details end and is wider than the inventory.
            CHECK(std::abs(Right(details) - detailsRight) < 1.f);
            CHECK(details->GetBox().GetSize(Rml::BoxArea::Border).x > inventory->GetBox().GetSize(Rml::BoxArea::Border).x);
            CHECK(Left(details) >= Right(inventory) - 1);
        }
        view.Release();
    }
}

TEST_CASE("Auction inventory tile stays marked after Compare market and Back to selling [auction][rml]")
{
    Fixture fixture;
    UI::RmlBridge::SetActiveThemeName("legacy");
    UI::RmlBridge::ThemedView<UI::AuctionHouse::WindowModel> view(
        "auction_house", Bind, {{"Data/Interface/RmlUi/auction_house.rml", [&fixture] { return fixture.context; }}});
    REQUIRE(view.Ensure());
    auto* document = view.Document();
    auto& model = view.GetModel();
    fixture.context->SetDimensions({1280, 960});
    FillSellEquipment(model);
    constexpr int ArmourSlot = 14;
    // Selecting the armour marks its tile; the comparison (Browse) clears the details; Back to selling
    // selects the same slot again.
    CHECK(UI::AuctionHouse::MarkSelectedInventory(model.inventory, ArmourSlot));
    CHECK(UI::AuctionHouse::MarkSelectedInventory(model.inventory, -1));
    CHECK(std::none_of(model.inventory.begin(), model.inventory.end(), [](const auto& row) { return row.selected; }));
    CHECK(UI::AuctionHouse::MarkSelectedInventory(model.inventory, ArmourSlot));
    CHECK_FALSE(UI::AuctionHouse::MarkSelectedInventory(model.inventory, ArmourSlot));
    Refresh(view, fixture.context);
    const auto tiles = Visible(document, "inventory-item");
    REQUIRE(tiles.size() == model.inventory.size());
    int marked = 0;
    for (std::size_t i = 0; i < tiles.size(); ++i)
    {
        const bool selected = model.inventory[i].index == ArmourSlot;
        CHECK(tiles[i]->IsClassSet("selected") == selected);
        marked += selected;
    }
    CHECK(marked == 1);
    // The mark does not depend on keyboard focus.
    auto* markedTile = tiles[ArmourSlot - 12];
    markedTile->Blur();
    fixture.context->Update();
    CHECK(markedTile->GetProperty(Rml::PropertyId::BorderTopColor)->Get<Rml::Colourb>() !=
          tiles[0]->GetProperty(Rml::PropertyId::BorderTopColor)->Get<Rml::Colourb>());
    view.Release();
}

TEST_CASE("Auction Review purchase shows its state and reason next to it [auction][rml]")
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
        for (const auto& [width, height] : PlayerResolutions)
            for (const char* state : {"ready", "shortfall", "invalid-quantity", "own-listing", "cannot-equip"})
            {
                INFO(std::string(theme), " ", width, "x", height, " ", std::string(state));
                fixture.context->SetDimensions({width, height});
                fixture.context->SetDensityIndependentPixelRatio(1.f);
                const std::string_view name = state;
                model = {};
                const bool goods = name == "ready" || name == "shortfall" || name == "invalid-quantity";
                model.goods = goods;
                model.category = goods ? "1" : "0";
                model.selected = true;
                model.selectedName = goods ? "Jewel of Bless" : "Excellent Dragon Armor +9";
                model.rows.assign(3, {model.selectedName, "", "590k Zen each", "20 available"});
                model.quoteTiers = {{"590k", "6", "6"}, {"600k", "20", "2"}};
                model.quoteTotal = "4,740,000 Zen";
                model.buyQuantity = name == "invalid-quantity" ? "0" : "8";
                model.buyHint = name == "invalid-quantity" ? "Whole number 1–26" : "26 available";
                model.buyHintError = name == "invalid-quantity";
                model.cannotEquip = name == "cannot-equip";
                model.selectedPrice = "12.5kk Zen";
                if (name == "shortfall")
                {
                    model.buyBlocked = true;
                    model.buyNote = "Need 4,740,000 Zen, you have 1,000,000";
                }
                if (name == "own-listing")
                {
                    model.buyBlocked = true;
                    model.buyNote = "This is your listing.";
                }
                Refresh(view, fixture.context);
                auto* button = document->GetElementById("review-purchase");
                REQUIRE(button);
                CHECK(button->IsVisible(true));
                CHECK(button->GetInnerRML() == "Review purchase");
                CHECK(button->IsClassSet("disabled") == model.buyBlocked);
                CHECK(button->IsClassSet("incomplete") == model.buyHintError);
                auto* details = Single(document, "details");
                CheckWithin(button, details);
                const auto notes = Visible(document, "buy-note");
                if (model.buyNote.empty())
                    CHECK(notes.empty());
                else
                {
                    REQUIRE(notes.size() == 1);
                    CHECK(notes.front()->GetInnerRML() == model.buyNote);
                    // Directly under the button.
                    CHECK(Top(notes.front()) >= Bottom(button) - 1);
                    CHECK(Top(notes.front()) - Bottom(button) < 1.5f * notes.front()->GetBox().GetSize(Rml::BoxArea::Border).y);
                    CheckWithin(notes.front(), details);
                }
            }
        view.Release();
    }
}

TEST_CASE("Auction a live market move does not move the purchase controls [auction][rml]")
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
        for (const auto& [width, height] : PlayerResolutions)
        {
            INFO(std::string(theme), " ", width, "x", height);
            fixture.context->SetDimensions({width, height});
            fixture.context->SetDensityIndependentPixelRatio(1.f);
            model = {};
            model.goods = true;
            model.category = "1";
            model.selected = true;
            model.selectedName = "Jewel of Bless";
            model.rows.assign(3, {model.selectedName, "", "595k Zen each", "22 available"});
            model.quoteTiers = {{"595k", "4", "4"}, {"600k", "18", "4"}};
            model.quoteTotal = "4,780,000 Zen";
            model.buyQuantity = "8";
            model.buyHint = "22 available";
            model.quoteAge = "Updated 3 s ago";
            Refresh(view, fixture.context);
            auto* quantity = document->GetElementById("buy-quantity");
            auto* button = document->GetElementById("review-purchase");
            REQUIRE(quantity);
            REQUIRE(button);
            const float quantityTop = Top(quantity);
            const float buttonTop = Top(button);
            CHECK(Visible(document, "quote-move").empty());

            // The cheaper offer arrives while the player reaches for Review purchase.
            model.quoteTiers = {{"590k", "6", "6"}, {"595k", "4", "2"}, {"600k", "18", "-"}};
            model.quoteTotal = "4,730,000 Zen";
            model.buyHint = "28 available";
            model.quoteMove = "Lowest fell 595k → 590k";
            Refresh(view, fixture.context);
            const auto moves = Visible(document, "quote-move");
            REQUIRE(moves.size() == 1);
            CHECK(moves.front()->GetInnerRML() == model.quoteMove);
            auto* details = Single(document, "details");
            CheckWithin(moves.front(), details);
            CHECK(std::abs(Top(quantity) - quantityTop) < 0.5f);
            CHECK(std::abs(Top(button) - buttonTop) < 0.5f);
            const auto refresh = Visible(document, "refresh-quote");
            REQUIRE(refresh.size() == 1);
            CheckWithin(refresh.front(), details);
            CHECK(Bottom(moves.front()) <= Top(button));

            // When the move text expires the age returns in the same row.
            model.quoteMove.clear();
            Refresh(view, fixture.context);
            CHECK(Visible(document, "quote-move").empty());
            CHECK(std::abs(Top(button) - buttonTop) < 0.5f);
        }
        view.Release();
    }
}
