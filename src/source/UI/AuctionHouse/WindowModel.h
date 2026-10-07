#pragma once

#include <RmlUi/Core/Types.h>
#include <vector>

namespace Rml
{
class DataModelConstructor;
}

namespace UI::AuctionHouse
{
struct RowModel
{
    Rml::String name, detail, price, meta, tone, icon;
    int index = 0;
    bool cannotEquip = false, own = false;
    Rml::String exactPrice, matched;
    bool selected = false;
};

struct ChoiceModel
{
    Rml::String value, label;
};

// Facts and editable values only; layout and semantic colors belong to the theme.
struct WindowModel
{
    Rml::String tab = "browse", category = "0", equipment = "0", itemType = "0";
    Rml::String ancientSet, search, option = "0", family = "0", classLine = "255", sort = "0";
    Rml::String upgradeMin = "0", upgradeMax = "15", minExcellent = "0", minSockets = "0", maxPrice;
    bool usable = false, luck = false, skill = false;
    int pending = 0;
    Rml::String confirmBalance;
    Rml::String wallet, status, count, duration, saleTotal, postingFee, saleFee, proceeds;
    Rml::String price, quantity = "1", buyQuantity = "1";
    Rml::String selectedName, selectedDetail, selectedPrice, selectedIcon, selectedTone;
    Rml::String quoteTotal, stock, durationValue;
    Rml::String confirmTitle, confirmName, confirmTotal, confirmFee, confirmAction;
    bool loading = false, more = false, selected = false, cannotEquip = false;
    bool stepDown = false, stepUp = false, statusError = false;
    bool goods = false, sellGoods = false, sellReady = false, busy = false, review = false;
    std::vector<RowModel> rows, inventory;
    std::vector<ChoiceModel> options, types, durations, sets;
};
void BindWindowModel(Rml::DataModelConstructor& constructor, WindowModel& model);
} // namespace UI::AuctionHouse
