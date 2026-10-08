#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include <RmlUi/Core/Event.h>
#include "UI/RmlBridge/RmlTooltip.h"

namespace UI::AuctionHouse
{
void Window::Bind(Rml::DataModelConstructor& constructor, WindowModel& model)
{
    BindWindowModel(constructor, model);
    BindEvents(constructor);
}

void Window::BindEvents(Rml::DataModelConstructor& c)
{
    BindFilterEvents(c);
    BindPriceEvents(c);
    auto event = [this, &c](const char* name, const std::function<void(const Rml::VariantList&)>& action)
    {
        c.BindEventCallback(name, [this, action](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                            { m_Events.push_back([action, args] { action(args); }); });
    };
    auto index = [](const auto& args) { return args.empty() ? -1 : args.front().template Get<int>(); };
    event("close", [this](const auto&) { Show(false); });
    event("tab",
          [this](const auto& args)
          {
              if (!args.empty())
                  SwitchTab(args.front().template Get<Rml::String>());
          });
    event("filter", [this](const auto&) { FilterChanged(); });
    auto toggle = [this, &event](const char* name, bool WindowModel::* field)
    {
        event(name,
              [this, field](const auto&)
              {
                  auto& model = m_View.GetModel();
                  model.*field = !(model.*field);
                  FilterChanged();
              });
    };
    toggle("usable", &WindowModel::usable);
    toggle("luck", &WindowModel::luck);
    toggle("skill", &WindowModel::skill);
    event("reset",
          [this](const auto&)
          {
              // Resets the active category only; the sort order is not a filter.
              BrowseQuery query;
              query.sort = m_View.GetModel().sort;
              ApplyQuery(query);
              FilterChanged();
          });
    event("clear_name",
          [this](const auto&)
          {
              m_View.GetModel().search.clear();
              FilterChanged();
          });
    event("category",
          [this](const auto& args)
          {
              if (args.empty())
                  return;
              SwitchCategory(args.front().template Get<Rml::String>());
          });
    event("equipment",
          [this](const auto&)
          {
              m_View.GetModel().itemType = "0";
              FilterChanged();
          });
    event("select",
          [this, index](const auto& args)
          {
              if (!m_View.GetModel().review)
                  SelectRow(index(args));
          });
    event("compare_market", [this](const auto&) { CompareMarket(); });
    event("back_to_selling", [this](const auto&) { BackToSelling(); });
    event("inventory",
          [this, index](const auto& args)
          {
              if (!m_View.GetModel().review)
                  SelectInventory(index(args));
          });
    event("hover", [this, index](const auto& args) { Hover(index(args), false); });
    event("inventory_hover", [this, index](const auto& args) { Hover(index(args), true); });
    event("leave", [this](const auto&) { UI::RmlBridge::Tooltip::Hide(this); });
    event("refresh_hover",
          [this](const auto&)
          {
              UI::RmlBridge::Tooltip::Config config;
              config.anchorX = static_cast<float>(MouseX);
              config.anchorY = static_cast<float>(MouseY);
              config.lines.push_back({"Reload your inventory and check the selected item again"});
              UI::RmlBridge::Tooltip::Show(config, this);
          });
    event("refresh",
          [this](const auto&)
          {
              const auto& tab = m_View.GetModel().tab;
              if (tab == "browse")
                  FilterChanged();
              else if (tab == "own")
                  QueueRead(Read::Own, true);
              else if (tab == "collection")
                  QueueRead(Read::Collection, true);
              else
              {
                  RefreshInventory();
                  if (m_SellSlot >= 0)
                      QueueRead(Read::Sell);
              }
          });
    event("more",
          [this](const auto&)
          {
              if (m_View.GetModel().loading || !m_View.GetModel().more)
                  return;
              QueueRead(m_View.GetModel().tab == "collection" ? Read::Collection
                        : m_View.GetModel().goods             ? Read::Goods
                                                              : Read::Search);
          });
    event("quote",
          [this](const auto&)
          {
              DismissReview();
              SetStatus({});
              auto& model = m_View.GetModel();
              model.quoteTotal.clear();
              RefreshBuyNote();
              const auto quantity = ParseQuantity(model.buyQuantity, std::numeric_limits<std::uint32_t>::max());
              if (!quantity || !*quantity)
              {
                  if (m_Read == Read::Quote)
                      m_Read = Read::None;
                  model.loading = m_SentRead != Read::None;
                  m_Dirty = true;
                  return;
              }
              QueueRead(Read::Quote);
          });
    event("sell_change",
          [this](const auto&)
          {
              DismissReview();
              SetStatus({});
              SaveSellDraft();
              RefreshSelling();
          });
    event("refresh_quote", [this](const auto&) { RequestQuoteNow(); });
    event("buy", [this](const auto&) { ReviewBuy(); });
    event("sell", [this](const auto&) { ReviewSale(); });
    event("cancel_listing", [this, index](const auto& args) { ReviewCancel(index(args)); });
    event("claim", [this, index](const auto& args) { Collect(index(args)); });
    event("dismiss",
          [this](const auto&)
          {
              if (!m_View.GetModel().busy)
                  DismissReview();
          });
    event("confirm", [this](const auto&) { Confirm(); });
}

void Window::Dirty()
{
    static constexpr const char* fields[] = {"filters_open",
                                             "active_filters",
                                             "draft_family",
                                             "draft_ancient_set",
                                             "draft_class_line",
                                             "draft_option",
                                             "draft_upgrade_min",
                                             "draft_upgrade_max",
                                             "draft_min_excellent",
                                             "draft_min_sockets",
                                             "draft_max_price",
                                             "draft_luck",
                                             "draft_skill",
                                             "filter_error",
                                             "skill_applies",
                                             "usable_label",
                                             "empty_text",
                                             "can_clear_name",
                                             "can_reset_filters",
                                             "comparing",
                                             "can_compare",
                                             "selected_equip_reasons",
                                             "option2",
                                             "selected_seller",
                                             "selected_options",
                                             "selected_requirements",
                                             "step_down",
                                             "step_up",
                                             "ancient_set",
                                             "sets",
                                             "pending",
                                             "confirm_balance",
                                             "tab",
                                             "category",
                                             "equipment",
                                             "item_type",
                                             "search",
                                             "option",
                                             "family",
                                             "class_line",
                                             "sort",
                                             "upgrade_min",
                                             "upgrade_max",
                                             "min_excellent",
                                             "min_sockets",
                                             "max_price",
                                             "usable",
                                             "luck",
                                             "skill",
                                             "wallet",
                                             "status",
                                             "status_error",
                                             "count",
                                             "duration",
                                             "sale_total",
                                             "posting_fee",
                                             "sale_fee",
                                             "proceeds",
                                             "price",
                                             "quantity",
                                             "buy_quantity",
                                             "selected_name",
                                             "selected_detail",
                                             "selected_price",
                                             "selected_icon",
                                             "selected_tone",
                                             "quote_total",
                                             "quote_tiers",
                                             "quote_order",
                                             "quote_beyond",
                                             "quote_short",
                                             "quote_age",
                                             "stock",
                                             "duration_value",
                                             "confirm_title",
                                             "confirm_name",
                                             "confirm_total",
                                             "confirm_fee",
                                             "confirm_detail",
                                             "confirm_warning",
                                             "quantity_hint",
                                             "quantity_hint_error",
                                             "listing_count",
                                             "listing_count_full",
                                             "sell_note",
                                             "buy_hint",
                                             "buy_hint_error",
                                             "buy_note",
                                             "confirm_action",
                                             "loading",
                                             "more",
                                             "selected",
                                             "cannot_equip",
                                             "goods",
                                             "sell_goods",
                                             "sell_ready",
                                             "busy",
                                             "review",
                                             "rows",
                                             "inventory",
                                             "options",
                                             "types",
                                             "durations",
                                             "price_source",
                                             "price_note",
                                             "market_lowest",
                                             "max_quantity",
                                             "can_match",
                                             "can_undercut",
                                             "can_forget",
                                             "sell_tiers"};
    for (const char* field : fields)
        m_View.MarkDirty(field);
}
} // namespace UI::AuctionHouse
