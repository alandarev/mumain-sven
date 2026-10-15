#pragma once

#include <RmlUi/Core/Types.h>
#include <RmlUi/Core/ObserverPtr.h>

#include <cstdint>
#include <functional>

// Native drawing rendered into a texture an RmlUi document shows like any other image, so it sits at
// its element's own depth -- under a window that covers it, beside the text drawn over it -- instead
// of above or below a whole context.
//
// Bind Source() to an <img src>. The drawer runs once a frame from the renderer's offscreen seam
// (MuRenderer::SetOffscreenRenderCallback), inside a capture of exactly Width() x Height() physical
// pixels. The capture brings its own viewport and a transparent clear, so the drawer sets only its
// projection, for that aspect, and draws.
namespace Rml
{
class Element;
}

namespace UI::RmlBridge
{
class RenderTarget
{
public:
    enum class ImageExtent
    {
        Content,
        DrawnContent
    };
    using Drawer = std::function<void(std::uint32_t width, std::uint32_t height)>;

    explicit RenderTarget(Drawer drawer);
    ~RenderTarget();
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    // Physical pixels. A new size takes a new texture, and Source() moves to it only once it has
    // been drawn, so a document never samples one that holds nothing yet.
    void Resize(std::uint32_t width, std::uint32_t height);
    // Draws only while enabled, so a hidden window costs nothing.
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    // One owner per element. Readiness never overwrites authored visibility or intrinsic size.
    void BindImage(Rml::Element* image, bool enabled = true, ImageExtent extent = ImageExtent::Content);
    static std::uint64_t PublicationRevision();

    // Empty until the first frame has been drawn.
    const Rml::String& Source() const { return m_source; }

    // Drained once per frame from Winmain.cpp's SetOffscreenRenderCallback.
    static void RenderAll();

    // For RmlUiRenderInterface::LoadTexture(): whether `source` names a render target, and the raw
    // texture behind it with its size. Null once that texture has been retired.
    static bool IsSource(const Rml::String& source);
    static void* Resolve(const Rml::String& source, int& width, int& height);

private:
    struct Texture
    {
        std::uint32_t id = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    void Draw();
    void PublishImage();
    void UnbindImage();
    bool PrepareImageExtent();
    Rml::ObserverPtr<Rml::Element> m_image;
    Rml::String m_bindingToken;
    Rml::String m_boundSource;
    bool m_imageEnabled = true;
    bool m_imageBound = false;
    ImageExtent m_imageExtent = ImageExtent::Content;

    Drawer m_drawer;
    Texture m_current;
    Texture m_pending;
    Rml::String m_source;
    bool m_enabled = false;
};
} // namespace UI::RmlBridge
