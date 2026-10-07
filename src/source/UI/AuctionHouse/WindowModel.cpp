#include "UI/AuctionHouse/WindowModel.h"
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataTypeRegister.h>

namespace UI::AuctionHouse
{
namespace
{
void RegisterRows(Rml::DataModelConstructor& constructor)
{
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
    constructor.RegisterArray<std::vector<RowModel>>();
    auto choice = constructor.RegisterStruct<ChoiceModel>();
    choice.RegisterMember("value", &ChoiceModel::value);
    choice.RegisterMember("label", &ChoiceModel::label);
    constructor.RegisterArray<std::vector<ChoiceModel>>();
}
} // namespace

void BindWindowModel(Rml::DataModelConstructor& c, WindowModel& m)
{
    RegisterRows(c);
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
    c.Bind("selected_price", &m.selectedPrice);
    c.Bind("selected_icon", &m.selectedIcon);
    c.Bind("selected_tone", &m.selectedTone);
    c.Bind("quote_total", &m.quoteTotal);
    c.Bind("stock", &m.stock);
    c.Bind("duration_value", &m.durationValue);
    c.Bind("confirm_title", &m.confirmTitle);
    c.Bind("confirm_name", &m.confirmName);
    c.Bind("confirm_total", &m.confirmTotal);
    c.Bind("confirm_fee", &m.confirmFee);
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
}
} // namespace UI::AuctionHouse
