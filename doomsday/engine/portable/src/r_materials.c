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

materialnum_t numMaterials;
material_t** materials;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static boolean isCustomMaterial(const material_t* mat)
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
        materialnum_t       i;

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
    materialnum_t       i;

    // Mark all existing textures as in need of update.
    for(i = 0; i < numMaterials; ++i)
        materials[i]->flags |= MATF_CHANGED;
}

/**
 * Deletes all GL textures of materials which match the specified type.
 *
 * @param type          The type of material.
 */
void R_DeleteMaterialTextures(materialtype_t type)
{
    materialnum_t       i;

    for(i = 0; i < numMaterials; ++i)
        if(materials[i]->type == type)
            R_MaterialDeleteTex(materials[i]);
}

/**
 * Updates the minification mode of ALL registered materials.
 *
 * @param minMode       The DGL minification mode to set.
 */
void R_SetAllMaterialsMinMode(int minMode)
{
    materialnum_t       i;

    for(i = 0; i < numMaterials; ++i)
        R_MaterialSetMinMode(materials[i], minMode);
}

/**
 * Create a new material. If there exists one by the same name and of the
 * same material type, it is returned else a new material is created.
 *
 * \note: May fail if the name is invalid.
 *
 * @param name          Name of the new material.
 * @param ofTypeId      Texture/Flat/Sprite etc idx.
 * @param type          MAT_* material type.
 *
 * @return              The created material, ELSE @c NULL.
 */
material_t* R_MaterialCreate(const char* name, int ofTypeID,
                             materialtype_t type)
{
    materialnum_t       i;
    material_t*         mat;

    if(!name || !name[0])
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

/**
 * Given a Texture/Flat/Sprite etc idx and a MAT_* material type, search
 * for a matching material.
 *
 * @param ofTypeID      Texture/Flat/Sprite etc idx.
 * @param type          MAT_* material type.
 *
 * @return              The associated material, ELSE @c NULL.
 */
material_t *R_GetMaterial(int ofTypeID, materialtype_t type)
{
    materialnum_t       i;
    material_t*         mat;

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
 * Given a unique material number return the associated material.
 *
 * @param num           Unique material number.
 *
 * @return              The associated material, ELSE @c NULL.
 */
material_t* R_GetMaterialByNum(materialnum_t num)
{
    if(num != 0 && num <= numMaterials)
        return materials[num - 1]; // 1-based index.

    return NULL;
}

/**
 * Retrieve the unique material number for the given material.
 *
 * @param mat           The material to retrieve the unique number of.
 *
 * @return              The associated unique number.
 */
materialnum_t R_GetMaterialNum(const material_t* mat)
{
    if(mat)
    {
        materialnum_t       i;

        for(i = 0; i < numMaterials; ++i)
            if(materials[i] == mat)
                return i + 1; // 1-based index.
    }

    return 0;
}

/**
 * Given a name and material type, search the materials db for a match.
 * \note Part of the Doomsday public API.
 *
 * @param name          Name of the material to search for.
 * @param type          MAT_* material type.
 *
 * @return              Unique number of the found material, else zero.
 */
materialnum_t R_MaterialCheckNumForName(const char* name,
                                        materialtype_t type)
{
    materialnum_t       i;

    for(i = 0; i < numMaterials; ++i)
    {
        material_t*         mat = materials[i];

        if(mat->type == type && !strncasecmp(mat->name, name, 8))
            return i + 1;
    }

    return 0;
}

/**
 * Given a name and material type, search the materials db for a match.
 * \note Part of the Doomsday public API.
 * \note2 Same as R_MaterialCheckNumForName except will log an error
 *        message if the material being searched for is not found.
 *
 * @param name          Name of the material to search for.
 * @param type          MAT_* material type.
 *
 * @return              Unique identifier of the found material, else zero.
 */
materialnum_t R_MaterialNumForName(const char* name, materialtype_t type)
{
    materialnum_t       i;

    for(i = 0; i < numMaterials; ++i)
    {
        material_t*         mat = materials[i];

        if(mat->type == type && !strncasecmp(mat->name, name, 8))
            return i + 1;
    }

    // Not found.
    if(!levelSetup) // Don't announce during level setup.
        Con_Message("R_MaterialNumForName: %.8s type %i not found!\n",
                    name, type);
    return 0;
}

/**
 * Given a unique material identifier, lookup the associated name.
 *
 * @param num           The unique identifier.
 *
 * @return              The associated name.
 */
const char *R_MaterialNameForNum(materialnum_t num)
{
    if(num != 0 && num <= numMaterials)
        return materials[num-1]->name;

    return NULL;
}

/**
 * Sets the minification mode of the specified material.
 *
 * @param mat           The material to be updated.
 * @param minMode       The DGL minification mode to set.
 */
void R_MaterialSetMinMode(material_t* mat, int minMode)
{
    if(mat)
    {
        if(mat->dgl.tex) // Is the texture loaded?
        {
            DGL_Bind(mat->dgl.tex);
            DGL_TexFilter(DGL_MIN_FILTER, minMode);
        }
    }
}

void R_MaterialSetTranslation(material_t* mat, material_t* current,
                              material_t* next, float inter)
{
    if(!mat || !current || !next)
    {
#if _DEBUG
Con_Error("R_MaterialSetTranslation: Invalid paramaters.");
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
void R_MaterialGetColor(material_t* mat, float* rgb)
{
    if(mat)
    {
        // Ensure we've already prepared this material.
        if(!mat->dgl.tex)
            GL_PrepareMaterial(mat);

        memcpy(rgb, mat->dgl.color, sizeof(float) * 3);
    }
}

/**
 * Retrieve the reflection definition associated with the material.
 *
 * @return              The associated reflection definition, else @c NULL.
 */
ded_reflection_t* R_MaterialGetReflection(material_t* mat)
{
    if(mat)
    {
        // Ensure we've already prepared this material.
        if(!mat->dgl.tex)
            GL_PrepareMaterial(mat);

        return mat->reflection;
    }

    return NULL;
}

/**
 * Retrieve the decoration definition associated with the material.
 *
 * @return              The associated decoration definition, else @c NULL.
 */
const ded_decor_t* R_MaterialGetDecoration(material_t* mat)
{
    if(mat)
    {
        // Ensure we've already prepared this material.
        if(!mat->dgl.tex)
            GL_PrepareMaterial(mat);

        return mat->current->decoration;
    }

    return NULL;
}

/**
 * Retrieve the ptcgen definition associated with the material.
 *
 * @return              The associated ptcgen definition, else @c NULL.
 */
const ded_ptcgen_t* R_MaterialGetPtcGen(material_t* mat)
{
    if(mat)
    {
        // Ensure we've already prepared this material.
        if(!mat->dgl.tex)
            GL_PrepareMaterial(mat);

        return mat->ptcGen;
    }

    return NULL;
}

/**
 * Populate 'info' with information about the requested material.
 *
 * @param mat           Material num.
 * @param info          Ptr to info to be populated.
 */
boolean R_MaterialGetInfo(materialnum_t num, materialinfo_t* info)
{
    material_t*         mat;

    if(!info)
        Con_Error("R_MaterialGetInfo: Info paramater invalid.");

    if(!(mat = R_GetMaterialByNum(num)))
        return false;

    info->num = num;
    info->type = mat->type;
    info->width = (int) mat->width;
    info->height = (int) mat->height;

    return true;
}

/**
 * Deletes a texture (not for rawlumptexs' etc.).
 */
void R_MaterialDeleteTex(material_t* mat)
{
    if(mat->dgl.tex)
    {
        DGL_DeleteTextures(1, &mat->dgl.tex);
        mat->dgl.tex = 0;
    }
}

/**
 * @return              @c true, if the texture is probably not from
 *                      the original game.
 */
boolean R_MaterialIsCustom(materialnum_t num)
{
    return isCustomMaterial(R_GetMaterialByNum(num));
}

boolean R_MaterialIsCustom2(const material_t* mat)
{
    return isCustomMaterial(mat);
}

/**
 * Prepares all resources associated with the specified material including
 * all in the same animation group.
 */
void R_MaterialPrecache(material_t* mat)
{
    if(mat->inGroup)
    {   // The material belongs in one or more animgroups.
        int                 i;

        for(i = 0; i < numgroups; ++i)
        {
            if(R_IsInAnimGroup(groups[i].id, mat))
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
