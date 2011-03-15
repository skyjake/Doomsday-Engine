/**\file p_material.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
#include "de_render.h"
#include "de_graphics.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_defs.h"

#include "s_environ.h"
#include "texture.h"

void Material_Ticker(material_t* mat, timespan_t time)
{
    assert(mat);
    {
    const ded_material_t* def = mat->def;
    uint i;

    if(!def)
        return; // Its a system generated material.

    // Update layers
    for(i = 0; i < mat->numLayers; ++i)
    {
        const ded_material_layer_t* lDef = &def->layers[i];
        const ded_material_layer_stage_t* lsDef, *lsDefNext;
        material_layer_t* layer = &mat->layers[i];
        float inter;

        if(!(lDef->stageCount.num > 1))
            continue;

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

        {const texture_t* glTex;
        if((glTex = GL_TextureByUri(lsDef->texture)))
        {
            layer->tex = glTex->id;
            mat->inter = inter;
        }
        else
        {
            /// @fixme Should reset this to the non-stage animated texture here.
            //layer->tex = 0;
            //mat->inter = 0;
        }}

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
}

void Material_SetTranslation(material_t* mat, material_t* current, material_t* next, float inter)
{
    assert(mat);
    assert(current);
    if(!next)
    {
#if _DEBUG
Con_Error("Material_SetTranslation: Invalid paramaters.");
#endif
        return;
    }

    mat->current = current;
    mat->next = next;
    mat->inter = 0;
}

material_env_class_t Material_GetEnvClass(const material_t* mat)
{
    assert(mat);
    if(mat->flags & MATF_NO_DRAW)
        return MEC_UNKNOWN;
    return mat->envClass;
}

void Material_SetEnvClass(material_t* mat, material_env_class_t envClass)
{
    assert(mat);
    mat->envClass = envClass;
}

void Material_DeleteTextures(material_t* mat)
{
    assert(mat);
    {uint i;
    for(i = 0; i < mat->numLayers; ++i)
        GL_ReleaseTexture(mat->layers[i].tex);
    }
}

/**
 * Update the material, property is selected by DMU_* name.
 */
boolean Material_SetProperty(material_t* mat, const setargs_t* args)
{
    Con_Error("Material_SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    return true; // Unreachable.
}

/**
 * Get the value of a material property, selected by DMU_* name.
 */
boolean Material_GetProperty(const material_t* mat, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_FLAGS:
        DMU_GetValue(DMT_MATERIAL_FLAGS, &mat->flags, args, 0);
        break;
    case DMU_WIDTH:
        DMU_GetValue(DMT_MATERIAL_WIDTH, &mat->width, args, 0);
        break;
    case DMU_HEIGHT:
        DMU_GetValue(DMT_MATERIAL_HEIGHT, &mat->height, args, 0);
        break;
    default:
        Con_Error("Sector_GetProperty: No property %s.\n", DMU_Str(args->prop));
    }
    return true; // Continue iteration.
}
