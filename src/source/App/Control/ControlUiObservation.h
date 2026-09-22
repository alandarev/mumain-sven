#pragma once

#include <optional>

namespace App::Control
{
// A missing object or unavailable registry must not become a negative observation.
// Queries are invoked only for available objects; callers supply read-only accessors.
template <class Object, class Query>
[[nodiscard]] std::optional<bool> ObserveUiActivity(Object* object, bool available, Query query)
{
    if (object == nullptr || !available)
    {
        return std::nullopt;
    }
    return query(*object);
}
} // namespace App::Control
