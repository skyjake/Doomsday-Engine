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
 * p_materialmanager.c: Materials manager.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h> // For tolower()

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_audio.h" // For texture, environmental audio properties.

// MACROS ------------------------------------------------------------------

#define MATERIALS_BLOCK_ALLOC (32) // Num materials to allocate per block.
#define MATERIAL_NAME_HASH_SIZE (512)

// TYPES -------------------------------------------------------------------

typedef struct materialbind_s {
    char            name[9];
    material_t*     mat;
    byte            prepared;
    struct ded_detailtexture_s* detail[2];
    struct ded_decor_s* decoration[2];
    struct ded_ptcgen_s* ptcGen[2];
    struct ded_reflection_s* reflection[2];

    uint            hashNext; // 1-based index
} materialbind_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(ListMaterials);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void animateAnimGroups(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean ddMapSetup;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

materialnum_t numMaterialBinds = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initedOk = false;

/**
 * The following data structures and variables are intrinsically linked and
 * are inter-dependant. The scheme used is somewhat complicated due to the
 * required traits of the materials themselves and in of the system itself:
 *
 * 1) Pointers to material_t are eternal, they are always valid and continue
 *    to reference the same logical material data even after engine reset.
 * 2) Public material identifiers (materialnum_t) are similarly eternal.
 *    Note that they are used to index the material name bindings array.
 * 3) Dynamic creation/update of materials.
 * 4) Material name bindings are semi-independant from the materials. There
 *    may be multiple name bindings for a given material (aliases).
 *    The only requirement is that the name is unique among materials in
 *    a given material mnamespace.
 * 5) Super-fast look up by public material identifier.
 * 6) Fast look up by material name (a hashing scheme is used).
 */
static zblockset_t* materialsBlockSet;
static material_t* materialsHead; // Head of the linked list of materials.

static materialbind_t* materialBinds;
static materialnum_t maxMaterialBinds;
static uint hashTable[NUM_MATERIAL_NAMESPACES][MATERIAL_NAME_HASH_SIZE];

// CODE --------------------------------------------------------------------

void P_MaterialManagerRegister(void)
{
    C_CMD("listmaterials",  NULL,     ListMaterials);
}

/**
 * Is the specified id a valid (known) material namespace.
 *
 * \note The special case MN_ANY is considered invalid here as it does not
 *       relate to one specific mnamespace.
 *
 * @return              @c true, iff the id is valid.
 */
static boolean isKnownMNamespace(material_namespace_t mnamespace)
{
    if(mnamespace >= MN_FIRST && mnamespace < NUM_MATERIAL_NAMESPACES)
        return true;

    return false;
}

/**
 * This is a hash function. Given a material name it generates a
 * somewhat-random number between 0 and MATERIAL_NAME_HASH_SIZE.
 *
 * @return              The generated hash index.
 */
static uint hashForName(const char* name)
{
    ushort              key = 0;
    int                 i;

    // Stop when the name ends.
    for(i = 0; *name; ++i, name++)
    {
        if(i == 0)
            key ^= (int) (*name);
        else if(i == 1)
            key *= (int) (*name);
        else if(i == 2)
        {
            key -= (int) (*name);
            i = -1;
        }
    }

    return key % MATERIAL_NAME_HASH_SIZE;
}

/**
 * Given a name and namespace, search the materials db for a match.
 * \assume Caller knows what it's doing; params arn't validity checked.
 *
 * @param name          Name of the material to search for. Must have been
 *                      transformed to all lower case.
 * @param mnamespace    Specific MG_* material namespace NOT @c MN_ANY.
 * @return              Unique number of the found material, else zero.
 */
static materialnum_t getMaterialNumForName(const char* name, uint hash,
                                           material_namespace_t mnamespace)
{
    // Go through the candidates.
    if(hashTable[mnamespace][hash])
    {
        materialbind_t*     mb = &materialBinds[
            hashTable[mnamespace][hash] - 1];

        for(;;)
        {
            material_t*        mat;

            mat = mb->mat;

            if(!strncmp(mb->name, name, 8))
                return ((mb) - materialBinds) + 1;

            if(!mb->hashNext)
                break;

            mb = &materialBinds[mb->hashNext - 1];
        }
    }

    return 0; // Not found.
}

/**
 * Given an index and namespace, search the materials db for a match.
 * \assume Caller knows what it's doing; params arn't validity checked.
 *
 * @param id            gltextureid to search for.
 * @param mnamespace    Specific MG_* material namespace NOT @c MN_ANY.
 * @return              Unique number of the found material, else zero.
 */
static materialnum_t getMaterialNumForIndex(uint idx,
                                            material_namespace_t mnamespace)
{
    materialnum_t       i;

    // Go through the candidates.
    for(i = 0; i < numMaterialBinds; ++i)
    {
        materialbind_t*     mb = &materialBinds[i];
        material_t*         mat = mb->mat;

        if(mat->mnamespace == mnamespace &&
           GL_GetGLTexture(mat->layers[0].tex)->ofTypeID == idx)
            return ((mb) - materialBinds) + 1;
    }

    return 0; // Not found.
}

static void newMaterialNameBinding(material_t* mat, const char* name,
                                   material_namespace_t mnamespace,
                                   uint hash)
{
    materialbind_t*     mb;

    if(++numMaterialBinds > maxMaterialBinds)
    {   // Allocate more memory.
        maxMaterialBinds += MATERIALS_BLOCK_ALLOC;
        materialBinds =
            Z_Realloc(materialBinds, sizeof(*materialBinds) * maxMaterialBinds,
                      PU_STATIC);
    }

    // Add the new material to the end.
    mb = &materialBinds[numMaterialBinds - 1];
    memset(mb, 0, sizeof(*mb));
    strncpy(mb->name, name, 8);
    mb->name[8] = '\0';
    mb->mat = mat;
    mb->prepared = 0;

    // We also hash the name for faster searching.
    mb->hashNext = hashTable[mnamespace][hash];
    hashTable[mnamespace][hash] = (mb - materialBinds) + 1;
}

/**
 * Find name-associated definitions for the known material bindings.
 */
void P_LinkAssociatedDefinitionsToMaterials(void)
{
    uint i;
    for(i = 0; i < numMaterialBinds; ++i)
    {
        materialbind_t* mb = &materialBinds[i];
        if(mb->mat->mnamespace == MN_SYSTEM)
            continue;

        // Surface decorations (lights and models).
        mb->decoration[0] = Def_GetDecoration(mb->mat, 0);
        mb->decoration[1] = Def_GetDecoration(mb->mat, 1);

        // Reflection (aka shiny surface).
        mb->reflection[0] = Def_GetReflection(mb->mat, 0);
        mb->reflection[1] = Def_GetReflection(mb->mat, 1);

        // Generator (particles).
        mb->ptcGen[0] = Def_GetGenerator(mb->mat, 0);
        mb->ptcGen[1] = Def_GetGenerator(mb->mat, 1);

        // Detail texture.
        mb->detail[0] = Def_GetDetailTex(mb->mat, 0);
        mb->detail[1] = Def_GetDetailTex(mb->mat, 1);
    }
}

/**
 * One time initialization of the materials list. Called during init.
 */
void P_InitMaterialManager(void)
{
    if(initedOk)
        return; // Already been here.

    materialsBlockSet = Z_BlockCreate(sizeof(material_t),
                                      MATERIALS_BLOCK_ALLOC, PU_STATIC);
    materialsHead = NULL;

    materialBinds = NULL;
    numMaterialBinds = maxMaterialBinds = 0;

    // Clear the name bind hash tables.
    memset(hashTable, 0, sizeof(hashTable));

    initedOk = true;
}

/**
 * Release all memory acquired for the materials list.
 * Called during shutdown.
 */
void P_ShutdownMaterialManager(void)
{
    if(!initedOk)
        return;

    Z_BlockDestroy(materialsBlockSet);
    materialsBlockSet = NULL;
    materialsHead = NULL;

    // Destroy the bindings.
    if(materialBinds)
    {
        Z_Free(materialBinds);
        materialBinds = NULL;
    }
    numMaterialBinds = maxMaterialBinds = 0;

    initedOk = false;
}

/**
 * Deletes all GL texture instances, linked to materials.
 *
 * @param mnamespace    @c MN_ANY = delete everything, ELSE
 *                      Only delete those currently in use by materials
 *                      in the specified namespace.
 */
void P_DeleteMaterialTextures(material_namespace_t mnamespace)
{
    if(mnamespace == MN_ANY)
    {   // Delete the lot.
        GL_DeleteAllTexturesForGLTextures(GLT_ANY);
        return;
    }

    if(!isKnownMNamespace(mnamespace))
        Con_Error("P_DeleteMaterialTextures: Internal error, "
                  "invalid materialgroup '%i'.", (int) mnamespace);

    if(materialBinds)
    {
        uint                i;

        for(i = 0; i < MATERIAL_NAME_HASH_SIZE; ++i)
            if(hashTable[mnamespace][i])
            {
                materialbind_t*     mb = &materialBinds[
                    hashTable[mnamespace][i] - 1];

                for(;;)
                {
                    material_t*         mat = mb->mat;
                    uint                j;

                    for(j = 0; j < mat->numLayers; ++j)
                        GL_ReleaseGLTexture(mat->layers[j].tex);

                    if(!mb->hashNext)
                        break;

                    mb = &materialBinds[mb->hashNext - 1];
                }
            }
    }
}

static material_t* createMaterial(short width, short height, byte flags,
                                  material_namespace_t mnamespace,
                                  ded_material_t* def, gltextureid_t tex)
{
    uint                i;
    material_t*         mat = Z_BlockNewElement(materialsBlockSet);

    memset(mat, 0, sizeof(*mat));
    mat->header.type = DMU_MATERIAL;
    mat->mnamespace = mnamespace;
    mat->width = width;
    mat->height = height;
    mat->flags = flags;
    mat->envClass = MEC_UNKNOWN;
    mat->current = mat->next = mat;
    mat->def = def;
    mat->layers[0].tex = tex;
    mat->numLayers = 1;

    // Is this a custom material?
    mat->flags &= ~MATF_CUSTOM;
    for(i = 0; i < mat->numLayers; ++i)
    {
        mat->layers[i].glow = (def? def->layers[i].stages[0].glow : 0);
        if(!GLTexture_IsFromIWAD(GL_GetGLTexture(mat->layers[i].tex)))
        {
            mat->flags |= MATF_CUSTOM;
            break;
        }
    }

    // Link the new material into the global list of materials.
    mat->globalNext = materialsHead;
    materialsHead = mat;

    return mat;
}

static material_t* getMaterialByNum(materialnum_t num)
{
    if(num < numMaterialBinds)
        return materialBinds[num].mat;

    return NULL;
}

/**
 * Given a unique material number return the associated material.
 *
 * @param num           Unique material number.
 *
 * @return              The associated material, ELSE @c NULL.
 */
material_t* P_ToMaterial(materialnum_t num)
{
    if(!initedOk)
        return NULL;

    if(num != 0)
        return getMaterialByNum(num - 1); // 1-based index.

    return NULL;
}

/**
 * Retrieve the unique material number for the given material.
 *
 * @param mat           The material to retrieve the unique number of.
 *
 * @return              The associated unique number.
 */
materialnum_t P_ToMaterialNum(const material_t* mat)
{
    if(mat)
    {
        materialnum_t       i;

        for(i = 0; i < numMaterialBinds; ++i)
            if(materialBinds[i].mat == mat)
                return i + 1; // 1-based index.
    }

    return 0;
}

/**
 * Create a new material. If there exists one by the same name and in the
 * same namespace, it is returned else a new material is created.
 *
 * \note: May fail if the name is invalid.
 *
 * @param name          Name of the new material.
 * @param width         Width of the material (not of the texture).
 * @param height        Height of the material (not of the texture).
 * @param flags         MATF_* material flags
 * @param tex           Texture to use with this material.
 * @param mnamespace    MG_* material namespace.
 *                      MN_ANY is only valid when updating an existing material.
 *
 * @return              The created material, ELSE @c NULL.
 */
material_t* P_MaterialCreate(const char* rawName, short width, short height,
                             byte flags, gltextureid_t tex,
                             material_namespace_t mnamespace,
                             ded_material_t* def)
{
    int                 n;
    uint                hash;
    char                name[9];
    materialnum_t       oldMat;
    material_t*         mat;

    if(!initedOk)
        return NULL;

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(!rawName || !rawName[0] || rawName[0] == '-')
    {
#if _DEBUG
Con_Message("P_MaterialCreate: Warning, attempted to create material with "
            "NULL name\n.");
#endif
        return NULL;
    }

    // Prepare 'name'.
    for(n = 0; *rawName && n < 8; ++n, rawName++)
        name[n] = tolower(*rawName);
    name[n] = '\0';
    hash = hashForName(name);

    // Check if we've already created a material for this.
    if(mnamespace == MN_ANY)
    {   // Caller doesn't care which namespace. This is only valid if we
        // can find a material by this name using a priority search order.
        oldMat = getMaterialNumForName(name, hash, MN_SPRITES);
        if(!oldMat)
            oldMat = getMaterialNumForName(name, hash, MN_TEXTURES);
        if(!oldMat)
            oldMat = getMaterialNumForName(name, hash, MN_FLATS);
    }
    else
    {
        if(!isKnownMNamespace(mnamespace))
        {
#if _DEBUG
Con_Message("P_MaterialCreate: Warning, attempted to create material in "
            "unknown namespace '%i'.\n", (int) mnamespace);
#endif
            return NULL;
        }

        oldMat = getMaterialNumForName(name, hash, mnamespace);
    }

    if(oldMat)
    {   // We are updating an existing material.
        materialbind_t* mb = &materialBinds[oldMat - 1];
        uint i;

        mat = mb->mat;

        // Update the (possibly new) meta data.
        mat->flags = flags;
        if(tex)
            mat->layers[0].tex = tex;
        if(width > 0)
            mat->width = width;
        if(height > 0)
            mat->height = height;
        mat->inAnimGroup = false;
        mat->current = mat->next = mat;
        mat->def = def;
        mat->inter = 0;
        mat->envClass = MEC_UNKNOWN;
        mat->mnamespace = mnamespace;

        // Is this a custom material?
        mat->flags &= ~MATF_CUSTOM;
        for(i = 0; i < mat->numLayers; ++i)
        {
            mat->layers[i].glow = (def && def->layers[i].stageCount.num >= 1? def->layers[i].stages[0].glow : 0);
            if(!GLTexture_IsFromIWAD(GL_GetGLTexture(mat->layers[i].tex)))
            {
                mat->flags |= MATF_CUSTOM;
                break;
            }
        }
        return mat; // Yep, return it.
    }

    if(mnamespace == MN_ANY)
    {
#if _DEBUG
Con_Message("P_MaterialCreate: Warning, attempted to create material "
            "without specifying a namespace.\n");
#endif
        return NULL;
    }

    /**
     * A new material.
     */

    // Only create complete materials.
    // \todo Doing this here isn't ideal.
    if(tex == 0 || !(width > 0) || !(height > 0))
        return NULL;

    mat = createMaterial(width, height, flags, mnamespace, def, tex);

    // Now create a name binding for it.
    newMaterialNameBinding(mat, name, mnamespace, hash);

    return mat;
}

/**
 * Given a Texture/Flat/Sprite etc idx and a MG_* namespace, search
 * for a matching material.
 *
 * @param ofTypeID      Texture/Flat/Sprite etc idx.
 * @param mnamespace    Specific MG_* namespace (not MN_ANY).
 *
 * @return              The associated material, ELSE @c NULL.
 */
material_t* P_GetMaterial(int ofTypeID, material_namespace_t mnamespace)
{
    if(!initedOk)
        return 0;

    if(!isKnownMNamespace(mnamespace)) // MN_ANY is considered invalid.
    {
#if _DEBUG
Con_Message("P_GetMaterial: Internal error, invalid namespace '%i'\n",
            (int) mnamespace);
#endif
        return 0;
    }

    if(materialsHead)
    {
        material_t* mat = materialsHead;
        do
        {
            if(mnamespace == mat->mnamespace && GL_GetGLTexture(mat->layers[0].tex)->ofTypeID == ofTypeID)
            {
                if(mat->flags & MATF_NO_DRAW)
                   return 0;
                return mat;
            }
        } while((mat = mat->globalNext));
    }
    return 0;
}

materialnum_t P_MaterialCheckNumForIndex(uint idx, material_namespace_t mnamespace)
{
    if(!initedOk)
        return 0;

    // Caller wants a material in a specific namespace.
    if(!isKnownMNamespace(mnamespace))
    {
#if _DEBUG
Con_Message("P_GetMaterial: Internal error, invalid namespace '%i'\n",
            (int) mnamespace);
#endif
        return 0;
    }

    return getMaterialNumForIndex(idx, mnamespace);
}

/**
 * Given a texture/flat/sprite/etc index num, search the materials db for
 * a name-bound material.
 * \note Part of the Doomsday public API.
 * \note2 Sames as P_MaterialCheckNumForIndex except will log an error
 *        message if the material being searched for is not found.
 *
 * @param idx           Index of the texture/flat/sprite/etc.
 * @param mnamespace    MG_* namespace.
 *
 * @return              Unique identifier of the found material, else zero.
 */
materialnum_t P_MaterialNumForIndex(uint idx, material_namespace_t mnamespace)
{
    materialnum_t       result = P_MaterialCheckNumForIndex(idx, mnamespace);

    // Not found? Don't announce during map setup or if not yet inited.
    if(verbose && result == 0 && (!ddMapSetup || !initedOk))
        Con_Message("P_MaterialNumForIndex: %u in namespace %i not found!\n",
                    idx, mnamespace);
    return result;
}

/**
 * Given a name and namespace, search the materials db for a match.
 * \note Part of the Doomsday public API.
 *
 * @param name          Name of the material to search for.
 * @param mnamespace    Specific MG_* namespace ELSE,
 *                      @c MN_ANY = no namespace requirement in which case
 *                      the material is searched for among all namespaces
 *                      using a logic which prioritizes the namespaces as
 *                      follows:
 *                      1st: MN_SPRITES
 *                      2nd: MN_TEXTURES
 *                      3rd: MN_FLATS
 *
 * @return              Unique number of the found material, else zero.
 */
materialnum_t Materials_CheckNumForName(const char* rawName,
                                        material_namespace_t mnamespace)
{
    int                 n;
    uint                hash;
    char                name[9];

    if(!initedOk)
        return 0;

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(!rawName || !rawName[0] || rawName[0] == '-')
        return 0;

    if(mnamespace != MN_ANY && !isKnownMNamespace(mnamespace))
    {
#if _DEBUG
Con_Message("P_GetMaterial: Internal error, invalid namespace '%i'\n",
            (int) mnamespace);
#endif
        return 0;
    }

    // Prepare 'name'.
    for(n = 0; *rawName && n < 8; ++n, rawName++)
        name[n] = tolower(*rawName);
    name[n] = '\0';
    hash = hashForName(name);

    if(mnamespace == MN_ANY)
    {   // Caller doesn't care which namespace.
        materialnum_t       matNum;

        // Check for the material in these namespaces, in priority order.
        if((matNum = getMaterialNumForName(name, hash, MN_SPRITES)))
            return matNum;
        if((matNum = getMaterialNumForName(name, hash, MN_TEXTURES)))
            return matNum;
        if((matNum = getMaterialNumForName(name, hash, MN_FLATS)))
            return matNum;

        return 0; // Not found.
    }

    // Caller wants a material in a specific namespace.
    return getMaterialNumForName(name, hash, mnamespace);
}

/**
 * Given a name and namespace, search the materials db for a match.
 * \note Part of the Doomsday public API.
 * \note2 Same as Materials_CheckNumForName except will log an error
 *        message if the material being searched for is not found.
 *
 * @param name          Name of the material to search for.
 * @param mnamespace    MG_* namespace.
 *
 * @return              Unique identifier of the found material, else zero.
 */
materialnum_t Materials_NumForName(const char* name,
                                   material_namespace_t mnamespace)
{
    materialnum_t       result;

    if(!initedOk)
        return 0;

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(!name || !name[0] || name[0] == '-')
        return 0;

    result = Materials_CheckNumForName(name, mnamespace);

    // Not found?
    if(verbose && result == 0 && !ddMapSetup) // Don't announce during map setup.
        Con_Message("Materials_NumForName: \"%.8s\" in namespace %i not found!\n",
                    name, mnamespace);
    return result;
}

/**
 * Given a unique material identifier, lookup the associated name.
 * \note Part of the Doomsday public API.
 *
 * @param mat           The material to lookup the name for.
 *
 * @return              The associated name.
 */
const char* P_GetMaterialName(material_t* mat)
{
    materialnum_t       num;

    if(!initedOk)
        return NULL;

    if(mat && (num = P_ToMaterialNum(mat)))
        return materialBinds[num-1].name;

    return "NOMAT"; // Should never happen.
}

/**
 * Precache the specified material.
 * \note Part of the Doomsday public API.
 *
 * @param mat           The material to be precached.
 */
void P_MaterialPrecache2(material_t* mat, boolean yes, boolean inGroup)
{
    if(!initedOk)
        return;
    if(!mat)
        return;
    if(yes)
        mat->inFlags |= MATIF_PRECACHE;
    else
        mat->inFlags &= ~MATIF_PRECACHE;

    if(inGroup && mat->inAnimGroup)
    {   // The material belongs in one or more animgroups, precache the group.
        Materials_PrecacheAnimGroup(mat, yes);
    }
}

void P_MaterialPrecache(material_t* mat, boolean yes)
{
    P_MaterialPrecache2(mat, yes, true);
}

/**
 * Called every tic by P_Ticker.
 */
void P_MaterialManagerTicker(timespan_t time)
{
    static trigger_t fixed = { 1.0 / 35, 0 };
    material_t* mat;

    // The animation will only progress when the game is not paused.
    if(clientPaused)
        return;

    mat = materialsHead;
    while(mat)
    {
        Material_Ticker(mat, time);
        mat = mat->globalNext;
    }

    if(!M_RunTrigger(&fixed, time))
        return;

    animateAnimGroups();
}

static materialbind_t* bindForMaterial(material_t* mat)
{
    if(materialBinds && mat)
    {
        uint i;
        for(i = 0; i < numMaterialBinds; ++i)
        {
            materialbind_t* mb = &materialBinds[i];
            if(mb->mat == mat)
                return mb;
        }
    }
    return NULL;
}

/**
 * Subroutine of Materials_Prepare().
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

byte Materials_Prepare(material_snapshot_t* snapshot, material_t* mat, boolean smoothed, material_load_params_t* params)
{
    materialnum_t num;
    if((num = P_ToMaterialNum(mat)))
    {
        materialbind_t* mb;
        const gltexture_inst_t* texInst[DDMAX_MATERIAL_LAYERS];
        const gltexture_inst_t* detailInst = NULL, *shinyInst = NULL, *shinyMaskInst = NULL;
        const ded_detailtexture_t* detail = NULL;
        const ded_reflection_t* reflection = NULL;
        const ded_decor_t* decor = NULL;
        byte tmpResult = 0;
        uint i;

        if(smoothed)
            mat = mat->current;
        assert(mat->numLayers > 0);

        // Ensure all resources needed to visualize this material are loaded.
        for(i = 0; i < mat->numLayers; ++i)
        {
            material_layer_t* ml = &mat->layers[i];
            byte result;

            // Pick the instance matching the specified context.
            texInst[i] = GL_PrepareGLTexture(ml->tex, params, &result);

            if(result)
                tmpResult = result;
        }

        if((mb = bindForMaterial(mat)))
        {
            // Do we need to reproject any decorations?
            if(tmpResult)
                decor = mb->decoration[tmpResult-1];

            // Do we need to prepare a detail texture?
            if(tmpResult && (detail = mb->detail[tmpResult-1]))
            {
                detailtex_t* dTex;
                lumpnum_t lump = W_CheckNumForName(detail->detailLump.path);
                const char* external = (detail->isExternal? detail->detailLump.path : NULL);

                /**
                 * \todo No need to look up the detail texture record every time!
                 * This will change anyway once the gltexture for the detailtex is
                 * linked to (and prepared) via the layers (above).
                 */

                if((dTex = R_GetDetailTexture(lump, external)))
                {
                    float contrast = detail->strength * detailFactor;

                    // Pick an instance matching the specified context.
                    detailInst = GL_PrepareGLTexture(dTex->id, &contrast, NULL);
                }
            }

            // Do we need to prepare a shiny texture (and possibly a mask)?
            if(tmpResult && (reflection = mb->reflection[tmpResult-1]))
            {
                shinytex_t* sTex;
                masktex_t* mTex;

                /**
                 * \todo No need to look up the shiny texture record every time!
                 * This will change anyway once the gltexture for the shinytex is
                 * linked to (and prepared) via the layers (above).
                 */

                if((sTex = R_GetShinyTexture(reflection->shinyMap.path)))
                {
                    // Pick an instance matching the specified context.
                    shinyInst = GL_PrepareGLTexture(sTex->id, NULL, NULL);
                }

                if(shinyInst && // Don't bother searching unless the above succeeds.
                   (mTex = R_GetMaskTexture(reflection->maskMap.path)))
                {
                    // Pick an instance matching the specified context.
                    shinyMaskInst = GL_PrepareGLTexture(mTex->id, NULL, NULL);
                }
            }

            if(tmpResult)
                mb->prepared = tmpResult;
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
        snapshot->glowing = mat->layers[0].glow * glowingTextures;
        snapshot->decorated = (decor? true : false);

        // Setup the primary texturing pass.
        if(mat->layers[0].tex)
        {
            const gltexture_t*  tex = GL_GetGLTexture(mat->layers[0].tex);
            int c, magMode = glmode[texMagMode];

            if(tex->type == GLT_SPRITE)
                magMode = filterSprites? GL_LINEAR : GL_NEAREST;

            setTexUnit(snapshot, MTU_PRIMARY, BM_NORMAL, magMode, texInst[0],
                       1.f / snapshot->width, 1.f / snapshot->height, 0, 0, 1);

            snapshot->isOpaque = !texInst[0]->isMasked;

            /// \fixme what about the other texture types?
            if(tex->type == GLT_DOOMTEXTURE || tex->type == GLT_FLAT)
            {
                for(c = 0; c < 3; ++c)
                {
                    snapshot->color[c] = texInst[0]->data.texture.color[c];
                    snapshot->colorAmplified[c] = texInst[0]->data.texture.colorAmplified[c];
                    snapshot->topColor[c] = texInst[0]->data.texture.topColor[c];
                }
            }
            else
            {
                snapshot->color[CR] = snapshot->color[CG] = snapshot->color[CB] = 1;
                snapshot->colorAmplified[CR] = snapshot->colorAmplified[CG] = snapshot->colorAmplified[CB] = 1;
                snapshot->topColor[CR] = snapshot->topColor[CG] = snapshot->topColor[CB] = 1;
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
            if(detailInst && detail && snapshot->isOpaque)
            {
                float width, height, scale;

                width  = GLTexture_GetWidth(detailInst->tex);
                height = GLTexture_GetHeight(detailInst->tex);
                scale  = MAX_OF(1, detail->scale);
                // Apply the global scaling factor.
                if(detailScale > .001f)
                    scale *= detailScale;

                setTexUnit(snapshot, MTU_DETAIL, BM_NORMAL,
                           GL_LINEAR, detailInst, 1.f / width * scale,
                           1.f / height * scale, 0, 0, 1);
            }

            // Setup the reflection (aka shiny) texturing pass(es)?
            if(shinyInst && reflection)
            {
                snapshot->shiny.minColor[CR] = reflection->minColor[CR];
                snapshot->shiny.minColor[CG] = reflection->minColor[CG];
                snapshot->shiny.minColor[CB] = reflection->minColor[CB];

                setTexUnit(snapshot, MTU_REFLECTION, reflection->blendMode,
                           GL_LINEAR, shinyInst, 1, 1, 0, 0, reflection->shininess);

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
    return 0;
}

const ded_reflection_t* Materials_Reflection(materialnum_t num)
{
    if(num > 0)
    {
        const materialbind_t* mb = bindForMaterial(P_ToMaterial(num));
        if(!mb->prepared)
            Materials_Prepare(NULL, mb->mat, false, 0);
        return mb->reflection[mb->prepared? mb->prepared-1:0];
    }
    return 0;
}

const ded_detailtexture_t* Materials_Detail(materialnum_t num)
{
    if(num > 0)
    {
        const materialbind_t* mb = bindForMaterial(P_ToMaterial(num));
        if(!mb->prepared)
            Materials_Prepare(NULL, mb->mat, false, 0);
        return mb->detail[mb->prepared? mb->prepared-1:0];
    }
    return 0;
}

const ded_decor_t* Materials_Decoration(materialnum_t num)
{
    if(num > 0)
    {
        const materialbind_t* mb = bindForMaterial(P_ToMaterial(num));
        if(!mb->prepared)
            Materials_Prepare(NULL, mb->mat, false, 0);
        return mb->decoration[mb->prepared? mb->prepared-1:0];
    }
    return 0;
}

const ded_ptcgen_t* Materials_PtcGen(materialnum_t num)
{
    if(num > 0)
    {
        const materialbind_t* mb = bindForMaterial(P_ToMaterial(num));
        if(!mb->prepared)
            Materials_Prepare(NULL, mb->mat, false, 0);
        return mb->ptcGen[mb->prepared? mb->prepared-1:0];
    }
    return 0;
}

uint Materials_Count(void)
{
    if(initedOk)
        return numMaterialBinds;
    return 0;
}

static void printMaterialInfo(materialnum_t num, boolean printNamespace)
{
    const materialbind_t* mb = &materialBinds[num];
    int numDigits = M_NumDigits(numMaterialBinds);
    uint i;

    Con_Printf(" %*u - \"%s\"", numDigits, (unsigned int) num, mb->name);
    if(printNamespace)
        Con_Printf(" (%i)", mb->mat->mnamespace);
    Con_Printf(" [%i, %i]", mb->mat->width, mb->mat->height);

    for(i = 0; i < mb->mat->numLayers; ++i)
    {
        Con_Printf(" %i:%s", i, GL_GetGLTexture(mb->mat->layers[i].tex)->name);
    }
    Con_Printf("\n");
}

static void printMaterials(material_namespace_t mnamespace, const char* like)
{
    materialnum_t       i;

    if(!(mnamespace < NUM_MATERIAL_NAMESPACES))
        return;

    if(mnamespace == MN_ANY)
    {
        Con_Printf("Known Materials (IDX - Name (Namespace) [width, height]):\n");

        for(i = 0; i < numMaterialBinds; ++i)
        {
            materialbind_t*     mb = &materialBinds[i];

            if(like && like[0] && strnicmp(mb->name, like, strlen(like)))
                continue;

            printMaterialInfo(i, true);
        }
    }
    else
    {
        Con_Printf("Known Materials in Namespace %i (IDX - Name "
                   "[width, height]):\n", mnamespace);

        if(materialBinds)
        {
            uint                i;

            for(i = 0; i < MATERIAL_NAME_HASH_SIZE; ++i)
                if(hashTable[mnamespace][i])
                {
                    materialnum_t       num = hashTable[mnamespace][i] - 1;
                    materialbind_t*     mb = &materialBinds[num];

                    for(;;)
                    {
                        if(like && like[0] && strnicmp(mb->name, like, strlen(like)))
                            continue;

                        printMaterialInfo(num, false);

                        if(!mb->hashNext)
                            break;

                        mb = &materialBinds[mb->hashNext - 1];
                    }
                }
        }
    }
}

typedef struct animframe_s {
    material_t*     mat;
    ushort          tics;
    ushort          random;
} animframe_t;

typedef struct animgroup_s {
    int             id;
    int             flags;
    int             index;
    int             maxTimer;
    int             timer;
    int             count;
    animframe_t*    frames;
} animgroup_t;

static int numgroups;
static animgroup_t* groups;

static animgroup_t* getAnimGroup(int number)
{
    if(--number < 0 || number >= numgroups)
        return NULL;

    return &groups[number];
}

static boolean isInAnimGroup(animgroup_t* group, const material_t* mat)
{
    int                 i;

    if(!mat || !group)
        return false;

    // Is it in there?
    for(i = 0; i < group->count; ++i)
    {
        animframe_t*        frame = &group->frames[i];

        if(frame->mat == mat)
            return true;
    }

    return false;
}

boolean Materials_MaterialLinkedToAnimGroup(int groupNum, material_t* mat)
{
    return isInAnimGroup(getAnimGroup(groupNum), mat);
}

int Materials_AnimGroupCount(void)
{
    return numgroups;
}

/**
 * Create a new animation group. Returns the group number.
 * \note Part of the Doomsday public API.
 */
int Materials_CreateAnimGroup(int flags)
{
    animgroup_t* group;

    // Allocating one by one is inefficient, but it doesn't really matter.
    groups = Z_Realloc(groups, sizeof(animgroup_t) * (numgroups + 1), PU_STATIC);

    // Init the new group.
    group = &groups[numgroups];
    memset(group, 0, sizeof(*group));

    // The group number is (index + 1).
    group->id = ++numgroups;
    group->flags = flags;

    return group->id;
}

/**
 * Called during engine reset to clear the existing animation groups.
 */
void Materials_DestroyAnimGroups(void)
{
    int                 i;

    if(numgroups > 0)
    {
        for(i = 0; i < numgroups; ++i)
        {
            animgroup_t*        group = &groups[i];
            Z_Free(group->frames);
        }

        Z_Free(groups);
        groups = NULL;
        numgroups = 0;
    }
}

/**
 * \note Part of the Doomsday public API.
 */
void Materials_AddAnimGroupFrame(int groupNum, materialnum_t num, int tics,
                      int randomTics)
{
    animgroup_t*        group;
    animframe_t*        frame;
    material_t*        mat;

    group = getAnimGroup(groupNum);
    if(!group)
        Con_Error("Materials_AddAnimGroupFrame: Unknown anim group '%i'\n.", groupNum);

    if(!num || !(mat = getMaterialByNum(num - 1)))
    {
        Con_Message("Materials_AddAnimGroupFrame: Invalid material num '%i'\n.", num);
        return;
    }

    // Mark the material as being in an animgroup.
    mat->inAnimGroup = true;

    // Allocate a new animframe.
    group->frames =
        Z_Realloc(group->frames, sizeof(animframe_t) * ++group->count,
                  PU_STATIC);

    frame = &group->frames[group->count - 1];

    frame->mat = mat;
    frame->tics = tics;
    frame->random = randomTics;
}

boolean Materials_IsPrecacheAnimGroup(int groupNum)
{
    animgroup_t*        group;

    if((group = getAnimGroup(groupNum)))
    {
        return (group->flags & AGF_PRECACHE)? true : false;
    }

    return false;
}

void Materials_AnimateAnimGroup(animgroup_t* group)
{
    // The Precache groups are not intended for animation.
    if((group->flags & AGF_PRECACHE) || !group->count)
        return;

    if(--group->timer <= 0)
    {
        // Advance to next frame.
        int i, timer;

        group->index = (group->index + 1) % group->count;
        timer = (int) group->frames[group->index].tics;

        if(group->frames[group->index].random)
        {
            timer += (int) RNG_RandByte() % (group->frames[group->index].random + 1);
        }
        group->timer = group->maxTimer = timer;

        // Update translations.
        for(i = 0; i < group->count; ++i)
        {
            material_t* real, *current, *next;

            real    = group->frames[i].mat;
            current = group->frames[(group->index + i    ) % group->count].mat;
            next    = group->frames[(group->index + i + 1) % group->count].mat;

            Material_SetTranslation(real, current, next, 0);

            // Just animate the first in the sequence?
            if(group->flags & AGF_FIRST_ONLY)
                break;
        }
        return;
    }

    // Update the interpolation point of animated group members.
    {int i;
    for(i = 0; i < group->count; ++i)
    {
        material_t* mat = group->frames[i].mat;

        if(mat->def && mat->def->layers[0].stageCount.num > 1)
        {
            if(GL_GetGLTextureByName(mat->def->layers[0].stages[0].name, mat->def->layers[0].stages[0].type))
                continue; // Animated elsewhere.
        }

        if(group->flags & AGF_SMOOTH)
        {
            mat->inter = 1 - group->timer / (float) group->maxTimer;
        }
        else
        {
            mat->inter = 0;
        }

        // Just animate the first in the sequence?
        if(group->flags & AGF_FIRST_ONLY)
            break;
    }}
}

static void animateAnimGroups(void)
{
    int i;
    for(i = 0; i < numgroups; ++i)
    {
        Materials_AnimateAnimGroup(&groups[i]);
    }
}

/**
 * All animation groups are reseted back to their original state.
 * Called when setting up a map.
 */
void Materials_ResetAnimGroups(void)
{
    animgroup_t* group;

    {uint i;
    for(i = 0; i < Materials_Count(); ++i)
    {
        material_t* mat = P_ToMaterial((materialnum_t)(i+1));
        uint j;
        for(j = 0; j < mat->numLayers; ++j)
        {
            material_layer_t* ml = &mat->layers[j];
            if(ml->stage == -1)
                break;
            ml->stage = 0;
        }
    }}

    {int i;
    for(i = 0, group = groups; i < numgroups; ++i, group++)
    {
        // The Precache groups are not intended for animation.
        if((group->flags & AGF_PRECACHE) || !group->count)
            continue;

        group->timer = 0;
        group->maxTimer = 1;

        // The anim group should start from the first step using the
        // correct timings.
        group->index = group->count - 1;
    }}

    // This'll get every group started on the first step.
    animateAnimGroups();
}

void Materials_PrecacheAnimGroup(material_t* mat, boolean yes)
{
    int i;
    for(i = 0; i < numgroups; ++i)
    {
        if(isInAnimGroup(&groups[i], mat))
        {
            int k;
            // Precache this group.
            for(k = 0; k < groups[i].count; ++k)
            {
                P_MaterialPrecache2(groups[i].frames[k].mat, yes, false);
            }
        }
    }
}

D_CMD(ListMaterials)
{
    material_namespace_t mnamespace = MN_ANY;

    if(argc > 1)
    {
        mnamespace = atoi(argv[1]);
        if(mnamespace < MN_FIRST)
        {
            mnamespace = MN_ANY;
        }
        else if(!(mnamespace < NUM_MATERIAL_NAMESPACES))
        {
            Con_Printf("Invalid namespace \"%s\".\n", argv[1]);
            return false;
        }
    }

    printMaterials(mnamespace, argc > 2? argv[2] : NULL);
    return true;
}
