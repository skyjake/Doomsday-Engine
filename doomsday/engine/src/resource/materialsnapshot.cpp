/** @file materialsnapshot.cpp Material Snapshot.
 *
 * @author Copyright &copy; 2011-2012 Daniel Swanson <danij@dengine.net>
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
#include "de_graphics.h"
#include "de_render.h"

#include <cstring>
#include <de/Error>
#include <de/Log>

#include "resource/material.h"
#include "resource/texture.h"

#include "resource/materialsnapshot.h"

namespace de {

struct MaterialSnapshot::Instance
{
    /// Variant Material used to derive this snapshot.
    MaterialVariant *material;

    /// @c true= this material is completely opaque.
    bool isOpaque;

    /// Glow strength factor.
    float glowStrength;

    /// Dimensions in the world coordinate space.
    QSize dimensions;

    /// Minimum ambient light color for reflections.
    vec3f_t reflectionMinColor;

    /// Textures used on each texture unit.
    TextureVariant *textures[NUM_MATERIAL_TEXTURE_UNITS];

    /// Texture unit configuration.
    rtexmapunit_t units[NUM_MATERIAL_TEXTURE_UNITS];

    Instance(MaterialVariant &_material)
        : material(&_material), isOpaque(true), glowStrength(0), dimensions(0, 0)
    {
        V3f_Set(reflectionMinColor, 0, 0, 0);

        for(int i = 0; i < NUM_MATERIAL_TEXTURE_UNITS; ++i)
        {
            Rtu_Init(&units[i]);
            textures[i] = NULL;
        }
    }

    void setupTexUnit(byte unit, TextureVariant *texture,
        blendmode_t blendMode, float sScale, float tScale, float sOffset,
        float tOffset, float opacity)
    {
        DENG2_ASSERT(unit < NUM_MATERIAL_TEXTURE_UNITS);

        textures[unit] = texture;
        rtexmapunit_t *tu = &units[unit];
        tu->texture.variant = reinterpret_cast<texturevariant_s *>(texture);
        tu->texture.flags = TUF_TEXTURE_IS_MANAGED;
        tu->blendMode = blendMode;
        V2f_Set(tu->scale, sScale, tScale);
        V2f_Set(tu->offset, sOffset, tOffset);
        tu->opacity = MINMAX_OF(0, opacity, 1);
    }
};

MaterialSnapshot::MaterialSnapshot(MaterialVariant &_material)
{
    d = new Instance(_material);
}

MaterialSnapshot::~MaterialSnapshot()
{
    delete d;
}

MaterialVariant &MaterialSnapshot::material() const
{
    return *d->material;
}

QSize const &MaterialSnapshot::dimensions() const
{
    return d->dimensions;
}

bool MaterialSnapshot::isOpaque() const
{
    return d->isOpaque;
}

float MaterialSnapshot::glowStrength() const
{
    return d->glowStrength;
}

vec3f_t const &MaterialSnapshot::reflectionMinColor() const
{
    return d->reflectionMinColor;
}

bool MaterialSnapshot::hasTexture(int index) const
{
    if(index < 0 || index >= NUM_MATERIAL_TEXTURE_UNITS) return false;
    return d->textures[index] != 0;
}

TextureVariant &MaterialSnapshot::texture(int index) const
{
    if(!hasTexture(index))
    {
        /// @throws InvalidUnitError Attempt to dereference with an invalid index.
        throw InvalidUnitError("MaterialSnapshot::texture", QString("Invalid unit index %1").arg(index));
    }
    return *d->textures[index];
}

rtexmapunit_t const &MaterialSnapshot::unit(int index) const
{
    if(index < 0 || index >= NUM_MATERIAL_TEXTURE_UNITS)
    {
        /// @throws InvalidUnitError Attempt to obtain a reference to a unit with an invalid index.
        throw InvalidUnitError("MaterialSnapshot::unit", QString("Invalid unit index %1").arg(index));
    }
    return d->units[index];
}

void MaterialSnapshot::update()
{
    TextureVariant *prepTextures[NUM_MATERIAL_TEXTURE_UNITS];
    material_t &mat = d->material->generalCase();
    materialvariantspecification_t const &spec = d->material->spec();

    std::memset(prepTextures, 0, sizeof prepTextures);

    // Ensure all resources needed to visualize this Material's layers have been prepared.
    int layerCount = Material_LayerCount(&mat);
    for(int i = 0; i < layerCount; ++i)
    {
        MaterialVariant::Layer const &ml = d->material->layer(i);
        preparetextureresult_t result;

        if(!ml.texture) continue;

        // Pick the instance matching the specified context.
        prepTextures[i] = reinterpret_cast<TextureVariant *>(GL_PrepareTextureVariant2(reinterpret_cast<texture_s *>(ml.texture), spec.primarySpec, &result));

        if(0 == i && (PTR_UPLOADED_ORIGINAL == result || PTR_UPLOADED_EXTERNAL == result))
        {
            // Primary texture was (re)prepared.
            Material_SetPrepared(&mat, result == PTR_UPLOADED_ORIGINAL? 1 : 2);

            materialid_t matId = Material_PrimaryBind(&mat);
            if(matId != NOMATERIALID)
            {
                Materials_UpdateTextureLinks(matId);
            }

            // Are we inheriting the logical dimensions from the texture?
            if(0 == Material_Width(&mat) && 0 == Material_Height(&mat))
            {
                Texture &tex = reinterpret_cast<Texture &>(*ml.texture);
                Size2Raw texSize(tex.width(), tex.height());
                Material_SetSize(&mat, &texSize);
            }
        }
    }

    // Do we need to prepare a DetailTexture?
    texture_s *tex = Material_DetailTexture(&mat);
    if(tex)
    {
        float const contrast = Material_DetailStrength(&mat) * detailFactor;
        texturevariantspecification_t *texSpec = GL_DetailTextureVariantSpecificationForContext(contrast);

        prepTextures[MTU_DETAIL] = reinterpret_cast<TextureVariant *>(GL_PrepareTextureVariant(tex, texSpec));
    }

    // Do we need to prepare a shiny texture (and possibly a mask)?
    tex = Material_ShinyTexture(&mat);
    if(tex)
    {
        texturevariantspecification_t *texSpec =
            GL_TextureVariantSpecificationForContext(TC_MAPSURFACE_REFLECTION,
                TSF_NO_COMPRESSION, 0, 0, 0, GL_REPEAT, GL_REPEAT, 1, 1, -1,
                false, false, false, false);

        prepTextures[MTU_REFLECTION] = reinterpret_cast<TextureVariant *>(GL_PrepareTextureVariant(tex, texSpec));

        // We are only interested in a mask if we have a shiny texture.
        if(prepTextures[MTU_REFLECTION] && (tex = Material_ShinyMaskTexture(&mat)))
        {
            texSpec = GL_TextureVariantSpecificationForContext(
                TC_MAPSURFACE_REFLECTIONMASK, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                -1, -1, -1, true, false, false, false);
            prepTextures[MTU_REFLECTION_MASK] = reinterpret_cast<TextureVariant *>(GL_PrepareTextureVariant(tex, texSpec));
        }
    }

    for(int i = 0; i < NUM_MATERIAL_TEXTURE_UNITS; ++i)
    {
        Rtu_Init(&d->units[i]);
        d->textures[i] = NULL;
    }

    Size2 const *materialDimensions = Material_Size(&mat);
    d->dimensions.setWidth(Size2_Width(materialDimensions));
    d->dimensions.setHeight(Size2_Height(materialDimensions));

    d->glowStrength = 0;
    d->isOpaque = true;
    V3f_Set(d->reflectionMinColor, 0, 0, 0);

    if(d->dimensions.isEmpty()) return;

    d->glowStrength = d->material->layer(0).glow * glowFactor;
    d->isOpaque = NULL != prepTextures[MTU_PRIMARY] && !prepTextures[MTU_PRIMARY]->isMasked();

    // Setup the primary texture unit.
    if(prepTextures[MTU_PRIMARY])
    {
        TextureVariant *tex = prepTextures[MTU_PRIMARY];
        float const sScale = 1.f / d->dimensions.width();
        float const tScale = 1.f / d->dimensions.height();

        d->setupTexUnit(MTU_PRIMARY, tex, BM_NORMAL,
                        sScale, tScale, d->material->layer(0).texOrigin[0],
                        d->material->layer(0).texOrigin[1], 1);
    }

    /**
     * If skymasked, we need only need to update the primary tex unit
     * (this is due to it being visible when skymask debug drawing is
     * enabled).
     */
    if(!Material_IsSkyMasked(&mat))
    {
        // Setup the detail texture unit?
        if(prepTextures[MTU_DETAIL] && d->isOpaque)
        {
            TextureVariant *tex = prepTextures[MTU_DETAIL];
            float const width  = tex->generalCase().width();
            float const height = tex->generalCase().height();
            float scale = Material_DetailScale(&mat);

            // Apply the global scaling factor.
            if(detailScale > .0001f)
                scale *= detailScale;

            d->setupTexUnit(MTU_DETAIL, tex, BM_NORMAL,
                            1.f / width * scale, 1.f / height * scale, 0, 0, 1);
        }

        // Setup the shiny texture units?
        if(prepTextures[MTU_REFLECTION])
        {
            TextureVariant *tex = prepTextures[MTU_REFLECTION];
            blendmode_t const blendmode = Material_ShinyBlendmode(&mat);
            float const strength = Material_ShinyStrength(&mat);

            d->setupTexUnit(MTU_REFLECTION, tex, blendmode, 1, 1, 0, 0, strength);
        }

        if(prepTextures[MTU_REFLECTION_MASK])
        {
            TextureVariant *tex = prepTextures[MTU_REFLECTION_MASK];

            d->setupTexUnit(MTU_REFLECTION_MASK, tex, BM_NORMAL,
                            1.f / (d->dimensions.width()  * tex->generalCase().width()),
                            1.f / (d->dimensions.height() * tex->generalCase().height()),
                            d->units[MTU_PRIMARY].offset[0], d->units[MTU_PRIMARY].offset[1], 1);
        }
    }

    if(MC_MAPSURFACE == spec.context && prepTextures[MTU_REFLECTION])
    {
        float const *minColor = Material_ShinyMinColor(&mat);
        d->reflectionMinColor[CR] = minColor[CR];
        d->reflectionMinColor[CG] = minColor[CG];
        d->reflectionMinColor[CB] = minColor[CB];
    }
}

} // namespace de
