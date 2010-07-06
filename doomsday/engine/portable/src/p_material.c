/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * p_material.c: Materials for world surfaces.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_defs.h"

#include "s_environ.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

#if 0 // Currently unused.
void Material_Ticker(material_t* mat, timespan_t time)
{
    uint                i;
    const ded_material_t* def = mat->def;

    if(!def)
        return; // Its a system generated material.

    // Update layers
    for(i = 0; i < mat->numLayers; ++i)
    {
        material_layer_t*   layer = &mat->layers[i];

        if(!(def->layers[i].stageCount.num > 1))
            continue;

        if(layer->tics-- <= 0)
        {
            // Advance to next stage.
            if(++layer->stage == def->layers[i].stageCount.num)
            {
                // Loop back to the beginning.
                layer->stage = 0;
                continue;
            }

            layer->tics = def->layers[i].stages[layer->stage].tics *
                (1 - def->layers[i].stages[layer->stage].variance *
                    RNG_RandFloat()) * TICSPERSEC;
        }
    }
}
#endif

void Material_SetTranslation(material_t* mat, material_t* current,
                              material_t* next, float inter)
{

    if(!mat || !current || !next)
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

material_env_class_t Material_GetEnvClass(material_t* mat)
{
    if(mat)
    {
        if(mat->envClass == MEC_UNKNOWN)
        {
            mat->envClass =
                S_MaterialClassForName(P_GetMaterialName(mat), mat->mnamespace);
        }

        if(!(mat->flags & MATF_NO_DRAW))
        {
            return mat->envClass;
        }
    }

    return MEC_UNKNOWN;
}

void Material_DeleteTextures(material_t* mat)
{
    if(mat)
    {
        uint                i;

        for(i = 0; i < mat->numLayers; ++i)
            GL_ReleaseGLTexture(mat->layers[i].tex);
    }
}

/**
 * Update the material, property is selected by DMU_* name.
 */
boolean Material_SetProperty(material_t* mat, const setargs_t* args)
{
    switch(args->prop)
    {
    default:
        Con_Error("Material_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
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
    case DMU_NAMESPACE:
        DMU_GetValue(DMT_MATERIAL_MNAMESPACE, &mat->mnamespace, args, 0);
        break;
    default:
        Con_Error("Sector_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
