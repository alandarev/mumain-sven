#include "stdafx.h"
#include "UI/Social/PhotoViewerControl.h"

#include "UI/Social/PhotoViewer.h"
#include "UI/RmlBridge/RmlTooltip.h"
#include "UI/Social/SocialWorkspace.h"
#include "Core/Utilities/StringUtils.h"
#include "I18N/All.h"

#include <RmlUi/Core.h>

#include <cmath>

namespace UI::Social
{
namespace
{
constexpr const char* SlotId = "photo_slot";
constexpr const char* ImageId = "photo_image";
constexpr const char* HelpId = "photo_help";
// A letter from Webzen shows its logo where a sender would stand.
constexpr const char* WebzenLogo = "/Data/Local/Webzenlogo.jpg";
constexpr float OrbitYawPerWell = 180.f;
constexpr float OrbitPitchPerWell = 90.f;
constexpr float OrbitZoomPerNotch = 0.08f;

// RmlUi reports the cursor in screen pixels; turning by the delta in dp keeps a drag feeling
// identical at every UI scale.
float ToDp(float rmlPixels)
{
    const float ratio = UI::Social::DpRatio();
    return ratio > 0 ? rmlPixels / ratio : rmlPixels;
}
} // namespace

PhotoViewerControl::~PhotoViewerControl()
{
    Detach();
}

void PhotoViewerControl::Attach(Rml::ElementDocument* document, CUIPhotoViewer& viewer)
{
    AttachTo(document, viewer, SlotId, ImageId, false);
}

void PhotoViewerControl::AttachOrbit(Rml::ElementDocument* document, CUIPhotoViewer& viewer, const char* slotId,
                                     const char* imageId)
{
    AttachTo(document, viewer, slotId, imageId, true);
}

void PhotoViewerControl::AttachTo(Rml::ElementDocument* document, CUIPhotoViewer& viewer, const char* slotId,
                                  const char* imageId, bool orbit)
{
    if (Document() == document && m_Viewer == &viewer && m_Orbit == orbit)
        return;
    Detach();
    if (document == nullptr)
        return;
    m_Document = document->GetObserverPtr();
    m_Viewer = &viewer;
    m_SlotId = slotId;
    m_ImageId = imageId;
    m_Orbit = orbit;
    if (auto* slot = document->GetElementById(m_SlotId))
    {
        slot->AddEventListener(Rml::EventId::Mousedown, this);
        if (orbit)
        {
            slot->AddEventListener(Rml::EventId::Drag, this);
            slot->AddEventListener(Rml::EventId::Dragend, this);
            slot->AddEventListener(Rml::EventId::Mouseup, this);
            slot->AddEventListener(Rml::EventId::Mousescroll, this);
        }
    }
    if (orbit)
        return;
    if (auto* help = document->GetElementById(HelpId))
        help->AddEventListener(Rml::EventId::Mousedown, this);
    // On the document, not the slot: a turn that leaves the slot has to keep turning, and the
    // release that ends it usually lands somewhere else entirely.
    document->AddEventListener(Rml::EventId::Mousemove, this);
    document->AddEventListener(Rml::EventId::Mouseup, this);
}

void PhotoViewerControl::Detach()
{
    Suspend();
    // A destroyed document has already dropped this listener along with its elements.
    if (auto* document = Document())
    {
        if (auto* slot = document->GetElementById(m_SlotId))
        {
            slot->RemoveEventListener(Rml::EventId::Mousedown, this);
            if (m_Orbit)
            {
                slot->RemoveEventListener(Rml::EventId::Drag, this);
                slot->RemoveEventListener(Rml::EventId::Dragend, this);
                slot->RemoveEventListener(Rml::EventId::Mouseup, this);
                slot->RemoveEventListener(Rml::EventId::Mousescroll, this);
            }
        }
        if (auto* help = document->GetElementById(HelpId))
            help->RemoveEventListener(Rml::EventId::Mousedown, this);
        document->RemoveEventListener(Rml::EventId::Mousemove, this);
        document->RemoveEventListener(Rml::EventId::Mouseup, this);
    }
    m_Document = {};
    m_Viewer = nullptr;
    m_Turning = false;
}

void PhotoViewerControl::Sync()
{
    auto* document = Document();
    if (document == nullptr || m_Viewer == nullptr)
        return;
    const bool webzen = m_Viewer->IsWebzenMail();
    auto& target = m_Viewer->Target();
    target.SetEnabled(!webzen);
    if (auto* slot = document->GetElementById(m_SlotId))
        slot->SetClass("webzen", webzen);
    if (auto* image = document->GetElementById(m_ImageId))
    {
        if (webzen)
        {
            target.BindImage(nullptr); // Static-logo owner is not clobbered by a later capture/destructor.
            if (image->GetAttribute<Rml::String>("src", "") != WebzenLogo)
                image->SetAttribute("src", Rml::String(WebzenLogo));
        }
        else
            target.BindImage(image);
    }
    if (!m_Orbit)
        SyncHelp(*document);
}

Rml::ElementDocument* PhotoViewerControl::Document() const
{
    return static_cast<Rml::ElementDocument*>(m_Document.get());
}

void PhotoViewerControl::Suspend()
{
    m_Turning = false;
    if (m_Viewer != nullptr)
        m_Viewer->Target().SetEnabled(false);
    UI::RmlBridge::Tooltip::Hide(this);
}

// Three white lines left-aligned in a box centred on the well, ending at its bottom -- where
// RenderTipTextList(..., RT3_SORT_LEFT) put them. Above the character now, since the character is
// an image inside this same document.
void PhotoViewerControl::SyncHelp(Rml::ElementDocument& document)
{
    const bool shown = m_Viewer->IsHelpShown() && !m_Viewer->IsWebzenMail();
    if (auto* help = document.GetElementById(HelpId))
        help->SetClass("active", shown);
    auto* slot = document.GetElementById(SlotId);
    if (!shown || slot == nullptr)
    {
        UI::RmlBridge::Tooltip::Hide(this);
        return;
    }

    UI::RmlBridge::Tooltip::Config config;
    for (const wchar_t* text :
         {I18N::Game::WheelButtonZoomInOut, I18N::Game::LeftClickRotation, I18N::Game::RightClickDefault})
    {
        UI::RmlBridge::Tooltip::Line line;
        line.text = StringUtils::WideToNarrow(text);
        config.lines.push_back(std::move(line));
    }
    const auto size = slot->GetBox().GetSize(Rml::BoxArea::Border);
    config.anchorX = slot->GetAbsoluteLeft() + size.x * 0.5f;
    config.anchorY = slot->GetAbsoluteTop() + size.y;
    config.anchor = UI::RmlBridge::Tooltip::AnchorPoint::AboveLeft;
    config.centerHorizontally = true;
    UI::RmlBridge::Tooltip::Show(config, this);
}

void PhotoViewerControl::ProcessEvent(Rml::Event& event)
{
    if (m_Viewer == nullptr)
        return;
    if (m_Orbit)
    {
        ProcessOrbitEvent(event);
        return;
    }

    const Rml::Element* target = event.GetTargetElement();
    const bool onHelp = target != nullptr && target->GetId() == HelpId;

    switch (event.GetId())
    {
    case Rml::EventId::Mousedown:
    {
        const int button = event.GetParameter<int>("button", 0);
        if (onHelp)
        {
            if (button == 0)
            {
                m_Viewer->ToggleHelp();
                event.StopPropagation(); // Never starts a turn as well.
            }
            return;
        }
        if (button == 1)
            m_Viewer->ResetView();
        else if (button == 0)
        {
            m_Turning = true;
            m_LastX = event.GetParameter<float>("mouse_x", 0.0f);
        }
        return;
    }
    case Rml::EventId::Mousemove:
    {
        if (!m_Turning)
            return;
        const float x = event.GetParameter<float>("mouse_x", 0.0f);
        m_Viewer->TurnBy(ToDp(x - m_LastX));
        m_LastX = x;
        return;
    }
    case Rml::EventId::Mouseup:
        m_Turning = false;
        return;
    default:
        return;
    }
}

void PhotoViewerControl::ProcessOrbitEvent(Rml::Event& event)
{
    switch (event.GetId())
    {
    case Rml::EventId::Mousedown:
        if (event.GetParameter<int>("button", 0) == 0)
        {
            m_Turning = true;
            m_LastX = event.GetParameter<float>("mouse_x", 0.f);
            m_LastY = event.GetParameter<float>("mouse_y", 0.f);
        }
        else if (event.GetParameter<int>("button", 0) == 1)
            m_Viewer->ResetView();
        return;
    case Rml::EventId::Drag:
        OrbitDrag(event);
        return;
    case Rml::EventId::Mouseup:
    case Rml::EventId::Dragend:
        m_Turning = false;
        return;
    case Rml::EventId::Mousescroll:
        m_Viewer->ZoomBy(-event.GetParameter<float>("wheel_delta_y", 0.f) * OrbitZoomPerNotch);
        event.StopPropagation();
        return;
    default:
        return;
    }
}

void PhotoViewerControl::OrbitDrag(Rml::Event& event)
{
    if (!m_Turning)
        return;
    const auto size = event.GetCurrentElement()->GetBox().GetSize(Rml::BoxArea::Content);
    if (size.x <= 0.f || size.y <= 0.f)
        return;
    const float x = event.GetParameter<float>("mouse_x", m_LastX);
    const float y = event.GetParameter<float>("mouse_y", m_LastY);
    m_Viewer->TurnBy((x - m_LastX) * OrbitYawPerWell / size.x);
    m_Viewer->TiltBy((m_LastY - y) * OrbitPitchPerWell / size.y);
    m_LastX = x;
    m_LastY = y;
    event.StopPropagation();
}
} // namespace UI::Social
