#include "UI/AuctionHouse/PriceInput.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>

namespace UI::AuctionHouse
{
namespace
{
constexpr std::string_view StepDownId = "price-down";
constexpr std::string_view StepUpId = "price-up";
constexpr const char* DisabledClass = "disabled";

unsigned StepsFor(const Rml::Event& event)
{
    return event.GetParameter<bool>("shift_key", false) ? ShiftPriceSteps : 1;
}

bool IsWithin(const Rml::Element* element, const Rml::Element* ancestor)
{
    for (; element; element = element->GetParentNode())
        if (element == ancestor)
            return true;
    return false;
}
} // namespace

std::optional<PriceStepInput> PriceStepFromKey(const Rml::Event& event)
{
    const auto key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
    if (key != Rml::Input::KI_UP && key != Rml::Input::KI_DOWN)
        return std::nullopt;
    return PriceStepInput{key == Rml::Input::KI_UP, StepsFor(event)};
}

std::optional<PriceStepInput> PriceStepFromWheel(const Rml::Event& event)
{
    // Bound on the − and + buttons only: the wheel over the price field or anywhere else scrolls the page,
    // so scrolling the panel can never change a price by accident.
    auto* button = event.GetCurrentElement();
    auto* context = button ? button->GetContext() : nullptr;
    if (!context || !IsWithin(context->GetHoverElement(), button))
        return std::nullopt;
    const auto delta = event.GetParameter<float>("wheel_delta_y", 0.f);
    if (delta == 0.f)
        return std::nullopt;
    // Wheel up (a negative delta) raises the price, over either button.
    return PriceStepInput{delta < 0.f, StepsFor(event)};
}

unsigned PriceStepsFromClick(const Rml::Event& event)
{
    return StepsFor(event);
}

bool BlurForStepper(const Rml::Event& blur)
{
    // The blur is sent on the press, before the new focus is set; the pressed button is the hovered one.
    auto* field = blur.GetCurrentElement();
    auto* context = field ? field->GetContext() : nullptr;
    for (auto* element = context ? context->GetHoverElement() : nullptr; element; element = element->GetParentNode())
    {
        const auto& id = element->GetId();
        if (id == StepDownId || id == StepUpId)
            return !element->IsClassSet(DisabledClass);
    }
    return false;
}

void BindPriceFieldEvents(Rml::DataModelConstructor& constructor, const PriceFieldHandlers& handlers)
{
    auto bind = [&constructor](const char* name, std::function<void(Rml::Event&)> handler)
    {
        constructor.BindEventCallback(
            name, [handler = std::move(handler)](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&)
            { handler(event); });
    };
    auto edit = handlers.edit;
    auto step = handlers.step;
    bind("price_change", [edit](Rml::Event& event) { edit(event.GetParameter<bool>("linebreak", false)); });
    bind("price_commit", [edit](Rml::Event& event) { edit(!BlurForStepper(event)); });
    auto stepWith = [step](auto read)
    {
        return [step, read](Rml::Event& event)
        {
            if (const auto input = read(event))
            {
                event.StopPropagation();
                step(*input);
            }
        };
    };
    bind("price_key", stepWith(&PriceStepFromKey));
    // The wheel steps only over the − and + buttons; there nothing scrolls.
    bind("price_wheel", stepWith(&PriceStepFromWheel));
    bind("minus", [step](Rml::Event& event) { step({false, PriceStepsFromClick(event)}); });
    bind("plus", [step](Rml::Event& event) { step({true, PriceStepsFromClick(event)}); });
}

void SelectContentIfFocused(Rml::Element* input)
{
    auto* context = input ? input->GetContext() : nullptr;
    if (!context || context->GetFocusElement() != input)
        return;
    if (auto* field = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(input))
        field->Select();
}
} // namespace UI::AuctionHouse
