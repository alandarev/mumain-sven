#include "stdafx.h"
#include "UI/RmlBridge/RmlRenderTarget.h"

#include "Render/Renderer/MuRenderer.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "Render/RmlUi/RmlUiRuntime.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace UI::RmlBridge
{
namespace
{
constexpr char kSourcePrefix[] = "rtt:";

// A retired texture is released only once no document can still be sampling it. A new Source() is
// picked up by the view's next sync and applied by RmlUi's next update, which may fall a frame
// after that, so the old one stays alive for a few frames past the switch.
constexpr std::uint64_t kRetireLag = 3;

struct Published
{
    std::uint32_t id = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct Retired
{
    Rml::String source;
    std::uint32_t id = 0;
    std::uint64_t releaseFrame = 0;
};

std::uint64_t s_frame = 0;
// Never reused, unlike texture ids: a recycled id matching a stale RmlUi cache entry would hand
// RmlUi a released texture without it ever asking LoadTexture() again.
std::uint64_t s_nextSerial = 1;
std::uint64_t s_nextBinding = 1;
std::uint64_t s_publicationRevision = 0;
constexpr char kBindingOwner[] = "data-render-target-owner";
constexpr char kPendingClass[] = "render-target-pending";

std::vector<RenderTarget*>& Targets()
{
    static std::vector<RenderTarget*> targets;
    return targets;
}

std::unordered_map<Rml::String, Published>& PublishedSources()
{
    static std::unordered_map<Rml::String, Published> sources;
    return sources;
}

std::vector<Retired>& RetiredTextures()
{
    static std::vector<Retired> retired;
    return retired;
}

void Retire(std::uint32_t id, const Rml::String& source)
{
    if (id != 0)
        RetiredTextures().push_back({source, id, s_frame + kRetireLag});
}

void ReleaseRetired()
{
    const bool rmlReady = RmlUiRuntime::Instance().IsCreated();
    auto& retired = RetiredTextures();
    for (auto it = retired.begin(); it != retired.end();)
    {
        if (it->releaseFrame > s_frame)
        {
            ++it;
            continue;
        }
        if (!it->source.empty())
        {
            PublishedSources().erase(it->source);
            // Before the texture goes, so RmlUi's cache never holds a handle whose address the
            // renderer could hand out again.
            if (rmlReady)
                Rml::ReleaseTexture(it->source);
        }
        mu::GetRenderer().ReleaseRenderTarget(it->id);
        it = retired.erase(it);
    }
}
} // namespace

RenderTarget::RenderTarget(Drawer drawer) : m_drawer(std::move(drawer))
{
    m_bindingToken = std::to_string(s_nextBinding++);
    Targets().push_back(this);
}

RenderTarget::~RenderTarget()
{
    UnbindImage();
    auto& targets = Targets();
    targets.erase(std::remove(targets.begin(), targets.end(), this), targets.end());
    // Never shown, so nothing can be sampling it.
    if (m_pending.id != 0)
        mu::GetRenderer().ReleaseRenderTarget(m_pending.id);
    Retire(m_current.id, m_source);
}

void RenderTarget::Resize(std::uint32_t width, std::uint32_t height)
{
    if (width == 0 || height == 0)
        return;
    const Texture& latest = m_pending.id != 0 ? m_pending : m_current;
    if (latest.id != 0 && latest.width == width && latest.height == height)
        return;
    // Superseded before it was ever shown.
    if (m_pending.id != 0)
        mu::GetRenderer().ReleaseRenderTarget(m_pending.id);
    m_pending = {};
    if (m_current.id != 0 && m_current.width == width && m_current.height == height)
        return;
    if (const std::uint32_t id = mu::GetRenderer().CreateRenderTarget(width, height))
        m_pending = {id, width, height};
}

bool RenderTarget::PrepareImageExtent()
{
    if (!m_imageBound)
        return true; // Manual/unbound targets retain their explicit size contract.
    if (!m_image || !m_imageEnabled || m_image->GetAttribute<Rml::String>(kBindingOwner, "") != m_bindingToken)
        return false;
    Rml::Vector2f size = m_image->GetBox().GetSize(Rml::BoxArea::Content), offset;
    if (m_imageExtent == ImageExtent::DrawnContent && !DrawnContentBox(*m_image.get(), offset, size))
        return false;
    const auto valid = [](float v)
    { return std::isfinite(v) && v >= 0.5f && v < static_cast<float>(std::numeric_limits<std::uint32_t>::max()); };
    if (!valid(size.x) || !valid(size.y))
        return false; // Never capture stale dimensions or force an unmeasured box to 1px.
    const auto width = static_cast<std::uint32_t>(std::lround(size.x)),
               height = static_cast<std::uint32_t>(std::lround(size.y));
    Resize(width, height);
    const auto& latest = m_pending.id ? m_pending : m_current;
    return latest.id && latest.width == width &&
           latest.height == height; // Allocation failure cannot capture an old-sized target.
}

void RenderTarget::Draw()
{
    if (!m_enabled || !m_drawer || !PrepareImageExtent())
        return;
    const bool promoting = m_pending.id != 0;
    const Texture target = promoting ? m_pending : m_current;
    if (target.id == 0)
        return;

    auto& renderer = mu::GetRenderer();
    if (renderer.BeginOffscreenCapture(target.id, target.width, target.height) == 0)
        return;
    m_drawer(target.width, target.height);
    renderer.EndOffscreenCapture();

    if (!promoting)
        return;
    Retire(m_current.id, m_source);
    m_current = target;
    m_pending = {};
    m_source = kSourcePrefix + std::to_string(s_nextSerial++);
    PublishedSources()[m_source] = {target.id, target.width, target.height};
    PublishImage();
}

void RenderTarget::UnbindImage()
{
    if (m_image && m_image->GetAttribute<Rml::String>(kBindingOwner, "") == m_bindingToken)
    {
        if (!m_boundSource.empty() && m_image->GetAttribute<Rml::String>("src", "") == m_boundSource)
            m_image->RemoveAttribute("src");
        m_image->SetClass(kPendingClass, false);
        m_image->RemoveAttribute(kBindingOwner);
        ++s_publicationRevision;
    }
    m_image.reset();
    m_boundSource.clear();
}

void RenderTarget::BindImage(Rml::Element* image, bool enabled, ImageExtent extent)
{
    if (m_image != image || (image && image->GetAttribute<Rml::String>(kBindingOwner, "") != m_bindingToken))
    {
        UnbindImage();
        if (image)
        {
            m_image = image->GetObserverPtr();
            // Rebinding cannot leave another target's stale source behind while this owner awaits its first capture.
            if (!image->GetAttribute<Rml::String>(kBindingOwner, "").empty())
                image->RemoveAttribute("src");
            image->SetAttribute(kBindingOwner, m_bindingToken);
        }
    }
    m_imageEnabled = enabled;
    m_imageBound = image != nullptr;
    m_imageExtent = extent;
    PublishImage();
}

void RenderTarget::PublishImage()
{
    if (!m_image || m_image->GetAttribute<Rml::String>(kBindingOwner, "") != m_bindingToken)
        return;
    const bool pending = !m_imageEnabled || m_source.empty();
    bool changed = false;
    if (m_image->IsClassSet(kPendingClass) != pending)
    {
        m_image->SetClass(kPendingClass, pending);
        changed = true;
    }
    if (!pending && m_image->GetAttribute<Rml::String>("src", "") != m_source)
    {
        m_image->SetAttribute("src", m_source);
        m_boundSource = m_source;
        changed = true;
    }
    if (changed)
        ++s_publicationRevision;
}

std::uint64_t RenderTarget::PublicationRevision()
{
    return s_publicationRevision;
}

void RenderTarget::RenderAll()
{
    ++s_frame;
    ReleaseRetired();
    // A drawer could destroy a target, its own or another's.
    const auto targets = Targets();
    for (RenderTarget* target : targets)
    {
        const auto& live = Targets();
        if (std::find(live.begin(), live.end(), target) != live.end())
            target->Draw();
    }
}

bool RenderTarget::IsSource(const Rml::String& source)
{
    return source.rfind(kSourcePrefix, 0) == 0;
}

void* RenderTarget::Resolve(const Rml::String& source, int& width, int& height)
{
    const auto found = PublishedSources().find(source);
    if (found == PublishedSources().end())
        return nullptr;
    width = static_cast<int>(found->second.width);
    height = static_cast<int>(found->second.height);
    return mu::GetRenderer().GetRawTexture(found->second.id);
}
} // namespace UI::RmlBridge
