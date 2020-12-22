/** @file sky.cpp
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "world/sky.h"
#include "resource/clientmaterial.h"
#include "resource/materialanimator.h"
#include "render/skydrawable.h"
#include "gl/gl_tex.h"
#include "render/rend_main.h"

#include <doomsday/res/texturemanifest.h>

using namespace de;

static const Vec3f AmbientLightColorDefault{1}; // Pure white.

Sky::Sky(const defn::Sky *def)
    : world::Sky(def)
{
    for (int i = 0; i < NUM_LAYERS; ++i)
    {
        layer(i).audienceForActiveChange()   += this;
        layer(i).audienceForMaskedChange()   += this;
        layer(i).audienceForMaterialChange() += this;
    }
}

void Sky::configure(const defn::Sky *def)
{
    world::Sky::configure(def);

    if(def)
    {
        Vec3f color(def->get("color"));
        if(color != Vec3f(0.0f))
        {
            ambientLight.setColor(color);
        }
    }
    else
    {
        ambientLight.reset();
    }
}

/**
 * @todo Move to SkyDrawable and have it simply update this component once the
 * ambient color has been calculated.
 *
 * @todo Re-implement me by rendering the sky to a low-quality cubemap and use
 *  that to obtain the lighting characteristics.
 */
void Sky::updateAmbientLightIfNeeded()
{
    // Never update if a custom color is defined.
    if (ambientLight.custom) return;

    // Is it time to update the color?
    if (!ambientLight.needUpdate) return;
    ambientLight.needUpdate = false;

    ambientLight.color = AmbientLightColorDefault;

    // Determine the first active layer.
    dint firstActiveLayer = -1; // -1 denotes 'no active layers'.
    for (dint i = 0; i < layerCount(); ++i)
    {
        if (layer(i).isActive())
        {
            firstActiveLayer = i;
            break;
        }
    }

    // A sky with no active layer uses the default color.
    if (firstActiveLayer < 0) return;

    Vec3f avgLayerColor;
    Vec3f bottomCapColor;
    Vec3f topCapColor;

    dint avgCount = 0;
    for (dint i = firstActiveLayer; i < layerCount(); ++i)
    {
        Layer &layer = this->layer(i);

        // Inactive layers won't be drawn.
        if (!layer.isActive()) continue;

        // A material is required for drawing.
        ClientMaterial *mat = static_cast<ClientMaterial *>(layer.material());
        if (!mat) continue;

        MaterialAnimator &matAnimator =
            mat->getAnimator(SkyDrawable::layerMaterialSpec(layer.isMasked()));

        // Ensure we've up to date info about the material.
        matAnimator.prepare();

        if (TextureVariant *tex = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture)
        {
            const auto *avgColor = reinterpret_cast<const averagecolor_analysis_t *>(
                tex->base().analysisDataPointer(ClientTexture::AverageColorAnalysis));
            if (!avgColor)
                throw Error("calculateSkyAmbientColor",
                            "Texture \"" + tex->base().manifest().composeUri().asText() +
                                "\" has no AverageColorAnalysis");

            if (i == firstActiveLayer)
            {
                const auto *avgLineColor = reinterpret_cast<const averagecolor_analysis_t *>(
                    tex->base().analysisDataPointer(ClientTexture::AverageTopColorAnalysis));
                if (!avgLineColor)
                    throw Error("calculateSkyAmbientColor",
                                "Texture \"" + tex->base().manifest().composeUri().asText() +
                                    "\" has no AverageTopColorAnalysis");

                topCapColor = Vec3f(avgLineColor->color.rgb);

                avgLineColor = reinterpret_cast<const averagecolor_analysis_t *>(
                    tex->base().analysisDataPointer(ClientTexture::AverageBottomColorAnalysis));
                if (!avgLineColor)
                    throw Error("calculateSkyAmbientColor",
                                "Texture \"" + tex->base().manifest().composeUri().asText() +
                                    "\" has no AverageBottomColorAnalysis");

                bottomCapColor = Vec3f(avgLineColor->color.rgb);
            }

            avgLayerColor += Vec3f(avgColor->color.rgb);
            ++avgCount;
        }
    }

    // The caps cover a large amount of the sky sphere, so factor it in too.
    // Each cap is another unit.
    ambientLight.setColor((avgLayerColor + topCapColor + bottomCapColor) / (avgCount + 2),
                          false /*not a custom color*/);
}

/// Observes Layer ActiveChange
void Sky::skyLayerActiveChanged(Layer &)
{
    ambientLight.needUpdate = true;
}

/// Observes Layer MaterialChange
void Sky::skyLayerMaterialChanged(Layer &layer)
{
    // We may need to recalculate the ambient color of the sky.
    if(!layer.isActive()) return;
    //if(ambientLight.custom) return;

    ambientLight.needUpdate = true;
}

/// Observes Layer MaskedChange
void Sky::skyLayerMaskedChanged(Layer &layer)
{
    // We may need to recalculate the ambient color of the sky.
    if(!layer.isActive()) return;
    //if(ambientLight.custom) return;

    ambientLight.needUpdate = true;
}

const Vec3f &Sky::ambientColor() const
{
    if(ambientLight.custom || rendSkyLightAuto)
    {
        const_cast<Sky *>(this)->updateAmbientLightIfNeeded();
        return ambientLight.color;
    }
    return AmbientLightColorDefault;
}

void Sky::setAmbientColor(const Vec3f &newColor)
{
    ambientLight.setColor(newColor);
}

void Sky::AmbientLight::setColor(const de::Vec3f &newColor, bool isCustom)
{
    color  = newColor.min(Vec3f(1.0f)).max(Vec3f(0.0f));
    custom = isCustom;
}

void Sky::AmbientLight::reset()
{
    custom     = false;
    color      = AmbientLightColorDefault;
    needUpdate = true;
}
