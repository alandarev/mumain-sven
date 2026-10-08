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
    if (operation == O::Search || operation == O::GoodsList || operation == O::OwnListings ||
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
            m_Clock.Busy(RequestClock::Time::clock::now());
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
    const bool reselect = operation == O::GoodsList && m_SentRestart && m_KeepGoods;
    if (reselect)
    {
        // After a purchase the bought goods stay selected, with their quantity, and their quote refreshes.
        const auto keep = *std::exchange(m_KeepGoods, std::nullopt);
        const auto kept = std::find_if(m_Goods.begin(), m_Goods.end(), [&keep](const auto& goods)
                                       { return goods.type == keep.first && goods.level == keep.second; });
        if (kept != m_Goods.end())
        {
            m_Selected = static_cast<int>(kept - m_Goods.begin());
            model.selected = true;
        }
        else
            ClearQuote();
    }
    RefreshRows();
    if (reselect && m_Selected >= 0 && m_Read == Read::None)
        m_Read = Read::Quote;
    // An empty page supersedes a read error, but a receipt stays for its lifetime.
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
            row.price = CompactZen(goods.lowestUnitPrice) + " Zen / each";
            row.exactPrice = ExactZen(goods.lowestUnitPrice) + " Zen / each";
            row.meta = std::to_string(goods.available) + " available";
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
            row.price = CompactZen(listing.unitPrice) + " Zen" + (listing.kind == 2 ? " / each" : "");
            row.exactPrice = ExactZen(listing.unitPrice) + " Zen" + (listing.kind == 2 ? " / each" : "");
            const auto remaining = Market::SecondsRemaining(
                listing.secondsLeft, Market::GetState().OwnListingsReceivedAt(), RequestClock::Time::clock::now());
            row.meta =
                (listing.kind == 2 ? std::to_string(listing.remaining) + "/" + std::to_string(listing.listed) + " · "
                                   : "") +
                std::to_string((remaining + 3599) / 3600) + "h";
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
        m_Clock.Busy(RequestClock::Time::clock::now());
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

void Window::OnNotice(const Market::Notice& notice)
{
    const bool sold =
        notice.kind == Market::NoticeKind::SoldToCollection || notice.kind == Market::NoticeKind::SoldToInventory;
    const std::string text = sold ? "Auction sold: " + ExactZen(notice.net) + " Zen."
                             : notice.kind == Market::NoticeKind::DeliveriesWaiting ? "Collection ready."
                                                                                    : "Auction expired.";
    if (IsVisible())
    {
        DismissReview();
        const bool waiting = notice.kind == Market::NoticeKind::SoldToCollection ||
                             notice.kind == Market::NoticeKind::ExpiredToCollection ||
                             notice.kind == Market::NoticeKind::DeliveriesWaiting;
        m_Status.Show(text, StatusTone::Info, waiting ? StatusLink::Collection : StatusLink::None,
                      RequestClock::Time::clock::now());
        ShowStatus();
        RefreshRules();
        m_View.GetModel().pending = notice.pendingCollection;
        if (m_View.GetModel().tab == "own")
            QueueRead(Read::Own, true);
        if (m_View.GetModel().tab == "collection")
            QueueRead(Read::Collection, true);
    }
    else if (g_pChatListBox)
        g_pChatListBox->AddText(L"", StringUtils::NarrowToWide(text).c_str(), mu::ui::window::TYPE_SYSTEM_MESSAGE);
}
} // namespace UI::AuctionHouse
