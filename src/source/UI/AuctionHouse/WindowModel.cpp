#include "UI/AuctionHouse/WindowModel.h"
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataTypeRegister.h>

namespace UI::AuctionHouse
{
namespace
{
void RegisterRows(Rml::DataModelConstructor& constructor)
{
    auto mark = constructor.RegisterStruct<OptionMark>();
    mark.RegisterMember("label", &OptionMark::label);
    mark.RegisterMember("matched", &OptionMark::matched);
    constructor.RegisterArray<std::vector<OptionMark>>();
    auto row = constructor.RegisterStruct<RowModel>();
    row.RegisterMember("name", &RowModel::name);
    row.RegisterMember("detail", &RowModel::detail);
    row.RegisterMember("price", &RowModel::price);
    row.RegisterMember("meta", &RowModel::meta);
    row.RegisterMember("tone", &RowModel::tone);
    row.RegisterMember("icon", &RowModel::icon);
    row.RegisterMember("index", &RowModel::index);
    row.RegisterMember("cannot_equip", &RowModel::cannotEquip);
    row.RegisterMember("own", &RowModel::own);
    row.RegisterMember("exact_price", &RowModel::exactPrice);
    row.RegisterMember("matched", &RowModel::matched);
    row.RegisterMember("selected", &RowModel::selected);
    row.RegisterMember("summary", &RowModel::summary);
    row.RegisterMember("units", &RowModel::units);
    row.RegisterMember("changed", &RowModel::changed);
    row.RegisterMember("breakdown", &RowModel::breakdown);
    row.RegisterMember("excellent", &RowModel::excellent);
    row.RegisterMember("equip_reason", &RowModel::equipReason);
    constructor.RegisterArray<std::vector<RowModel>>();
    auto choice = constructor.RegisterStruct<ChoiceModel>();
    choice.RegisterMember("value", &ChoiceModel::value);
    choice.RegisterMember("label", &ChoiceModel::label);
    constructor.RegisterArray<std::vector<ChoiceModel>>();
    auto tier = constructor.RegisterStruct<TierModel>();
    tier.RegisterMember("price", &TierModel::price);
    tier.RegisterMember("stock", &TierModel::stock);
    tier.RegisterMember("order", &TierModel::order);
    tier.RegisterMember("note", &TierModel::note);
    tier.RegisterMember("changed", &TierModel::changed);
    constructor.RegisterArray<std::vector<TierModel>>();
    auto notice = constructor.RegisterStruct<NoticeModel>();
    notice.RegisterMember("text", &NoticeModel::text);
    notice.RegisterMember("link", &NoticeModel::link);
    constructor.RegisterArray<std::vector<NoticeModel>>();
}

void BindFilterDraft(Rml::DataModelConstructor& c, WindowModel& m)
{
    c.Bind("filters_open", &m.filtersOpen);
    c.Bind("active_filters", &m.activeFilters);
    c.Bind("draft_family", &m.draft.family);
    c.Bind("draft_ancient_set", &m.draft.ancientSet);
    c.Bind("draft_class_line", &m.draft.classLine);
    c.Bind("draft_option", &m.draft.option);
    c.Bind("draft_upgrade_min", &m.draft.upgradeMin);
    c.Bind("draft_upgrade_max", &m.draft.upgradeMax);
    c.Bind("draft_min_excellent", &m.draft.minExcellent);
    c.Bind("draft_min_sockets", &m.draft.minSockets);
    c.Bind("draft_max_price", &m.draft.maxPrice);
    c.Bind("draft_luck", &m.draft.luck);
    c.Bind("draft_skill", &m.draft.skill);
    c.Bind("filter_error", &m.filterError);
    c.Bind("skill_applies", &m.skillApplies);
}

void BindBrowseState(Rml::DataModelConstructor& c, WindowModel& m)
{
    c.Bind("usable_label", &m.usableLabel);
    c.Bind("empty_text", &m.emptyText);
    c.Bind("can_clear_name", &m.canClearName);
    c.Bind("can_reset_filters", &m.canResetFilters);
    c.Bind("comparing", &m.comparing);
    c.Bind("can_compare", &m.canCompare);
    c.Bind("selected_equip_reasons", &m.selectedEquipReasons);
}

void BindSellerHelp(Rml::DataModelConstructor& c, WindowModel& m)
{
    c.Bind("depth_open", &m.depthOpen);
    c.Bind("depth_label", &m.depthLabel);
    c.Bind("price_examples", &m.priceExamples);
    c.Bind("price_grid", &m.priceGrid);
    c.Bind("price_range", &m.priceRange);
    c.Bind("buy_blocked", &m.buyBlocked);
    c.Bind("quote_move", &m.quoteMove);
    c.Bind("notices", &m.notices);
}
} // namespace

bool MarkSelectedInventory(std::vector<RowModel>& inventory, int sellSlot)
{
    bool changed = false;
    for (auto& row : inventory)
    {
        const bool selected = sellSlot >= 0 && row.index == sellSlot;
        changed |= row.selected != selected;
        row.selected = selected;
    }
    return changed;
}

void BindWindowModel(Rml::DataModelConstructor& c, WindowModel& m)
{
    RegisterRows(c);
    BindFilterDraft(c, m);
    BindBrowseState(c, m);
    BindSellerHelp(c, m);
    c.Bind("ancient_set", &m.ancientSet);
    c.Bind("sets", &m.sets);
    c.Bind("pending", &m.pending);
    c.Bind("confirm_balance", &m.confirmBalance);
    c.Bind("step_down", &m.stepDown);
    c.Bind("step_up", &m.stepUp);
    c.Bind("tab", &m.tab);
    c.Bind("category", &m.category);
    c.Bind("equipment", &m.equipment);
    c.Bind("item_type", &m.itemType);
    c.Bind("search", &m.search);
    c.Bind("option", &m.option);
    c.Bind("option2", &m.option2);
    c.Bind("family", &m.family);
    c.Bind("class_line", &m.classLine);
    c.Bind("sort", &m.sort);
    c.Bind("upgrade_min", &m.upgradeMin);
    c.Bind("upgrade_max", &m.upgradeMax);
    c.Bind("min_excellent", &m.minExcellent);
    c.Bind("min_sockets", &m.minSockets);
    c.Bind("max_price", &m.maxPrice);
    c.Bind("usable", &m.usable);
    c.Bind("luck", &m.luck);
    c.Bind("skill", &m.skill);
    c.Bind("wallet", &m.wallet);
    c.Bind("status", &m.status);
    c.Bind("status_error", &m.statusError);
    c.Bind("count", &m.count);
    c.Bind("duration", &m.duration);
    c.Bind("sale_total", &m.saleTotal);
    c.Bind("posting_fee", &m.postingFee);
    c.Bind("sale_fee", &m.saleFee);
    c.Bind("proceeds", &m.proceeds);
    c.Bind("price", &m.price);
    c.Bind("quantity", &m.quantity);
    c.Bind("buy_quantity", &m.buyQuantity);
    c.Bind("selected_name", &m.selectedName);
    c.Bind("selected_detail", &m.selectedDetail);
    c.Bind("selected_options", &m.selectedOptions);
    c.Bind("selected_requirements", &m.selectedRequirements);
    c.Bind("selected_seller", &m.selectedSeller);
    c.Bind("selected_price", &m.selectedPrice);
    c.Bind("selected_icon", &m.selectedIcon);
    c.Bind("selected_tone", &m.selectedTone);
    c.Bind("quote_total", &m.quoteTotal);
    c.Bind("quote_tiers", &m.quoteTiers);
    c.Bind("quote_order", &m.quoteOrder);
    c.Bind("quote_beyond", &m.quoteBeyond);
    c.Bind("quote_short", &m.quoteShort);
    c.Bind("quote_age", &m.quoteAge);
    c.Bind("stock", &m.stock);
    c.Bind("duration_value", &m.durationValue);
    c.Bind("confirm_title", &m.confirmTitle);
    c.Bind("confirm_name", &m.confirmName);
    c.Bind("confirm_total", &m.confirmTotal);
    c.Bind("confirm_fee", &m.confirmFee);
    c.Bind("confirm_detail", &m.confirmDetail);
    c.Bind("confirm_warning", &m.confirmWarning);
    c.Bind("status_receipt", &m.statusReceipt);
    c.Bind("status_link", &m.statusLink);
    c.Bind("quantity_hint", &m.quantityHint);
    c.Bind("quantity_hint_error", &m.quantityHintError);
    c.Bind("listing_count", &m.listingCount);
    c.Bind("listing_count_full", &m.listingCountFull);
    c.Bind("sell_note", &m.sellNote);
    c.Bind("buy_hint", &m.buyHint);
    c.Bind("buy_hint_error", &m.buyHintError);
    c.Bind("buy_note", &m.buyNote);
    c.Bind("confirm_action", &m.confirmAction);
    c.Bind("loading", &m.loading);
    c.Bind("more", &m.more);
    c.Bind("selected", &m.selected);
    c.Bind("cannot_equip", &m.cannotEquip);
    c.Bind("goods", &m.goods);
    c.Bind("sell_goods", &m.sellGoods);
    c.Bind("sell_ready", &m.sellReady);
    c.Bind("busy", &m.busy);
    c.Bind("review", &m.review);
    c.Bind("rows", &m.rows);
    c.Bind("inventory", &m.inventory);
    c.Bind("options", &m.options);
    c.Bind("types", &m.types);
    c.Bind("durations", &m.durations);
    c.Bind("price_source", &m.priceSource);
    c.Bind("price_note", &m.priceNote);
    c.Bind("market_lowest", &m.marketLowest);
    c.Bind("max_quantity", &m.maxQuantity);
    c.Bind("can_match", &m.canMatch);
    c.Bind("can_undercut", &m.canUndercut);
    c.Bind("can_forget", &m.canForget);
    c.Bind("sell_tiers", &m.sellTiers);
}
} // namespace UI::AuctionHouse
