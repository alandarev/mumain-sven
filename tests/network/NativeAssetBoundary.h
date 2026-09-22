#pragma once

#include "doctest.h"
#include "Render/Sprites/GlobalBitmap.h"

namespace SEASON3B
{
// Native Release methods call UnloadImages even without Create. UnloadImage
// returns after its map lookup for absent IDs, before renderer/resource access.
// This executable must never load assets: refuse preexisting state, never clear it.
class NativeAssetBoundary
{
public:
    NativeAssetBoundary()
    {
        REQUIRE(Bitmaps.GetNumberOfTexture() == 0);
        REQUIRE(Bitmaps.GetUsedTextureMemory() == 0);
    }

    ~NativeAssetBoundary()
    {
        CHECK(Bitmaps.GetNumberOfTexture() == 0);
        CHECK(Bitmaps.GetUsedTextureMemory() == 0);
    }

    NativeAssetBoundary(const NativeAssetBoundary&) = delete;
    NativeAssetBoundary& operator=(const NativeAssetBoundary&) = delete;
};
} // namespace SEASON3B
