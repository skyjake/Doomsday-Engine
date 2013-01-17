/** @file materialsnapshot.cpp Material Snapshot.
 *
 * @author Copyright &copy; 2011-2013 Daniel Swanson <danij@dengine.net>
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

#include <cstring>
#include <de/Log>
#include "de_base.h"
#ifdef __CLIENT__
#  include "de_defs.h"
#endif

#ifdef __CLIENT__
#  include "render/rend_main.h" // detailFactor, detailScale, smoothTexAnim, etc...
#  include "gl/gl_texmanager.h"
#  include "gl/sys_opengl.h"
#endif

#include "resource/material.h"
#include "resource/texture.h"

#include "resource/materialsnapshot.h"

namespace de {

struct Store {
    /// @c true= this material is completely opaque.
    bool opaque;

    /// Glow strength factor.
    float glowStrength;

    /// Dimensions in the world coordinate space.
    QSize dimensions;

    /// Minimum ambient light color for reflections.
    Vector3f reflectionMinColor;

    /// Textures used on each logical material texture unit.
    Texture::Variant *textures[NUM_MATERIAL_TEXTURE_UNITS];

#ifdef __CLIENT__
    /// Decoration configuration.
    MaterialSnapshot::Decoration decorations[Material::Variant::max_decorations];

    /// Prepared render texture unit configuration. These map directly
    /// to the texture units supplied to the render lists module.
    rtexmapunit_t units[NUM_TEXMAP_UNITS];
#endif

    Store() { initialize(); }

    void initialize()
    {
        dimensions         = QSize(0, 0);
        reflectionMinColor = Vector3f(0, 0, 0);
        opaque             = true;
        glowStrength       = 0;

        std::memset(textures, 0, sizeof(textures));

#ifdef __CLIENT__
        std::memset(decorations, 0, sizeof(decorations));

        for(int i = 0; i < NUM_TEXMAP_UNITS; ++i)
        {
            Rtu_Init(&units[i]);
        }
#endif
    }

#ifdef __CLIENT__
    void writeTexUnit(rtexmapunitid_t unit, Texture::Variant *texture,
                      blendmode_t blendMode, QSizeF scale, QPointF offset,
                      float opacity)
    {
        DENG2_ASSERT(unit >= 0 && unit < NUM_TEXMAP_UNITS);
        rtexmapunit_t &tu = units[unit];

        tu.texture.variant = reinterpret_cast<texturevariant_s *>(texture);
        tu.texture.flags   = TUF_TEXTURE_IS_MANAGED;
        tu.opacity   = MINMAX_OF(0, opacity, 1);
        tu.blendMode = blendMode;
        V2f_Set(tu.scale, scale.width(), scale.height());
        V2f_Set(tu.offset, offset.x(), offset.y());
    }
#endif // __CLIENT__
};

struct MaterialSnapshot::Instance
{
    /// Variant Material used to derive this snapshot.
    Material::Variant *material;

    Store stored;

    Instance(Material::Variant &_material)
        : material(&_material), stored()
    {}

    void takeSnapshot();

#ifdef __CLIENT__
    void updateMaterial(preparetextureresult_t result);
#endif
};

MaterialSnapshot::MaterialSnapshot(Material::Variant &_material)
{
    d = new Instance(_material);
}

MaterialSnapshot::~MaterialSnapshot()
{
    delete d;
}

Material::Variant &MaterialSnapshot::material() const
{
    return *d->material;
}

QSize const &MaterialSnapshot::dimensions() const
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

Vector3f const &MaterialSnapshot::reflectionMinColor() const
{
    return d->stored.reflectionMinColor;
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
        /// @throw InvalidUnitError Attempt to dereference with an invalid index.
        throw InvalidUnitError("MaterialSnapshot::texture", QString("Invalid texture index %1").arg(index));
    }
    return *d->stored.textures[index];
}

#ifdef __CLIENT__
rtexmapunit_t const &MaterialSnapshot::unit(rtexmapunitid_t id) const
{
    if(id < 0 || id >= NUM_TEXMAP_UNITS)
    {
        /// @throw InvalidUnitError Attempt to obtain a reference to a unit with an invalid id.
        throw InvalidUnitError("MaterialSnapshot::unit", QString("Invalid unit id %1").arg(id));
    }
    return d->stored.units[id];
}

MaterialSnapshot::Decoration &MaterialSnapshot::decoration(int index) const
{
    if(index < 0 || index >= Material::Variant::max_decorations)
    {
        /// @throw InvalidDecorationError Attempt to obtain a reference to a decoration with an invalid index.
        throw InvalidDecorationError("MaterialSnapshot::decoration", QString("Invalid decoration index %1").arg(index));
    }
    return d->stored.decorations[index];
}

static Texture *findTextureByResourceUri(String nameOfScheme, de::Uri const &resourceUri)
{
    if(resourceUri.isEmpty()) return 0;
    try
    {
        return App_Textures()->scheme(nameOfScheme).findByResourceUri(resourceUri).texture();
    }
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.
    return 0;
}

/// @todo Optimize: Cache this result at material level.
static Texture *findTextureForLayerStage(ded_material_layer_stage_t const &def)
{
    try
    {
        return App_Textures()->find(*reinterpret_cast<de::Uri *>(def.texture)).texture();
    }
    catch(Textures::NotFoundError const &)
    {} // Ignore this error.
    return 0;
}

static inline Texture *findDetailTextureForDef(ded_detailtexture_t const &def)
{
    if(!def.detailTex) return 0;
    return findTextureByResourceUri("Details", reinterpret_cast<de::Uri const &>(*def.detailTex));
}

static inline Texture *findShinyTextureForDef(ded_reflection_t const &def)
{
    if(!def.shinyMap) return 0;
    return findTextureByResourceUri("Reflections", reinterpret_cast<de::Uri const &>(*def.shinyMap));
}

static inline Texture *findShinyMaskTextureForDef(ded_reflection_t const &def)
{
    if(!def.maskMap) return 0;
    return findTextureByResourceUri("Masks", reinterpret_cast<de::Uri const &>(*def.maskMap));
}

void MaterialSnapshot::Instance::updateMaterial(preparetextureresult_t result)
{
    Material *mat = &material->generalCase();
    MaterialManifest &manifest = mat->manifest();

    mat->setPrepared(result == PTR_UPLOADED_ORIGINAL? 1 : 2);

    ded_detailtexture_t const *dtlDef = manifest.detailTextureDef();
    mat->setDetailTexture(reinterpret_cast<texture_s *>(dtlDef? findDetailTextureForDef(*dtlDef) : NULL));
    mat->setDetailStrength(dtlDef? dtlDef->strength : 0);
    mat->setDetailScale(dtlDef? dtlDef->scale : 0);

    ded_reflection_t const *refDef = manifest.reflectionDef();
    mat->setShinyTexture(reinterpret_cast<texture_s *>(refDef? findShinyTextureForDef(*refDef) : NULL));
    mat->setShinyMaskTexture(reinterpret_cast<texture_s *>(refDef? findShinyMaskTextureForDef(*refDef) : NULL));
    mat->setShinyBlendmode(refDef? refDef->blendMode : BM_ADD);
    float const black[3] = { 0, 0, 0 };
    mat->setShinyMinColor(refDef? refDef->minColor : black);
    mat->setShinyStrength(refDef? refDef->shininess : 0);
}
#endif // __CLIENT__

/// @todo Implement more useful methods of interpolation. (What do we want/need here?)
void MaterialSnapshot::Instance::takeSnapshot()
{
#define LERP(start, end, pos) (end * pos + start * (1 - pos))

    Material *mat = &material->generalCase();
    Material::Layers const &layers = mat->layers();
    MaterialVariantSpec const &spec = material->spec();

    Texture::Variant *prepTextures[NUM_MATERIAL_TEXTURE_UNITS][2];
    std::memset(prepTextures, 0, sizeof prepTextures);

    // Reinitialize the stored values.
    stored.initialize();

#ifdef __CLIENT__
    /*
     * Ensure all resources needed to visualize this have been prepared.
     *
     * If skymasked, we only need to update the primary tex unit (due to
     * it being visible when skymask debug drawing is enabled).
     */
    for(int i = 0; i < layers.count(); ++i)
    {
        Material::Variant::LayerState const &l   = material->layer(i);

        ded_material_layer_stage_t const *lsCur  = layers[i]->stages()[l.stage];
        if(Texture *tex = findTextureForLayerStage(*lsCur))
        {
            // Pick the instance matching the specified context.
            preparetextureresult_t result;
            prepTextures[i][0] = GL_PrepareTexture(*tex, *spec.primarySpec, &result);

            // Primary texture was (re)prepared?
            if(0 == i && (PTR_UPLOADED_ORIGINAL == result || PTR_UPLOADED_EXTERNAL == result))
            {
                // Are we inheriting the logical dimensions from the texture?
                if(0 == mat->width() && 0 == mat->height())
                {
                    Size2Raw newDimensions(tex->width(), tex->height());
                    mat->setDimensions(&newDimensions);
                }
                updateMaterial(result);
            }
        }

        // Smooth Texture Animation?
        if(!smoothTexAnim || layers[i]->stageCount() < 2) continue;

        ded_material_layer_stage_t const *lsNext = layers[i]->stages()[(l.stage + 1) % layers[i]->stageCount()];
        if(Texture *tex = findTextureForLayerStage(*lsNext))
        {
            // Pick the instance matching the specified context.
            preparetextureresult_t result;
            prepTextures[i][1] = GL_PrepareTexture(*tex, *spec.primarySpec, &result);
        }
    }

    // Do we need to prepare a DetailTexture?
    if(!mat->isSkyMasked())
    if(Texture *tex = reinterpret_cast<Texture *>(mat->detailTexture()))
    {
        float const contrast = mat->detailStrength() * detailFactor;
        texturevariantspecification_t &texSpec = *GL_DetailTextureVariantSpecificationForContext(contrast);

        prepTextures[MTU_DETAIL][0] = GL_PrepareTexture(*tex, texSpec);
    }

    // Do we need to prepare a shiny texture (and possibly a mask)?
    if(!mat->isSkyMasked())
    if(Texture *tex = reinterpret_cast<Texture *>(mat->shinyTexture()))
    {
        texturevariantspecification_t &texSpec =
            *GL_TextureVariantSpecificationForContext(TC_MAPSURFACE_REFLECTION,
                 TSF_NO_COMPRESSION, 0, 0, 0, GL_REPEAT, GL_REPEAT, 1, 1, -1,
                 false, false, false, false);

        prepTextures[MTU_REFLECTION][0] = GL_PrepareTexture(*tex, texSpec);

    }

    // We are only interested in a mask if we have a shiny texture.
    if(prepTextures[MTU_REFLECTION][0])
    if(Texture *tex = reinterpret_cast<Texture *>(mat->shinyMaskTexture()))
    {
        texturevariantspecification_t &texSpec =
            *GL_TextureVariantSpecificationForContext(
                 TC_MAPSURFACE_REFLECTIONMASK, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                 -1, -1, -1, true, false, false, false);

        prepTextures[MTU_REFLECTION_MASK][0] = GL_PrepareTexture(*tex, texSpec);
    }
#endif // __CLIENT__

    stored.dimensions.setWidth(mat->width());
    stored.dimensions.setHeight(mat->height());

#ifdef __CLIENT__
    stored.opaque = (prepTextures[MTU_PRIMARY][0] && !prepTextures[MTU_PRIMARY][0]->isMasked());
#endif

    if(stored.dimensions.isEmpty()) return;

    Material::Variant::LayerState const &l   = material->layer(0);
    ded_material_layer_stage_t const *lsCur  = layers[0]->stages()[l.stage];
    ded_material_layer_stage_t const *lsNext = layers[0]->stages()[(l.stage + 1) % layers[0]->stageCount()];

    // Glow strength is presently taken from layer #0.
    if(l.inter == 0)
    {
        stored.glowStrength = lsCur->glowStrength;
    }
    else // Interpolate.
    {
        stored.glowStrength = LERP(lsCur->glowStrength, lsNext->glowStrength, l.inter);
    }

    if(MC_MAPSURFACE == spec.context && prepTextures[MTU_REFLECTION][0])
    {
        stored.reflectionMinColor = Vector3f(mat->shinyMinColor());
    }

    // Setup the primary texture unit.
    if(Texture::Variant *tex = prepTextures[MTU_PRIMARY][0])
    {
        stored.textures[MTU_PRIMARY] = tex;
#ifdef __CLIENT__
        QPointF offset;
        if(l.inter == 0)
        {
            offset = QPointF(lsCur->texOrigin[0], lsCur->texOrigin[1]);
        }
        else // Interpolate.
        {
            offset.setX(LERP(lsCur->texOrigin[0], lsNext->texOrigin[0], l.inter));
            offset.setY(LERP(lsCur->texOrigin[1], lsNext->texOrigin[1], l.inter));
        }

        stored.writeTexUnit(RTU_PRIMARY, tex, BM_NORMAL,
                            QSizeF(1.f / stored.dimensions.width(),
                                   1.f / stored.dimensions.height()),
                            offset, 1);
#endif
    }

#ifdef __CLIENT__
    // Setup the inter primary texture unit.
    if(Texture::Variant *tex = prepTextures[MTU_PRIMARY][1])
    {
        // If fog is active, inter=0 is accepted as well. Otherwise
        // flickering may occur if the rendering passes don't match for
        // blended and unblended surfaces.
        if(!(!usingFog && l.inter == 0))
        {
            QPointF offset;
            if(l.inter == 0)
            {
                offset = QPointF(lsCur->texOrigin[0], lsCur->texOrigin[1]);
            }
            else // Interpolate.
            {
                offset.setX(LERP(lsCur->texOrigin[0], lsNext->texOrigin[0], l.inter));
                offset.setY(LERP(lsCur->texOrigin[1], lsNext->texOrigin[1], l.inter));
            }

            stored.writeTexUnit(RTU_INTER, tex, BM_NORMAL,
                                QSizeF(1.f / stored.dimensions.width(),
                                       1.f / stored.dimensions.height()),
                                offset, l.inter);
        }
    }
#endif

    // Setup the detail texture unit.
    if(Texture::Variant *tex = prepTextures[MTU_DETAIL][0])
    {
        stored.textures[MTU_DETAIL] = tex;
#ifdef __CLIENT__
        float scaleFactor = mat->detailScale();
        if(detailScale > .0001f)
            scaleFactor *= detailScale; // Global scale factor.

        stored.writeTexUnit(RTU_PRIMARY_DETAIL, tex, BM_NORMAL,
                            QSizeF(1.f / tex->generalCase().width()  * scaleFactor,
                                   1.f / tex->generalCase().height() * scaleFactor),
                            QPointF(0, 0), 1);
#endif
    }

/*#ifdef __CLIENT__
    // Setup the inter detail texture unit.
    if(Texture::Variant *tex = prepTextures[MTU_DETAIL][1])
    {
        // If fog is active, inter=0 is accepted as well. Otherwise
        // flickering may occur if the rendering passes don't match for
        // blended and unblended surfaces.
        if(!(!usingFog && l.inter == 0))
        {
            float scaleFactor = mat->detailScale();
            if(detailScale > .0001f)
                scaleFactor *= detailScale; // Global scale factor.

            stored.writeTexUnit(RTU_INTER_DETAIL, tex, BM_NORMAL,
                                QSizeF(1.f / tex->generalCase().width()  * scaleFactor,
                                       1.f / tex->generalCase().height() * scaleFactor),
                                QPointF(0, 0), l.inter);
        }
    }
#endif*/

    // Setup the shiny texture units.
    if(Texture::Variant *tex = prepTextures[MTU_REFLECTION][0])
    {
        stored.textures[MTU_REFLECTION] = tex;
#ifdef __CLIENT__
        stored.writeTexUnit(RTU_REFLECTION, tex, mat->shinyBlendmode(),
                            QSizeF(1, 1), QPointF(0, 0),
                            mat->shinyStrength());
#endif

    }

    if(prepTextures[MTU_REFLECTION][0])
        if(Texture::Variant *tex = prepTextures[MTU_REFLECTION_MASK][0])
    {
        stored.textures[MTU_REFLECTION_MASK] = tex;
#ifdef __CLIENT__
        stored.writeTexUnit(RTU_REFLECTION_MASK, tex, BM_NORMAL,
                            QSizeF(1.f / (stored.dimensions.width()  * tex->generalCase().width()),
                                   1.f / (stored.dimensions.height() * tex->generalCase().height())),
                            QPointF(stored.units[RTU_PRIMARY].offset[0],
                                    stored.units[RTU_PRIMARY].offset[1]), 1);
#endif
    }

#ifdef __CLIENT__
    uint idx = 0;
    Material::Decorations const &decorations = mat->decorations();
    for(Material::Decorations::const_iterator it = decorations.begin();
        it != decorations.end(); ++it, ++idx)
    {
        Material::Variant::DecorationState const &l = material->decoration(idx);
        Material::Decoration const *lDef = *it;
        ded_decorlight_stage_t const *lsCur  = lDef->stages()[l.stage];
        ded_decorlight_stage_t const *lsNext = lDef->stages()[(l.stage + 1) % lDef->stageCount()];

        MaterialSnapshot::Decoration &decor = stored.decorations[idx];

        if(l.inter == 0)
        {
            decor.pos[0]         = lsCur->pos[0];
            decor.pos[1]         = lsCur->pos[1];
            decor.elevation      = lsCur->elevation;
            decor.radius         = lsCur->radius;
            decor.haloRadius     = lsCur->haloRadius;
            decor.lightLevels[0] = lsCur->lightLevels[0];
            decor.lightLevels[1] = lsCur->lightLevels[1];

            std::memcpy(decor.color, lsCur->color, sizeof(decor.color));
        }
        else // Interpolate.
        {
            decor.pos[0]         = LERP(lsCur->pos[0], lsNext->pos[0], l.inter);
            decor.pos[1]         = LERP(lsCur->pos[1], lsNext->pos[1], l.inter);
            decor.elevation      = LERP(lsCur->elevation, lsNext->elevation, l.inter);
            decor.radius         = LERP(lsCur->radius, lsNext->radius, l.inter);
            decor.haloRadius     = LERP(lsCur->haloRadius, lsNext->haloRadius, l.inter);
            decor.lightLevels[0] = LERP(lsCur->lightLevels[0], lsNext->lightLevels[0], l.inter);
            decor.lightLevels[1] = LERP(lsCur->lightLevels[1], lsNext->lightLevels[1], l.inter);

            for(int c = 0; c < 3; ++c)
            {
                decor.color[c] = LERP(lsCur->color[c], lsNext->color[c], l.inter);
            }
        }

        decor.tex      = GL_PrepareLightmap(lsCur->sides);
        decor.ceilTex  = GL_PrepareLightmap(lsCur->up);
        decor.floorTex = GL_PrepareLightmap(lsCur->down);
        decor.flareTex = GL_PrepareFlareTexture(lsCur->flare, lsCur->flareTexture);
    }
#endif // __CLIENT__

#undef LERP
}

void MaterialSnapshot::update()
{
    d->takeSnapshot();
}

} // namespace de
