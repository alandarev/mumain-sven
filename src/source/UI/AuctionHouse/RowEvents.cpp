#include "UI/AuctionHouse/RowEvents.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>

namespace UI::AuctionHouse
{
namespace
{
int RowIndex(const Rml::VariantList& arguments)
{
    return arguments.empty() ? -1 : arguments.front().Get<int>();
}
} // namespace

void BindRowEvents(Rml::DataModelConstructor& constructor, const RowEventHandlers& handlers)
{
    auto bind = [&constructor](const char* name, std::function<void(int)> handler, bool stop)
    {
        constructor.BindEventCallback(name,
                                      [handler = std::move(handler), stop](Rml::DataModelHandle, Rml::Event& event,
                                                                           const Rml::VariantList& arguments)
                                      {
                                          if (stop)
                                              event.StopPropagation();
                                          handler(RowIndex(arguments));
                                      });
    };
    bind("select", handlers.select, false);
    bind("cancel_listing", handlers.cancel, true);
    bind("claim", handlers.claim, true);
}
} // namespace UI::AuctionHouse
