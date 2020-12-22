/** @file materialanimator.cpp  Animator for a draw-context Material variant.
 *
 * @authors Copyright © 2011-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "resource/materialanimator.h"

#include <array>
#include <de/log.h>
#include <doomsday/res/textures.h>
#include <doomsday/world/detailtexturemateriallayer.h>
#include <doomsday/world/shinetexturemateriallayer.h>
#include <doomsday/world/texturemateriallayer.h>

#include "clientapp.h"
#include "client/cl_def.h"      // playback / clientPaused
#include "resource/materialvariantspec.h"
#include "gl/gl_texmanager.h"
#include "render/r_main.h"      // frameCount, frameTimePos
#include "render/rend_main.h"
#include "render/rend_halo.h"
#include "render/viewports.h"
#include "resource/clientresources.h"
#include "resource/lightmaterialdecoration.h"

using namespace de;

static inline ClientResources &resSys()
{
    return ClientApp::resources();
}

/**
 * Attempt to locate and prepare a flare texture. Somewhat more complicated than
 * it needs to be due to the fact there are two different selection methods.
 *
 * @param texture  Logical texture to prepare an variant of.
 * @param oldIdx   Old method of flare texture selection, by id.
 *
 * @return  @c 0= Use the automatic selection logic.
 */
static DGLuint prepareFlaremap(ClientTexture *texture, int oldIdx)
{
    if (texture)
    {
        if (const TextureVariant *variant = texture->prepareVariant(Rend_HaloTextureSpec()))
        {
            return variant->glName();
        }
        // Dang...
    }
    else if (oldIdx > 0 && oldIdx < NUM_SYSFLARE_TEXTURES)
    {
        return GL_PrepareSysFlaremap(flaretexid_t(oldIdx - 1));
    }
    return 0; // Use the automatic selection logic.
}

DE_PIMPL_NOREF(MaterialAnimator::Decoration)
{
    MaterialDecoration *matDecor = nullptr;

    int stage   = 0;  ///< Animation stage else @c -1 => decoration not in use.
    short tics  = 0;  ///< Remaining (sharp) tics in the current stage.
    float inter = 0;  ///< Intermark from the current stage to the next [0..1].

    // State snapshot:
    Vec2f origin;           ///< Relative position in material space.
    Vec3f color;            ///< Light color.
    float elevation      = 0;  ///< Distance from the surface.
    float radius         = 0;  ///< Dynamic light radius (-1 = no light).
    float lightLevels[2];      ///< Fade by sector lightlevel.

    float flareSize      = 0;  ///< Halo radius (zero = no halo).
    DGLuint flareTex     = 0;

    ClientTexture *tex      = nullptr;
    ClientTexture *ceilTex  = nullptr;
    ClientTexture *floorTex = nullptr;

    Impl() { de::zap(lightLevels); }

    bool useInterpolation() const
    {
        DE_ASSERT(matDecor);
        if (const auto *light = maybeAs<LightMaterialDecoration>(matDecor))
        {
            return light->useInterpolation();
        }
        return true;
    }
};

MaterialAnimator::Decoration::Decoration(MaterialDecoration &decor)
    : d(new Impl)
{
    d->matDecor = &decor;
}

MaterialDecoration &MaterialAnimator::Decoration::decor() const
{
    DE_ASSERT(d->matDecor);
    return *d->matDecor;
}

Vec2f MaterialAnimator::Decoration::origin() const
{
    return d->origin;
}

Vec3f MaterialAnimator::Decoration::color() const
{
    return d->color;
}

float MaterialAnimator::Decoration::elevation() const
{
    return d->elevation;
}

float MaterialAnimator::Decoration::radius() const
{
    return d->radius;
}

void MaterialAnimator::Decoration::lightLevels(float &min, float &max) const
{
    min = d->lightLevels[0];
    max = d->lightLevels[1];
}

float MaterialAnimator::Decoration::flareSize() const
{
    return d->flareSize;
}

DGLuint MaterialAnimator::Decoration::flareTex() const
{
    return d->flareTex;
}

ClientTexture *MaterialAnimator::Decoration::tex() const
{
    return d->tex;
}

ClientTexture *MaterialAnimator::Decoration::ceilTex() const
{
    return d->ceilTex;
}

ClientTexture *MaterialAnimator::Decoration::floorTex() const
{
    return d->floorTex;
}

void MaterialAnimator::Decoration::rewind()
{
    d->stage = 0;
    d->tics  = decor().stage(0).tics;
    d->inter = 0;
}

bool MaterialAnimator::Decoration::animate()
{
    if (decor().isAnimated())
    {
        d->inter = 0;

        if (DD_IsSharpTick() && d->tics-- <= 0)
        {
            // Advance to next stage.
            if (++d->stage == decor().stageCount())
            {
                // Loop back to the beginning.
                d->stage = 0;
            }

            const MaterialDecoration::Stage &stage = decor().stage(d->stage);
            if (stage.variance != 0)
                d->tics = stage.tics * (1 - stage.variance * RNG_RandFloat());
            else
                d->tics = stage.tics;

            return true;
        }

        if (d->useInterpolation())
        {
            const MaterialDecoration::Stage &stage = decor().stage(d->stage);
            d->inter = 1.f - d->tics / float( stage.tics );
        }
    }
    return false;
}

void MaterialAnimator::Decoration::update()
{
    if (auto *lightDecor = maybeAs<LightMaterialDecoration>(decor()))
    {
        const LightMaterialDecoration::AnimationStage &stage = lightDecor->stage(d->stage);
        const LightMaterialDecoration::AnimationStage &next  = lightDecor->stage(d->stage + 1);

        d->origin         = de::lerp(stage.origin,          next.origin,          d->inter);
        d->elevation      = de::lerp(stage.elevation,       next.elevation,       d->inter);
        d->radius         = de::lerp(stage.radius,          next.radius,          d->inter);
        d->flareSize      = de::lerp(stage.haloRadius,      next.haloRadius,      d->inter);
        d->lightLevels[0] = de::lerp(stage.lightLevels.min, next.lightLevels.min, d->inter);
        d->lightLevels[1] = de::lerp(stage.lightLevels.max, next.lightLevels.max, d->inter);
        d->color          = de::lerp(stage.color,           next.color,           d->inter);
        d->tex            = stage.tex;
        d->ceilTex        = stage.ceilTex;
        d->floorTex       = stage.floorTex;

        d->flareTex       = prepareFlaremap(stage.flareTex, stage.sysFlareIdx);
    }
}

void MaterialAnimator::Decoration::reset()
{
    d->origin    = Vec2i(0, 0);
    d->color     = Vec3f(0.0f);
    d->elevation = 0;
    d->radius    = 0;
    de::zap(d->lightLevels);
    d->flareSize = 0;
    d->flareTex  = 0;
    d->tex       = nullptr;
    d->ceilTex   = nullptr;
    d->floorTex  = nullptr;
}

// ------------------------------------------------------------------------------------

enum StagePropertyId { TextureProperty, MaskTextureProperty };

/**
 * Returns the Texture in effect for the given animation stage, if any.
 */
static ClientTexture *findTextureForAnimationStage(const world::TextureMaterialLayer::AnimationStage &stage,
                                                   /*const String &propertyName = "texture"*/
                                                   StagePropertyId prop = TextureProperty)
{
    try
    {
        return static_cast<ClientTexture *>
                (&res::Textures::get().texture(prop == TextureProperty? stage.texture
                                                                      : stage.maskTexture));
    }
    catch (res::TextureManifest::MissingTextureError &)
    {}
    catch (Resources::MissingResourceManifestError &)
    {}
    return nullptr;
}

DE_PIMPL(MaterialAnimator)
{
    ClientMaterial *material = nullptr;         ///< Material to animate (not owned).
    const MaterialVariantSpec *spec = nullptr;  ///< Variant specification.

    /// Current state of a layer animation.
    struct LayerState
    {
        int stage     = 0;   ///< Animation stage else @c -1 => layer not in use.
        int nextStage = 0;
        short tics    = 0;   ///< Remaining (sharp) tics in the current stage.
        float inter   = 0.f; ///< Intermark from the current stage to the next [0..1].

        List<ClientTexture *> stageTextures;

        String synopsis() const {
            return Stringf("stage: %i tics: %i inter: %f", stage, tics, inter);
        }
    };
    /// Layer animation states.
    List<LayerState *> layers;

    /**
     * Cached animation state snapshot.
     *
     * Stage-animated or interpolated material property values are cached in a
     * per-frame data store to avoid repeat calculation. All other values that do
     * not change should be obtained directly from the Material.
     */
    struct Snapshot
    {
        bool opaque;
        float glowStrength;
        Vec2ui dimensions;

        blendmode_t shineBlendMode;
        Vec3f shineMinColor;

        /// Textures for each logical texture unit.
        std::array<TextureVariant *, MaterialAnimator::NUM_TEXTUREUNITS> textures;

        /// Prepared GL texture unit configurations. These are mapped directly by
        /// the renderer's DrawLists module.
        std::array<GLTextureUnit, MaterialAnimator::NUM_TEXTUREUNITS> units;

        Snapshot() { clear(); }

        void clear()
        {
            dimensions     = Vec2ui(0, 0);
            shineBlendMode = BM_NORMAL;
            shineMinColor  = Vec3f(0.0f);
            opaque         = true;
            glowStrength   = 0;

            textures.fill(nullptr);
            units.fill(GLTextureUnit());
        }
    };
    std::unique_ptr<Snapshot> snapshot;
    int lastSnapshotUpdate = -1;  ///< Frame count of last snapshot update.

    /// Animated material decorations.
    List<Decoration *> decorations;

    Impl(Public *i) : Base(i) {}
    ~Impl()
    {
        clearLayers();
        clearDecorations();
    }

    void clearLayers()
    {
        deleteAll(layers); layers.clear();
    }

    void initLayers()
    {
        clearLayers();
        for (int i = 0; i < self().material().layerCount(); ++i)
        {
            layers << new LayerState;
        }
        fetchStageTextures();
    }

    void fetchStageTextures()
    {
        for (duint i = 0; i < layers.size(); ++i)
        {
            layers[i]->stageTextures.clear();

            if (const world::TextureMaterialLayer *layer = maybeAs<world::TextureMaterialLayer>(self().material().layer(i)))
            {
                for (int k = 0; k < layer->stageCount(); ++k)
                {
                    layers[i]->stageTextures.append(findTextureForAnimationStage(layer->stage(k)));
                }
            }
        }
    }

    void clearDecorations()
    {
        deleteAll(decorations); decorations.clear();
    }

    void initDecorations()
    {
        clearDecorations();
        self().material().forAllDecorations([this] (MaterialDecoration &decor)
        {
            decorations << new Decoration(decor);
            return LoopContinue;
        });
    }

    void attachMissingSnapshot()
    {
        // Already been here?
        if (snapshot) return;

        snapshot.reset(new Snapshot);
        lastSnapshotUpdate = -1; // Force an update.
    }

    /// @todo Implement more useful methods of interpolation. (What do we want/need here?)
    void updateSnapshotIfNeeded(bool force = false)
    {
        attachMissingSnapshot();

        // Time to update?
        if (!force && lastSnapshotUpdate == R_FrameCount())
            return;

        lastSnapshotUpdate = R_FrameCount();

        snapshot->clear();
        for (Decoration *decor : decorations) decor->reset();

        /*
         * Ensure all resources needed to visualize this have been prepared. If
         * skymasked, we only need to update the primary tex unit (due to it being
         * visible when skymask debug drawing is enabled).
         */
        if (!material->isSkyMasked() || ::devRendSkyMode)
        {
            int texLayerIndex = 0;
            for (int i = 0; i < material->layerCount(); ++i)
            {
                const world::MaterialLayer &layer = material->layer(i);
                const LayerState &ls       = *layers[i];

                if (const auto *detailLayer = maybeAs<world::DetailTextureMaterialLayer>(layer))
                {
                    const auto &stage = detailLayer->stage(ls.stage).as<world::DetailTextureMaterialLayer::AnimationStage>();
                    const auto &next  = detailLayer->stage(ls.nextStage).as<world::DetailTextureMaterialLayer::AnimationStage>();

                    if (ClientTexture *tex = ls.stageTextures[ls.stage])
                    {
                        const float contrast = de::clamp(0.f, stage.strength, 1.f) * ::detailFactor /*Global strength multiplier*/;
                        snapshot->textures[TU_DETAIL] = tex->prepareVariant(resSys().detailTextureSpec(contrast));
                    }
                    // Smooth Texture Animation?
                    if (::smoothTexAnim && &stage != &next)
                    {
                        if (ClientTexture *tex = ls.stageTextures[ls.nextStage])
                        {
                            const float contrast = de::clamp(0.f, next.strength, 1.f) * ::detailFactor /*Global strength multiplier*/;
                            snapshot->textures[TU_DETAIL_INTER] = tex->prepareVariant(resSys().detailTextureSpec(contrast));
                        }
                    }
                }
                else if (is<world::ShineTextureMaterialLayer>(layer))
                {
                    const world::TextureMaterialLayer::AnimationStage &stage = layer.as<world::TextureMaterialLayer>().stage(ls.stage);
                    //const TextureMaterialLayer::AnimationStage &next  = layer.stage(l.stage + 1);

                    if (ClientTexture *tex = ls.stageTextures[ls.stage])
                    {
                        snapshot->textures[TU_SHINE] = tex->prepareVariant(Rend_MapSurfaceShinyTextureSpec());

                        // We are only interested in a mask if we have a shiny texture.
                        if (ClientTexture *maskTex = findTextureForAnimationStage(stage, MaskTextureProperty))
                        {
                            snapshot->textures[TU_SHINE_MASK] = maskTex->prepareVariant(Rend_MapSurfaceShinyMaskTextureSpec());
                        }
                    }
                }
                else if (const auto *texLayer = maybeAs<world::TextureMaterialLayer>(layer))
                {
                    const world::TextureMaterialLayer::AnimationStage &stage = texLayer->stage(ls.stage);
                    const world::TextureMaterialLayer::AnimationStage &next  = texLayer->stage(ls.nextStage);

                    if (ClientTexture *tex = ls.stageTextures[ls.stage])
                    {
                        snapshot->textures[TU_LAYER0 + texLayerIndex] = tex->prepareVariant(*spec->primarySpec);
                    }
                    // Smooth Texture Animation?
                    if (::smoothTexAnim && &stage != &next)
                    {
                        if (ClientTexture *tex = ls.stageTextures[ls.nextStage])
                        {
                            snapshot->textures[TU_LAYER0_INTER + texLayerIndex] = tex->prepareVariant(*spec->primarySpec);
                        }
                    }

                    texLayerIndex += 1;
                }
            }
        }

        snapshot->dimensions = material->dimensions();
        snapshot->opaque     = (snapshot->textures[TU_LAYER0] && !snapshot->textures[TU_LAYER0]->isMasked());

        if (snapshot->dimensions == Vec2ui()) return;

        if (material->isSkyMasked() && !::devRendSkyMode) return;

        int texLayerIndex = 0;
        for (int i = 0; i < material->layerCount(); ++i)
        {
            const world::MaterialLayer &layer = material->layer(i);
            const LayerState &ls = *layers[i];

            if (const auto *detailLayer = maybeAs<world::DetailTextureMaterialLayer>(layer))
            {
                if (TextureVariant *tex = snapshot->textures[TU_DETAIL])
                {
                    const auto &stage = detailLayer->stage(ls.stage).as<world::DetailTextureMaterialLayer::AnimationStage>();
                    const auto &next  = detailLayer->stage(ls.nextStage).as<world::DetailTextureMaterialLayer::AnimationStage>();

                    float scale = de::lerp(stage.scale, next.scale, ls.inter);
                    if (::detailScale > .0001f) scale *= ::detailScale; // Global scale factor.

                    snapshot->units[TU_DETAIL] =
                            GLTextureUnit(*tex, Vec2f(1, 1) / tex->base().dimensions() * scale);

                    // Setup the inter detail texture unit.
                    if (TextureVariant *tex = snapshot->textures[TU_DETAIL_INTER])
                    {
                        // If fog is active, inter=0 is accepted as well. Otherwise
                        // flickering may occur if the rendering passes don't match for
                        // blended and unblended surfaces.
                        if (!(!fogParams.usingFog && ls.inter == 0))
                        {
                            snapshot->units[TU_DETAIL_INTER] =
                                GLTextureUnit(*tex,
                                              snapshot->units[TU_DETAIL].scale,
                                              snapshot->units[TU_DETAIL].offset,
                                              de::clamp(0.f, ls.inter, 1.f));
                        }
                    }
                }
            }
            else if (is<world::ShineTextureMaterialLayer>(layer))
            {
                if (TextureVariant *tex = snapshot->textures[TU_SHINE])
                {
                    const auto &stage = layer.stage(ls.stage)    .as<world::ShineTextureMaterialLayer::AnimationStage>();
                    const auto &next  = layer.stage(ls.nextStage).as<world::ShineTextureMaterialLayer::AnimationStage>();

                    const Vec2f origin   = de::lerp(stage.origin,   next.origin,   ls.inter);
                    const Vec3f minColor = de::lerp(stage.minColor, next.minColor, ls.inter);
                    const float opacity  = de::lerp(stage.opacity,  next.opacity,  ls.inter);

                    snapshot->shineBlendMode = stage.blendMode;
                    snapshot->shineMinColor  = minColor.min(Vec3f(1)).max(Vec3f(0.0f));

                    snapshot->units[TU_SHINE] = GLTextureUnit(*tex, Vec2f(1, 1), origin, de::clamp(0.0f, opacity, 1.0f));

                    // Setup the shine mask texture unit.
                    if (TextureVariant *maskTex = snapshot->textures[TU_SHINE_MASK])
                    {
                        snapshot->units[TU_SHINE_MASK] =
                            GLTextureUnit(*maskTex, Vec2f(1, 1) / (snapshot->dimensions * maskTex->base().dimensions()),
                                          snapshot->units[TU_LAYER0].offset);
                    }
                }
            }
            else if (const auto *texLayer = maybeAs<world::TextureMaterialLayer>(layer))
            {
                if (TextureVariant *tex = snapshot->textures[TU_LAYER0 + texLayerIndex])
                {
                    const world::TextureMaterialLayer::AnimationStage &stage = texLayer->stage(ls.stage);
                    const world::TextureMaterialLayer::AnimationStage &next  = texLayer->stage(ls.nextStage);

                    const Vec2f scale   = Vec2f(1, 1) / snapshot->dimensions;
                    const Vec2f origin  = de::lerp(stage.origin,  next.origin,  ls.inter);
                    const float opacity = de::lerp(stage.opacity, next.opacity, ls.inter);

                    snapshot->units[TU_LAYER0 + texLayerIndex] = GLTextureUnit(*tex, scale, origin, de::clamp(0.0f, opacity, 1.0f));

                    // Glow strength is taken from texture layer #0.
                    if (texLayerIndex == 0)
                    {
                        snapshot->glowStrength = de::lerp(stage.glowStrength, next.glowStrength, ls.inter);
                    }

                    // Setup the inter texture unit.
                    if (TextureVariant *tex = snapshot->textures[TU_LAYER0_INTER + texLayerIndex])
                    {
                        // If fog is active, inter=0 is accepted as well. Otherwise
                        // flickering may occur if the rendering passes don't match for
                        // blended and unblended surfaces.
                        if (!(!fogParams.usingFog && ls.inter == 0))
                        {
                            snapshot->units[TU_LAYER0_INTER + texLayerIndex] =
                                GLTextureUnit(*tex,
                                              snapshot->units[TU_LAYER0 + texLayerIndex].scale,
                                              snapshot->units[TU_LAYER0 + texLayerIndex].offset,
                                              de::clamp(0.f, ls.inter, 1.f));
                        }
                    }

                    texLayerIndex += 1;
                }
            }
        }

        if (!material->isSkyMasked())
        for (Decoration *decor : decorations)
        {
            decor->update();
        }
    }

    void rewindLayer(LayerState &ls, const world::MaterialLayer &layer)
    {
        ls.stage = 0;
        ls.nextStage = layer.nextStageIndex(0);
        ls.tics  = layer.stage(0).tics;
        ls.inter = 0;
    }

    void animateLayer(LayerState &ls, const world::MaterialLayer &layer)
    {
        if (DD_IsSharpTick() && ls.tics-- <= 0)
        {
            // Advance to next stage.
            if (++ls.stage == layer.stageCount())
            {
                // Loop back to the beginning.
                ls.stage = 0;
            }
            ls.nextStage = layer.nextStageIndex(ls.stage);
            ls.inter = 0;

            const world::MaterialLayer::Stage &stage = layer.stage(ls.stage);
            if (stage.variance != 0)
                ls.tics = stage.tics * (1 - stage.variance * RNG_RandFloat());
            else
                ls.tics = stage.tics;
        }
        else
        {
            const world::MaterialLayer::Stage &stage = layer.stage(ls.stage);
            ls.inter = 1.f - ls.tics / float( stage.tics );
        }
    }
};

MaterialAnimator::MaterialAnimator(ClientMaterial &material, const MaterialVariantSpec &spec)
    : d(new Impl(this))
{
    d->material = &material;
    d->spec     = &spec;
    d->initLayers();
    d->initDecorations();

    // Prepare for animation.
    rewind();
}

ClientMaterial &MaterialAnimator::material() const
{
    DE_ASSERT(d->material);
    return *d->material;
}

const MaterialVariantSpec &MaterialAnimator::variantSpec() const
{
    DE_ASSERT(d->spec);
    return *d->spec;
}

bool MaterialAnimator::isPaused() const
{
    // Depending on the usage context, the animation should only progress
    // when the game is not paused.
    MaterialContextId context = variantSpec().contextId;
    return (clientPaused && (context == MapSurfaceContext ||
                             context == SpriteContext     ||
                             context == ModelSkinContext  ||
                             context == PSpriteContext    ||
                             context == SkySphereContext));
}

void MaterialAnimator::animate(timespan_t /*ticLength*/)
{
    // Animation ceases once the material is no longer valid.
    if (!material().isValid()) return;

    // Animation will only progress when not paused.
    if (isPaused()) return;

    /*
     * Animate layers:
     */
    for (int i = 0; i < material().layerCount(); ++i)
    {
        const world::MaterialLayer &layer = material().layer(i);
        if (layer.isAnimated() && is<world::TextureMaterialLayer>(layer))
        {
            d->animateLayer(*d->layers[i], layer);
        }
    }

    /*
     * Animate decorations:
     */
    bool decorationStageChanged = false;
    for (Decoration *decor : d->decorations)
    {
        if (decor->animate())
        {
            decorationStageChanged = true;
        }
    }

    if (decorationStageChanged)
    {
        // Notify interested parties.
        DE_NOTIFY_VAR(DecorationStageChange, i) i->materialAnimatorDecorationStageChanged(*this);
    }
}

void MaterialAnimator::rewind()
{
    // Animation ceases once the material is no longer valid.
    if (!material().isValid()) return;

    for (int i = 0; i < material().layerCount(); ++i)
    {
        d->rewindLayer(*d->layers[i], material().layer(i));
    }

    for (int i = 0; i < material().decorationCount(); ++i)
    {
        d->decorations[i]->rewind();
    }
}

void MaterialAnimator::prepare(bool fullUpdate)
{
    d->updateSnapshotIfNeeded(fullUpdate);
}

void MaterialAnimator::cacheAssets()
{
    prepare(true);
    if (material().isSkyMasked() && !::devRendSkyMode) return;

    for (int i = 0; i < material().layerCount(); ++i)
    {
        if (const world::TextureMaterialLayer *layer = maybeAs<world::TextureMaterialLayer>(material().layer(i)))
        {
            for (int k = 0; k < layer->stageCount(); ++k)
            {
                const auto &stage = layer->stage(k);

                if (ClientTexture *tex = d->layers.at(i)->stageTextures.at(k))
                {
                    if (is<world::DetailTextureMaterialLayer>(layer))
                    {
                        const auto &detailStage = stage.as<world::DetailTextureMaterialLayer::AnimationStage>();
                        const float contrast = de::clamp(0.f, detailStage.strength, 1.f) * detailFactor /*Global strength multiplier*/;
                        tex->prepareVariant(resSys().detailTextureSpec(contrast));
                    }
                    else if (is<world::ShineTextureMaterialLayer>(layer))
                    {
                        tex->prepareVariant(Rend_MapSurfaceShinyTextureSpec());
                        if (ClientTexture *maskTex = findTextureForAnimationStage(stage, MaskTextureProperty))
                        {
                            maskTex->prepareVariant(Rend_MapSurfaceShinyMaskTextureSpec());
                        }
                    }
                    else
                    {
                        tex->prepareVariant(*variantSpec().primarySpec);
                    }
                }
            }
        }
    }
}

bool MaterialAnimator::isOpaque() const
{
    d->updateSnapshotIfNeeded();
    return d->snapshot->opaque;
}

const Vec2ui &MaterialAnimator::dimensions() const
{
    d->updateSnapshotIfNeeded();
    return d->snapshot->dimensions;
}

float MaterialAnimator::glowStrength() const
{
    d->updateSnapshotIfNeeded();
    return d->snapshot->glowStrength;
}

blendmode_t MaterialAnimator::shineBlendMode() const
{
    d->updateSnapshotIfNeeded();
    return d->snapshot->shineBlendMode;
}

const Vec3f &MaterialAnimator::shineMinColor() const
{
    d->updateSnapshotIfNeeded();
    return d->snapshot->shineMinColor;
}

GLTextureUnit &MaterialAnimator::texUnit(int unitIndex) const
{
    d->updateSnapshotIfNeeded();
    if (unitIndex >= 0 && unitIndex < NUM_TEXTUREUNITS) return d->snapshot->units[unitIndex];
    /// @throw MissingTextureUnitError  Invalid GL-texture unit reference.
    throw MissingTextureUnitError("MaterialAnimator::glTextureUnit", "Unknown GL texture unit #" + String::asText(unitIndex));
}

MaterialAnimator::Decoration &MaterialAnimator::decoration(int decorIndex) const
{
    d->updateSnapshotIfNeeded();
    if (decorIndex >= 0 && decorIndex < d->decorations.count()) return *d->decorations[decorIndex];
    /// @throw MissingDecorationError  Invalid decoration reference.
    throw MissingDecorationError("MaterialAnimator::decoration", "Unknown decoration #" + String::asText(decorIndex));
}
