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
#  include "de_network.h" // playback /clientPaused
#endif

#include "map/r_world.h"
#include "gl/gl_texmanager.h"
#include "render/r_main.h"

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
    /// Superior Material of which this is a derivative.
    material_t *material;

    /// Specification used to derive this variant.
    MaterialVariantSpec const &spec;

    /// Cached animation state snapshot (if any).
    MaterialSnapshot *snapshot;

    /// Frame count when the snapshot was last prepared/updated.
    int snapshotPrepareFrame;

    /// Layer animation states.
    MaterialVariant::LayerState layers[MaterialVariant::max_layers];

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
    /// For "smoothed" Material animation:
    bool hasTranslation;
    MaterialVariant *current;
    MaterialVariant *next;
    float inter;
#endif

    Instance(material_t &generalCase, MaterialVariantSpec const &_spec)
        : material(&generalCase),
          spec(_spec), snapshot(0), snapshotPrepareFrame(-1)
#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
          hasTranslation(false), current(0), next(0), inter(0),
#endif
    {}

    ~Instance()
    {
        if(snapshot) M_Free(snapshot);
    }
};

MaterialVariant::MaterialVariant(material_t &generalCase, MaterialVariantSpec const &spec)
{
    d = new Instance(generalCase, spec);
    // Initialize layer animation states.
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

void MaterialVariant::ticker(timespan_t /*time*/)
{
    // Animation ceases once the material is no longer valid.
    if(!Material_IsValid(d->material)) return;

    // Animation will only progress when not paused.
    if(isPaused()) return;

    // Animate layers:
    ded_material_t const *def = Material_Definition(d->material);
    int const layerCount      = Material_LayerCount(d->material);
    for(int i = 0; i < layerCount; ++i)
    {
        ded_material_layer_t const *layerDef = &def->layers[i];

        // Not animated?
        if(layerDef->stageCount.num == 1) continue;

        ded_material_layer_stage_t const *lsCur = 0;
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

            lsCur = &layerDef->stages[l.stage];
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
            lsCur = &layerDef->stages[l.stage];
            l.inter = 1 - (l.tics - frameTimePos) / float( lsCur->tics );
        }

        if(l.inter == 0)
        {
            l.texOrigin[0] = lsCur->texOrigin[0];
            l.texOrigin[1] = lsCur->texOrigin[1];
            l.glowStrength = lsCur->glowStrength;
            continue;
        }

        // Interpolate.
        ded_material_layer_stage_t const *lsNext =
            &layerDef->stages[(l.stage + 1) % layerDef->stageCount.num];

        /// @todo Implement a more useful method of interpolation (but what? what do we want/need here?).
        l.texOrigin[0] = lsCur->texOrigin[0] * (1 - l.inter) + lsNext->texOrigin[0] * l.inter;
        l.texOrigin[1] = lsCur->texOrigin[1] * (1 - l.inter) + lsNext->texOrigin[1] * l.inter;

        l.glowStrength = lsCur->glowStrength * (1 - l.inter) + lsNext->glowStrength * l.inter;
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
        l.texOrigin[0] = def->layers[i].stages[0].texOrigin[0];
        l.texOrigin[1] = def->layers[i].stages[0].texOrigin[1];
        l.glowStrength = def->layers[i].stages[0].glowStrength;
    }
}

MaterialVariant::LayerState const &MaterialVariant::layer(int layer)
{
    if(layer >= 0 && layer < Material_LayerCount(d->material))
        return d->layers[layer];

    /// @throw InvalidLayerError Invalid layer reference.
    throw InvalidLayerError("MaterialVariant::layer", QString("Invalid material layer #%1").arg(layer));
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
    return d->snapshotPrepareFrame;
}

void MaterialVariant::setSnapshotPrepareFrame(int frame)
{
    d->snapshotPrepareFrame = frame;
}

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
MaterialVariant *MaterialVariant::translationNext()
{
    if(!d->hasTranslation) return this;
    return d->next;
}

MaterialVariant *MaterialVariant::translationCurrent()
{
    if(!d->hasTranslation) return this;
    return d->current;
}

float MaterialVariant::translationPoint()
{
    return d->inter;
}

void MaterialVariant::setTranslation(MaterialVariant *newCurrent, MaterialVariant *newNext)
{
    if(newCurrent && newNext && (newCurrent != this || newNext != this))
    {
        d->hasTranslation = true;
        d->current = newCurrent;
        d->next    = newNext;
    }
    else
    {
        d->hasTranslation = false;
        d->current = d->next = 0;
    }
    d->inter = 0;
}

void MaterialVariant::setTranslationPoint(float newInter)
{
    d->inter = newInter;
}
#endif

} // namespace de
