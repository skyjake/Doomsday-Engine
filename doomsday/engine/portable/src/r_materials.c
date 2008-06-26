/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * r_materials.c: Materials (texture/flat/sprite/etc abstract interface).
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_dgl.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_audio.h" // For texture, environmental audio properties.

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean levelSetup;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

uint numMaterials;
material_t** materials;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * One time initialization of the materials list. Called during init.
 */
void R_InitMaterials(void)
{
    numMaterials = 0;
    materials = NULL;
}

/**
 * Release all memory acquired for the materials list.
 * Called during shutdown.
 */
void R_ShutdownMaterials(void)
{
    if(materials)
    {
        uint                i;

        for(i = 0; i < numMaterials; ++i)
            Z_Free(materials[i]);

        Z_Free(materials);
        materials = NULL;
        numMaterials = 0;
    }
}

/**
 * Mark all existing materials as requiring an update. Called during a
 * renderer reset.
 */
void R_MarkMaterialsForUpdating(void)
{
    uint                i;

    // Mark all existing textures as in need of update.
    for(i = 0; i < numMaterials; ++i)
        materials[i]->flags |= MATF_CHANGED;
}

material_t* R_MaterialCreate(const char* name, int ofTypeID,
                             materialtype_t type)
{
    uint                i;
    material_t*         mat;

    if(!name)
        return NULL;

    // Check if we've already created a material for this.
    for(i = 0; i < numMaterials; ++i)
    {
        mat = materials[i];
        if(mat->type == type && !strnicmp(mat->name, name, 8))
        {
            // Update the (possibly new) meta data.
            mat->ofTypeID = ofTypeID;
            mat->flags &= ~MATF_CHANGED;
            mat->inGroup = false;
            mat->current = mat->next = mat;
            mat->inter = 0;
            mat->decoration = NULL;
            mat->ptcGen = NULL;
            mat->reflection = NULL;

            if(mat->dgl.tex)
            {
                DGL_DeleteTextures(1, &mat->dgl.tex);
                mat->dgl.tex = 0;
            }
            mat->envClass = S_MaterialClassForName(mat->name, mat->type);

            return mat; // Yep, return it.
        }
    }

    // A new material.
    mat = Z_Calloc(sizeof(*mat), PU_STATIC, 0);
    strncpy(mat->name, name, 8);
    mat->name[8] = '\0';
    mat->ofTypeID = ofTypeID;
    mat->type = type;
    mat->envClass = S_MaterialClassForName(mat->name, mat->type);
    mat->current = mat->next = mat;

    /**
     * Link the new material into the list of materials.
     */

    // Resize the existing list.
    materials =
        Z_Realloc(materials, sizeof(material_t*) * ++numMaterials, PU_STATIC);
    // Add the new material;
    materials[numMaterials - 1] = mat;

    return mat;
}

material_t *R_GetMaterial(int ofTypeID, materialtype_t type)
{
    uint                i;
    material_t         *mat;

    for(i = 0; i < numMaterials; ++i)
    {
        mat = materials[i];

        if(type == mat->type && mat->ofTypeID == ofTypeID)
        {
            if(mat->flags & MATF_NO_DRAW)
               return NULL;

            return mat;
        }
    }

    return NULL;
}

/**
 * Deletes a texture (not for rawlumptexs' etc.).
 */
void R_DeleteMaterialTex(material_t* mat)
{
    if(mat->dgl.tex)
    {
        DGL_DeleteTextures(1, &mat->dgl.tex);
        mat->dgl.tex = 0;
    }
}

/**
 * Deletes all GL textures of materials which match the specified type.
 *
 * @param type          The type of material.
 */
void R_DeleteMaterialTextures(materialtype_t type)
{
    uint                i;

    for(i = 0; i < numMaterials; ++i)
        if(materials[i]->type == type)
            R_DeleteMaterialTex(materials[i]);
}

void R_SetMaterialMinMode(int minMode)
{
    uint                i;

    // Materials: Textures, flats and sprites.
    for(i = 0; i < numMaterials; ++i)
    {
        material_t*         mat = materials[i];

        if(mat->type == MAT_TEXTURE || mat->type == MAT_FLAT ||
           mat->type == MAT_SPRITE)
        {
            if(mat->dgl.tex) // Is the texture loaded?
            {
                DGL_Bind(mat->dgl.tex);
                DGL_TexFilter(DGL_MIN_FILTER, minMode);
            }
        }
    }
}

static boolean isCustomMaterial(material_t* mat)
{
    switch(mat->type)
    {
    case MAT_TEXTURE:
        if(!R_TextureIsFromIWAD(mat->ofTypeID))
            return true;
        break;

    case MAT_FLAT:
        if(!W_IsFromIWAD(flats[mat->ofTypeID]->lump))
            return true;
        break;

    case MAT_DDTEX:
        return true; // Its definetely not.

    case MAT_SPRITE:
        if(!W_IsFromIWAD(spriteTextures[mat->ofTypeID]->lump))
            return true;
        break;
    }

    // This is most likely from the original game data.
    return false;
}

/**
 * @return              @c true, if the texture is probably not from
 *                      the original game.
 */
boolean R_IsCustomMaterial(int ofTypeID, materialtype_t type)
{
    return isCustomMaterial(R_GetMaterial(ofTypeID, type));
}

void R_SetMaterialTranslation(material_t* mat, material_t* current,
                              material_t* next, float inter)
{
    if(!mat || !current || !next)
    {
#if _DEBUG
Con_Error("R_SetMaterialTranslation: Invalid paramaters.");
#endif
        return;
    }

    mat->current = current;
    mat->next = next;
    mat->inter = 0;
}

/**
 * Copy the averaged texture color into the dest buffer 'rgb'.
 *
 * @param mat           Ptr to the material.
 * @param rgb           The dest buffer.
 */
void R_GetMaterialColor(const material_t* mat, float* rgb)
{
    if(mat)
    {
        memcpy(rgb, mat->dgl.color, sizeof(float) * 3);
    }
}

/**
 * Prepares all resources associated with the specified material including
 * all in the same animation group.
 */
void R_PrecacheMaterial(material_t* mat)
{
    if(mat->inGroup)
    {   // The material belongs in one or more animgroups.
        int                 i;

        for(i = 0; i < numgroups; ++i)
        {
            if(R_IsInAnimGroup(groups[i].id, mat->type, mat->ofTypeID))
            {
                int                 k;

                // Precache this group.
                for(k = 0; k < groups[i].count; ++k)
                {
                    animframe_t        *frame = &groups[i].frames[k];

                    GL_PrepareMaterial(frame->mat);
                }
            }
        }

        return;
    }

    // Just this one material.
    GL_PrepareMaterial(mat);
}

/**
 * Retrieve the reflection definition associated with the material.
 *
 * @return              The associated reflection definition, else @c NULL.
 */
ded_reflection_t* R_GetMaterialReflection(material_t* mat)
{
    if(!mat)
        return NULL;

    return mat->reflection;
}

/**
 * Retrieve the decoration definition associated with the material.
 *
 * @return              The associated decoration definition, else @c NULL.
 */
const ded_decor_t* R_GetMaterialDecoration(const material_t* mat)
{
    if(!mat)
        return NULL;

    return mat->current->decoration;
}

/**
 * Retrieve the ptcgen definition associated with the material.
 *
 * @return              The associated ptcgen definition, else @c NULL.
 */
const ded_ptcgen_t* P_GetMaterialPtcGen(const material_t* mat)
{
    if(!mat)
        return NULL;

    return mat->ptcGen;
}

int R_CheckMaterialNumForName(const char* name, materialtype_t type)
{
    uint                i;

    for(i = 0; i < numMaterials; ++i)
    {
        material_t*         mat = materials[i];

        if(mat->type == type && !strncasecmp(mat->name, name, 8))
            return mat->ofTypeID;
    }

    return -1;
}

const char *R_MaterialNameForNum(int ofTypeID, materialtype_t type)
{
    uint                i;

    for(i = 0; i < numMaterials; ++i)
    {
        material_t*         mat = materials[i];

        if(mat->type == type && mat->ofTypeID == ofTypeID)
            return mat->name;
    }

    return NULL;
}

int R_MaterialNumForName(const char* name, materialtype_t type)
{
    int                 i;

    i = R_CheckMaterialNumForName(name, type);

    if(i == -1 && !levelSetup)  // Don't announce during level setup.
        Con_Message("R_MaterialNumForName: %.8s type %i not found!\n",
                    name, type);
    return i;
}
