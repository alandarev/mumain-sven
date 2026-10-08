#include "UI/AuctionHouse/BrowseState.h"

namespace UI::AuctionHouse
{
const BrowseQuery& BrowseState::SwitchTo(Category category, const BrowseQuery& live)
{
    m_Queries[Index(m_Active)] = live;
    m_Active = category;
    return m_Queries[Index(m_Active)];
}

const BrowseQuery& BrowseState::BeginComparison(const BrowseQuery& live, MarketComparison comparison)
{
    if (!m_Comparison)
    {
        m_Queries[Index(m_Active)] = live;
        m_QueriesBeforeComparison = m_Queries;
        m_ActiveBeforeComparison = m_Active;
    }
    m_Active = Category::Equipment;
    m_Queries[Index(m_Active)] = ComparisonQuery(comparison.baseName, comparison.level);
    m_Comparison = std::move(comparison);
    return m_Queries[Index(m_Active)];
}

const BrowseQuery& BrowseState::EndComparison()
{
    if (m_Comparison)
    {
        m_Queries = m_QueriesBeforeComparison;
        m_Active = m_ActiveBeforeComparison;
        m_Comparison.reset();
    }
    return m_Queries[Index(m_Active)];
}

std::optional<std::uint16_t> BrowseState::ComparedType(std::string_view search) const
{
    if (!ComparisonShown() || search != m_Comparison->baseName)
        return std::nullopt;
    return m_Comparison->type;
}
} // namespace UI::AuctionHouse
