/**\file materialvariant.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Daniel Swanson <danij@dengine.net>
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

#include "materialvariant.h"

materialvariant_t* MaterialVariant_Construct(material_t* generalCase,
    materialvariantspecification_t* spec)
{
    assert(generalCase && spec);
    {
    materialvariant_t* mat = (materialvariant_t*) malloc(sizeof(*mat));
    ded_material_t* def = Material_Definition(generalCase);
    int layerCount = Material_LayerCount(generalCase);

    if(NULL == mat)
        Con_Error("MaterialVariant::Construct: Failed on allocation of %lu bytes for "
                  "new MaterialVariant.", (unsigned int) sizeof(*mat));

    mat->generalCase = generalCase;
    mat->_spec = spec;
    mat->current = mat->next = mat;
    mat->inter = 0;
    memset(&mat->layers, 0, sizeof(mat->layers));

    // Initialize layers according to the Material definition.
    { int i;
    for(i = 0; i < layerCount; ++i)
    {
        mat->layers[i].tex = generalCase->layers[i].tex;

        if(def && def->layers[i].stageCount.num > 0)
        {
            mat->layers[i].glow = def->layers[i].stages[0].glow;
            mat->layers[i].texOrigin[0] = def->layers[i].stages[0].texOrigin[0];
            mat->layers[i].texOrigin[1] = def->layers[i].stages[0].texOrigin[1];
            continue;
        }

        mat->layers[i].glow = 0;
        mat->layers[i].texOrigin[0] = generalCase->layers[i].texOrigin[0];
        mat->layers[i].texOrigin[1] = generalCase->layers[i].texOrigin[1];
    }}
    return mat;
    }
}

void MaterialVariant_Destruct(materialvariant_t* mat)
{
    assert(mat);
    free(mat);
}

material_t* MaterialVariant_GeneralCase(materialvariant_t* mat)
{
    assert(mat);
    return mat->generalCase;
}

materialvariantspecification_t* MaterialVariant_Spec(const materialvariant_t* mat)
{
    assert(mat);
    return mat->_spec;
}

void MaterialVariant_ResetGroupAnim(materialvariant_t* mat)
{
    assert(mat);
    {
    uint i, numLayers = Material_LayerCount(mat->generalCase);
    for(i = 0; i < numLayers; ++i)
    {
        materialvariant_layer_t* ml = &mat->layers[i];
        if(ml->stage == -1)
            break;
        ml->stage = 0;
    }
    }
}
void MaterialVariant_SetTranslation(materialvariant_t* mat,
    materialvariant_t* current, materialvariant_t* next)
{
    assert(mat && current);
    if(next)
    {
        mat->current = current;
        mat->next = next;
        mat->inter = 0;
        return;
    }
#if _DEBUG
    Con_Error("MaterialVariant::SetTranslation: Invalid paramaters.");
#endif
}

void MaterialVariant_SetTranslationPoint(materialvariant_t* mat, float inter)
{
    assert(mat);
    mat->inter = inter;
}
