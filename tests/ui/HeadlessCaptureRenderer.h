#pragma once
#include "Render/Renderer/MuRenderer.h"
#include <unordered_set>

// Test only: capture ids/ordering/lifetimes are real RenderTarget paths; no GPU or game drawing.
class HeadlessCaptureRenderer : public mu::IMuRenderer
{
public:
    void RenderQuad2D(std::span<const mu::Vertex2D>, std::uint32_t) override {}
    void RenderTriangles(std::span<const mu::Vertex3D>, std::uint32_t) override {}
    void RenderQuad3D(std::span<const mu::Vertex3D>, std::uint32_t) override {}
    void RenderQuadStrip(std::span<const mu::Vertex3D>, std::uint32_t) override {}
    void RenderLines(std::span<const mu::Vertex3D>, std::uint32_t) override {}
    void SetBlendMode(mu::BlendMode) override {}
    void DisableBlend() override {}
    void SetDepthTest(bool) override {}
    void SetFog(const mu::FogParams&) override {}
    void BeginScene(int, int, int, int) override {}
    void EndScene() override {}
    void Begin2DPass() override {}
    void End2DPass() override {}
    void ClearScreen() override {}
    std::uint32_t CreateRenderTarget(std::uint32_t, std::uint32_t) override
    {
        lastCreated = ++serial;
        return serial;
    }
    void ReleaseRenderTarget(std::uint32_t id) override
    {
        released.insert(id);
    }
    std::uint32_t BeginOffscreenCapture(std::uint32_t id, std::uint32_t, std::uint32_t) override
    {
        return allowed.contains(id) ? id : 0;
    }
    void EndOffscreenCapture() override {}
    void* GetRawTexture(std::uint32_t id) override
    {
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id));
    }
    std::uint32_t lastCreated = 0;
    std::unordered_set<std::uint32_t> allowed, released;

private:
    std::uint32_t serial = 0;
};
