/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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

uint    numMaterials;
material_t **materials;

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

material_t *R_MaterialCreate(const char *name, int ofTypeID,
                             materialtype_t type)
{
    uint                i;
    material_t         *mat;

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
            return mat; // Yep, return it.
        }
    }

    // A new material.
    mat = Z_Calloc(sizeof(*mat), PU_STATIC, 0);
    strncpy(mat->name, name, sizeof(mat->name));
    mat->name[8] = '\0';
    mat->ofTypeID = ofTypeID;
    mat->type = type;
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
            if((type == MAT_FLAT && (flats[mat->ofTypeID]->flags & TXF_NO_DRAW)) ||
               (type == MAT_TEXTURE && (textures[mat->ofTypeID]->flags & TXF_NO_DRAW)))
               return NULL;

            return mat;
        }
    }

    return NULL;
}

/**
 * Deletes a texture (not for rawlumptexs' etc.).
 */
void R_DeleteMaterialTex(int ofTypeID, materialtype_t type)
{
    switch(type)
    {
    case MAT_TEXTURE:
        if(ofTypeID < 0 || ofTypeID >= numTextures)
            return;

        if(textures[ofTypeID]->tex)
        {
            DGL_DeleteTextures(1, &textures[ofTypeID]->tex);
            textures[ofTypeID]->tex = 0;
        }
        break;

    case MAT_FLAT:
        if(ofTypeID < 0 || ofTypeID >= numFlats)
            return;

        if(flats[ofTypeID]->tex)
        {
            DGL_DeleteTextures(1, &flats[ofTypeID]->tex);
            flats[ofTypeID]->tex = 0;
        }
        break;

    case MAT_SPRITE:
        if(ofTypeID < 0 || ofTypeID >= numSpriteTextures)
            return;

        if(spriteTextures[ofTypeID]->tex)
        {
            DGL_DeleteTextures(1, &spriteTextures[ofTypeID]->tex);
            spriteTextures[ofTypeID]->tex = 0;
        }
        break;

    case MAT_DDTEX:
        if(ofTypeID < 0 || ofTypeID >= NUM_DD_TEXTURES)
            return;

        if(ddTextures[ofTypeID].tex)
        {
            DGL_DeleteTextures(1, &ddTextures[ofTypeID].tex);
            ddTextures[ofTypeID].tex = 0;
        }
        break;

    default:
        Con_Error("R_DeleteMaterial: Unknown material type %i.", type);
    }
}

/**
 * @return              @c true, if the texture is probably not from
 *                      the original game.
 */
boolean R_IsCustomMaterial(int ofTypeID, materialtype_t type)
{
    int                 i, lump;

    switch(type)
    {
    case MAT_TEXTURE:
        // First check the texture definitions.
        lump = W_CheckNumForName("TEXTURE1");
        if(lump >= 0 && !W_IsFromIWAD(lump))
            return true;

        lump = W_CheckNumForName("TEXTURE2");
        if(lump >= 0 && !W_IsFromIWAD(lump))
            return true;

        // Go through the patches.
        for(i = 0; i < textures[ofTypeID]->patchCount; ++i)
        {
            if(!W_IsFromIWAD(textures[ofTypeID]->patches[i].patch))
                return true;
        }
        break;

    case MAT_FLAT:
        if(!W_IsFromIWAD(flats[ofTypeID]->lump))
            return true;
        break;

    case MAT_DDTEX:
        return true; // Its definetely not.

    case MAT_SPRITE:
        if(!W_IsFromIWAD(spriteTextures[ofTypeID]->lump))
            return true;
        break;

    default:
        Con_Error("R_IsCustomMaterial: Unknown material type %i.", type);
    }

    // This is most likely from the original game data.
    return false;
}

void R_SetMaterialTranslation(material_t *mat, material_t *current,
                              material_t *next, float inter)
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
 *
 * @return              @c true, if the material has an average color.
 */
boolean R_GetMaterialColor(const material_t *mat, float *rgb)
{
    if(!mat)
        return false;

    switch(mat->type)
    {
    case MAT_TEXTURE:
    {
        texture_t          *tex = textures[mat->ofTypeID];
        memcpy(rgb, tex->color, sizeof(float) * 3);
        break;
    }
    case MAT_FLAT:
    {
        flat_t             *flat = flats[mat->ofTypeID];
        memcpy(rgb, flat->color, sizeof(float) * 3);
        break;
    }
    default:
#ifdef _DEBUG
        Con_Message("R_GetMaterialColor: No avg color for material (type=%i id=%i).\n",
                    mat->type, mat->ofTypeID);
#endif
        return false;
    }

    return true;
}

int R_GetMaterialFlags(material_t *mat)
{
    int                 ofTypeID;
    if(!mat)
        return 0;

    switch(mat->current->type)
    {
    case MAT_TEXTURE:
        ofTypeID = mat->current->ofTypeID;
        if(ofTypeID < 0)
            return 0;

        return textures[ofTypeID]->flags;

    case MAT_FLAT:
        ofTypeID = mat->current->ofTypeID;
        if(ofTypeID < 0)
            return 0;

        return flats[ofTypeID]->flags;

    case MAT_SPRITE:
        ofTypeID = mat->current->ofTypeID;
        if(ofTypeID < 0)
            return 0;

        return spriteTextures[ofTypeID]->flags;

    default:
        return 0;
    };
}

/**
 * Retrieve the decoration definition associated with the material.
 *
 * @return              The associated decoration definition, else @c NULL
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
    int                 i;

    switch(type)
    {
    case MAT_FLAT:
        if(name[0] == '-') // No flat marker.
            return 0;

        for(i = 0; i < numFlats; ++i)
            if(!strncasecmp(flats[i]->name, name, 8))
            {
                return i;
            }
        break;

    case MAT_TEXTURE:
        if(name[0] == '-') // No texture marker.
            return 0;

        for(i = 0; i < numTextures; ++i)
            if(!strncasecmp(textures[i]->name, name, 8))
            {
                return i;
            }
            break;

    default:
        Con_Error("R_CheckMaterialNumForName: Unknown material type %i.",
                  type);
    }

    return -1;
}

const char *R_MaterialNameForNum(int ofTypeID, materialtype_t type)
{
    switch(type)
    {
    case MAT_FLAT:
        if(ofTypeID < 0 || ofTypeID > numFlats - 1)
            return NULL;
        return flats[ofTypeID]->name;

    case MAT_TEXTURE:
        if(ofTypeID < 0 || ofTypeID > numTextures - 1)
            return NULL;
        return textures[ofTypeID]->name;

    default:
        Con_Error("R_MaterialNameForNum: Unknown material type %i.", type);
    }

    return NULL;
}

int R_MaterialNumForName(const char *name, materialtype_t type)
{
    int                 i;

    i = R_CheckMaterialNumForName(name, type);

    if(i == -1 && !levelSetup)  // Don't announce during level setup.
        Con_Message("R_MaterialNumForName: %.8s type %i not found!\n",
                    name, type);
    return i;
}

unsigned int R_GetMaterialName(int ofTypeID, materialtype_t type)
{
    switch(type)
    {
    case MAT_TEXTURE:
        if(ofTypeID < 0 || ofTypeID >= numTextures)
            break;
        return textures[ofTypeID]->tex;

    case MAT_FLAT:
        if(ofTypeID < 0 || ofTypeID >= numFlats)
            break;
        return flats[ofTypeID]->tex;

    case MAT_DDTEX:
        if(ofTypeID >= NUM_DD_TEXTURES)
            break;
        return ddTextures[ofTypeID].tex;

    case MAT_SPRITE:
        if(ofTypeID < 0 || ofTypeID >= numSpriteTextures)
            break;
        return spriteTextures[ofTypeID]->tex;

    default:
        Con_Error("R_GetMaterialName: Unknown material type %i.", type);
    }

    return 0;
}
