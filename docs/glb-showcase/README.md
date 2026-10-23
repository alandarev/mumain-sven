# GLB monster showcase: four AI-generated HD monsters

This branch is the GLB monster model branch (`docs/glb-models.md`) plus four
HD monster models and their entries in `Data/Models/ModelFiles.json`. Nothing
else differs. Each model replaces its BMD's bones and meshes. The BMD's
actions (key counts, play speed, loop and lock flags) still drive the model,
so game timing is unchanged.

**These models are AI-generated.** Image and 3D generators made their shape
and colour. Rigging, skinning, LODs, texture baking and cleanup were done
with a local pipeline that is not part of this branch. Each model keeps its
original's skeleton, so the game's bone-based effects (Yeti breath, wolf eye
glow, dragon fire) still attach where they did.

## The models

| Monster | File | Size | SHA-256 |
|---|---|---|---|
| Elite Yeti (`MODEL_ELITE_YETI`, Devias) | `src/bin/Data/Monster/Monster14.glb` | 11.1 MiB | `be027ec34578b682eca97013fac29b931fa375123c8cd29b43b8a219d81fb3ac` |
| Budge Dragon (`MODEL_BUDGE_DRAGON`, Lorencia) | `src/bin/Data/Monster/Monster03.glb` | 9.5 MiB | `5dd6a7dd13fc0c5fd52c63cd2152f850cf34f0b5d115be78f5d0902f548f7756` |
| Bloody Wolf (`MODEL_BLOODY_WOLF`, Tarkan) | `src/bin/Data/Monster/Monster44.glb` | 13.8 MiB | `be44518a7cedd8a18b969765a9b2643fd36689e7fe239e493837b151fcfeb9d0` |
| Vepar (`MODEL_VEPAR`, Atlans) | `src/bin/Data/Monster/Monster35.glb` | 10.3 MiB | `0ae257fa7ab815c4d312589f9e7c4fd73cf7b47e9e6553e412057d4236cea1d7` |

Every file follows the loader's rules (`docs/glb-models.md`):
- one skin carrying the BMD's bones by name;
- seven clips `action00`–`action06`;
- LOD 0, 1 and 2;
- base colour and normal map images at LOD 0: 2048 × 2048 (1024 × 1024 for
  the Vepar).

The Vepar has a second, see-through material for its fins and crests.

### Generators per model

| Monster | Concept (image) | Shape (3D) | Colour | Detail normals |
|---|---|---|---|---|
| Elite Yeti | Qwen-Image 2.1 edit of the original's four lit views; refined with SeedVR2 7B | Hunyuan3D-Omni, the original mesh as point control; the head redone by a second Hunyuan3D-Omni pass | Concept projection, filled with Hunyuan3D-Paint 2.1 | Marigold v2 normals, plus ambient occlusion |
| Budge Dragon | Qwen-Image 2.1 edit, as the Yeti | Hunyuan3D-Omni, point control (no head pass) | Concept projection, filled with Hunyuan3D-Paint 2.1 | Marigold v2 normals, plus ambient occlusion (same stage sequence as the Yeti; not recorded separately for this model) |
| Bloody Wolf | Qwen-Image 2.1 edit, a "reimagined" level | TRELLIS.2 from the concept's side view, mesh repair | Concept projection with per-part masks, filled with Hunyuan3D-Paint 2.1 | Marigold v2 normals, plus ambient occlusion (same stage sequence as the Yeti; not recorded separately for this model) |
| Vepar | OpenAI image model (via Codex): a reimagined four-view colour sheet | Hunyuan 3D 3.1 multi-view (hosted, 3D AI Studio), from the keyed views; textured there | Hunyuan 3D 3.1's own texture, transferred onto the game mesh | Baked from the generated high-resolution mesh (no image model) |

The rest was done locally and without generative models:
- the move into MU's model space;
- remeshing to the LOD triangle budgets;
- UV atlases, then baking the base colour and normal maps;
- four-weight skinning transferred from the original mesh;
- the seven actions, and the Yeti's face fit and smoothing;
- a texture-only contrast pass tuned against in-game captures;
- for the Vepar, the see-through fin material.

The generator credits come from the pipeline's records of each accepted
model. The two "not recorded separately" cells are an inference from the
order of the pipeline's stages, which all three ComfyUI-built models
went through.

### Changes made for publication

The four files are the accepted builds with only their glTF JSON header
rewritten:
- `asset.generator` is set to a neutral text;
- build notes and local paths in `asset.extras` are removed;
- image, material and scene names are neutral (`elite_yeti_lod0`, …).

The binary chunk (geometry, skin, clips and embedded images) is
byte-identical to the accepted build:

| File | SHA-256 of the binary chunk |
|---|---|
| `Monster14.glb` | `5d4849495e648063b1dc4ae8a33b2be7427f3a9a07b5dd88c8456cb16239d969` |
| `Monster03.glb` | `8dd8bb25b4199eeba8510ac5f771fad7e620dfde1e8a0bd59dd4c8594c3753f1` |
| `Monster44.glb` | `ea3f9c7f752243e5ad8ba53bde487f97e0a93a79784458e324e00ab42885b752` |
| `Monster35.glb` | `1fcf0c750dfcbd3c0eb6834471806e552b078eb7167ecba226344a2c8214acbe` |

## Enabling them

- **This branch:** build and run the client as usual. The shipped table names
  the four files, so the log shows
  `Model MODEL_ELITE_YETI is drawn from Data/Monster/Monster14.glb` (and the
  same for the other three).
- **The GLB model branch without these files:** copy the four `.glb` files and
  `src/bin/Data/Models/ModelFiles.json` into the game's `Data/` (`Data/Monster/`,
  `Data/Models/`), or into a directory named by `MU_DATA_OVERLAY`:

  ```sh
  mkdir -p /tmp/overlay/Data/Monster /tmp/overlay/Data/Models
  cp src/bin/Data/Monster/Monster{14,03,44,35}.glb /tmp/overlay/Data/Monster/
  cp src/bin/Data/Models/ModelFiles.json /tmp/overlay/Data/Models/
  MU_DATA_OVERLAY=/tmp/overlay ./Main
  ```

To compare with the originals, run with an overlay whose
`Data/Models/ModelFiles.json` is `{"formatVersion": 1, "models": []}`: every
monster is then drawn from its BMD.

## Screenshots

These are in-game captures of this branch on Linux (SDL GPU, Vulkan) at
2560 × 1440, cropped around the monster. "Classic" and "Enhanced" are the
client's two looks (`$look classic`, `$look enhanced`). Normal maps are drawn
only in Enhanced.

| Monster | Classic | Enhanced |
|---|---|---|
| Elite Yeti | ![idle](elite-yeti-classic-idle.jpg) ![death](elite-yeti-classic-death.jpg) | ![idle](elite-yeti-enhanced-idle.jpg) ![walk](elite-yeti-enhanced-walk.jpg) ![attack](elite-yeti-enhanced-attack.jpg) |
| Budge Dragon | ![idle](budge-dragon-classic-idle.jpg) ![attack](budge-dragon-classic-attack.jpg) | ![idle](budge-dragon-enhanced-idle.jpg) ![walk](budge-dragon-enhanced-walk.jpg) |
| Bloody Wolf | ![idle](bloody-wolf-classic-idle.jpg) ![attack](bloody-wolf-classic-attack.jpg) | ![idle](bloody-wolf-enhanced-idle.jpg) ![walk](bloody-wolf-enhanced-walk.jpg) ![death](bloody-wolf-enhanced-death.jpg) |
| Vepar | ![idle](vepar-classic-idle.jpg) ![walk](vepar-classic-walk.jpg) | ![idle](vepar-enhanced-idle.jpg) ![attack](vepar-enhanced-attack.jpg) ![death](vepar-enhanced-death.jpg) |

The idle pairs were taken on a frozen world (the same frame in both looks).
The walk, attack and death shots are of live monsters (the attack ones just
after their hit landed on the player).
