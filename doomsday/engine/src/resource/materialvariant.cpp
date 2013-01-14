/** @file materialvariant.cpp Logical Material Variant.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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
#include "gl/gl_texmanager.h" // GL_CompareTextureVariantSpecifications
#include "render/r_main.h" // frameTimePos

#include "resource/materialvariant.h"

namespace de {

bool MaterialVariantSpec::compare(MaterialVariantSpec const &other) const
{
    if(this == &other) return 1;
    if(context != other.context) return 0;
    return 1 == GL_CompareTextureVariantSpecifications(primarySpec, other.primarySpec);
}

struct MaterialVariant::Instance
{
    /// Superior material of which this is a derivative.
    material_t *material;

    /// Specification used to derive this variant.
    MaterialVariantSpec const &spec;

    /// Cached animation state snapshot (if any).
    MaterialSnapshot *snapshot;

    /// Frame count when the snapshot was last prepared/updated.
    int snapshotPrepareFrame;

    /// Layer animation states.
    MaterialVariant::LayerState layers[MaterialVariant::max_layers];

    /// Decoration animation states.
    MaterialVariant::DecorationState decorations[MaterialVariant::max_decorations];

    Instance(material_t &generalCase, MaterialVariantSpec const &_spec)
        : material(&generalCase),
          spec(_spec), snapshot(0), snapshotPrepareFrame(-1)
    {}

    ~Instance()
    {
        if(snapshot) M_Free(snapshot);
    }
};

MaterialVariant::MaterialVariant(material_t &generalCase, MaterialVariantSpec const &spec)
{
    d = new Instance(generalCase, spec);
    // Initialize animation states.
    resetAnim();
}

MaterialVariant::~MaterialVariant()
{
    delete d;
}

material_t &MaterialVariant::generalCase() const
{
    return *d->material;
}

MaterialVariantSpec const &MaterialVariant::spec() const
{
    return d->spec;
}

bool MaterialVariant::isPaused() const
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

void MaterialVariant::ticker(timespan_t /*ticLength*/)
{
    // Animation ceases once the material is no longer valid.
    if(!Material_IsValid(d->material)) return;

    // Animation will only progress when not paused.
    if(isPaused()) return;

    /*
     * Animate layers:
     */
    ded_material_t const *def = Material_Definition(d->material);
    int const layerCount      = Material_LayerCount(d->material);
    for(int i = 0; i < layerCount; ++i)
    {
        ded_material_layer_t const *layerDef = &def->layers[i];

        // Not animated?
        if(layerDef->stageCount.num == 1) continue;
        LayerState &l = d->layers[i];

        if(DD_IsSharpTick() && l.tics-- <= 0)
        {
            // Advance to next stage.
            if(++l.stage == layerDef->stageCount.num)
            {
                // Loop back to the beginning.
                l.stage = 0;
            }
            l.inter = 0;

            ded_material_layer_stage_t const *lsCur = &layerDef->stages[l.stage];
            if(lsCur->variance != 0)
                l.tics = lsCur->tics * (1 - lsCur->variance * RNG_RandFloat());
            else
                l.tics = lsCur->tics;
        }
        else
        {
            ded_material_layer_stage_t const *lsCur = &layerDef->stages[l.stage];
            l.inter = 1 - (l.tics - frameTimePos) / float( lsCur->tics );
        }
    }

    /*
     * Animate decorations:
     */
    uint idx = 0;
    Material::Decorations const &decorations = Material_Decorations(d->material);
    for(Material::Decorations::const_iterator it = decorations.begin();
        it != decorations.end(); ++it, ++idx)
    {
        ded_decorlight_t const *lightDef = (*it)->def;

        // Not animated?
        if(lightDef->stageCount.num == 1) continue;
        DecorationState &l = d->decorations[idx];

        if(DD_IsSharpTick() && l.tics-- <= 0)
        {
            // Advance to next stage.
            if(++l.stage == lightDef->stageCount.num)
            {
                // Loop back to the beginning.
                l.stage = 0;
            }
            l.inter = 0;

            ded_decorlight_stage_t const *lsCur = &lightDef->stages[l.stage];
            if(lsCur->variance != 0)
                l.tics = lsCur->tics * (1 - lsCur->variance * RNG_RandFloat());
            else
                l.tics = lsCur->tics;

            // Notify interested parties about this.
            if(d->spec.context == MC_MAPSURFACE)
            {
                // Surfaces using this material may need to be updated.
                R_UpdateMapSurfacesOnMaterialChange(d->material);
            }
        }
        else
        {
            ded_decorlight_stage_t const *lsCur = &lightDef->stages[l.stage];
            l.inter = 1 - (l.tics - frameTimePos) / float( lsCur->tics );
        }
    }
}

void MaterialVariant::resetAnim()
{
    if(!Material_IsValid(d->material)) return;

    ded_material_t const *def = Material_Definition(d->material);
    int const layerCount      = Material_LayerCount(d->material);
    for(int i = 0; i < layerCount; ++i)
    {
        LayerState &l = d->layers[i];

        l.stage = 0;
        l.tics  = def->layers[i].stages[0].tics;
        l.inter = 0;
    }

    uint idx = 0;
    Material::Decorations const &decorations = Material_Decorations(d->material);
    for(Material::Decorations::const_iterator it = decorations.begin();
        it != decorations.end(); ++it, ++idx)
    {
        DecorationState &l = d->decorations[idx];

        l.stage = 0;
        l.tics  = (*it)->def->stages[0].tics;
        l.inter = 0;
    }
}

MaterialVariant::LayerState const &MaterialVariant::layer(int layerNum)
{
    if(layerNum >= 0 && layerNum < Material_LayerCount(d->material))
        return d->layers[layerNum];

    /// @throw InvalidLayerError Invalid layer reference.
    throw InvalidLayerError("MaterialVariant::layer", QString("Invalid material layer #%1").arg(layerNum));
}

MaterialVariant::DecorationState const &MaterialVariant::decoration(int decorNum)
{
    if(decorNum >= 0 && decorNum < Material_DecorationCount(d->material))
        return d->decorations[decorNum];

    /// @throw InvalidDecorationError Invalid decoration reference.
    throw InvalidDecorationError("MaterialVariant::decoration", QString("Invalid material decoration #%1").arg(decorNum));
}

MaterialSnapshot &MaterialVariant::attachSnapshot(MaterialSnapshot &newSnapshot)
{
    if(d->snapshot)
    {
        LOG_AS("MaterialVariant::AttachSnapshot");
        LOG_WARNING("A snapshot is already attached to %p, it will be replaced.") << (void *) this;
        M_Free(d->snapshot);
    }
    d->snapshot = &newSnapshot;
    return newSnapshot;
}

MaterialSnapshot *MaterialVariant::detachSnapshot()
{
    MaterialSnapshot *detachedSnapshot = d->snapshot;
    d->snapshot = 0;
    return detachedSnapshot;
}

MaterialSnapshot *MaterialVariant::snapshot() const
{
    return d->snapshot;
}

int MaterialVariant::snapshotPrepareFrame() const
{
    if(d->snapshot)
    {
        return d->snapshotPrepareFrame;
    }
    /// @throw MissingSnapshotError A snapshot is needed for this.
    throw MissingSnapshotError("MaterialVariant::snapshotPrepareFrame", "Snapshot data is required");
}

void MaterialVariant::setSnapshotPrepareFrame(int frameNum)
{
    if(d->snapshot)
    {
        d->snapshotPrepareFrame = frameNum;
    }
}

} // namespace de
