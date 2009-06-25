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

/**
 * Subroutine of Material_Prepare().
 */
static __inline void setTexUnit(material_snapshot_t* ss, byte unit,
                                blendmode_t blendMode, int magMode,
                                const gltexture_inst_t* texInst,
                                float sScale, float tScale,
                                float sOffset, float tOffset, float alpha)
{
    material_textureunit_t* mtp = &ss->units[unit];

    mtp->texInst = texInst;
    mtp->magMode = magMode;
    mtp->blendMode = blendMode;
    mtp->alpha = MINMAX_OF(0, alpha, 1);
    mtp->scale[0] = sScale;
    mtp->scale[1] = tScale;
    mtp->offset[0] = 0;
    mtp->offset[1] = 0;
}

byte Material_Prepare(material_snapshot_t* snapshot, material_t* mat,
                      boolean smoothed, material_load_params_t* params)
{
    uint                i;
    byte                tmpResult = 0;
    const gltexture_inst_t* texInst[DDMAX_MATERIAL_LAYERS];
    const gltexture_inst_t* detailInst = NULL, *shinyInst = NULL,
                       *shinyMaskInst = NULL;

    if(!mat || novideo)
        return 0;

    if(smoothed)
        mat = mat->current;

    assert(mat->numLayers > 0);

    // Ensure all resources needed to visualize this material are loaded.
    for(i = 0; i < mat->numLayers; ++i)
    {
        material_layer_t*   ml = &mat->layers[i];
        byte                result;

        // Pick the instance matching the specified context.
        texInst[i] = GL_PrepareGLTexture(ml->tex, params, &result);

        if(result)
            tmpResult = result;
    }

    if(tmpResult)
    {   // We need to update the assocated enhancements.
        // Decorations (lights, models, etc).
        mat->decoration = Def_GetDecoration(mat, tmpResult == 2);

        // Reflection (aka shiny surface).
        mat->reflection = Def_GetReflection(mat, tmpResult == 2);

        // Generator (particles).
        mat->ptcGen = Def_GetGenerator(mat, tmpResult == 2);

        // Detail texture.
        mat->detail = Def_GetDetailTex(mat, tmpResult == 2);
    }

    // Do we need to prepare any lightmaps?
    if(mat->decoration)
    {
        /**
         * \todo No need to look up the lightmap texture records every time!
         */

        for(i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
        {
            const ded_decorlight_t* light = &mat->decoration->lights[i];
            lightmap_t*         lmap;

            if(!R_IsValidLightDecoration(light))
                break;

            if((lmap = R_GetLightMap(light->up.id)))
                GL_PrepareGLTexture(lmap->id, NULL, NULL);
            if((lmap = R_GetLightMap(light->down.id)))
                GL_PrepareGLTexture(lmap->id, NULL, NULL);
            if((lmap = R_GetLightMap(light->sides.id)))
                GL_PrepareGLTexture(lmap->id, NULL, NULL);
        }
    }

    // Do we need to prepare a detail texture?
    if(mat->detail)
    {
        detailtex_t*        dTex;
        lumpnum_t           lump =
            W_CheckNumForName(mat->detail->detailLump.path);
        const char*         external =
            (mat->detail->isExternal? mat->detail->detailLump.path : NULL);

        /**
         * \todo No need to look up the detail texture record every time!
         * This will change anyway once the gltexture for the detailtex is
         * linked to (and prepared) via the layers (above).
         */

        if((dTex = R_GetDetailTexture(lump, external)))
        {
            float               contrast =
                mat->detail->strength * detailFactor;

            // Pick an instance matching the specified context.
            detailInst = GL_PrepareGLTexture(dTex->id, &contrast, NULL);
        }
    }

    // Do we need to prepare a shiny texture (and possibly a mask)?
    if(mat->reflection)
    {
        shinytex_t*         sTex;
        masktex_t*          mTex;

        /**
         * \todo No need to look up the shiny texture record every time!
         * This will change anyway once the gltexture for the shinytex is
         * linked to (and prepared) via the layers (above).
         */

        if((sTex = R_GetShinyTexture(mat->reflection->shinyMap.path)))
        {
            // Pick an instance matching the specified context.
            shinyInst = GL_PrepareGLTexture(sTex->id, NULL, NULL);
        }

        if(shinyInst && // Don't bother searching unless the above succeeds.
           (mTex = R_GetMaskTexture(mat->reflection->maskMap.path)))
        {
            // Pick an instance matching the specified context.
            shinyMaskInst = GL_PrepareGLTexture(mTex->id, NULL, NULL);
        }
    }

    // If we arn't taking a snapshot, get out of here.
    if(!snapshot)
        return tmpResult;

    /**
     * Take a snapshot:
     */

    // Reset to the default state.
    for(i = 0; i < DDMAX_MATERIAL_LAYERS; ++i)
        setTexUnit(snapshot, i, BM_NORMAL, GL_LINEAR, NULL, 1, 1, 0, 0, 0);

    snapshot->width = mat->width;
    snapshot->height = mat->height;

    // Setup the primary texturing pass.
    if(mat->layers[0].tex)
    {
        const gltexture_t*  tex = GL_GetGLTexture(mat->layers[0].tex);
        int                 c;
        int                 magMode = glmode[texMagMode];

        if(tex->type == GLT_SPRITE)
            magMode = filterSprites? GL_LINEAR : GL_NEAREST;

        setTexUnit(snapshot, MTU_PRIMARY, BM_NORMAL, magMode, texInst[0],
                   1.f / snapshot->width, 1.f / snapshot->height, 0, 0, 1);

        snapshot->isOpaque = !(texInst[0]->flags & GLTF_MASKED);

        /// \fixme what about the other texture types?
        if(tex->type == GLT_DOOMTEXTURE || tex->type == GLT_FLAT)
        {
            for(c = 0; c < 3; ++c)
            {
                snapshot->color[c] = texInst[0]->data.texture.color[c];
                snapshot->topColor[c] = texInst[0]->data.texture.topColor[c];
            }
        }
        else
        {
            snapshot->color[CR] = snapshot->color[CG] =
                snapshot->color[CB] = 1;

            snapshot->topColor[CR] = snapshot->topColor[CG] =
                snapshot->topColor[CB] = 1;
        }
    }

    /**
     * If skymasked, we need only need to update the primary tex unit
     * (this is due to it being visible when skymask debug drawing is
     * enabled).
     */
    if(!(mat->flags & MATF_SKYMASK))
    {
        // Setup the detail texturing pass?
        if(detailInst && snapshot->isOpaque)
        {
            float               width, height, scale;

            width  = GLTexture_GetWidth(detailInst->tex);
            height = GLTexture_GetHeight(detailInst->tex);
            scale  = MAX_OF(1, mat->detail->scale);
            // Apply the global scaling factor.
            if(detailScale > .001f)
                scale *= detailScale;

            setTexUnit(snapshot, MTU_DETAIL, BM_NORMAL,
                       GL_LINEAR, detailInst, 1.f / width * scale,
                       1.f / height * scale, 0, 0, 1);
        }

        // Setup the reflection (aka shiny) texturing pass(es)?
        if(shinyInst)
        {
            ded_reflection_t* def = mat->reflection;

            snapshot->shiny.minColor[CR] = def->minColor[CR];
            snapshot->shiny.minColor[CG] = def->minColor[CG];
            snapshot->shiny.minColor[CB] = def->minColor[CB];

            setTexUnit(snapshot, MTU_REFLECTION, def->blendMode,
                       GL_LINEAR, shinyInst, 1, 1, 0, 0, def->shininess);

            if(shinyMaskInst)
                setTexUnit(snapshot, MTU_REFLECTION_MASK, BM_NORMAL,
                           snapshot->units[MTU_PRIMARY].magMode,
                           shinyMaskInst,
                           1.f / (snapshot->width * maskTextures[
                               shinyMaskInst->tex->ofTypeID]->width),
                           1.f / (snapshot->height * maskTextures[
                               shinyMaskInst->tex->ofTypeID]->height),
                           snapshot->units[MTU_PRIMARY].offset[0],
                           snapshot->units[MTU_PRIMARY].offset[1], 1);
        }
    }

    return tmpResult;
}

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

/**
 * Retrieve the decoration definition associated with the material.
 *
 * @return              The associated decoration definition, else @c NULL.
 */
const ded_decor_t* Material_GetDecoration(material_t* mat)
{
    if(mat)
    {
        // Ensure we've already prepared this material.
        Material_Prepare(NULL, mat, true, NULL);

        return mat->current->decoration;
    }

    return NULL;
}

/**
 * Retrieve the ptcgen definition associated with the material.
 *
 * @return              The associated ptcgen definition, else @c NULL.
 */
const ded_ptcgen_t* Material_GetPtcGen(material_t* mat)
{
    if(mat)
    {
        // Ensure we've already prepared this material.
        //Material_Prepare(NULL, mat, true, NULL);

        return mat->ptcGen;
    }

    return NULL;
}

material_env_class_t Material_GetEnvClass(material_t* mat)
{
    if(mat)
    {
        if(mat->envClass == MEC_UNKNOWN)
        {
            S_MaterialClassForName(P_GetMaterialName(mat), mat->mnamespace);
        }

        if(!(mat->flags & MATF_NO_DRAW))
        {
            return mat->envClass;
        }
    }

    return MEC_UNKNOWN;
}

/**
 * Prepares all resources associated with the specified material including
 * all in the same animation group.
 *
 * \note Part of the Doomsday public API.
 *
 * \todo What about the load params? By limiting to the default params here, we
 * may be precaching unused texture instances.
 */
void Material_Precache(material_t* mat)
{
    if(!mat)
        return;

    if(mat->inAnimGroup)
    {   // The material belongs in one or more animgroups, precache the group.
        R_MaterialsPrecacheGroup(mat);
        return;
    }

    // Just this one material.
    Material_Prepare(NULL, mat, true, NULL);
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
