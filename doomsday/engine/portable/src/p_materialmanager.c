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
 * p_materialmanager.c: Materials manager.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h> // For tolower()

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

#define MATERIALS_BLOCK_ALLOC (32) // Num materials to allocate per block.
#define MATERIALS_NAME_HASH_SIZE (512)

// TYPES -------------------------------------------------------------------

typedef struct materialbind_s {
    char            name[9];
    material_t*     mat;

    uint            hashNext; // 1-based index
} materialbind_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(ListMaterials);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void animateAnimGroups(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean mapSetup;

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
 *    a given material name group.
 * 5) Super-fast look up by public material identifier.
 * 6) Fast look up by material name (a hashing scheme is used).
 */
static zblockset_t* materialsBlockSet;
static material_t* materialsHead; // Head of the linked list of materials.

static materialbind_t* materialBinds;
static materialnum_t maxMaterialBinds;
static uint hashTable[MATERIALS_NAME_HASH_SIZE];

// CODE --------------------------------------------------------------------

void P_MaterialManagerRegister(void)
{
    C_CMD("listmaterials",  NULL,     ListMaterials);
}

/**
 * Is the specified group number a valid (known) material group.
 *
 * \note The special case MG_ANY is considered invalid here as it does not
 *       relate to one specific group.
 *
 * @return              @c true, iff the groupNum is valid.
 */
static boolean isValidMaterialGroup(materialgroup_t groupNum)
{
    if(groupNum >= MG_FIRST && groupNum < NUM_MATERIAL_GROUPS)
        return true;

    return false;
}

/**
 * This is a hash function. Given a material name it generates a
 * somewhat-random number between 0 and MATERIALS_NAME_HASH_SIZE.
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

    return key % MATERIALS_NAME_HASH_SIZE;
}

/**
 * Given a name and material group, search the materials db for a match.
 * \assume Caller knows what it's doing; params arn't validity checked.
 *
 * @param name          Name of the material to search for. Must have been
 *                      transformed to all lower case.
 * @param groupNum      Specific MG_* material group NOT @c MG_ANY.
 * @return              Unique number of the found material, else zero.
 */
static materialnum_t getMaterialNumForName(const char* name, uint hash,
                                           materialgroup_t groupNum)
{
    // Go through the candidates.
    if(hashTable[hash])
    {
        materialbind_t*     mb = &materialBinds[hashTable[hash] - 1];

        for(;;)
        {
            material_t*        mat;

            mat = mb->mat;

            if(mat->group == groupNum && !strncmp(mb->name, name, 8))
                return ((mb) - materialBinds) + 1;

            if(!mb->hashNext)
                break;

            mb = &materialBinds[mb->hashNext - 1];
        }
    }

    return 0; // Not found.
}

/**
 * Given an index and material group, search the materials db for a match.
 * \assume Caller knows what it's doing; params arn't validity checked.
 *
 * @param id            gltextureid to search for.
 * @param groupNum      Specific MG_* material group NOT @c MG_ANY.
 * @return              Unique number of the found material, else zero.
 */
static materialnum_t getMaterialNumForIndex(uint idx, materialgroup_t groupNum)
{
    materialnum_t       i;

    // Go through the candidates.
    for(i = 0; i < numMaterialBinds; ++i)
    {
        materialbind_t*     mb = &materialBinds[i];
        material_t*         mat = mb->mat;

        if(mat->group == groupNum &&
           GL_GetGLTexture(mat->layers[0].tex)->ofTypeID == idx)
            return ((mb) - materialBinds) + 1;
    }

    return 0; // Not found.
}

static void newMaterialNameBinding(material_t* mat, const char* name,
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
    strncpy(mb->name, name, 8);
    mb->name[8] = '\0';
    mb->mat = mat;

    // We also hash the name for faster searching.
    mb->hashNext = hashTable[hash];
    hashTable[hash] = (mb - materialBinds) + 1;
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
 * @param group         @c MG_ANY = delete everything, ELSE
 *                      Only delete those currently in use by materials
 *                      in the specified material group.
 */
void P_DeleteMaterialTextures(materialgroup_t groupNum)
{
    if(groupNum == MG_ANY)
    {   // Delete the lot.
        GL_DeleteAllTexturesForGLTextures(GLT_ANY);
        return;
    }

    if(!isValidMaterialGroup(groupNum))
        Con_Error("P_DeleteMaterialTextures: Internal error, "
                  "invalid materialgroup '%i'.", (int) groupNum);

    if(materialsHead)
    {
        material_t*        mat;

        mat = materialsHead;
        do
        {
            if(mat->group == groupNum)
            {
                uint                i;

                for(i = 0; i < mat->numLayers; ++i)
                    GL_ReleaseGLTexture(mat->layers[i].tex);
            }
        } while((mat = mat->globalNext));
    }
}

static material_t* createMaterial(short width, short height, byte flags,
                                  materialgroup_t groupNum,
                                  ded_material_t* def, gltextureid_t tex)
{
    uint                i;
    material_t*         mat = Z_BlockNewElement(materialsBlockSet);

    memset(mat, 0, sizeof(*mat));
    mat->header.type = DMU_MATERIAL;
    mat->group = groupNum;
    mat->width = width;
    mat->height = height;
    mat->flags = flags;
    mat->envClass = MEC_UNKNOWN;
    mat->current = mat->next = mat;
    mat->def = def;
    mat->decoration = NULL;
    mat->ptcGen = NULL;
    mat->detail = NULL;
    mat->layers[0].tex = tex;
    mat->numLayers = 1;

    // Is this a custom material?
    mat->flags &= ~MATF_CUSTOM;
    for(i = 0; i < mat->numLayers; ++i)
    {
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
 * same material group, it is returned else a new material is created.
 *
 * \note: May fail if the name is invalid.
 *
 * @param name          Name of the new material.
 * @param width         Width of the material (not of the texture).
 * @param height        Height of the material (not of the texture).
 * @param flags         MATF_* material flags
 * @param tex           Texture to use with this material.
 * @param group         MG_* material group.
 *                      MG_ANY is only valid when updating an existing material.
 *
 * @return              The created material, ELSE @c NULL.
 */
material_t* P_MaterialCreate(const char* rawName, short width, short height,
                             byte flags, gltextureid_t tex,
                             materialgroup_t groupNum, ded_material_t* def)
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
    if(groupNum == MG_ANY)
    {   // Caller doesn't care which group. This is only valid if we can
        // find a material by this name using a priority search order.
        oldMat = getMaterialNumForName(name, hash, MG_SPRITES);
        if(!oldMat)
            oldMat = getMaterialNumForName(name, hash, MG_TEXTURES);
        if(!oldMat)
            oldMat = getMaterialNumForName(name, hash, MG_FLATS);
    }
    else
    {
        if(!isValidMaterialGroup(groupNum))
        {
#if _DEBUG
Con_Message("P_MaterialCreate: Warning, attempted to create material in "
            "unknown group '%i'.\n", (int) groupNum);
#endif
            return NULL;
        }

        oldMat = getMaterialNumForName(name, hash, groupNum);
    }

    if(oldMat)
    {   // We are updating an existing material.
        materialbind_t*     mb = &materialBinds[oldMat - 1];

        mat = mb->mat;

        // Update the (possibly new) meta data.
        if(tex)
            mat->layers[0].tex = tex;
        mat->flags = flags;
        if(width > 0)
            mat->width = width;
        if(height > 0)
            mat->height = height;
        mat->inAnimGroup = false;
        mat->current = mat->next = mat;
        mat->def = def;
        mat->inter = 0;
        mat->decoration = NULL;
        mat->ptcGen = NULL;
        mat->detail = NULL;
        mat->envClass = MEC_UNKNOWN;

        return mat; // Yep, return it.
    }

    if(groupNum == MG_ANY)
    {
#if _DEBUG
Con_Message("P_MaterialCreate: Warning, attempted to create material "
            "without groupNum.\n");
#endif
        return NULL;
    }

    /**
     * A new material.
     */

    // Sanity checks.
    assert(tex != 0);
    assert(width > 0);
    assert(height > 0);

    mat = createMaterial(width, height, flags, groupNum, def, tex);

    // Now create a name binding for it.
    newMaterialNameBinding(mat, name, hash);

    return mat;
}

/**
 * Given a Texture/Flat/Sprite etc idx and a MG_* material group, search
 * for a matching material.
 *
 * @param ofTypeID      Texture/Flat/Sprite etc idx.
 * @param group         Specific MG_* material group (not MG_ANY).
 *
 * @return              The associated material, ELSE @c NULL.
 */
material_t* P_GetMaterial(int ofTypeID, materialgroup_t groupNum)
{
    if(!initedOk)
        return NULL;

    if(!isValidMaterialGroup(groupNum)) // MG_ANY is considered invalid.
    {
#if _DEBUG
Con_Message("P_GetMaterial: Internal error, invalid material group '%i'\n",
            (int) groupNum);
#endif
        return NULL;
    }

    if(materialsHead)
    {
        material_t*        mat;

        mat = materialsHead;
        do
        {
            if(groupNum == mat->group &&
               GL_GetGLTexture(mat->layers[0].tex)->ofTypeID == ofTypeID)
            {
                if(mat->flags & MATF_NO_DRAW)
                   return NULL;

                return mat;
            }
        } while((mat = mat->globalNext));
    }

    return NULL;
}

materialnum_t P_MaterialCheckNumForIndex(uint idx, materialgroup_t groupNum)
{
    if(!initedOk)
        return 0;

    // Caller wants a material in a specific group.
    if(!isValidMaterialGroup(groupNum))
    {
#if _DEBUG
Con_Message("P_GetMaterial: Internal error, invalid material group '%i'\n",
            (int) groupNum);
#endif
        return 0;
    }

    return getMaterialNumForIndex(idx, groupNum);
}

/**
 * Given a texture/flat/sprite/etc index num, search the materials db for
 * a name-bound material.
 * \note Part of the Doomsday public API.
 * \note2 Sames as P_MaterialCheckNumForIndex except will log an error
 *        message if the material being searched for is not found.
 *
 * @param idx           Index of the texture/flat/sprite/etc.
 * @param group         MG_* material group.
 *
 * @return              Unique identifier of the found material, else zero.
 */
materialnum_t P_MaterialNumForIndex(uint idx, materialgroup_t group)
{
    materialnum_t       result = P_MaterialCheckNumForIndex(idx, group);

    // Not found? Don't announce during map setup or if not yet inited.
    if(result == 0 && (!mapSetup || !initedOk))
        Con_Message("P_MaterialNumForIndex: %u in group %i not found!\n",
                    idx, group);
    return result;
}

/**
 * Given a name and material group, search the materials db for a match.
 * \note Part of the Doomsday public API.
 *
 * @param name          Name of the material to search for.
 * @param groupNum      Specific MG_* material group ELSE,
 *                      @c MG_ANY = no group requirement in which case the
 *                      material is searched for among all groups using a
 *                      logic which prioritizes the material groups as
 *                      follows:
 *                      1st: MG_SPRITES
 *                      2nd: MG_TEXTURES
 *                      3rd: MG_FLATS
 *
 * @return              Unique number of the found material, else zero.
 */
materialnum_t P_MaterialCheckNumForName(const char* rawName,
                                        materialgroup_t groupNum)
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

    if(groupNum != MG_ANY && !isValidMaterialGroup(groupNum))
    {
#if _DEBUG
Con_Message("P_GetMaterial: Internal error, invalid material group '%i'\n",
            (int) groupNum);
#endif
        return 0;
    }

    // Prepare 'name'.
    for(n = 0; *rawName && n < 8; ++n, rawName++)
        name[n] = tolower(*rawName);
    name[n] = '\0';
    hash = hashForName(name);

    if(groupNum == MG_ANY)
    {   // Caller doesn't care which group.
        materialnum_t       matNum;

        // Check for the material in these groups, in group priority order.
        if((matNum = getMaterialNumForName(name, hash, MG_SPRITES)))
            return matNum;
        if((matNum = getMaterialNumForName(name, hash, MG_TEXTURES)))
            return matNum;
        if((matNum = getMaterialNumForName(name, hash, MG_FLATS)))
            return matNum;

        return 0; // Not found.
    }

    // Caller wants a material in a specific group.
    return getMaterialNumForName(name, hash, groupNum);
}

/**
 * Given a name and material group, search the materials db for a match.
 * \note Part of the Doomsday public API.
 * \note2 Same as P_MaterialCheckNumForName except will log an error
 *        message if the material being searched for is not found.
 *
 * @param name          Name of the material to search for.
 * @param group         MG_* material group.
 *
 * @return              Unique identifier of the found material, else zero.
 */
materialnum_t P_MaterialNumForName(const char* name, materialgroup_t group)
{
    materialnum_t       result;

    if(!initedOk)
        return 0;

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(!name || !name[0] || name[0] == '-')
        return 0;

    result = P_MaterialCheckNumForName(name, group);

    // Not found?
    if(result == 0 && !mapSetup) // Don't announce during map setup.
        Con_Message("P_MaterialNumForName: \"%.8s\" in group %i not found!\n",
                    name, group);
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
        return materialBinds[num].name;

    return "NOMAT"; // Should never happen.
}

/**
 * Precache the specified material.
 * \note Part of the Doomsday public API.
 *
 * @param mat           The material to be precached.
 */
void P_MaterialPrecache(material_t* mat)
{
    if(!initedOk)
        return;

    Material_Precache(mat);
}

/**
 * Called every tic by P_Ticker.
 */
void P_MaterialManagerTicker(timespan_t time)
{
//    material_t*         mat;
    static trigger_t    fixed = { 1.0 / 35, 0 };

    // The animation will only progress when the game is not paused.
    if(clientPaused)
        return;

#if 0
    mat = materialsHead;
    while(mat)
    {
        Material_Ticker(mat, time);
        mat = mat->globalNext;
    }
#endif

    if(!M_RunTrigger(&fixed, time))
        return;

    animateAnimGroups();
}

static void printMaterials(materialgroup_t grp, const char* like)
{
    materialnum_t       i, numDigits;

    if(!(grp < NUM_MATERIAL_GROUPS))
        return;

    if(grp == MG_ANY)
        Con_Printf("Known Materials (IDX - Name (Group) [width, height]):\n");
    else
        Con_Printf("Known Materials in Group %i (IDX - Name [width, height]):\n", grp);

    numDigits = M_NumDigits(numMaterialBinds);
    for(i = 0; i < numMaterialBinds; ++i)
    {
        uint                j;
        materialbind_t*     mb = &materialBinds[i];
        material_t*         mat = mb->mat;

        if(grp != MG_ANY && mat->group != grp)
            continue;

        if(like && like[0] && strnicmp(mb->name, like, strlen(like)))
            continue;

        Con_Printf(" %*lu - \"%s\"", numDigits, i, mb->name);
        if(grp == MG_ANY)
            Con_Printf(" (%i)", mat->group);
        Con_Printf(" [%i, %i]", mat->width, mat->height);

        for(j = 0; j < mat->numLayers; ++j)
        {
            Con_Printf(" %i:%s", j, GL_GetGLTexture(mat->layers[j].tex)->name);
        }
        Con_Printf("\n");
    }
}

D_CMD(ListMaterials)
{
    materialgroup_t     grp = MG_ANY;

    if(argc > 1)
    {
        grp = atoi(argv[1]);
        if(grp < MG_FIRST)
        {
            grp = MG_ANY;
        }
        else if(!(grp < NUM_MATERIAL_GROUPS))
        {
            Con_Printf("Invalid material group \"%s\".\n", argv[1]);
            return false;
        }
    }

    printMaterials(grp, argc > 2? argv[2] : NULL);
    return true;
}

// Code bellow needs re-implementing --------------------------------------

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

boolean R_IsInAnimGroup(int groupNum, material_t* mat)
{
    return isInAnimGroup(getAnimGroup(groupNum), mat);
}

int R_NumAnimGroups(void)
{
    return numgroups;
}

/**
 * Create a new animation group. Returns the group number.
 * \note Part of the Doomsday public API.
 */
int R_CreateAnimGroup(int flags)
{
    animgroup_t*        group;

    // Allocating one by one is inefficient, but it doesn't really matter.
    groups =
        Z_Realloc(groups, sizeof(animgroup_t) * (numgroups + 1), PU_STATIC);

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
void R_DestroyAnimGroups(void)
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
void R_AddToAnimGroup(int groupNum, materialnum_t num, int tics,
                      int randomTics)
{
    animgroup_t*        group;
    animframe_t*        frame;
    material_t*        mat;

    group = getAnimGroup(groupNum);
    if(!group)
        Con_Error("R_AddToAnimGroup: Unknown anim group '%i'\n.", groupNum);

    if(!num || !(mat = getMaterialByNum(num - 1)))
    {
        Con_Message("R_AddToAnimGroup: Invalid material num '%i'\n.", num);
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

boolean R_IsPrecacheGroup(int groupNum)
{
    animgroup_t*        group;

    if(group = getAnimGroup(groupNum))
    {
        return (group->flags & AGF_PRECACHE)? true : false;
    }

    return false;
}

static void animateAnimGroups(void)
{
    int                 i, timer, k;
    animgroup_t*        group;

    for(i = 0, group = groups; i < numgroups; ++i, group++)
    {
        // The Precache groups are not intended for animation.
        if((group->flags & AGF_PRECACHE) || !group->count)
            continue;

        if(--group->timer <= 0)
        {
            // Advance to next frame.
            group->index = (group->index + 1) % group->count;
            timer = (int) group->frames[group->index].tics;

            if(group->frames[group->index].random)
            {
                timer += (int) RNG_RandByte() % (group->frames[group->index].random + 1);
            }
            group->timer = group->maxTimer = timer;

            // Update translations.
            for(k = 0; k < group->count; ++k)
            {
                material_t*            real, *current, *next;

                real = group->frames[k].mat;
                current =
                    group->frames[(group->index + k) % group->count].mat;
                next =
                    group->frames[(group->index + k + 1) % group->count].mat;

                Material_SetTranslation(real, current, next, 0);

                // Just animate the first in the sequence?
                if(group->flags & AGF_FIRST_ONLY)
                    break;
            }
        }
        else
        {
            // Update the interpolation point of animated group members.
            for(k = 0; k < group->count; ++k)
            {
                material_t*            mat = group->frames[k].mat;

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
            }
        }
    }
}

/**
 * All animation groups are reseted back to their original state.
 * Called when setting up a map.
 */
void R_ResetAnimGroups(void)
{
    int                 i;
    animgroup_t*        group;

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
    }

    // This'll get every group started on the first step.
    animateAnimGroups();
}

void R_MaterialsPrecacheGroup(material_t* mat)
{
    int                 i;

    for(i = 0; i < numgroups; ++i)
    {
        if(isInAnimGroup(&groups[i], mat))
        {
            int                 k;

            // Precache this group.
            for(k = 0; k < groups[i].count; ++k)
            {
                animframe_t*        frame = &groups[i].frames[k];

                Material_Prepare(NULL, frame->mat, true, NULL);
            }
        }
    }
}
