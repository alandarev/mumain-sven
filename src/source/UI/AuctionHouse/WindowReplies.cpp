#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include "UI/AuctionHouse/ItemPresentation.h"
#include "UI/Core/WindowSystem.h"
#include "Core/Utilities/StringUtils.h"

namespace UI::AuctionHouse
{
void Window::OnChanged(Market::Operation operation)
{
    if (!IsVisible())
        return;
    using O = Market::Operation;
    if (operation == O::GoodsList && m_SentRead == Read::GoodsCount)
        ApplyGoodsCount();
    else if (operation == O::Search || operation == O::GoodsList || operation == O::OwnListings ||
             operation == O::Collection)
        HandlePage(operation);
    else if (operation == O::SellCheck)
    {
        if (m_SentRead == Read::Sell)
            m_SentRead = Read::None;
        m_View.GetModel().loading = false;
        RefreshSelling();
    }
    else if (operation == O::Quote)
    {
        const bool sellQuote = m_SentRead == Read::SellQuote;
        if (m_SentRead == Read::Quote || sellQuote)
            m_SentRead = Read::None;
        m_View.GetModel().loading = false;
        if (sellQuote || m_View.GetModel().tab == "sell")
            RefreshSellMarket();
        else
            RefreshQuote();
    }
    else
        HandleOutcome(operation);
    // A review or collection asked for while this read was in flight starts now (ReviewGate).
    if (auto start = m_ReviewGate.TakeWaiting(m_SentRead != Read::None))
        start();
    m_Dirty = true;
}

void Window::HandlePage(Market::Operation operation)
{
    using O = Market::Operation;
    const Read read = operation == O::Search        ? Read::Search
                      : operation == O::GoodsList   ? Read::Goods
                      : operation == O::OwnListings ? Read::Own
                                                    : Read::Collection;
    if (m_SentRead != read)
        return;
    m_SentRead = Read::None;
    if (m_SentGeneration != m_Generation)
        return;
    auto& model = m_View.GetModel();
    model.loading = false;
    const auto& state = Market::GetState();
    const auto outcome = state.LastOutcome(operation);
    if (!outcome)
        return;
    if (outcome->result != Market::Result::Success)
    {
        if (outcome->result == Market::Result::Busy)
            m_Clock.Busy(Now());
        SetStatus(ResultText(outcome->result));
        model.more = false;
        return;
    }
    if (m_SentRestart)
    {
        m_Loaded = 0;
        m_ListingTimes.clear();
        m_Listings.clear();
        m_Goods.clear();
        m_SelectedCountExact = true;
        m_Collection.clear();
        model.rows.clear();
        m_Selected = -1;
        model.selected = false;
    }
    if (operation == O::Search)
    {
        const auto& page = state.Search();
        m_Loaded += static_cast<std::uint32_t>(page.rows.size());
        for (const auto& row : page.rows)
            if (std::none_of(m_Listings.begin(), m_Listings.end(),
                             [&row](const auto& old) { return old.id == row.id; }))
            {
                m_Listings.push_back(row);
                m_ListingTimes.push_back(page.receivedAt);
            }
        m_Total = page.totalCount;
        model.more = !page.rows.empty() && m_Loaded < m_Total && page.page < 65535;
        m_Page = page.page + (page.page < 65535 ? 1 : 0);
    }
    else if (operation == O::GoodsList)
    {
        const auto& page = state.Goods();
        m_Loaded += static_cast<std::uint32_t>(page.rows.size());
        for (const auto& row : page.rows)
        {
            if (row.type >= MAX_ITEM || !ItemAttribute)
                continue;
            const auto name = StringUtils::WideToNarrow(ItemAttribute[row.type].Name);
            if (!MatchesGoodsName(name, model.category == "1" ? Category::Jewels : Category::Materials, model.search))
                continue;
            if (std::none_of(m_Goods.begin(), m_Goods.end(),
                             [&row](const auto& old) { return old.type == row.type && old.level == row.level; }))
                m_Goods.push_back(row);
        }
        m_Total = page.totalCount;
        model.more = !page.rows.empty() && m_Loaded < m_Total && page.page < 65535;
        m_Page = page.page + (page.page < 65535 ? 1 : 0);
    }
    else if (operation == O::Collection)
    {
        const auto& page = state.Collection();
        m_Loaded += static_cast<std::uint32_t>(page.rows.size());
        for (const auto& row : page.rows)
            if (std::none_of(m_Collection.begin(), m_Collection.end(),
                             [&row](const auto& old) { return old.id == row.id; }))
                m_Collection.push_back(row);
        m_Total = page.totalCount;
        model.more = !page.rows.empty() && m_Loaded < m_Total && page.page < 65535;
        m_Page = page.page + (page.page < 65535 ? 1 : 0);
    }
    else
    {
        m_Total = static_cast<std::uint32_t>(state.OwnListings().size());
        model.more = false;
    }
    // After a purchase the bought goods stay selected, with their quantity, and their quote refreshes.
    // The reloaded directory is searched page by page until they are found or it ends (FR-03).
    bool reselected = false;
    if (operation == O::GoodsList && m_KeepGoods)
    {
        const auto keep = *m_KeepGoods;
        const auto kept = std::find_if(m_Goods.begin(), m_Goods.end(), [&keep](const auto& goods)
                                       { return goods.type == keep.first && goods.level == keep.second; });
        if (kept != m_Goods.end())
        {
            m_Selected = static_cast<int>(kept - m_Goods.begin());
            model.selected = true;
            m_KeepGoods.reset();
            reselected = true;
        }
        else if (!model.more)
        {
            m_KeepGoods.reset();
            ClearQuote();
        }
    }
    RefreshRows();
    if (reselected && m_Read == Read::None)
        m_Read = Read::Quote;
    else if (m_KeepGoods && m_Read == Read::None)
        QueueRead(Read::Goods);
    // An empty page supersedes a read error; a receipt stays until the next deliberate action.
    if (model.rows.empty() && !model.more && m_Status.Tone() != StatusTone::Success)
        SetStatus({});
}

void Window::RefreshRows()
{
    auto& model = m_View.GetModel();
    model.rows.clear();
    m_Icons.clear();
    auto add = [this, &model](Item item, RowModel row)
    {
        row.index = static_cast<int>(model.rows.size());
        row.selected = row.index == m_Selected;
        model.rows.push_back(std::move(row));
        m_Icons.push_back({std::move(item), nullptr});
    };
    if (model.tab == "browse" && !model.goods)
        for (const auto& listing : m_Listings)
        {
            auto item = Decode(listing.itemData);
            auto row = Describe(item.get(), 0);
            row.price = CompactZen(listing.price) + " Zen";
            row.exactPrice = ExactZen(listing.price) + " Zen";
            const auto index = static_cast<std::size_t>(&listing - m_Listings.data());
            const auto seconds =
                Market::SecondsRemaining(listing.secondsLeft, m_ListingTimes[index], RequestClock::Time::clock::now());
            row.meta = std::to_string((seconds + 3599) / 3600) + "h";
            row.cannotEquip = !listing.requirementsMet;
            if (row.cannotEquip && item)
                ExplainCannotEquip(row, *item);
            row.own = listing.ownListing;
            add(std::move(item), std::move(row));
        }
    else if (model.tab == "browse")
        for (const auto& goods : m_Goods)
        {
            auto item = GoodsItem(goods);
            auto row = Describe(item.get(), 0);
            row.price = CompactZen(goods.lowestUnitPrice) + " Zen each";
            row.exactPrice = ExactZen(goods.lowestUnitPrice) + " Zen each";
            const bool selected =
                static_cast<std::size_t>(&goods - m_Goods.data()) == static_cast<std::size_t>(m_Selected);
            row.meta = AvailabilityText({goods.available, !selected || m_SelectedCountExact});
            row.detail.clear();
            row.stats.clear();
            row.options.clear();
            row.requirements.clear();
            row.summary.clear();
            add(std::move(item), std::move(row));
        }
    else if (model.tab == "own")
        for (const auto& listing : Market::GetState().OwnListings())
        {
            auto item = Decode(listing.itemData);
            if (!item && listing.type < MAX_ITEM)
                item = GoodsItem({listing.type, listing.level});
            auto row = Describe(item.get(), 0);
            const bool goods = listing.kind == 2;
            row.price = CompactZen(listing.unitPrice) + (goods ? " Zen each" : " Zen");
            row.exactPrice = ExactZen(listing.unitPrice) + (goods ? " Zen each" : " Zen");
            const auto remaining = Market::SecondsRemaining(
                listing.secondsLeft, Market::GetState().OwnListingsReceivedAt(), RequestClock::Time::clock::now());
            row.meta = OwnListingLine(listing, (remaining + 3599) / 3600);
            add(std::move(item), std::move(row));
        }
    else if (model.tab == "collection")
        for (const auto& entry : m_Collection)
        {
            auto item = Decode(entry.itemData);
            if (!item && entry.type < MAX_ITEM)
                item = GoodsItem({entry.type, entry.level});
            auto row = Describe(item.get(), 0);
            if (entry.kind == 2)
            {
                // Proceeds: the sold item (type and level; the exact variant is not on the wire), the
                // quantity and the net; gross and fee expand under the row.
                row.name = (row.name.empty() ? "Sold item" : row.name) +
                           (entry.quantity > 1 ? " × " + std::to_string(entry.quantity) : "") + " · " +
                           ExactZen(entry.net) + " Zen";
                if (!item)
                    row.icon = "zen";
                row.meta = "Sold";
                row.price.clear();
                row.exactPrice = ExactZen(entry.net) + " Zen";
                row.detail = "Gross " + ExactZen(entry.gross) + " Zen\nSale fee " + ExactZen(entry.fee) + " Zen";
                row.breakdown = "Gross " + ExactZen(entry.gross) + " Zen · Sale fee " + ExactZen(entry.fee) + " Zen";
            }
            else
            {
                if (entry.quantity > 1)
                    row.name += " ×" + std::to_string(entry.quantity);
                row.price.clear();
                row.meta = entry.kind == 3 ? "Cancelled" : entry.kind == 4 ? "Expired" : "Purchased";
            }
            add(std::move(item), std::move(row));
        }
    model.count = model.tab == "browse" && model.goods ? (model.more ? "" : std::to_string(m_Goods.size()))
                                                       : std::to_string(m_Total);
    if (!model.count.empty())
        model.count += model.tab == "own" ? " active" : " listings";
    if (model.tab == "collection")
        model.pending = static_cast<int>(m_Total);
    m_Dirty = true;
}

void Window::HandleOutcome(Market::Operation operation)
{
    const auto outcome = Market::GetState().LastOutcome(operation);
    if (!outcome)
        return;
    const auto review = m_Review;
    m_View.GetModel().busy = false;
    m_ActionSentAt = {};
    DismissReview();
    if (outcome->result == Market::Result::Busy)
        m_Clock.Busy(Now());
    if (operation == Market::Operation::CreateListing && outcome->result != Market::Result::Success)
        m_PublishedPrice.reset();
    using O = Market::Operation;
    const bool partialClaim = operation == O::Claim && outcome->result == Market::Result::Partial;
    if (outcome->result != Market::Result::Success && !partialClaim)
    {
        // The draft stays as it was: the player can correct it and review again.
        SetStatus(ResultText(outcome->result));
        if (outcome->result == Market::Result::RulesChanged)
            RefreshRules();
        if ((operation == Market::Operation::BuyListing) &&
            (outcome->result == Market::Result::QuoteChanged || outcome->result == Market::Result::NotFound))
            QueueRead(Read::Search, true);
        return;
    }
    auto receipt = BuildReceipt(*outcome, {review.subject, review.amount});
    auto& model = m_View.GetModel();
    if (operation == O::Claim)
        model.pending = outcome->remainingEntries;
    else
        model.pending += receipt.collectionEntries;
    if (operation == O::CreateListing && !RememberPublishedPrice())
        receipt.text += " · the price could not be remembered";
    ShowReceipt(receipt);
    if (operation == O::CreateListing)
    {
        m_SellDrafts.erase(m_SellKey);
        ClearSellSelection();
        RefreshInventory();
        RefreshSelling();
    }
    else if (operation == O::CancelListing)
        QueueRead(Read::Own, true);
    else if (operation == O::Claim)
        QueueRead(Read::Collection, true);
    else if (operation == O::BuyGoods)
    {
        m_KeepGoods.emplace(review.type, review.level);
        QueueRead(Read::Goods, true);
    }
    else
        QueueRead(Read::Search, true);
}

std::string Window::NoticeItemName(const Market::Notice& notice) const
{
    if (notice.kind == Market::NoticeKind::DeliveriesWaiting)
        return {};
    // The notice carries the type and level only, as the Collection rows do.
    auto item = GoodsItem({notice.type, notice.level});
    return item ? Describe(item.get(), 0).name : std::string{};
}

void Window::OnNotice(const Market::Notice& notice)
{
    const auto line = DescribeNotice(notice, NoticeItemName(notice));
    if (!IsVisible())
    {
        if (g_pChatListBox)
            g_pChatListBox->AddText(L"", StringUtils::NarrowToWide(line.text).c_str(),
                                    mu::ui::window::TYPE_SYSTEM_MESSAGE);
        return;
    }
    // Its own line: an open review, the result line and the draft stay as they are (D19).
    m_Notices.Push(line);
    ShowNotices();
    RefreshRules();
    m_View.GetModel().pending = notice.pendingCollection;
    if (m_View.GetModel().review || m_View.GetModel().busy)
        m_RefreshAfterReview = true;
    else
        RefreshListsAfterNotice();
}

void Window::RefreshListsAfterNotice()
{
    m_RefreshAfterReview = false;
    if (m_View.GetModel().tab == "own")
        QueueRead(Read::Own, true);
    else if (m_View.GetModel().tab == "collection")
        QueueRead(Read::Collection, true);
}
} // namespace UI::AuctionHouse
