/** @file materialanimation.cpp Logical Material Animation.
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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
#include "de_network.h" // playback / clientPaused
#include "world/gamemap.h" // theMap - remove me
#include "render/r_main.h" // frameCount, frameTimePos
#include <de/Log>
#include <de/mathutil.h>

#include "resource/material.h"

using namespace de;

DENG2_PIMPL(Material::Animation)
{
    /// Material to be animated.
    Material &material;

    /// Render context identifier.
    MaterialContextId context;

    /// Layer animation states.
    Material::Animation::LayerState layers[Material::max_layers];
    Material::Animation::LayerState detailLayer;
    Material::Animation::LayerState shineLayer;

    /// Decoration animation states.
    Material::Animation::DecorationState decorations[Material::max_decorations];

    Instance(Public *i, Material &_material, MaterialContextId _context)
        : Base(i), material(_material), context(_context)
    {}

    template <typename StageType>
    void resetLayer(Material::Animation::LayerState &ls, StageType const &stage)
    {
        ls.stage = 0;
        ls.tics  = stage.tics;
        ls.inter = 0;
    }

    void resetDecoration(Material::Animation::DecorationState &ds,
                         Material::Decoration::Stage const &stage)
    {
        ds.stage = 0;
        ds.tics  = stage.tics;
        ds.inter = 0;
    }

    template <typename LayerType>
    void animateLayer(Material::Animation::LayerState &ls, LayerType const &layer)
    {
        if(DD_IsSharpTick() && ls.tics-- <= 0)
        {
            // Advance to next stage.
            if(++ls.stage == layer.stageCount())
            {
                // Loop back to the beginning.
                ls.stage = 0;
            }
            ls.inter = 0;

            typename LayerType::Stage const *lsCur = layer.stages()[ls.stage];
            if(lsCur->variance != 0)
                ls.tics = lsCur->tics * (1 - lsCur->variance * RNG_RandFloat());
            else
                ls.tics = lsCur->tics;
        }
        else
        {
            typename LayerType::Stage const *lsCur = layer.stages()[ls.stage];
            ls.inter = 1.f - ls.tics / float( lsCur->tics );
        }
    }

    void animateDecoration(Material::Animation::DecorationState &ds, Material::Decoration const &decor)
    {
        if(DD_IsSharpTick() && ds.tics-- <= 0)
        {
            // Advance to next stage.
            if(++ds.stage == decor.stageCount())
            {
                // Loop back to the beginning.
                ds.stage = 0;
            }
            ds.inter = 0;

            Material::Decoration::Stage const *lsCur = decor.stages()[ds.stage];
            if(lsCur->variance != 0)
                ds.tics = lsCur->tics * (1 - lsCur->variance * RNG_RandFloat());
            else
                ds.tics = lsCur->tics;

            // Notify interested parties about this.
            if(context == MapSurfaceContext)
            {
                // Surfaces using this material may need to be updated.
                if(theMap)
                {
                    /// @todo Replace with a de::Observers-based mechanism.
                    theMap->updateSurfacesOnMaterialChange(material);
                }
            }
        }
        else
        {
            Material::Decoration::Stage const *lsCur = decor.stages()[ds.stage];
            ds.inter = 1.f - ds.tics / float( lsCur->tics );
        }
    }
};

Material::Animation::Animation(Material &material, MaterialContextId context)
    : d(new Instance(this, material, context))
{
    // Initialize the animation.
    restart();
}

MaterialContextId Material::Animation::context() const
{
    return d->context;
}

bool Material::Animation::isPaused() const
{
    // Depending on the usage context, the animation should only progress
    // when the game is not paused.
    return (clientPaused && (d->context == MapSurfaceContext ||
                             d->context == SpriteContext     ||
                             d->context == ModelSkinContext  ||
                             d->context == PSpriteContext    ||
                             d->context == SkySphereContext));
}

void Material::Animation::animate(timespan_t /*ticLength*/)
{
    // Animation ceases once the material is no longer valid.
    if(!d->material.isValid()) return;

    // Animation will only progress when not paused.
    if(isPaused()) return;

    /*
     * Animate layers:
     */
    Material::Layers const &layers = d->material.layers();
    for(int i = 0; i < layers.count(); ++i)
    {
        if(layers[i]->isAnimated())
            d->animateLayer(d->layers[i], *layers[i]);
    }

    if(d->material.isDetailed() && d->material.detailLayer().isAnimated())
    {
        d->animateLayer(d->detailLayer, d->material.detailLayer());
    }

    if(d->material.isShiny() && d->material.shineLayer().isAnimated())
    {
        d->animateLayer(d->shineLayer, d->material.shineLayer());
    }

    /*
     * Animate decorations:
     */
    Material::Decorations const &decorations = d->material.decorations();
    for(int i = 0; i < decorations.count(); ++i)
    {
        if(decorations[i]->isAnimated())
            d->animateDecoration(d->decorations[i], *decorations[i]);
    }
}

void Material::Animation::restart()
{
    // Animation ceases once the material is no longer valid.
    if(!d->material.isValid()) return;

    Material::Layers const &layers = d->material.layers();
    for(int i = 0; i < layers.count(); ++i)
    {
        d->resetLayer(d->layers[i], *layers[i]->stages()[0]);
    }

    if(d->material.isDetailed())
    {
        d->resetLayer(d->detailLayer, *d->material.detailLayer().stages()[0]);
    }

    if(d->material.isShiny())
    {
        d->resetLayer(d->shineLayer, *d->material.shineLayer().stages()[0]);
    }

    Material::Decorations const &decorations = d->material.decorations();
    for(int i = 0; i < decorations.count(); ++i)
    {
        d->resetDecoration(d->decorations[i], *decorations[i]->stages()[0]);
    }
}

Material::Animation::LayerState const &Material::Animation::layer(int layerNum) const
{
    if(layerNum >= 0 && layerNum < d->material.layerCount())
    {
        return d->layers[layerNum];
    }
    /// @throw Material::UnknownLayerError Invalid layer reference.
    throw Material::UnknownLayerError("Material::AnimationState::layer", QString("Invalid material layer #%1").arg(layerNum));
}

Material::Animation::LayerState const &Material::Animation::detailLayer() const
{
    if(d->material.isDetailed())
    {
        return d->detailLayer;
    }
    /// @throw Material::UnknownLayerError The material has no details layer.
    throw Material::UnknownLayerError("Material::AnimationState::detailLayer", "Material has no details layer");
}

Material::Animation::LayerState const &Material::Animation::shineLayer() const
{
    if(d->material.isShiny())
    {
        return d->shineLayer;
    }
    /// @throw Material::UnknownLayerError The material has no shine layer.
    throw Material::UnknownLayerError("Material::AnimationState::shineLayer", "Material has no shine layer");
}

Material::Animation::DecorationState const &Material::Animation::decoration(int decorNum) const
{
    if(decorNum >= 0 && decorNum < d->material.decorationCount())
    {
        return d->decorations[decorNum];
    }
    /// @throw Material::UnknownDecorationError Invalid decoration reference.
    throw Material::UnknownDecorationError("Material::AnimationState::decoration", QString("Invalid material decoration #%1").arg(decorNum));
}
