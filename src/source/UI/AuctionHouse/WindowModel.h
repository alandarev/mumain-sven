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
    Rml::String summary, stats, options, requirements;
    bool highlighted = false;
    int units = 1;
    // A goods row whose lowest price just changed in the live quote.
    bool changed = false;
};

struct ChoiceModel
{
    Rml::String value, label;
};

// One price tier of the buyer's goods quote: its stock, the units of the entered order it fills,
// and a change mark ("new", or the stock as "4 → 10") while the live quote highlights it.
struct TierModel
{
    Rml::String price, stock, order, note;
    bool changed = false;
};

struct AdvancedFilters
{
    Rml::String family = "0", ancientSet, classLine = "255", option = "0";
    Rml::String upgradeMin = "0", upgradeMax = "15", minExcellent = "0", minSockets = "0", maxPrice;
    bool luck = false, skill = false;
};

// Facts and editable values only; layout and semantic colors belong to the theme.
struct WindowModel
{
    Rml::String tab = "browse", category = "0", equipment = "0", itemType = "0";
    Rml::String ancientSet, search, option = "0", family = "0", classLine = "255", sort = "0";
    Rml::String upgradeMin = "0", upgradeMax = "15", minExcellent = "0", minSockets = "0", maxPrice;
    bool usable = false, luck = false, skill = false;
    Rml::String option2 = "0";
    AdvancedFilters draft;
    bool filtersOpen = false;
    std::vector<ChoiceModel> activeFilters;
    int pending = 0;
    Rml::String confirmBalance;
    Rml::String wallet, status, count, duration, saleTotal, postingFee, saleFee, proceeds;
    Rml::String price, quantity = "1", buyQuantity = "1";
    Rml::String selectedName, selectedDetail, selectedPrice, selectedIcon, selectedTone;
    Rml::String selectedOptions, selectedRequirements;
    Rml::String selectedSeller;
    Rml::String quoteTotal, stock, durationValue;
    Rml::String confirmTitle, confirmName, confirmTotal, confirmFee, confirmAction;
    // Review lines above the total: the order split per price, or an item's option summary.
    Rml::String confirmDetail;
    bool loading = false, more = false, selected = false, cannotEquip = false;
    bool stepDown = false, stepUp = false, statusError = false;
    bool goods = false, sellGoods = false, sellReady = false, busy = false, review = false;
    std::vector<RowModel> rows, inventory;
    std::vector<ChoiceModel> options, types, durations, sets;
    std::vector<TierModel> quoteTiers;
    // Below the buyer's tiers: "8 for 4,730,000 Zen", "+N at higher prices · X Zen", "Only N available".
    Rml::String quoteOrder, quoteBeyond, quoteShort, quoteAge;
    // Sell price entry: where the value came from, a hint or reason, and the goods market beside it.
    Rml::String priceSource, priceNote, marketLowest, maxQuantity;
    bool canMatch = false, canUndercut = false, canForget = false;
    std::vector<ChoiceModel> sellTiers;
};
void BindWindowModel(Rml::DataModelConstructor& constructor, WindowModel& model);
} // namespace UI::AuctionHouse
