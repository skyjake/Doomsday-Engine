/** @file materialvariant.cpp Context-specialized logical material variant.
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
#include "de_network.h" // playback / clientPaused
#include "map/r_world.h" // R_UpdateMapSurfacesOnMaterialChange
#include "render/r_main.h" // frameCount, frameTimePos
#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include <de/Error>
#include <de/Log>
#include <de/mathutil.h>

#include "resource/material.h"

using namespace de;

bool MaterialVariantSpec::compare(MaterialVariantSpec const &other) const
{
    if(this == &other) return 1;
    if(context != other.context) return 0;
    return 1 == TextureVariantSpec_Compare(primarySpec, other.primarySpec);
}

DENG2_PIMPL(Material::Variant)
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

    Instance(Public *i, Material &generalCase, Material::VariantSpec const &_spec)
        : Base(i), material(&generalCase),
          spec(_spec),
          snapshot(0),
          snapshotPrepareFrame(-1)
    {}

    ~Instance()
    {
        if(snapshot) delete snapshot;
    }

    /**
     * Attach new snapshot data to the variant. If an existing snapshot is already
     * present it will be replaced. Ownership of @a materialSnapshot is given to
     * the variant.
     */
    void attachSnapshot(Material::Snapshot *newSnapshot)
    {
        DENG2_ASSERT(newSnapshot);
        if(snapshot)
        {
#ifdef DENG_DEBUG
            LOG_AS("Material::Variant::AttachSnapshot");
            LOG_WARNING("A snapshot is already attached to %p, it will be replaced.") << de::dintptr(this);
#endif
            delete snapshot;
        }
        snapshot = newSnapshot;
    }

    template <typename Type>
    void resetLayer(Material::Variant::LayerState &ls, Type const &stage)
    {
        ls.stage = 0;
        ls.tics  = stage.tics;
        ls.inter = 0;
    }

    void resetDecoration(Material::Variant::DecorationState &ds,
                         Material::Decoration::Stage const &stage)
    {
        ds.stage = 0;
        ds.tics  = stage.tics;
        ds.inter = 0;
    }

    template <typename LayerType>
    void animateLayer(Material::Variant::LayerState &ls, LayerType const &layer)
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

            LayerType::Stage const *lsCur = layer.stages()[ls.stage];
            if(lsCur->variance != 0)
                ls.tics = lsCur->tics * (1 - lsCur->variance * RNG_RandFloat());
            else
                ls.tics = lsCur->tics;
        }
        else
        {
            LayerType::Stage const *lsCur = layer.stages()[ls.stage];
            ls.inter = 1 - (ls.tics - frameTimePos) / float( lsCur->tics );
        }
    }

    void animateDecoration(Material::Variant::DecorationState &ds, Material::Decoration const &decor)
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
            if(spec.context == MC_MAPSURFACE)
            {
                // Surfaces using this material may need to be updated.
                R_UpdateMapSurfacesOnMaterialChange(material);
            }
        }
        else
        {
            Material::Decoration::Stage const *lsCur = decor.stages()[ds.stage];
            ds.inter = 1 - (ds.tics - frameTimePos) / float( lsCur->tics );
        }
    }
};

Material::Variant::Variant(Material &generalCase, Material::VariantSpec const &spec)
    : d(new Instance(this, generalCase, spec))
{
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
    // Depending on the usage context, the animation should only progress
    // when the game is not paused.
    return (clientPaused && (d->spec.context == MC_MAPSURFACE ||
                             d->spec.context == MC_SPRITE     ||
                             d->spec.context == MC_MODELSKIN  ||
                             d->spec.context == MC_PSPRITE    ||
                             d->spec.context == MC_SKYSPHERE));
}

Material::Snapshot &Material::Variant::snapshot() const
{
    // Time to attach a snapshot?
    if(!d->snapshot)
    {
        d->attachSnapshot(new Material::Snapshot(*const_cast<Material::Variant *>(this)));

        // Mark the snapshot as dirty.
        d->snapshotPrepareFrame = frameCount - 1;
    }
    return *d->snapshot;
}

Material::Snapshot const &Material::Variant::prepare(bool forceSnapshotUpdate)
{
    Material::Snapshot &snapshot_ = snapshot();
    // Time to update the snapshot?
    if(forceSnapshotUpdate || d->snapshotPrepareFrame != frameCount)
    {
        d->snapshotPrepareFrame = frameCount;
        snapshot_.update();
    }
    return snapshot_;
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
        if(layers[i]->isAnimated())
            d->animateLayer(d->layers[i], *layers[i]);
    }

    if(d->material->isDetailed() && d->material->detailLayer().isAnimated())
    {
        d->animateLayer(d->detailLayer, d->material->detailLayer());
    }

    if(d->material->isShiny() && d->material->shineLayer().isAnimated())
    {
        d->animateLayer(d->shineLayer, d->material->shineLayer());
    }

    /*
     * Animate decorations:
     */
    Material::Decorations const &decorations = d->material->decorations();
    for(int i = 0; i < decorations.count(); ++i)
    {
        if(decorations[i]->isAnimated())
            d->animateDecoration(d->decorations[i], *decorations[i]);
    }
}

void Material::Variant::resetAnim()
{
    if(!d->material->isValid()) return;

    Material::Layers const &layers = d->material->layers();
    for(int i = 0; i < layers.count(); ++i)
    {
        d->resetLayer(d->layers[i], *layers[i]->stages()[0]);
    }

    if(d->material->isDetailed())
    {
        d->resetLayer(d->detailLayer, *d->material->detailLayer().stages()[0]);
    }

    if(d->material->isShiny())
    {
        d->resetLayer(d->shineLayer, *d->material->shineLayer().stages()[0]);
    }

    Material::Decorations const &decorations = d->material->decorations();
    for(int i = 0; i < decorations.count(); ++i)
    {
        d->resetDecoration(d->decorations[i], *decorations[i]->stages()[0]);
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
