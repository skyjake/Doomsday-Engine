/** @file materialvariant.cpp Logical Material Variant.
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

#include <de/Error>
#include <de/Log>
#include <de/memory.h>

#include "de_base.h"
#include "de_console.h"
#include "de_resource.h"

#include "m_misc.h"
#include "resource/materialsnapshot.h"
#include "render/r_main.h"

namespace de {

MaterialVariant::MaterialVariant(material_t &generalCase,
    materialvariantspecification_t const &spec, ded_material_t const &def)
    : material(&generalCase),
      hasTranslation(false), current(0), next(0), inter(0),
      varSpec(&spec), snapshot_(0), snapshotPrepareFrame_(0)
{
    // Initialize layers.
    int const layerCount = Material_LayerCount(material);
    for(int i = 0; i < layerCount; ++i)
    {
        layers[i].stage = 0;
        layers[i].tics = def.layers[i].stages[0].tics;
        layers[i].glow = def.layers[i].stages[0].glow;

        layers[i].texture = 0;
        de::Uri *texUri = reinterpret_cast<de::Uri *>(def.layers[i].stages[0].texture);
        if(texUri)
        {
            try
            {
                layers[i].texture = reinterpret_cast<texture_s *>(App_Textures()->find(*texUri).texture());
            }
            catch(Textures::NotFoundError const &)
            {} // Ignore this error.
        }

        layers[i].texOrigin[0] = def.layers[i].stages[0].texOrigin[0];
        layers[i].texOrigin[1] = def.layers[i].stages[0].texOrigin[1];
    }
}

MaterialVariant::~MaterialVariant()
{
    if(snapshot_) M_Free(snapshot_);
}

void MaterialVariant::ticker(timespan_t /*time*/)
{
    ded_material_t const *def = Material_Definition(material);
    if(!def)
    {
        // Material is no longer valid. We can't yet purge them because
        // we lack a reference counting mechanism (the game may be holding
        // Material pointers and/or indices, which are considered eternal).
        return;
    }

    // Update layers.
    int const layerCount = Material_LayerCount(material);
    for(int i = 0; i < layerCount; ++i)
    {
        ded_material_layer_t const *lDef = &def->layers[i];
        ded_material_layer_stage_t const *lsDef, *lsDefNext;
        materialvariant_layer_t *layer = &layers[i];
        float inter;

        if(!(lDef->stageCount.num > 1)) continue;

        if(layer->tics-- <= 0)
        {
            // Advance to next stage.
            if(++layer->stage == lDef->stageCount.num)
            {
                // Loop back to the beginning.
                layer->stage = 0;
            }

            lsDef = &lDef->stages[layer->stage];
            if(lsDef->variance != 0)
                layer->tics = lsDef->tics * (1 - lsDef->variance * RNG_RandFloat());
            else
                layer->tics = lsDef->tics;

            inter = 0;
        }
        else
        {
            lsDef = &lDef->stages[layer->stage];
            inter = 1.0f - (layer->tics - frameTimePos) / (float) lsDef->tics;
        }

        /* Texture const *tex;
        de::Uri *texUri = reinterpret_cast<de::Uri *>(lsDef->texture);
        if(texUri && (tex = Textures::resolveUri(*texUri)))
        {
            layer->tex = tex->id();
            setTranslationPoint(inter);
        }
        else
        {
            /// @todo Should reset this to the non-stage animated texture here.
            //layer->tex = 0;
            //generalCase->inter = 0;
        }*/

        if(inter == 0)
        {
            layer->glow = lsDef->glow;
            layer->texOrigin[0] = lsDef->texOrigin[0];
            layer->texOrigin[1] = lsDef->texOrigin[1];
            continue;
        }
        lsDefNext = &lDef->stages[(layer->stage+1) % lDef->stageCount.num];

        layer->glow = lsDefNext->glow * inter + lsDef->glow * (1 - inter);

        /// @todo Implement a more useful method of interpolation (but what? what do we want/need here?).
        layer->texOrigin[0] = lsDefNext->texOrigin[0] * inter + lsDef->texOrigin[0] * (1 - inter);
        layer->texOrigin[1] = lsDefNext->texOrigin[1] * inter + lsDef->texOrigin[1] * (1 - inter);
    }
}

void MaterialVariant::resetAnim()
{
    int const layerCount = Material_LayerCount(material);
    for(int i = 0; i < layerCount; ++i)
    {
        materialvariant_layer_t *ml = &layers[i];
        if(ml->stage == -1) break;
        ml->stage = 0;
    }
}

materialvariant_layer_t const *MaterialVariant::layer(int layer)
{
    if(layer >= 0 && layer < Material_LayerCount(material))
        return &layers[layer];
    return 0;
}

MaterialSnapshot &MaterialVariant::attachSnapshot(MaterialSnapshot &newSnapshot)
{
    if(snapshot_)
    {
        LOG_AS("MaterialVariant::AttachSnapshot");
        LOG_WARNING("A snapshot is already attached to %p, it will be replaced.") << (void *) this;
        M_Free(snapshot_);
    }
    snapshot_ = &newSnapshot;
    return newSnapshot;
}

MaterialSnapshot *MaterialVariant::detachSnapshot()
{
    MaterialSnapshot *detachedSnapshot = snapshot_;
    snapshot_ = 0;
    return detachedSnapshot;
}

void MaterialVariant::setSnapshotPrepareFrame(int frame)
{
    snapshotPrepareFrame_ = frame;
}

MaterialVariant *MaterialVariant::translationNext()
{
    if(!hasTranslation) return this;
    return next;
}

MaterialVariant* MaterialVariant::translationCurrent()
{
    if(!hasTranslation) return this;
    return current;
}

float MaterialVariant::translationPoint()
{
    return inter;
}

void MaterialVariant::setTranslation(MaterialVariant *newCurrent, MaterialVariant *newNext)
{
    if(newCurrent && newNext && (newCurrent != this || newNext != this))
    {
        hasTranslation = true;
        current = newCurrent;
        next    = newNext;
    }
    else
    {
        hasTranslation = false;
        current = next = 0;
    }
    inter = 0;
}

void MaterialVariant::setTranslationPoint(float newInter)
{
    inter = newInter;
}

} // namespace de
