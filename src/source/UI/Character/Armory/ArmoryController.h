#pragma once
#include "ArmoryData.h"
#include "UI/RmlBridge/RmlRenderTarget.h"
#include "UI/RmlBridge/RmlTooltip.h"
#include "UI/Social/PhotoViewerControl.h"
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ObserverPtr.h>
#include <chrono>
#include <memory>
#include <set>
namespace Rml
{
class DataModelConstructor;
class Element;
class ElementDocument;
} // namespace Rml
class CUIPhotoViewer;
class RmlViewLayoutFixture;
struct tagITEM;
namespace UI::Character
{
// Owns the Armory's live data and native render targets. Presentation stays in RML/RCSS.
class ArmoryController
{
    friend class ::RmlViewLayoutFixture;

public:
    ArmoryController();
    ~ArmoryController();
    void Bind(Rml::DataModelConstructor& constructor);
    bool Sync(Rml::ElementDocument* document, bool shown);
    void Suspend();
    void Release();
    bool CancelEditor();
    void RenderTooltip();

private:
    struct AttributeView
    {
        int index = 0, addition = 0;
        Rml::String label, total, note, explanation, input;
        bool proposed = false, highlighted = false;
    };
    struct RowView
    {
        int index = 0;
        Rml::String label, value, detail;
        bool expanded = false, highlighted = false;
    };
    struct CategoryView
    {
        Rml::String name;
        std::vector<RowView> rows;
    };
    void BindActions(Rml::DataModelConstructor& constructor);
    void Accept(ArmorySnapshot snapshot);
    void RefreshSkill();
    bool RenderSkillTooltip();
    void RefreshEditor();
    void Edit();
    void Change(int index, int value, bool emptyInput = false);
    void Apply();
    bool Send(bool apply, bool retry = false);
    void SyncNetwork();
    void SyncTargets(Rml::ElementDocument* document);
    void Hover(int slot, int row);
    void HoverAttribute(int index);
    void RefreshHighlights();
    void Dirty(const char* name);
    Rml::DataModelHandle m_Handle;
    // Expires when a theme switch's rebuild retires the document, ahead of the next Sync().
    Rml::ObserverPtr<Rml::Element> m_Document;
    std::unique_ptr<CUIPhotoViewer> m_Viewer;
    Social::PhotoViewerControl m_PhotoControl;
    std::unique_ptr<tagITEM[]> m_Items;
    std::array<std::unique_ptr<RmlBridge::RenderTarget>, 12> m_Targets;
    ArmorySnapshot m_Snapshot;
    ArmoryRequest m_EditRequest, m_Request;
    std::array<Rml::String, 5> m_EditText;
    std::vector<AttributeView> m_Attributes;
    std::vector<CategoryView> m_Categories;
    std::set<Rml::String> m_Expanded;
    Rml::String m_Character, m_Title, m_Status = "Reading character statistics…", m_Budget, m_Points;
    Rml::String m_SkillName, m_SkillOutput, m_SkillKind, m_Costs, m_Restriction, m_Target;
    std::vector<Rml::String> m_Calculation;
    RmlBridge::Tooltip::Config m_SkillTooltip;
    std::array<Rml::String, 3> m_Resources, m_ResourceDetails;
    std::array<bool, 3> m_ResourceExpanded{}, m_ResourceHighlighted{};
    bool m_Shown = false, m_Ready = false, m_Editing = false, m_Waiting = false, m_Valid = false;
    bool m_Ratings = false, m_Retry = false, m_ViewerReady = false, m_OverBudget = false;
    bool m_TooltipNeedsHide = false;
    bool m_SkillHovered = false;
    int m_HoveredSlot = -1, m_HoveredRow = -1, m_HoveredAttribute = -1, m_Skill = 0;
    std::chrono::steady_clock::time_point m_OpenedAt{}, m_LastRequest{}, m_LastEquipment{}, m_LastPetRequest{};
};
} // namespace UI::Character
