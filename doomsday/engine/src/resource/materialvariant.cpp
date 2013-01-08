/** @file materialvariant.cpp Logical Material Variant.
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

#include <de/Error>
#include <de/Log>
#include <de/mathutil.h>
#include <de/memory.h>

#include "de_base.h"
#include "de_console.h"
#include "de_resource.h"

#include "resource/materialsnapshot.h"
#include "render/r_main.h"
#include "gl/gl_texmanager.h"

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

    /// For "smoothed" Material animation:
    bool hasTranslation;
    MaterialVariant *current;
    MaterialVariant *next;
    float inter;

    /// Specification used to derive this variant.
    MaterialVariantSpec const *varSpec;

    /// Cached copy of current state if any.
    MaterialSnapshot *snapshot;

    /// Frame count when the snapshot was last prepared/updated.
    int snapshotPrepareFrame;

    MaterialVariant::LayerState layers[MaterialVariant::max_layers];

    Instance(material_t &generalCase, MaterialVariantSpec const &spec,
             ded_material_t const &def)
        : material(&generalCase),
          hasTranslation(false), current(0), next(0), inter(0),
          varSpec(&spec), snapshot(0), snapshotPrepareFrame(0)
    {
        // Initialize layer states.
        int const layerCount = Material_LayerCount(material);
        for(int i = 0; i < layerCount; ++i)
        {
            layers[i].stage = 0;
            layers[i].tics = def.layers[i].stages[0].tics;
            layers[i].glowStrength = def.layers[i].stages[0].glowStrength;

            layers[i].texture = 0;
            de::Uri *texUri = reinterpret_cast<de::Uri *>(def.layers[i].stages[0].texture);
            if(texUri)
            {
                try
                {
                    layers[i].texture = App_Textures()->find(*texUri).texture();
                }
                catch(Textures::NotFoundError const &)
                {} // Ignore this error.
            }

            layers[i].texOrigin[0] = def.layers[i].stages[0].texOrigin[0];
            layers[i].texOrigin[1] = def.layers[i].stages[0].texOrigin[1];
        }
    }

    ~Instance()
    {
        if(snapshot) M_Free(snapshot);
    }
};

MaterialVariant::MaterialVariant(material_t &generalCase, MaterialVariantSpec const &spec,
                                 ded_material_t const &def)
{
    d = new Instance(generalCase, spec, def);
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
    return *d->varSpec;
}

void MaterialVariant::ticker(timespan_t /*time*/)
{
    ded_material_t const *def = Material_Definition(d->material);
    if(!def)
    {
        // Material is no longer valid. We can't yet purge them because
        // we lack a reference counting mechanism (the game may be holding
        // Material pointers and/or indices, which are considered eternal).
        return;
    }

    // Update layers.
    int const layerCount = Material_LayerCount(d->material);
    for(int i = 0; i < layerCount; ++i)
    {
        ded_material_layer_t const *lDef = &def->layers[i];
        ded_material_layer_stage_t const *lsDef, *lsDefNext;
        LayerState &layer = d->layers[i];
        float inter;

        if(!(lDef->stageCount.num > 1)) continue;

        if(layer.tics-- <= 0)
        {
            // Advance to next stage.
            if(++layer.stage == lDef->stageCount.num)
            {
                // Loop back to the beginning.
                layer.stage = 0;
            }

            lsDef = &lDef->stages[layer.stage];
            if(lsDef->variance != 0)
                layer.tics = lsDef->tics * (1 - lsDef->variance * RNG_RandFloat());
            else
                layer.tics = lsDef->tics;

            inter = 0;
        }
        else
        {
            lsDef = &lDef->stages[layer.stage];
            inter = 1.0f - (layer.tics - frameTimePos) / (float) lsDef->tics;
        }

        /* Texture const *tex;
        de::Uri *texUri = reinterpret_cast<de::Uri *>(lsDef->texture);
        if(texUri && (tex = Textures::resolveUri(*texUri)))
        {
            layer.tex = tex->id();
            setTranslationPoint(inter);
        }
        else
        {
            /// @todo Should reset this to the non-stage animated texture here.
            //layer.tex = 0;
            //generalCase->inter = 0;
        }*/

        if(inter == 0)
        {
            layer.glowStrength = lsDef->glowStrength;
            layer.texOrigin[0] = lsDef->texOrigin[0];
            layer.texOrigin[1] = lsDef->texOrigin[1];
            continue;
        }
        lsDefNext = &lDef->stages[(layer.stage + 1) % lDef->stageCount.num];

        layer.glowStrength = lsDefNext->glowStrength * inter + lsDef->glowStrength * (1 - inter);

        /// @todo Implement a more useful method of interpolation (but what? what do we want/need here?).
        layer.texOrigin[0] = lsDefNext->texOrigin[0] * inter + lsDef->texOrigin[0] * (1 - inter);
        layer.texOrigin[1] = lsDefNext->texOrigin[1] * inter + lsDef->texOrigin[1] * (1 - inter);
    }
}

void MaterialVariant::resetAnim()
{
    int const layerCount = Material_LayerCount(d->material);
    for(int i = 0; i < layerCount; ++i)
    {
        LayerState &ml = d->layers[i];
        if(ml.stage == -1) break;
        ml.stage = 0;
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

} // namespace de
