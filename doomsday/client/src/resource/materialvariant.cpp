/** @file materialvariant.cpp Logical Material Variant.
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

#include <de/Error>
#include <de/Log>
#include <de/mathutil.h>

#include "de_base.h"
#ifdef __CLIENT__
#  include "de_network.h" // playback / clientPaused
#endif

#include "map/r_world.h" // R_UpdateMapSurfacesOnMaterialChange
#include "render/r_main.h" // frameCount, frameTimePos

#include "resource/materialvariantspec.h"

namespace de {

bool MaterialVariantSpec::compare(MaterialVariantSpec const &other) const
{
    if(this == &other) return 1;
    if(context != other.context) return 0;
    return 1 == TextureVariantSpec_Compare(primarySpec, other.primarySpec);
}

}

using namespace de;

struct Material::Variant::Instance
{
    /// Superior material of which this is a derivative.
    Material *material;

    /// Specification used to derive this variant.
    Material::VariantSpec const &spec;

    /// Cached animation state snapshot (if any).
    Material::Snapshot *snapshot;

    /// Frame count when the snapshot was last prepared/updated.
    int snapshotPrepareFrame;

    /// Layer animation states.
    Material::Variant::LayerState layers[Material::max_layers];
    Material::Variant::LayerState detailLayer;
    Material::Variant::LayerState shineLayer;

    /// Decoration animation states.
    Material::Variant::DecorationState decorations[Material::max_decorations];

    Instance(Material &generalCase, Material::VariantSpec const &_spec)
        : material(&generalCase),
          spec(_spec), snapshot(0), snapshotPrepareFrame(-1)
    {}

    ~Instance()
    {
        if(snapshot) M_Free(snapshot);
    }

    /**
     * Attach new snapshot data to the variant. If an existing snapshot is already
     * present it will be replaced. Ownership of @a materialSnapshot is given to
     * the variant.
     */
    void attachSnapshot(Material::Snapshot &newSnapshot)
    {
        if(snapshot)
        {
#ifdef DENG_DEBUG
            LOG_AS("Material::Variant::AttachSnapshot");
            LOG_WARNING("A snapshot is already attached to %p, it will be replaced.") << de::dintptr(this);
#endif
            delete snapshot;
        }
        snapshot = &newSnapshot;
    }

    /**
     * Detach the snapshot data from the variant, relinquishing ownership to the caller.
     */
    Material::Snapshot *detachSnapshot()
    {
        Material::Snapshot *detachedSnapshot = snapshot;
        snapshot = 0;
        return detachedSnapshot;
    }
};

Material::Variant::Variant(Material &generalCase, Material::VariantSpec const &spec)
{
    d = new Instance(generalCase, spec);
    // Initialize animation states.
    resetAnim();
}

Material::Variant::~Variant()
{
    delete d;
}

Material &Material::Variant::generalCase() const
{
    return *d->material;
}

Material::VariantSpec const &Material::Variant::spec() const
{
    return d->spec;
}

bool Material::Variant::isPaused() const
{
#ifdef __CLIENT__
    // Depending on the usage context, the animation should only progress
    // when the game is not paused.
    return (clientPaused && (d->spec.context == MC_MAPSURFACE ||
                             d->spec.context == MC_SPRITE     ||
                             d->spec.context == MC_MODELSKIN  ||
                             d->spec.context == MC_PSPRITE    ||
                             d->spec.context == MC_SKYSPHERE));
#else
    return false;
#endif
}

void Material::Variant::ticker(timespan_t /*ticLength*/)
{
    // Animation ceases once the material is no longer valid.
    if(!d->material->isValid()) return;

    // Animation will only progress when not paused.
    if(isPaused()) return;

    /*
     * Animate layers:
     */
    Material::Layers const &layers = d->material->layers();
    for(int i = 0; i < layers.count(); ++i)
    {
        Material::Layer const &layerDef = *layers[i];

        // Not animated?
        if(layerDef.stageCount() == 1) continue;
        LayerState &ls = d->layers[i];

        if(DD_IsSharpTick() && ls.tics-- <= 0)
        {
            // Advance to next stage.
            if(++ls.stage == layerDef.stageCount())
            {
                // Loop back to the beginning.
                ls.stage = 0;
            }
            ls.inter = 0;

            Layer::Stage const *lsCur = layerDef.stages()[ls.stage];
            if(lsCur->variance != 0)
                ls.tics = lsCur->tics * (1 - lsCur->variance * RNG_RandFloat());
            else
                ls.tics = lsCur->tics;
        }
        else
        {
            Layer::Stage const *lsCur = layerDef.stages()[ls.stage];
            ls.inter = 1 - (ls.tics - frameTimePos) / float( lsCur->tics );
        }
    }

    if(d->material->isDetailed())
    {
        Material::DetailLayer const &layerDef = d->material->detailLayer();
        // Not animated?
        if(layerDef.stageCount() > 1)
        {
            LayerState &ls = d->detailLayer;

            if(DD_IsSharpTick() && ls.tics-- <= 0)
            {
                // Advance to next stage.
                if(++ls.stage == layerDef.stageCount())
                {
                    // Loop back to the beginning.
                    ls.stage = 0;
                }
                ls.inter = 0;

                Material::DetailLayer::Stage const *lsCur = layerDef.stages()[ls.stage];
                if(lsCur->variance != 0)
                    ls.tics = lsCur->tics * (1 - lsCur->variance * RNG_RandFloat());
                else
                    ls.tics = lsCur->tics;
            }
            else
            {
                Material::DetailLayer::Stage const *lsCur = layerDef.stages()[ls.stage];
                ls.inter = 1 - (ls.tics - frameTimePos) / float( lsCur->tics );
            }
        }
    }

    if(d->material->isShiny())
    {
        Material::ShineLayer const &layerDef = d->material->shineLayer();
        // Not animated?
        if(layerDef.stageCount() > 1)
        {
            LayerState &ls = d->shineLayer;

            if(DD_IsSharpTick() && ls.tics-- <= 0)
            {
                // Advance to next stage.
                if(++ls.stage == layerDef.stageCount())
                {
                    // Loop back to the beginning.
                    ls.stage = 0;
                }
                ls.inter = 0;

                Material::ShineLayer::Stage const *lsCur = layerDef.stages()[ls.stage];
                if(lsCur->variance != 0)
                    ls.tics = lsCur->tics * (1 - lsCur->variance * RNG_RandFloat());
                else
                    ls.tics = lsCur->tics;
            }
            else
            {
                Material::ShineLayer::Stage const *lsCur = layerDef.stages()[ls.stage];
                ls.inter = 1 - (ls.tics - frameTimePos) / float( lsCur->tics );
            }
        }
    }

    /*
     * Animate decorations:
     */
    Material::Decorations const &decorations = d->material->decorations();
    for(int i = 0; i < decorations.count(); ++i)
    {
        Material::Decoration const *lightDef = decorations[i];

        // Not animated?
        if(lightDef->stageCount() == 1) continue;
        DecorationState &ds = d->decorations[i];

        if(DD_IsSharpTick() && ds.tics-- <= 0)
        {
            // Advance to next stage.
            if(++ds.stage == lightDef->stageCount())
            {
                // Loop back to the beginning.
                ds.stage = 0;
            }
            ds.inter = 0;

            ded_decorlight_stage_t const *lsCur = lightDef->stages()[ds.stage];
            if(lsCur->variance != 0)
                ds.tics = lsCur->tics * (1 - lsCur->variance * RNG_RandFloat());
            else
                ds.tics = lsCur->tics;

            // Notify interested parties about this.
            if(d->spec.context == MC_MAPSURFACE)
            {
                // Surfaces using this material may need to be updated.
                R_UpdateMapSurfacesOnMaterialChange(d->material);
            }
        }
        else
        {
            ded_decorlight_stage_t const *lsCur = lightDef->stages()[ds.stage];
            ds.inter = 1 - (ds.tics - frameTimePos) / float( lsCur->tics );
        }
    }
}

Material::Snapshot const &Material::Variant::prepare(bool forceSnapshotUpdate)
{
    // Acquire the snapshot we are interested in.
    Material::Snapshot *snapshot = d->snapshot;
    if(!snapshot)
    {
        // Time to allocate the snapshot.
        snapshot = new Material::Snapshot(*this);
        d->attachSnapshot(*snapshot);

        // Update the snapshot right away.
        forceSnapshotUpdate = true;
    }
    else if(d->snapshotPrepareFrame != frameCount)
    {
        // Time to update the snapshot.
        forceSnapshotUpdate = true;
    }

    // We have work to do?
    if(forceSnapshotUpdate)
    {
        d->snapshotPrepareFrame = frameCount;
        snapshot->update();
    }

    return *snapshot;
}

void Material::Variant::resetAnim()
{
    if(!d->material->isValid()) return;

    Material::Layers const &layers = d->material->layers();
    for(int i = 0; i < layers.count(); ++i)
    {
        LayerState &ls = d->layers[i];

        ls.stage = 0;
        ls.tics  = layers[i]->stages()[0]->tics;
        ls.inter = 0;
    }

    if(d->material->isDetailed())
    {
        LayerState &ls = d->detailLayer;
        ls.stage = 0;
        ls.tics  = d->material->detailLayer().stages()[0]->tics;
        ls.inter = 0;
    }

    if(d->material->isShiny())
    {
        LayerState &ls = d->shineLayer;
        ls.stage = 0;
        ls.tics  = d->material->shineLayer().stages()[0]->tics;
        ls.inter = 0;
    }

    Material::Decorations const &decorations = d->material->decorations();
    for(int i = 0; i < decorations.count(); ++i)
    {
        DecorationState &ds = d->decorations[i];

        ds.stage = 0;
        ds.tics  = decorations[i]->stages()[0]->tics;
        ds.inter = 0;
    }
}

Material::Variant::LayerState const &Material::Variant::layer(int layerNum) const
{
    if(layerNum >= 0 && layerNum < d->material->layerCount())
    {
        return d->layers[layerNum];
    }
    /// @throw Material::UnknownLayerError Invalid layer reference.
    throw Material::UnknownLayerError("Material::Variant::layer", QString("Invalid material layer #%1").arg(layerNum));
}

Material::Variant::LayerState const &Material::Variant::detailLayer() const
{
    if(d->material->isDetailed())
    {
        return d->detailLayer;
    }
    /// @throw Material::UnknownLayerError The material has no details layer.
    throw Material::UnknownLayerError("Material::Variant::detailLayer", "Material has no details layer");
}

Material::Variant::LayerState const &Material::Variant::shineLayer() const
{
    if(d->material->isShiny())
    {
        return d->shineLayer;
    }
    /// @throw Material::UnknownLayerError The material has no shine layer.
    throw Material::UnknownLayerError("Material::Variant::shineLayer", "Material has no shine layer");
}

Material::Variant::DecorationState const &Material::Variant::decoration(int decorNum) const
{
    if(decorNum >= 0 && decorNum < d->material->decorationCount())
    {
        return d->decorations[decorNum];
    }
    /// @throw Material::UnknownDecorationError Invalid decoration reference.
    throw Material::UnknownDecorationError("Material::Variant::decoration", QString("Invalid material decoration #%1").arg(decorNum));
}

Material::Snapshot *Material::Variant::snapshot() const
{
    return d->snapshot;
}

//} // namespace de
