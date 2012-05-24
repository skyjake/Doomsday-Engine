/**\file materialvariant.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "m_misc.h"
#include "texture.h"

#include "materialvariant.h"

struct materialvariant_s {
    materialvariant_layer_t _layers[MATERIALVARIANT_MAXLAYERS];

    /// Superior Material of which this is a derivative.
    struct material_s* _generalCase;

    /// For "smoothed" Material animation:
    struct materialvariant_s* _current;
    struct materialvariant_s* _next;
    float _inter;

    /// Specification used to derive this variant.
    const materialvariantspecification_t* _spec;

    /// Cached copy of current state if any.
    materialsnapshot_t* _snapshot;

    /// Frame count when MaterialVariant::_snapshot was last prepared/updated.
    int _snapshotPrepareFrame;
};

materialvariant_t* MaterialVariant_New(material_t* generalCase,
    const materialvariantspecification_t* spec)
{
    materialvariant_t* mat;
    ded_material_t* def = Material_Definition(generalCase);
    int i, layerCount = Material_LayerCount(generalCase);
    assert(generalCase && spec);

    mat = (materialvariant_t*) malloc(sizeof *mat);
    if(!mat)
        Con_Error("MaterialVariant::Construct: Failed on allocation of %lu bytes for new MaterialVariant.", (unsigned long) sizeof *mat);

    mat->_generalCase = generalCase;
    mat->_spec = spec;
    mat->_current = mat->_next = mat;
    mat->_inter = 0;
    mat->_snapshot = NULL;

    // Initialize layers.
    for(i = 0; i < layerCount; ++i)
    {
        mat->_layers[i].stage = 0;
        mat->_layers[i].tics = def->layers[i].stages[0].tics;
        mat->_layers[i].glow = def->layers[i].stages[0].glow;
        mat->_layers[i].texture = Textures_ToTexture(Textures_ResolveUri2(def->layers[i].stages[0].texture, true/*quiet please*/));
        mat->_layers[i].texOrigin[0] = def->layers[i].stages[0].texOrigin[0];
        mat->_layers[i].texOrigin[1] = def->layers[i].stages[0].texOrigin[1];
    }
    return mat;
}

void MaterialVariant_Delete(materialvariant_t* mat)
{
    assert(mat);
    if(mat->_snapshot)
        free(mat->_snapshot), mat->_snapshot = NULL;
    free(mat);
}

void MaterialVariant_Ticker(materialvariant_t* mat, timespan_t time)
{
    const ded_material_t* def;
    int i, layerCount;
    assert(mat);

    def = Material_Definition(mat->_generalCase);
    if(!def)
    {
        // Material is no longer valid. We can't yet purge them because
        // we lack a reference counting mechanism (the game may be holding
        // Material pointers and/or indices, which are considered eternal).
        return;
    }

    // Update layers.
    layerCount = Material_LayerCount(mat->_generalCase);
    for(i = 0; i < layerCount; ++i)
    {
        const ded_material_layer_t* lDef = &def->layers[i];
        const ded_material_layer_stage_t* lsDef, *lsDefNext;
        materialvariant_layer_t* layer = &mat->_layers[i];
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

        /*{const Texture* glTex;
        if((glTex = Textures_ResolveUri(lsDef->texture)))
        {
            layer->tex = Texture_Id(glTex);
            MaterialVariant_SetTranslationPoint(mat, inter);
        }
        else
        {
            /// @todo Should reset this to the non-stage animated texture here.
            //layer->tex = 0;
            //generalCase->inter = 0;
        }}*/

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

void MaterialVariant_ResetAnim(materialvariant_t* mat)
{
    int i, layerCount;
    assert(mat);

    layerCount = Material_LayerCount(mat->_generalCase);
    for(i = 0; i < layerCount; ++i)
    {
        materialvariant_layer_t* ml = &mat->_layers[i];
        if(ml->stage == -1) break;
        ml->stage = 0;
    }
}

material_t* MaterialVariant_GeneralCase(materialvariant_t* mat)
{
    assert(mat);
    return mat->_generalCase;
}

const materialvariantspecification_t* MaterialVariant_Spec(const materialvariant_t* mat)
{
    assert(mat);
    return mat->_spec;
}

const materialvariant_layer_t* MaterialVariant_Layer(materialvariant_t* mat, int layer)
{
    assert(mat);
    if(layer >= 0 && layer < Material_LayerCount(mat->_generalCase))
        return &mat->_layers[layer];
    return NULL;
}

materialsnapshot_t* MaterialVariant_AttachSnapshot(materialvariant_t* mat, materialsnapshot_t* ms)
{
    assert(mat && ms);
    if(mat->_snapshot)
    {
#if _DEBUG
        Con_Message("Warning:MaterialVariant::AttachSnapshot: A snapshot is already attached to %p, it will be replaced.\n", (void*) mat);
#endif
        free(mat->_snapshot);
    }
    mat->_snapshot = ms;
    return ms;
}

materialsnapshot_t* MaterialVariant_DetachSnapshot(materialvariant_t* mat)
{
    materialsnapshot_t* ms;
    assert(mat);
    ms = mat->_snapshot;
    mat->_snapshot = NULL;
    return ms;
}

materialsnapshot_t* MaterialVariant_Snapshot(const materialvariant_t* mat)
{
    assert(mat);
    return mat->_snapshot;
}

int MaterialVariant_SnapshotPrepareFrame(const materialvariant_t* mat)
{
    assert(mat);
    return mat->_snapshotPrepareFrame;
}

void MaterialVariant_SetSnapshotPrepareFrame(materialvariant_t* mat, int frame)
{
    assert(mat);
    mat->_snapshotPrepareFrame;
}

materialvariant_t* MaterialVariant_TranslationNext(materialvariant_t* mat)
{
    assert(mat);
    return mat->_next;
}

materialvariant_t* MaterialVariant_TranslationCurrent(materialvariant_t* mat)
{
    assert(mat);
    return mat->_current;
}

float MaterialVariant_TranslationPoint(materialvariant_t* mat)
{
    assert(mat);
    return mat->_inter;
}

void MaterialVariant_SetTranslation(materialvariant_t* mat,
    materialvariant_t* current, materialvariant_t* next)
{
    assert(mat);
    if(next && current)
    {
        mat->_current = current;
        mat->_next = next;
        mat->_inter = 0;
        return;
    }

    mat->_current = mat->_next = mat;
    mat->_inter = 0;
}

void MaterialVariant_SetTranslationPoint(materialvariant_t* mat, float inter)
{
    assert(mat);
    mat->_inter = inter;
}
