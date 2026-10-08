#include "UI/AuctionHouse/EquipmentDetails.h"
#include "UI/AuctionHouse/Presentation.h"

#include <algorithm>

namespace UI::AuctionHouse
{
namespace
{
constexpr std::array<std::string_view, 7> ClassNames = {"Dark Wizard", "Dark Knight", "Fairy Elf",   "Magic Gladiator",
                                                        "Dark Lord",   "Summoner",    "Rage Fighter"};
constexpr std::uint8_t WizardClass = 0, KnightClass = 1, GladiatorClass = 3;
constexpr std::uint8_t ThirdStep = 3;

// The client's equip rule: a Magic Gladiator also wears what both the Dark Wizard and the Dark Knight wear.
bool ClassAllowed(const EquipFacts& facts)
{
    if (facts.baseClass >= facts.requireClass.size())
        return false;
    return facts.requireClass[facts.baseClass] != 0 ||
           (facts.baseClass == GladiatorClass && facts.requireClass[WizardClass] && facts.requireClass[KnightClass]);
}

std::string ClassReason(const EquipFacts& facts)
{
    if (!ClassAllowed(facts))
    {
        std::string classes;
        for (std::size_t i = 0; i < ClassNames.size(); ++i)
            if (facts.requireClass[i])
                classes += (classes.empty() ? "" : ", ") + std::string(ClassNames[i]);
        return classes.empty() ? "Not for your class" : "For " + classes + " only";
    }
    const auto step = facts.baseClass < facts.requireClass.size() ? facts.requireClass[facts.baseClass] : 0;
    if (step <= facts.classStep)
        return {};
    return step >= ThirdStep ? "Needs the 3rd class" : "Needs the 2nd class";
}
} // namespace

std::vector<OptionMark> ExcellentMarks(std::uint16_t itemSlots, std::uint8_t excellentFlags)
{
    std::vector<OptionMark> marks;
    for (const auto& option : Options())
        if (option.mask && (option.slots & itemSlots) && (excellentFlags & option.mask))
            marks.push_back({std::string(option.label), option.mask});
    return marks;
}

void MarkSearchedOptions(std::vector<OptionMark>& marks, std::uint8_t searchedMask)
{
    for (auto& mark : marks)
        mark.matched = (searchedMask & mark.bit) != 0;
}

std::vector<std::string> CannotEquipReasons(const EquipFacts& facts)
{
    std::vector<std::string> reasons;
    if (auto reason = ClassReason(facts); !reason.empty())
        reasons.push_back(std::move(reason));
    for (const auto& stat : facts.stats)
        if (stat.required > stat.current)
            reasons.push_back(std::string(stat.name) + " " + ExactZen(static_cast<std::uint64_t>(stat.required)) +
                              " · you have " + ExactZen(static_cast<std::uint64_t>(std::max(stat.current, 0))));
    return reasons;
}
} // namespace UI::AuctionHouse
