#include "stdafx.h"
#include "Core/Utilities/FrameProfiler.h"

#include "UI/Placement/WindowPlacement.h"
#include "UI/Placement/WindowGroupState.h"

#include "Data/GameConfig/GameConfig.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/InventoryCtrl.h"
#include "UI/RmlBridge/RmlDraggable.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "UI/RmlBridge/RmlPanelGeometry.h"
#include "UI/RmlBridge/RmlThemedView.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/Scaling/UITransform.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementUtilities.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <unordered_map>

namespace UI::Placement
{
namespace
{
// The docked panels' size in their own layout units: a content slot's size when its window has
// no document to read a #panel from.
constexpr float DefaultPanelWidth = 190.f;
constexpr float DefaultPanelHeight = 429.f;
constexpr int MaximumRegionFitPasses = 3;
constexpr float RegionFitTolerance = 0.0001f;
constexpr int MaximumIntrinsicFitPasses = 8;
// All reconciliation residuals are projected screen pixels, not component/reference units.
constexpr float IntrinsicFitTolerancePixels = 0.25f;
constexpr float GroupPreferenceUnits = 1000000.f;

struct Entry
{
    std::uint32_t windowId = 0;
    GetWindow getWindow;
    std::string document;
    PlacementParticipant participant;
    PlacementParticipant::Box box;
    bool hasBox = false;
    bool lastVisible = false;
    bool placed = false;
    bool warnedUnsupportedFill = false;
    PlacementParticipant::Box previousBox;
    bool previouslyPlaced = false;
    bool reservesSpace = false;
    bool fill = false;
    bool unresolved = false;
    Rml::Vector2f measuredSize;
};

std::unordered_map<std::string, Entry> g_windows;
bool g_namesChecked = false;
UI::Scaling::Transform g_lastDock{};
bool g_dirty = true;
ReconciliationResult g_reconciliation = ReconciliationResult::Converged;
std::string g_lastFailureSignature;
// Regions the theme capped below their reference dimensions, and the factor that makes them fit.
std::unordered_map<Rml::Element*, float> g_regionFit;
std::unordered_map<std::string, WindowGroupState> g_groups;
std::unordered_map<Rml::Element*, Rml::Vector2f> g_groupOffsets;
bool g_groupViewportChanged = false;
bool g_reservedSizeChanged = false;
bool g_intrinsicSizeChanged = false;
unsigned int g_lastWidth = 0;
unsigned int g_lastHeight = 0;
float g_uncoveredLeft = 0.f;
float g_uncoveredRight = 0.f;
// A rebuilt workspace has new slots: every window is placed again.
void OnWorkspaceReloaded()
{
    g_namesChecked = false;
    g_groups.clear();
    g_groupOffsets.clear();
    for (auto& [name, entry] : g_windows)
    {
        entry.placed = false;
        entry.warnedUnsupportedFill = false;
    }
    Arrange();
}

// Never shown, so it is never drawn or hit; Arrange() lays it out itself.
UI::RmlBridge::ThemedView<> g_workspaceView{{{"Data/Interface/RmlUi/workspace.rml"}},
                                            {.afterReload = [] { OnWorkspaceReloaded(); }}};

Rml::ElementDocument* Workspace()
{
    return g_workspaceView.Ensure() ? g_workspaceView.Document() : nullptr;
}

// A name the workspace uses but no window answers to does nothing, so say so once per workspace.
void CheckNames(const Rml::ElementList& slots)
{
    if (g_namesChecked || g_windows.empty())
        return;
    g_namesChecked = true;
    const auto check = [](const std::string& name, const char* attribute)
    {
        if (!name.empty() && g_windows.find(name) == g_windows.end())
            g_ErrorReport.Write(L"> [Placement] workspace.rml %hs names unknown window '%hs'.\r\n", attribute,
                                name.c_str());
    };
    for (Rml::Element* slot : slots)
    {
        check(slot->GetAttribute<Rml::String>("data-window", ""), "data-window");
        std::istringstream closes(slot->GetAttribute<Rml::String>("data-closes", ""));
        for (std::string name; closes >> name;)
            check(name, "data-closes");
    }
}

bool IsOpen(const Entry& entry)
{
    return entry.participant.visible ? entry.participant.visible()
                                     : g_pNewUISystem != nullptr && g_pNewUISystem->IsVisible(entry.windowId);
}

Entry* EntryFor(Rml::Element* slot)
{
    const auto it = g_windows.find(slot->GetAttribute<Rml::String>("data-window", ""));
    return it == g_windows.end() ? nullptr : &it->second;
}

void SetLength(Rml::Element* element, Rml::PropertyId property, float px)
{
    element->SetProperty(property, Rml::Property(px, Rml::Unit::PX));
}

// The scale a region's windows are drawn at, chosen by its data-scale.
float BaseRegionScale(Rml::Element* region, const UI::Scaling::Transform& dock)
{
    const std::string scale = region != nullptr ? region->GetAttribute<Rml::String>("data-scale", "") : "";
    if (scale == "panel")
        return UI::Scaling::PanelTransform(WindowWidth, WindowHeight).scaleX;
    if (scale == "hud")
        return UI::Scaling::BottomHudScale(WindowWidth, WindowHeight);
    if (scale == "window")
    {
        const float unit = UI::RmlBridge::ThemeWindowUnitPixels(WindowWidth, WindowHeight);
        return unit > 0.f ? unit : dock.scaleX;
    }
    return dock.scaleX;
}

// The base scale, smaller where the theme caps the region below its reference height.
float RegionScale(Rml::Element* region, const UI::Scaling::Transform& dock)
{
    const auto fit = g_regionFit.find(region);
    return BaseRegionScale(region, dock) * (fit != g_regionFit.end() ? fit->second : 1.f);
}

Rml::Element* SlotGroup(Rml::Element* slot)
{
    Rml::Element* parent = slot != nullptr ? slot->GetParentNode() : nullptr;
    return parent != nullptr && parent->HasAttribute("data-group") ? parent : nullptr;
}

float SlotScale(Rml::Element* slot, const UI::Scaling::Transform& dock)
{
    Rml::Element* region = slot->GetParentNode();
    const auto fit = g_regionFit.find(region);
    const float base = BaseRegionScale(slot->HasAttribute("data-scale") ? slot : region, dock);
    return base * (fit != g_regionFit.end() ? fit->second : 1.f);
}

Rml::ElementDocument* DocumentFor(const Entry& entry)
{
    if (auto* window = entry.getWindow ? entry.getWindow() : nullptr)
        if (auto* document = window->GetPlacedDocument())
            return document;
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (entry.document.empty() || context == nullptr)
        return nullptr;
    const std::string suffix = "/" + entry.document;
    for (int i = 0; i < context->GetNumDocuments(); ++i)
    {
        Rml::ElementDocument* document = context->GetDocument(i);
        const std::string& url = document->GetSourceURL();
        if (url.size() >= suffix.size() && url.compare(url.size() - suffix.size(), suffix.size(), suffix) == 0)
            return document;
    }
    return nullptr;
}

// The window's own #panel size, in its layout units; the docked windows' size without one.
Rml::Vector2f PanelSize(const Entry& entry)
{
    if (entry.participant.measure)
    {
        const auto size = entry.participant.measure();
        return {size.width, size.height};
    }
    const Rml::Vector2f fallback{DefaultPanelWidth, DefaultPanelHeight};
    Rml::ElementDocument* document = DocumentFor(entry);
    if (document == nullptr)
        return fallback;
    // The panel's layout box: its own units, before the scale a slot gives it.
    const auto panelSize = [document]() -> Rml::Vector2f
    {
        Rml::Element* panel = document->GetElementById("panel");
        return panel != nullptr ? panel->GetBox().GetSize(Rml::BoxArea::Border) : Rml::Vector2f{};
    };
    Rml::Vector2f size = panelSize();
    // The context leaves hidden documents out of its update: a window about to open may still be laid
    // out as it was when hidden (before a resize or a content change), or never laid out at all.
    if (!document->IsVisible() || size.x <= 0.f || size.y <= 0.f)
    {
        {
            FRAME_PROFILE(UILayout);
            document->UpdateDocument();
        }
        size = panelSize();
    }
    return size.x > 0.f && size.y > 0.f ? size : fallback;
}

using GroupBounds = WindowGroupState::Bounds;
using GroupPoint = WindowGroupState::Point;

GroupBounds ElementBounds(Rml::Element* element)
{
    const auto offset = element->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto size = element->GetBox().GetSize(Rml::BoxArea::Border);
    return {offset.x, offset.y, size.x, size.y};
}

GroupBounds SafeBounds()
{
    Rml::ElementDocument* workspace = g_workspaceView.Document();
    Rml::Element* safe = workspace != nullptr ? workspace->GetElementById("safe_area") : nullptr;
    return safe != nullptr ? ElementBounds(safe)
                           : GroupBounds{0.f, 0.f, static_cast<float>(WindowWidth), static_cast<float>(WindowHeight)};
}

GroupBounds CombinedBounds(const Rml::ElementList& slots, bool placed = false)
{
    GroupBounds result{};
    bool first = true;
    for (Rml::Element* slot : slots)
    {
        if (!slot->IsClassSet("open"))
            continue;
        const Entry* entry = EntryFor(slot);
        const auto& box = entry != nullptr ? entry->box : PlacementParticipant::Box{};
        const GroupBounds next = placed ? GroupBounds{box.left, box.top, box.width, box.height} : ElementBounds(slot);
        if (first)
            result = next;
        else
        {
            const float right = std::max(result.left + result.width, next.left + next.width);
            const float bottom = std::max(result.top + result.height, next.top + next.height);
            result.left = std::min(result.left, next.left);
            result.top = std::min(result.top, next.top);
            result.width = right - result.left;
            result.height = bottom - result.top;
        }
        first = false;
    }
    return result;
}

Rml::Element* NamedSlot(std::string_view name)
{
    Rml::ElementDocument* workspace = Workspace();
    if (workspace == nullptr)
        return nullptr;
    Rml::ElementList slots;
    workspace->QuerySelectorAll(slots, ".slot");
    for (Rml::Element* slot : slots)
        if (slot->GetAttribute<Rml::String>("data-window", "") == name)
            return slot;
    return nullptr;
}

Rml::Element* GroupAnchor(Rml::Element* group)
{
    const std::string name = group->GetAttribute<Rml::String>("data-anchor", "");
    for (int i = 0; i < group->GetNumChildren(); ++i)
    {
        Rml::Element* child = group->GetChild(i);
        if (child->GetAttribute<Rml::String>("data-window", "") == name)
            return child;
    }
    return nullptr;
}

GroupPoint RightTop(const GroupBounds& bounds)
{
    return {bounds.left + bounds.width, bounds.top};
}

WindowGroupState& GroupState(Rml::Element* group)
{
    const auto [found, added] = g_groups.try_emplace(group->GetAttribute<Rml::String>("data-group", ""));
    if (!added)
        return found->second;
    const std::string key = group->GetAttribute<Rml::String>("data-position-key", "");
    int x = 0;
    int y = 0;
    if (!key.empty() && GameConfig::GetInstance().GetWindowPosition(std::wstring(key.begin(), key.end()), x, y))
        found->second.SetPreferred({x / GroupPreferenceUnits, y / GroupPreferenceUnits});
    return found->second;
}

void PlaceGroupedEntry(Entry& entry)
{
    if (mu::ui::window::CObject* window = entry.getWindow ? entry.getWindow() : nullptr)
    {
        const auto& box = entry.box;
        window->PlaceInWorkspace(box.left, box.top, box.scale);
    }
}

GroupPoint PlacedAnchor(Rml::Element* group, const GroupBounds& extent)
{
    Rml::Element* anchor = GroupAnchor(group);
    const Entry* entry = anchor != nullptr && anchor->IsClassSet("open") ? EntryFor(anchor) : nullptr;
    return entry != nullptr && entry->hasBox ? GroupPoint{entry->box.left + entry->box.width, entry->box.top}
                                             : RightTop(extent);
}

void MoveGroup(std::string_view name, float left, float top)
{
    Rml::Element* slot = NamedSlot(name);
    Rml::Element* group = SlotGroup(slot);
    Entry* moving = slot != nullptr ? EntryFor(slot) : nullptr;
    if (group == nullptr || moving == nullptr || !moving->hasBox || mu::ui::window::CInventoryCtrl::GetPickedItem())
        return;
    Rml::ElementList slots;
    group->QuerySelectorAll(slots, ".slot.open");
    const GroupBounds bounds = CombinedBounds(slots, true);
    const GroupPoint anchor = PlacedAnchor(group, bounds);
    const GroupPoint desired{anchor.x + left - moving->box.left, anchor.y + top - moving->box.top};
    const GroupPoint shift = GroupState(group).Translation(SafeBounds(), bounds, anchor, desired);
    for (Rml::Element* child : slots)
    {
        Entry* entry = EntryFor(child);
        if (entry == nullptr || !entry->hasBox)
            continue;
        entry->box.left += shift.x;
        entry->box.top += shift.y;
        PlaceGroupedEntry(*entry);
    }
    const GroupBounds shifted{bounds.left + shift.x, bounds.top + shift.y, bounds.width, bounds.height};
    GroupState(group).lastAnchor = PlacedAnchor(group, shifted);
    Invalidate();
}

void SaveGroupPosition(std::string_view name)
{
    Rml::Element* group = SlotGroup(NamedSlot(name));
    if (group == nullptr)
        return;
    Rml::ElementList slots;
    group->QuerySelectorAll(slots, ".slot.open");
    const auto preferred = WindowGroupState::Normalize(PlacedAnchor(group, CombinedBounds(slots, true)), SafeBounds());
    GroupState(group).SetPreferred(preferred);
    const std::string key = group->GetAttribute<Rml::String>("data-position-key", "");
    if (!key.empty())
        GameConfig::GetInstance().SetWindowPosition(std::wstring(key.begin(), key.end()),
                                                    static_cast<int>(std::lround(preferred.x * GroupPreferenceUnits)),
                                                    static_cast<int>(std::lround(preferred.y * GroupPreferenceUnits)));
}

} // namespace

bool MakeGroupDraggable(std::string_view name, Rml::ElementDocument* document)
{
    Rml::Element* slot = NamedSlot(name);
    if (SlotGroup(slot) == nullptr)
        return false;
    if (document == nullptr)
        return true;
    const std::string selector = slot->GetAttribute<Rml::String>("data-drag-handle", "");
    Rml::Element* handle = selector.empty() ? nullptr : document->QuerySelector(selector);
    Rml::Element* panel = document->GetElementById("panel");
    if (handle == nullptr || panel == nullptr || handle->IsClassSet("placement-draggable"))
        return true;
    handle->SetClass("placement-draggable", true);
    const std::string ownedName(name);
    UI::RmlBridge::MakeExternalDraggable(
        handle, panel, [ownedName](float x, float y) { MoveGroup(ownedName, x, y); }, [ownedName]
        { SaveGroupPosition(ownedName); }, [] { return mu::ui::window::CInventoryCtrl::GetPickedItem() == nullptr; });
    return true;
}

void Invalidate()
{
    g_dirty = true;
}

void RegisterParticipant(std::string_view name, PlacementParticipant participant)
{
    Entry& entry = g_windows[std::string(name)];
    entry = {};
    entry.participant = std::move(participant);
    g_namesChecked = false;
    Invalidate();
}

void UnregisterParticipant(std::string_view name)
{
    g_windows.erase(std::string(name));
    Invalidate();
}

void RegisterHudWindow(std::string_view name, std::uint32_t windowId, GetWindow getWindow, float width, float height)
{
    PlacementParticipant participant;
    participant.visible = [windowId] { return g_pNewUISystem != nullptr && g_pNewUISystem->IsVisible(windowId); };
    participant.measure = [width, height] { return PlacementParticipant::Size{width, height}; };
    participant.place = [getWindow](const PlacementParticipant::Box* box)
    {
        mu::ui::window::CObject* window = getWindow ? getWindow() : nullptr;
        if (window == nullptr)
            return;
        if (box != nullptr)
            window->PlaceDocument(box->left, box->top, box->scale);
        else
            window->ClearPlacement();
    };
    RegisterParticipant(name, std::move(participant));
    g_windows[std::string(name)].getWindow = std::move(getWindow);
}

bool SlotBox(std::string_view name, PlacementParticipant::Box& box)
{
    const auto found = g_windows.find(std::string(name));
    if (found == g_windows.end() || !found->second.hasBox)
        return false;
    box = found->second.box;
    return true;
}

bool SlotBox(std::uint32_t windowId, PlacementParticipant::Box& box)
{
    for (const auto& [name, entry] : g_windows)
    {
        if (entry.windowId == windowId && entry.hasBox)
        {
            box = entry.box;
            return true;
        }
    }
    return false;
}

void RegisterWindow(std::uint32_t windowId, std::string_view slotName, GetWindow getWindow, const char* document)
{
    Entry& entry = g_windows[std::string(slotName)];
    entry = {};
    entry.windowId = windowId;
    entry.getWindow = std::move(getWindow);
    entry.document = document != nullptr ? document : "";
}

void RegisterName(std::uint32_t windowId, std::string_view name)
{
    Entry& entry = g_windows[std::string(name)];
    entry = {};
    entry.windowId = windowId;
}

void CloseForOpening(std::uint32_t windowId)
{
    Rml::ElementDocument* workspace = Workspace();
    if (workspace == nullptr || g_pNewUISystem == nullptr)
        return;

    Rml::ElementList slots;
    workspace->QuerySelectorAll(slots, ".slot");
    for (Rml::Element* slot : slots)
    {
        const Entry* opening = EntryFor(slot);
        if (opening == nullptr || opening->windowId != windowId)
            continue;

        std::istringstream names(slot->GetAttribute<Rml::String>("data-closes", ""));
        for (std::string name; names >> name;)
        {
            const auto it = g_windows.find(name);
            if (it != g_windows.end() && g_pNewUISystem->IsVisible(it->second.windowId))
                g_pNewUISystem->Hide(it->second.windowId);
        }
        return;
    }
}

static void PrepareRegions(Rml::ElementDocument* workspace, const UI::Scaling::Transform& dock, bool snapshot = true,
                           bool retainFit = true)
{
    // RCSS lays out the shell; only component sizes and region reference units enter from C++.
    for (auto& [name, entry] : g_windows)
    {
        if (snapshot)
        {
            entry.previousBox = entry.box;
            entry.previouslyPlaced = entry.hasBox;
        }
        entry.hasBox = false;
    }
    Rml::ElementList regions;
    workspace->QuerySelectorAll(regions, ".region");
    for (Rml::Element* region : regions)
    {
        region->SetClass("occupied", false);
        if (region->HasAttribute("data-group"))
        {
            const auto state = g_groups.find(region->GetAttribute<Rml::String>("data-group", ""));
            if (retainFit && state != g_groups.end() && state->second.active && !g_groupViewportChanged)
                g_regionFit[region] = state->second.fit;
        }
        const float referenceWidth = region->GetAttribute<float>("data-ref-width", 0.f);
        const float referenceHeight = region->GetAttribute<float>("data-ref-height", 0.f);
        if (referenceWidth > 0.f)
            SetLength(region, Rml::PropertyId::Width, referenceWidth * BaseRegionScale(region, dock));
        if (referenceHeight > 0.f)
            SetLength(region, Rml::PropertyId::Height, referenceHeight * BaseRegionScale(region, dock));
    }
}

// Fit both axes uniformly, including a column narrowed by its neighbouring windows.
static float RegionFit(Rml::Element* region, float scale)
{
    const Rml::Vector2f reference{region->GetAttribute<float>("data-ref-width", 0.f),
                                  region->GetAttribute<float>("data-ref-height", 0.f)};
    const Rml::Vector2f resolved = region->GetBox().GetSize(Rml::BoxArea::Border);
    float fit = 1.f;
    if (reference.x > 0.f && resolved.x > 0.f)
        fit = std::min(fit, resolved.x / (reference.x * scale));
    if (reference.y > 0.f && resolved.y > 0.f)
        fit = std::min(fit, resolved.y / (reference.y * scale));
    return fit;
}

// True when a region changed, so its slots need sizing again.
static bool FitRegions(Rml::ElementDocument* workspace, const UI::Scaling::Transform& dock)
{
    bool changed = false;
    Rml::ElementList regions;
    workspace->QuerySelectorAll(regions, ".region");
    for (Rml::Element* region : regions)
    {
        if (region->HasAttribute("data-group"))
            continue;
        const float fit = RegionFit(region, BaseRegionScale(region, dock));
        const auto previous = g_regionFit.find(region);
        const float previousFit = previous != g_regionFit.end() ? previous->second : 1.f;
        if (std::abs(fit - previousFit) > RegionFitTolerance)
        {
            g_regionFit[region] = fit;
            changed = true;
        }
    }
    return changed;
}

static Rml::ElementList PrepareSlots(Rml::ElementDocument* workspace, const UI::Scaling::Transform& dock)
{
    Rml::ElementList slots;
    workspace->QuerySelectorAll(slots, ".slot");
    CheckNames(slots);
    for (Rml::Element* slot : slots)
    {
        Entry* entry = EntryFor(slot);
        mu::ui::window::CObject* window = entry != nullptr && entry->getWindow ? entry->getWindow() : nullptr;
        const bool open = entry != nullptr && (window != nullptr || entry->participant.visible) && IsOpen(*entry);
        const bool wantsFill = slot->GetAttribute<Rml::String>("data-fit", "") == "fill";
        const bool fill = open && wantsFill && window != nullptr && window->SupportsFillPlacement();
        if (entry != nullptr)
            entry->fill = fill;
        if (entry != nullptr)
            entry->reservesSpace = slot->GetParentNode() != nullptr && slot->GetParentNode()->GetAttribute<Rml::String>(
                                                                           "data-participation", "") == "reserve";
        slot->SetClass("open", open);
        slot->SetClass("fill", fill);
        if (open && slot->GetParentNode() != nullptr)
            slot->GetParentNode()->SetClass("occupied", true);
        if (wantsFill && open && !fill && !entry->warnedUnsupportedFill)
        {
            g_ErrorReport.Write(L"> [Placement] window '%hs' does not support data-fit=fill.\r\n",
                                slot->GetAttribute<Rml::String>("data-window", "").c_str());
            entry->warnedUnsupportedFill = true;
        }
        if (window != nullptr)
            window->SetFillPlacementSize(0.f, 0.f);
        slot->RemoveProperty(Rml::PropertyId::MinWidth);
        slot->RemoveProperty(Rml::PropertyId::MinHeight);
        if (fill)
        {
            // The theme sizes a fill slot, but never below the window's content size.
            const float scale = SlotScale(slot, dock);
            Rml::Vector2f content = PanelSize(*entry);
            window->GetFillMinimumSize(content.x, content.y);
            slot->RemoveProperty(Rml::PropertyId::Width);
            slot->RemoveProperty(Rml::PropertyId::Height);
            SetLength(slot, Rml::PropertyId::MinWidth, content.x * scale);
            SetLength(slot, Rml::PropertyId::MinHeight, content.y * scale);
        }
        else if (open)
        {
            const float scale = SlotScale(slot, dock);
            const Rml::Vector2f size = PanelSize(*entry);
            entry->measuredSize = size;
            SetLength(slot, Rml::PropertyId::Width, size.x * scale);
            SetLength(slot, Rml::PropertyId::Height, size.y * scale);
            if (SlotGroup(slot) != nullptr)
                MakeGroupDraggable(slot->GetAttribute<Rml::String>("data-window", ""), DocumentFor(*entry));
        }
    }

    return slots;
}

static bool FitGroups(Rml::ElementDocument* workspace)
{
    bool changed = false;
    Rml::ElementList groups;
    workspace->QuerySelectorAll(groups, "[data-group]");
    for (Rml::Element* group : groups)
    {
        Rml::ElementList slots;
        group->QuerySelectorAll(slots, ".slot.open");
        if (slots.empty())
            continue;
        const GroupBounds requested = CombinedBounds(slots);
        float panelsWidth = 0.f;
        for (Rml::Element* slot : slots)
            panelsWidth += ElementBounds(slot).width;
        const float fit = WindowGroupState::FitFactor(ElementBounds(group), requested, requested.width - panelsWidth);
        if (fit < 1.f - RegionFitTolerance)
        {
            const auto previous = g_regionFit.find(group);
            g_regionFit[group] = fit * (previous != g_regionFit.end() ? previous->second : 1.f);
            changed = true;
        }
    }
    return changed;
}

static std::optional<GroupPoint> RetainedGroupAnchor(Rml::Element* group, const Rml::ElementList& slots,
                                                     GroupPoint anchor, const WindowGroupState& state)
{
    if (!state.active || g_groupViewportChanged)
        return std::nullopt;
    Rml::Element* primary = GroupAnchor(group);
    if (primary != nullptr && primary->IsClassSet("open"))
        return state.lastAnchor;
    // Closing the anchor itself keeps an existing companion where it was drawn.
    for (Rml::Element* slot : slots)
    {
        const Entry* entry = EntryFor(slot);
        if (entry != nullptr && entry->previouslyPlaced)
        {
            const auto current = ElementBounds(slot);
            return GroupPoint{anchor.x + entry->previousBox.left - current.left,
                              anchor.y + entry->previousBox.top - current.top};
        }
    }
    return std::nullopt;
}

static void ResolveGroups(Rml::ElementDocument* workspace)
{
    Rml::ElementList groups;
    workspace->QuerySelectorAll(groups, "[data-group]");
    g_groupOffsets.clear();
    for (Rml::Element* group : groups)
    {
        Rml::ElementList slots;
        group->QuerySelectorAll(slots, ".slot.open");
        if (slots.empty())
        {
            const auto state = g_groups.find(group->GetAttribute<Rml::String>("data-group", ""));
            if (state != g_groups.end())
            {
                state->second.active = false;
                state->second.fit = 1.f;
            }
            continue;
        }
        WindowGroupState& state = GroupState(group);
        const GroupBounds bounds = CombinedBounds(slots);
        Rml::Element* primary = GroupAnchor(group);
        const bool anchorOpen = primary != nullptr && primary->IsClassSet("open");
        const GroupPoint anchor = RightTop(anchorOpen ? ElementBounds(primary) : bounds);
        const auto retained = RetainedGroupAnchor(group, slots, anchor, state);
        const GroupPoint shift = state.Translation(SafeBounds(), bounds, anchor, retained);
        g_groupOffsets[group] = {shift.x, shift.y};
        if (anchorOpen)
            state.lastAnchor = GroupPoint{anchor.x + shift.x, anchor.y + shift.y};
        state.active = true;
        const auto fit = g_regionFit.find(group);
        state.fit = fit != g_regionFit.end() ? fit->second : 1.f;
    }
}

static void PublishWorkspaceBounds(Rml::ElementDocument* workspace, Rml::Element* content)
{
    Rml::Element* root = workspace->GetContext() != nullptr ? workspace->GetContext()->GetRootElement() : nullptr;
    if (root == nullptr)
        return;

    const float top = content != nullptr ? content->GetAbsoluteOffset(Rml::BoxArea::Border).y : 0.f;
    const float bottom = content != nullptr ? top + content->GetBox().GetSize(Rml::BoxArea::Border).y
                                           : static_cast<float>(WindowHeight);
    root->SetProperty("--world-left", std::to_string(g_uncoveredLeft) + "px");
    root->SetProperty("--world-right", std::to_string(g_uncoveredRight) + "px");
    root->SetProperty("--workspace-top", std::to_string(top) + "px");
    root->SetProperty("--workspace-bottom", std::to_string(bottom) + "px");
}

static void UpdateUncoveredArea(Rml::ElementDocument* workspace, const Rml::ElementList& slots)
{
    // Open slots in a covering region narrow the world from the content area's edge they pack
    // against: a region's open slots touching the right edge cover from the right, and so on. A
    // group touching neither edge counts on the side of the content area's centre it is on.
    const float screenWidth = static_cast<float>(WindowWidth);
    Rml::Element* content = workspace->GetElementById("safe_area");
    const float contentLeft = content != nullptr ? content->GetAbsoluteOffset(Rml::BoxArea::Border).x : 0.f;
    const float contentRight =
        content != nullptr ? contentLeft + content->GetBox().GetSize(Rml::BoxArea::Border).x : screenWidth;
    g_uncoveredLeft = contentLeft;
    g_uncoveredRight = contentRight;

    struct Group
    {
        float left;
        float right;
    };
    std::unordered_map<Rml::Element*, Group> groups;
    for (Rml::Element* slot : slots)
    {
        Rml::Element* region = slot->GetParentNode();
        if (!slot->IsClassSet("open") || region == nullptr || !region->HasAttribute("data-covers-world"))
            continue;
        const Entry* entry = EntryFor(slot);
        const bool grouped = SlotGroup(slot) != nullptr && entry != nullptr && entry->hasBox;
        const float left = grouped ? entry->box.left : slot->GetAbsoluteOffset(Rml::BoxArea::Border).x;
        const float right = left + (grouped ? entry->box.width : slot->GetBox().GetSize(Rml::BoxArea::Border).x);
        const auto [it, added] = groups.try_emplace(region, Group{left, right});
        if (!added)
            it->second = {std::min(it->second.left, left), std::max(it->second.right, right)};
    }
    constexpr float Touch = 1.f;
    for (const auto& [region, group] : groups)
    {
        const bool fromRight = group.right >= contentRight - Touch;
        const bool fromLeft = group.left <= contentLeft + Touch;
        if (fromRight && !fromLeft)
            g_uncoveredRight = std::min(g_uncoveredRight, group.left);
        else if (fromLeft && !fromRight)
            g_uncoveredLeft = std::max(g_uncoveredLeft, group.right);
        else if (group.left + group.right >= contentLeft + contentRight)
            g_uncoveredRight = std::min(g_uncoveredRight, group.left);
        else
            g_uncoveredLeft = std::max(g_uncoveredLeft, group.right);
    }
    PublishWorkspaceBounds(workspace, content);
}

static void PlaceSlots(const Rml::ElementList& slots, const UI::Scaling::Transform& dock)
{
    // Each open window's document goes to its slot: its root at the slot's top-left, at the region's
    // scale.
    for (Rml::Element* slot : slots)
    {
        Entry* entry = EntryFor(slot);
        mu::ui::window::CObject* window = entry != nullptr && entry->getWindow ? entry->getWindow() : nullptr;
        if (entry == nullptr)
            continue;
        if (!slot->IsClassSet("open"))
        {
            entry->placed = false;
            continue;
        }

        const float scale = SlotScale(slot, dock);
        Rml::Vector2f offset = slot->GetAbsoluteOffset(Rml::BoxArea::Border);
        const auto shift = g_groupOffsets.find(SlotGroup(slot));
        if (shift != g_groupOffsets.end())
            offset += shift->second;
        const Rml::Vector2f size = slot->GetBox().GetSize(Rml::BoxArea::Border);
        entry->box = {offset.x, offset.y, size.x, size.y, scale};
        entry->hasBox = true;
        entry->unresolved = false;
        if (entry->participant.place)
        {
            entry->participant.place(&entry->box);
            continue;
        }
        if (window == nullptr)
            continue;
        if (!entry->placed)
            window->ResetSlotDrag();
        window->PlaceInWorkspace(offset.x, offset.y, scale);
        window->SyncSlotPlacement(); // Equal-box reload still needs its new document attached.
        if (slot->IsClassSet("fill"))
        {
            const Rml::Vector2f size = slot->GetBox().GetSize(Rml::BoxArea::Border);
            window->SetFillPlacementSize(size.x / scale, size.y / scale);
        }
        entry->placed = true;
    }
}

static void RestoreUnslotted(const Rml::ElementList& slots)
{
    // A registered component the workspace gives no slot keeps its own placement.
    for (auto& [name, entry] : g_windows)
    {
        if (entry.participant.place && !entry.hasBox && !entry.unresolved)
            entry.participant.place(nullptr);
        mu::ui::window::CObject* window = entry.getWindow ? entry.getWindow() : nullptr;
        if (window == nullptr || !window->IsInWorkspace())
            continue;
        const bool hasSlot =
            std::any_of(slots.begin(), slots.end(), [&](Rml::Element* slot) { return EntryFor(slot) == &entry; });
        if (!hasSlot)
        {
            window->LeaveWorkspace();
            entry.placed = false;
        }
    }
}

static Rml::ElementList FitWorkspace(Rml::ElementDocument* workspace, const UI::Scaling::Transform& dock,
                                     Rml::ElementList slots)
{
    // A fitted neighbour can release width; keep the reference boxes unscaled while fitting slots.
    for (int pass = 0; pass < MaximumRegionFitPasses; ++pass)
    {
        const bool regionsChanged = FitRegions(workspace, dock);
        const bool groupsChanged = FitGroups(workspace);
        if (!regionsChanged && !groupsChanged)
            break;
        slots = PrepareSlots(workspace, dock);
        {
            FRAME_PROFILE(UILayout);
            workspace->UpdateDocument();
        }
    }
    // Match a fitted column to its windows after solving, so height fitting cannot leave a wide gap.
    bool fittedWidthChanged = false;
    for (const auto& [region, fit] : g_regionFit)
    {
        const float referenceWidth = region->GetAttribute<float>("data-ref-width", 0.f);
        if (referenceWidth > 0.f && fit < 1.f)
        {
            SetLength(region, Rml::PropertyId::Width, referenceWidth * RegionScale(region, dock));
            fittedWidthChanged = true;
        }
    }
    if (fittedWidthChanged)
    {
        FRAME_PROFILE(UILayout);
        workspace->UpdateDocument();
    }
    return slots;
}

static void FlushPlacedDocuments()
{
    for (auto& [name, entry] : g_windows)
    {
        if (!entry.hasBox)
            continue;
        auto* window = entry.getWindow ? entry.getWindow() : nullptr;
        auto* document = window ? window->GetPlacedDocument() : DocumentFor(entry);
        if (document != nullptr)
        {
            FRAME_PROFILE(UILayout);
            document->UpdateDocument(); // Publish class/style changes before reading intrinsic boxes.
        }
    }
}

static bool IntrinsicSizeChanged(const Entry& entry, Rml::Vector2f size)
{
    const float scale = entry.hasBox ? entry.box.scale : 1.f;
    return std::abs(size.x - entry.measuredSize.x) * scale > IntrinsicFitTolerancePixels ||
           std::abs(size.y - entry.measuredSize.y) * scale > IntrinsicFitTolerancePixels;
}

static bool PlacedContentChanged()
{
    FlushPlacedDocuments();
    bool changed = false;
    for (const auto& [name, entry] : g_windows)
        if (entry.hasBox && !entry.fill && IntrinsicSizeChanged(entry, PanelSize(entry)))
            changed = true;
    return changed;
}

// On exhaustion, freeze the already applied scale, not an oversized historic size. This can
// retain a measured safe candidate, but does not invent one when theme constraints cannot fit.
static void SizeSlotsAtAppliedScale(const Rml::ElementList& slots)
{
    for (Rml::Element* slot : slots)
        if (Entry* entry = EntryFor(slot); entry && entry->hasBox && !entry->fill)
        {
            entry->measuredSize = PanelSize(*entry);
            SetLength(slot, Rml::PropertyId::Width, entry->measuredSize.x * entry->box.scale);
            SetLength(slot, Rml::PropertyId::Height, entry->measuredSize.y * entry->box.scale);
        }
}

static bool CandidateMatchesLayout(const Entry& entry)
{
    // RmlUi updates paint transform matrices later in Context::Update. Validate the just-flushed
    // intrinsic layout at its applied scale here, not the previous render's matrix cache.
    const auto size = PanelSize(entry);
    return std::abs(size.x * entry.box.scale - entry.box.width) <= IntrinsicFitTolerancePixels &&
           std::abs(size.y * entry.box.scale - entry.box.height) <= IntrinsicFitTolerancePixels;
}

static bool Fits(const GroupBounds& outer, const PlacementParticipant::Box& box)
{
    return box.left >= outer.left - IntrinsicFitTolerancePixels && box.top >= outer.top - IntrinsicFitTolerancePixels &&
           box.left + box.width <= outer.left + outer.width + IntrinsicFitTolerancePixels &&
           box.top + box.height <= outer.top + outer.height + IntrinsicFitTolerancePixels;
}

static bool ValidatePublishedSlots(const Rml::ElementList& slots)
{
    bool safe = true;
    const auto available = SafeBounds();
    for (Rml::Element* slot : slots)
    {
        Entry* entry = EntryFor(slot);
        if (!entry || !entry->hasBox)
            continue;
        const bool fits = SlotGroup(slot) == nullptr || Fits(available, entry->box);
        if (!CandidateMatchesLayout(*entry) || !fits)
        {
            // Reject metadata only. No document hiding, owner/session/pref mutation or claim of
            // input quarantine: unsupported no-safe themes require a broader integration policy.
            entry->unresolved = true;
            entry->hasBox = false;
            safe = false;
        }
        entry->measuredSize = PanelSize(*entry); // Idle does not retry the same failed signature.
    }
    return safe;
}

static void ReportReconciliationFailure()
{
    std::ostringstream signature;
    signature << WindowWidth << ':' << WindowHeight << ':' << g_lastDock.scaleX << ':'
              << UI::RmlBridge::GetActiveThemeName();
    for (const auto& [name, entry] : g_windows)
        if (entry.lastVisible)
            signature << '|' << name << ':' << entry.measuredSize.x << ':' << entry.measuredSize.y << ':'
                      << entry.box.scale << ':' << entry.unresolved;
    if (signature.str() == g_lastFailureSignature)
        return;
    g_lastFailureSignature = signature.str();
    g_ErrorReport.Write(L"> [Placement] intrinsic layout did not settle within %d passes (0.25 screen px); %hs. "
                        L"Check responsive size rules/available workspace. Signature: %hs\r\n",
                        MaximumIntrinsicFitPasses,
                        g_reconciliation == ReconciliationResult::MeasuredSafe
                            ? "using measured safe geometry"
                            : "no safe geometry; bounds rejected, input quarantine not implemented",
                        g_lastFailureSignature.c_str());
}

static bool ReconcileContent(Rml::ElementDocument* workspace, const UI::Scaling::Transform& dock,
                             Rml::ElementList& slots)
{
    for (int pass = 0; pass < MaximumIntrinsicFitPasses; ++pass)
    {
        if (pass > 0)
        {
            g_regionFit.clear();
            PrepareRegions(workspace, dock, false, false); // Refit shrinking as well as growing content.
        }
        slots = PrepareSlots(workspace, dock);
        workspace->UpdateDocument();
        slots = FitWorkspace(workspace, dock, std::move(slots));
        // Scale/class publication may reflow a content panel. Group anchors/prefs are resolved
        // only once below, against final dimensions, not interim too-small clamps.
        g_groupOffsets.clear();
        PlaceSlots(slots, dock);
        if (!PlacedContentChanged())
            return true;
    }
    SizeSlotsAtAppliedScale(slots);
    workspace->UpdateDocument();
    return false;
}

static void ArrangeNow()
{
    Rml::ElementDocument* workspace = Workspace();
    if (workspace == nullptr)
        return;
    if (workspace->QuerySelector("[data-group]") != nullptr &&
        mu::ui::window::CInventoryCtrl::GetPickedItem() != nullptr)
    {
        Invalidate();
        return;
    }

    const auto dock = UI::Scaling::DockRightTransform(WindowWidth, WindowHeight);
    g_groupViewportChanged = g_reservedSizeChanged || WindowWidth != g_lastWidth || WindowHeight != g_lastHeight ||
                             dock.scaleX != g_lastDock.scaleX || dock.offsetY != g_lastDock.offsetY;
    g_lastDock = dock;
    g_reservedSizeChanged = false;
    g_dirty = false;
    g_lastWidth = WindowWidth;
    g_lastHeight = WindowHeight;
    for (auto& [name, entry] : g_windows)
        entry.lastVisible = IsOpen(entry);

    UI::RmlBridge::SyncDocumentLayoutScales();
    workspace->GetContext()->Update(); // Resolve owner bindings before measuring first-open/capacity.
    g_regionFit.clear();
    PrepareRegions(workspace, dock, true, !g_intrinsicSizeChanged);
    g_intrinsicSizeChanged = false;
    Rml::ElementList slots;
    const bool converged = ReconcileContent(workspace, dock, slots);
    ResolveGroups(workspace);
    PlaceSlots(slots, dock);
    FlushPlacedDocuments();
    const bool safe = ValidatePublishedSlots(slots);
    g_reconciliation = !safe       ? ReconciliationResult::Unresolved
                       : converged ? ReconciliationResult::Converged
                                   : ReconciliationResult::MeasuredSafe;
    if (g_reconciliation == ReconciliationResult::Converged)
        g_lastFailureSignature.clear();
    else
        ReportReconciliationFailure();
    UpdateUncoveredArea(workspace, slots);
    RestoreUnslotted(slots);
    workspace->GetContext()->Update();
    Rml::ElementUtilities::UpdateTransformCaches(workspace->GetContext()->GetRootElement());
    for (const auto& [name, entry] : g_windows)
        if (auto* window = entry.getWindow ? entry.getWindow() : nullptr)
            window->SyncNativeLayout();
}

void Arrange()
{
    // Immediate, so a window opened during this frame's input is drawn and hit-tested in place.
    ArrangeNow();
}

static void CheckIntrinsicSize(Entry& entry, bool visible)
{
    if (entry.reservesSpace && visible != entry.lastVisible)
        g_reservedSizeChanged = true;
    if (!visible || entry.fill)
        return;
    const auto size = PanelSize(entry); // Cached completed layout; no layout/Arrange on idle getters.
    if (IntrinsicSizeChanged(entry, size))
    {
        if (entry.reservesSpace)
            g_reservedSizeChanged = true; // Restore preferred anchor after a changed safe area.
        else
            g_intrinsicSizeChanged = true; // Refit at base scale, without discarding preferred/drag anchor.
        Invalidate();
    }
}

bool Update()
{
    const auto dock = UI::Scaling::DockRightTransform(WindowWidth, WindowHeight);
    for (auto& [name, entry] : g_windows)
    {
        const bool visible = IsOpen(entry);
        CheckIntrinsicSize(entry, visible);
        if (visible != entry.lastVisible)
        {
            entry.lastVisible = visible;
            Invalidate();
        }
    }
    const bool arrange = g_dirty || WindowWidth != g_lastWidth || WindowHeight != g_lastHeight ||
                         dock.scaleX != g_lastDock.scaleX || dock.offsetY != g_lastDock.offsetY;
    if (arrange)
        ArrangeNow();
    if (auto* context = RmlUiRuntime::Instance().GetContext())
        Rml::ElementUtilities::UpdateTransformCaches(context->GetRootElement());
    for (const auto& [name, entry] : g_windows)
    {
        if (mu::ui::window::CObject* window = entry.getWindow ? entry.getWindow() : nullptr)
        {
            window->SyncFillPlacement();
            window->SyncSlotPlacement();
            if (!arrange)
                window->SyncNativeLayout();
        }
    }
    return arrange;
}

void AfterRmlLayout(Rml::Context*)
{
    Update(); // Dirty arrangements flush their final context; idle only syncs native anchors.
}

ReconciliationResult LastReconciliationResult()
{
    return g_reconciliation;
}

float UncoveredWorldLeft()
{
    return g_workspaceView.Document() != nullptr ? g_uncoveredLeft : 0.f;
}

float UncoveredWorldRight()
{
    return g_workspaceView.Document() != nullptr ? g_uncoveredRight : static_cast<float>(WindowWidth);
}

bool InitialPosition(std::string_view slotName, float width, float height, float& x, float& y)
{
    Rml::ElementDocument* workspace = Workspace();
    if (workspace == nullptr)
        return false;

    Rml::ElementList slots;
    workspace->QuerySelectorAll(slots, ".slot");
    for (Rml::Element* slot : slots)
    {
        if (slot->GetAttribute<Rml::String>("data-window", "") != slotName)
            continue;
        // Laid out only to be read: the window floats, so it takes no room in its region.
        slot->SetClass("open", true);
        SetLength(slot, Rml::PropertyId::Width, width);
        SetLength(slot, Rml::PropertyId::Height, height);
        {
            FRAME_PROFILE(UILayout);
            workspace->UpdateDocument();
        }
        const Rml::Vector2f offset = slot->GetAbsoluteOffset(Rml::BoxArea::Border);
        x = offset.x;
        y = offset.y;
        slot->SetClass("open", false);
        slot->RemoveProperty(Rml::PropertyId::Width);
        slot->RemoveProperty(Rml::PropertyId::Height);
        {
            FRAME_PROFILE(UILayout);
            workspace->UpdateDocument();
        }
        return true;
    }
    return false;
}

float UncoveredWorldLeftIn(const UI::Scaling::Transform& transform)
{
    return (UncoveredWorldLeft() - transform.offsetX) / transform.scaleX;
}

float UncoveredWorldRightIn(const UI::Scaling::Transform& transform)
{
    return (UncoveredWorldRight() - transform.offsetX) / transform.scaleX;
}

void Release()
{
    g_workspaceView.Release();
    g_windows.clear();
    g_groups.clear();
    g_groupOffsets.clear();
    g_dirty = true;
    g_reconciliation = ReconciliationResult::Converged;
    g_lastFailureSignature.clear();
    g_intrinsicSizeChanged = false;
    g_reservedSizeChanged = false;
}
} // namespace UI::Placement
