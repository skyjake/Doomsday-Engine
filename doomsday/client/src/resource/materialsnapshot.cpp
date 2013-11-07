/** @file materialsnapshot.cpp Logical material state snapshot.
 *
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_defs.h"
#include "Lumobj" // Rend_LightmapTextureSpec()
#include "render/rend_halo.h" // Rend_HaloTextureSpec()
#include "render/rend_main.h" // detailFactor, detailScale, smoothTexAnim, etc...
#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"

#include "Material"
#include "Texture"
#include <de/Log>

#include "MaterialSnapshot"

namespace de {

struct Store
{
    bool opaque;                ///< @c true= this material is completely opaque.
    float glowStrength;
    Vector2i dimensions;        ///< Map space dimensions.
    blendmode_t shineBlendMode;
    Vector3f shineMinColor;     ///< Minimum ambient light level/color for shine.

    /// Textures used on each logical material texture unit.
    TextureVariant *textures[NUM_MATERIAL_TEXTURE_UNITS];

    /// Decoration configuration.
    MaterialSnapshotDecoration decorations[Material::max_decorations];

    /// Prepared render texture unit configuration. These map directly
    /// to the texture units supplied to the render lists module.
    GLTextureUnit units[NUM_TEXMAP_UNITS];

    Store() { initialize(); }

    void initialize()
    {
        dimensions     = Vector2i(0, 0);
        shineBlendMode = BM_NORMAL;
        shineMinColor  = Vector3f(0, 0, 0);
        opaque         = true;
        glowStrength   = 0;

        zap(textures);
        zap(decorations);
    }
};

DENG2_PIMPL(MaterialSnapshot)
{
    MaterialVariant *variant;
    Store stored;

    Instance(Public *i, MaterialVariant &variant)
        : Base(i)
        , variant(&variant)
    {}

    void takeSnapshot();
};

MaterialSnapshot::MaterialSnapshot(MaterialVariant &materialVariant)
    : d(new Instance(this, materialVariant))
{}

MaterialVariant &MaterialSnapshot::materialVariant() const
{
    return *d->variant;
}

Vector2i const &MaterialSnapshot::dimensions() const
{
    return d->stored.dimensions;
}

bool MaterialSnapshot::isOpaque() const
{
    return d->stored.opaque;
}

float MaterialSnapshot::glowStrength() const
{
    return d->stored.glowStrength;
}

blendmode_t MaterialSnapshot::shineBlendMode() const
{
    return d->stored.shineBlendMode;
}

Vector3f const &MaterialSnapshot::shineMinColor() const
{
    return d->stored.shineMinColor;
}

bool MaterialSnapshot::hasTexture(int index) const
{
    if(index < 0 || index >= NUM_MATERIAL_TEXTURE_UNITS) return false;
    return d->stored.textures[index] != 0;
}

Texture::Variant &MaterialSnapshot::texture(int index) const
{
    if(!hasTexture(index))
    {
        /// @throw UnknownUnitError Attempt to dereference with an invalid index.
        throw UnknownUnitError("MaterialSnapshot::texture", QString("Invalid texture #%1").arg(index));
    }
    return *d->stored.textures[index];
}

GLTextureUnit const &MaterialSnapshot::unit(int index) const
{
    if(index < 0 || index >= NUM_TEXMAP_UNITS)
    {
        /// @throw UnknownUnitError Attempt to obtain a reference to a unit with an invalid id.
        throw UnknownUnitError("MaterialSnapshot::unit", QString("Invalid unit #%1").arg(index));
    }
    return d->stored.units[index];
}

MaterialSnapshot::Decoration &MaterialSnapshot::decoration(int index) const
{
    if(index < 0 || index >= Material::max_decorations)
    {
        /// @throw UnknownDecorationError Attempt to obtain a reference to a decoration with an invalid index.
        throw UnknownDecorationError("MaterialSnapshot::decoration", QString("Invalid decoration #%1").arg(index));
    }
    return d->stored.decorations[index];
}

/**
 * Attempt to locate and prepare a flare texture. Somewhat more complicated than
 * it needs to be due to the fact there are two different selection methods.
 *
 * @param texture  Logical texture to prepare an variant of.
 * @param oldIdx  Old method of flare texture selection, by id.
 *
 * @return  @c 0= Use the automatic selection logic.
 */
static DGLuint prepareFlaremap(Texture *texture, int oldIdx)
{
    if(texture)
    {
        if(TextureVariant const *variant = texture->prepareVariant(Rend_HaloTextureSpec()))
        {
            return variant->glName();
        }
        // Dang...
    }
    else if(oldIdx > 0 && oldIdx < NUM_SYSFLARE_TEXTURES)
    {
        return GL_PrepareSysFlaremap(flaretexid_t(oldIdx - 1));
    }
    return 0; // Use the automatic selection logic.
}

/// @todo Implement more useful methods of interpolation. (What do we want/need here?)
void MaterialSnapshot::Instance::takeSnapshot()
{
#define LERP(start, end, pos) (end * pos + start * (1 - pos))

    Material *material = &variant->generalCase();
    Material::Layers const &layers = material->layers();
    Material::DetailLayer const *detailLayer = material->isDetailed()? &material->detailLayer() : 0;
    Material::ShineLayer const *shineLayer   =    material->isShiny()? &material->shineLayer()  : 0;

    TextureVariant *prepTextures[NUM_MATERIAL_TEXTURE_UNITS][2];
    zap(prepTextures);

    // Reinitialize the stored values.
    stored.initialize();

    /*
     * Ensure all resources needed to visualize this have been prepared. If
     * skymasked, we only need to update the primary tex unit (due to it being
     * visible when skymask debug drawing is enabled).
     */
    for(int i = 0; i < layers.count(); ++i)
    {
        MaterialAnimation::LayerState const &l = material->animation(variant->context()).layer(i);

        Material::Layer::Stage const *lsCur = layers[i]->stages()[l.stage];
        if(lsCur->texture)
        {
            prepTextures[i][0] =
                lsCur->texture->prepareVariant(*variant->spec().primarySpec);
        }

        // Smooth Texture Animation?
        if(smoothTexAnim && layers[i]->stageCount() > 1)
        {
            Material::Layer::Stage const *lsNext =
                layers[i]->stages()[(l.stage + 1) % layers[i]->stageCount()];

            if(lsNext->texture)
            {
                prepTextures[i][1] = lsNext->texture->prepareVariant(*variant->spec().primarySpec);
            }
        }
    }

    // Do we need to prepare detail texture(s)?
    if(!material->isSkyMasked() && material->isDetailed())
    {
        MaterialAnimation::LayerState const &l = material->animation(variant->context()).detailLayer();
        Material::DetailLayer::Stage const *lsCur = detailLayer->stages()[l.stage];

        if(lsCur->texture)
        {
            float const contrast = de::clamp(0.f, lsCur->strength, 1.f) * detailFactor /*Global strength multiplier*/;
            texturevariantspecification_t &dTexSpec =
                GL_DetailTextureSpec(contrast);

            prepTextures[MTU_DETAIL][0] =
                lsCur->texture->prepareVariant(dTexSpec);
        }

        // Smooth Texture Animation?
        if(smoothTexAnim && detailLayer->stageCount() > 1)
        {
            Material::DetailLayer::Stage const *lsNext = detailLayer->stages()[(l.stage + 1) % detailLayer->stageCount()];
            if(lsNext->texture)
            {
                float const contrast = de::clamp(0.f, lsNext->strength, 1.f) * detailFactor /*Global strength multiplier*/;
                texturevariantspecification_t &dTexSpec =
                    GL_DetailTextureSpec(contrast);

                prepTextures[MTU_DETAIL][1] =
                    lsNext->texture->prepareVariant(dTexSpec);
            }
        }
    }

    // Do we need to prepare a shiny texture (and possibly a mask)?
    if(!material->isSkyMasked() && material->isShiny())
    {
        MaterialAnimation::LayerState const &l = material->animation(variant->context()).shineLayer();
        Material::ShineLayer::Stage const *lsCur = shineLayer->stages()[l.stage];

        if(lsCur->texture)
        {
            prepTextures[MTU_REFLECTION][0] =
                lsCur->texture->prepareVariant(Rend_MapSurfaceShinyTextureSpec());
        }

        // We are only interested in a mask if we have a shiny texture.
        if(prepTextures[MTU_REFLECTION][0])
        {
            if(lsCur->maskTexture)
            {
                prepTextures[MTU_REFLECTION_MASK][0] =
                    lsCur->maskTexture->prepareVariant(Rend_MapSurfaceShinyMaskTextureSpec());
            }
        }
    }

    stored.dimensions = material->dimensions();

    stored.opaque = (prepTextures[MTU_PRIMARY][0] && !prepTextures[MTU_PRIMARY][0]->isMasked());

    if(stored.dimensions.x == 0 && stored.dimensions.y == 0) return;

    MaterialAnimation::LayerState const &l = material->animation(variant->context()).layer(0);
    Material::Layer::Stage const *lsCur  = layers[0]->stages()[l.stage];
    Material::Layer::Stage const *lsNext = layers[0]->stages()[(l.stage + 1) % layers[0]->stageCount()];

    // Glow strength is presently taken from layer #0.
    if(l.inter == 0)
    {
        stored.glowStrength = lsCur->glowStrength;
    }
    else // Interpolate.
    {
        stored.glowStrength = LERP(lsCur->glowStrength, lsNext->glowStrength, l.inter);
    }

    // Setup the primary texture unit.
    if(TextureVariant *tex = prepTextures[MTU_PRIMARY][0])
    {
        stored.textures[MTU_PRIMARY] = tex;
        Vector2f offset;
        if(l.inter == 0)
        {
            offset = lsCur->texOrigin;
        }
        else // Interpolate.
        {
            offset.x = LERP(lsCur->texOrigin.x, lsNext->texOrigin.x, l.inter);
            offset.y = LERP(lsCur->texOrigin.y, lsNext->texOrigin.y, l.inter);
        }

        stored.units[RTU_PRIMARY] =
            GLTextureUnit(*tex,
                          Vector2f(1.f / stored.dimensions.x,
                                   1.f / stored.dimensions.y),
                          offset);
    }

    // Setup the inter primary texture unit.
    if(TextureVariant *tex = prepTextures[MTU_PRIMARY][1])
    {
        // If fog is active, inter=0 is accepted as well. Otherwise
        // flickering may occur if the rendering passes don't match for
        // blended and unblended surfaces.
        if(!(!usingFog && l.inter == 0))
        {
            stored.units[RTU_INTER] =
                GLTextureUnit(*tex,
                              stored.units[RTU_PRIMARY].scale,
                              stored.units[RTU_PRIMARY].offset,
                              de::clamp(0.f, l.inter, 1.f));
        }
    }

    if(!material->isSkyMasked() && material->isDetailed())
    {
        MaterialAnimation::LayerState const &l = material->animation(variant->context()).detailLayer();
        Material::DetailLayer::Stage const *lsCur  = detailLayer->stages()[l.stage];
        Material::DetailLayer::Stage const *lsNext = detailLayer->stages()[(l.stage + 1) % detailLayer->stageCount()];

        // Setup the detail texture unit.
        if(TextureVariant *tex = prepTextures[MTU_DETAIL][0])
        {
            stored.textures[MTU_DETAIL] = tex;

            float scale;
            if(l.inter == 0)
            {
                scale = lsCur->scale;
            }
            else // Interpolate.
            {
                scale = LERP(lsCur->scale, lsNext->scale, l.inter);
            }
            // Apply the global scale factor.
            if(detailScale > .0001f)
            {
                scale *= detailScale;
            }

            stored.units[RTU_PRIMARY_DETAIL] =
                GLTextureUnit(*tex,
                              Vector2f(1.f / tex->generalCase().width()  * scale,
                                       1.f / tex->generalCase().height() * scale));
        }

        // Setup the inter detail texture unit.
        if(TextureVariant *tex = prepTextures[MTU_DETAIL][1])
        {
            // If fog is active, inter=0 is accepted as well. Otherwise
            // flickering may occur if the rendering passes don't match for
            // blended and unblended surfaces.
            if(!(!usingFog && l.inter == 0))
            {
                stored.units[RTU_INTER_DETAIL] =
                    GLTextureUnit(*tex,
                                  stored.units[RTU_PRIMARY_DETAIL].scale,
                                  stored.units[RTU_PRIMARY_DETAIL].offset,
                                  de::clamp(0.f, l.inter, 1.f));
            }
        }
    }

    if(!material->isSkyMasked() && material->isShiny())
    {
        MaterialAnimation::LayerState const &l = material->animation(variant->context()).shineLayer();
        Material::ShineLayer::Stage const *lsCur  = shineLayer->stages()[l.stage];
        Material::ShineLayer::Stage const *lsNext = shineLayer->stages()[(l.stage + 1) % shineLayer->stageCount()];

        // Setup the shine texture unit.
        if(TextureVariant *tex = prepTextures[MTU_REFLECTION][0])
        {
            stored.textures[MTU_REFLECTION] = tex;

            Vector3f minColor;
            for(int i = 0; i < 3; ++i)
            {
                if(l.inter == 0)
                {
                    minColor[i] = lsCur->minColor[i];
                }
                else // Interpolate.
                {
                    minColor[i] = LERP(lsCur->minColor[i], lsNext->minColor[i], l.inter);
                }
                minColor[i] = de::clamp(0.0f, minColor[i], 1.0f);
            }
            stored.shineMinColor = minColor;

            float shininess;
            if(l.inter == 0)
            {
                shininess = lsCur->shininess;
            }
            else // Interpolate.
            {
                shininess = LERP(lsCur->shininess, lsNext->shininess, l.inter);
            }

            stored.shineBlendMode = lsCur->blendMode;
            stored.units[RTU_REFLECTION] =
                GLTextureUnit(*tex, Vector2f(1, 1), Vector2f(0, 0),
                              de::clamp(0.0f, shininess, 1.0f));
        }

        // Setup the shine mask texture unit.
        if(prepTextures[MTU_REFLECTION][0])
        if(TextureVariant *tex = prepTextures[MTU_REFLECTION_MASK][0])
        {
            stored.textures[MTU_REFLECTION_MASK] = tex;
            stored.units[RTU_REFLECTION_MASK] =
                GLTextureUnit(*tex,
                              Vector2f(1.f / (stored.dimensions.x * tex->generalCase().width()),
                                       1.f / (stored.dimensions.y * tex->generalCase().height())),
                              stored.units[RTU_PRIMARY].offset);
        }
    }

    uint idx = 0;
    Material::Decorations const &decorations = material->decorations();
    for(Material::Decorations::const_iterator it = decorations.begin();
        it != decorations.end(); ++it, ++idx)
    {
        MaterialAnimation::DecorationState const &l = material->animation(variant->context()).decoration(idx);
        MaterialDecoration const *lDef = *it;
        MaterialDecoration::Stage const *lsCur  = lDef->stages()[l.stage];
        MaterialDecoration::Stage const *lsNext = lDef->stages()[(l.stage + 1) % lDef->stageCount()];

        MaterialSnapshot::Decoration &decor = stored.decorations[idx];

        if(l.inter == 0)
        {
            decor.pos            = lsCur->pos;
            decor.elevation      = lsCur->elevation;
            decor.radius         = lsCur->radius;
            decor.flareSize      = lsCur->haloRadius;
            decor.lightLevels[0] = lsCur->lightLevels.min;
            decor.lightLevels[1] = lsCur->lightLevels.max;
            decor.color          = lsCur->color;
        }
        else // Interpolate.
        {
            decor.pos.x          = LERP(lsCur->pos.x, lsNext->pos.x, l.inter);
            decor.pos.y          = LERP(lsCur->pos.y, lsNext->pos.y, l.inter);
            decor.elevation      = LERP(lsCur->elevation, lsNext->elevation, l.inter);
            decor.radius         = LERP(lsCur->radius, lsNext->radius, l.inter);
            decor.flareSize      = LERP(lsCur->haloRadius, lsNext->haloRadius, l.inter);
            decor.lightLevels[0] = LERP(lsCur->lightLevels.min, lsNext->lightLevels.min, l.inter);
            decor.lightLevels[1] = LERP(lsCur->lightLevels.max, lsNext->lightLevels.max, l.inter);

            for(int c = 0; c < 3; ++c)
            {
                decor.color[c] = LERP(lsCur->color[c], lsNext->color[c], l.inter);
            }
        }

        decor.tex      = lsCur->sides;
        decor.ceilTex  = lsCur->up;
        decor.floorTex = lsCur->down;

        decor.flareTex = prepareFlaremap(lsCur->flare, lsCur->sysFlareIdx);
    }

#undef LERP
}

void MaterialSnapshot::update()
{
    d->takeSnapshot();
}

} // namespace de
