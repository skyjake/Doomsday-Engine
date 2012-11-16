/**
 * @file materialvariant.cpp
 * Logical material variant. @ingroup resource
 *
 * @authors Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
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
#include "de_console.h"
#include "m_misc.h"
#include "resource/texture.h"
#include "resource/materialvariant.h"
#include <de/Error>
#include <de/LegacyCore>
#include <de/Log>
#include <de/memory.h>

de::MaterialVariant::MaterialVariant(material_t& generalCase,
    const materialvariantspecification_t& spec, const ded_material_t& def)
    : material(&generalCase),
      hasTranslation(false), current(0), next(0), inter(0),
      varSpec(&spec), snapshot_(0), snapshotPrepareFrame_(0)
{
    // Initialize layers.
    const int layerCount = Material_LayerCount(material);
    for(int i = 0; i < layerCount; ++i)
    {
        layers[i].stage = 0;
        layers[i].tics = def.layers[i].stages[0].tics;
        layers[i].glow = def.layers[i].stages[0].glow;

        layers[i].texture = Textures_ToTexture(Textures_ResolveUri2(def.layers[i].stages[0].texture, true/*quiet please*/));

        layers[i].texOrigin[0] = def.layers[i].stages[0].texOrigin[0];
        layers[i].texOrigin[1] = def.layers[i].stages[0].texOrigin[1];
    }
}

de::MaterialVariant::~MaterialVariant()
{
    if(snapshot_) M_Free(snapshot_);
}

void de::MaterialVariant::ticker(timespan_t /*time*/)
{
    const ded_material_t* def = Material_Definition(material);
    if(!def)
    {
        // Material is no longer valid. We can't yet purge them because
        // we lack a reference counting mechanism (the game may be holding
        // Material pointers and/or indices, which are considered eternal).
        return;
    }

    // Update layers.
    const int layerCount = Material_LayerCount(material);
    for(int i = 0; i < layerCount; ++i)
    {
        const ded_material_layer_t* lDef = &def->layers[i];
        const ded_material_layer_stage_t* lsDef, *lsDefNext;
        materialvariant_layer_t* layer = &layers[i];
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

        /*const Texture* glTex;
        if((glTex = Textures_ResolveUri(lsDef->texture)))
        {
            layer->tex = Texture_Id(glTex);
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

void de::MaterialVariant::resetAnim()
{
    const int layerCount = Material_LayerCount(material);
    for(int i = 0; i < layerCount; ++i)
    {
        materialvariant_layer_t* ml = &layers[i];
        if(ml->stage == -1) break;
        ml->stage = 0;
    }
}

const materialvariant_layer_t* de::MaterialVariant::layer(int layer)
{
    if(layer >= 0 && layer < Material_LayerCount(material))
        return &layers[layer];
    return 0;
}

materialsnapshot_t& de::MaterialVariant::attachSnapshot(materialsnapshot_t& newSnapshot)
{
    if(snapshot_)
    {
        LOG_AS("MaterialVariant::AttachSnapshot");
        LOG_WARNING("A snapshot is already attached to %p, it will be replaced.") << (void*) this;
        M_Free(snapshot_);
    }
    snapshot_ = &newSnapshot;
    return newSnapshot;
}

materialsnapshot_t* de::MaterialVariant::detachSnapshot()
{
    materialsnapshot_t* detachedSnapshot = snapshot_;
    snapshot_ = 0;
    return detachedSnapshot;
}

void de::MaterialVariant::setSnapshotPrepareFrame(int frame)
{
    snapshotPrepareFrame_ = frame;
}

de::MaterialVariant* de::MaterialVariant::translationNext()
{
    if(!hasTranslation) return this;
    return next;
}

de::MaterialVariant* de::MaterialVariant::translationCurrent()
{
    if(!hasTranslation) return this;
    return current;
}

float de::MaterialVariant::translationPoint()
{
    return inter;
}

void de::MaterialVariant::setTranslation(MaterialVariant* newCurrent, MaterialVariant* newNext)
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

void de::MaterialVariant::setTranslationPoint(float newInter)
{
    inter = newInter;
}

/**
 * C wrapper API
 **/

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::MaterialVariant*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<const de::MaterialVariant*>(inst) : NULL

#define TOPUBLIC(inst) \
    (inst) != 0? reinterpret_cast<MaterialVariant*>(inst) : NULL

#define TOPUBLIC_CONST(inst) \
    (inst) != 0? reinterpret_cast<const MaterialVariant*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::MaterialVariant* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    const de::MaterialVariant* self = TOINTERNAL_CONST(inst)

MaterialVariant* MaterialVariant_New(struct material_s* generalCase,
    const materialvariantspecification_t* spec)
{
    if(!generalCase)
        LegacyCore_FatalError("MaterialVariant_New: Attempted with invalid generalCase argument (=NULL).");
    if(!spec)
        LegacyCore_FatalError("MaterialVariant_New: Attempted with invalid spec argument (=NULL).");
    const ded_material_t* def = Material_Definition(generalCase);
    if(!def)
        LegacyCore_FatalError("MaterialVariant_New: Attempted on a material without a definition (=NULL).");
    return TOPUBLIC(new de::MaterialVariant(*generalCase, *spec, *def));
}

void MaterialVariant_Delete(MaterialVariant* mat)
{
    if(mat)
    {
        SELF(mat);
        delete self;
    }
}

void MaterialVariant_Ticker(MaterialVariant* mat, timespan_t time)
{
    SELF(mat);
    self->ticker(time);
}

void MaterialVariant_ResetAnim(MaterialVariant* mat)
{
    SELF(mat);
    self->resetAnim();
}

material_t* MaterialVariant_GeneralCase(MaterialVariant* mat)
{
    SELF(mat);
    return self->generalCase();
}

const materialvariantspecification_t* MaterialVariant_Spec(const MaterialVariant* mat)
{
    SELF_CONST(mat);
    return self->spec();
}

const materialvariant_layer_t* MaterialVariant_Layer(MaterialVariant* mat, int layer)
{
    SELF(mat);
    return self->layer(layer);
}

materialsnapshot_t* MaterialVariant_AttachSnapshot(MaterialVariant* mat, materialsnapshot_t* snapshot)
{
    if(!snapshot)
        LegacyCore_FatalError("MaterialVariant_AttachSnapshot: Attempted with invalid snapshot argument (=NULL).");
    SELF(mat);
    return &self->attachSnapshot(*snapshot);
}

materialsnapshot_t* MaterialVariant_DetachSnapshot(MaterialVariant* mat)
{
    SELF(mat);
    return self->detachSnapshot();
}

materialsnapshot_t* MaterialVariant_Snapshot(const MaterialVariant* mat)
{
    SELF_CONST(mat);
    return self->snapshot();
}

int MaterialVariant_SnapshotPrepareFrame(const MaterialVariant* mat)
{
    SELF_CONST(mat);
    return self->snapshotPrepareFrame();
}

void MaterialVariant_SetSnapshotPrepareFrame(MaterialVariant* mat, int frame)
{
    SELF(mat);
    self->setSnapshotPrepareFrame(frame);
}

MaterialVariant* MaterialVariant_TranslationNext(MaterialVariant* mat)
{
    SELF(mat);
    return TOPUBLIC(self->translationNext());
}

MaterialVariant* MaterialVariant_TranslationCurrent(MaterialVariant* mat)
{
    SELF(mat);
    return TOPUBLIC(self->translationCurrent());
}

float MaterialVariant_TranslationPoint(MaterialVariant* mat)
{
    SELF(mat);
    return self->translationPoint();
}

void MaterialVariant_SetTranslation(MaterialVariant* mat,
    MaterialVariant* current, MaterialVariant* next)
{
    SELF(mat);
    self->setTranslation(TOINTERNAL(current), TOINTERNAL(next));
}

void MaterialVariant_SetTranslationPoint(MaterialVariant* mat, float inter)
{
    SELF(mat);
    self->setTranslationPoint(inter);
}
