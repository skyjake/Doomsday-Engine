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

#include <math.h>
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
#define HASH_SIZE           (512)

// TYPES -------------------------------------------------------------------

typedef struct materialbind_s {
    char            name[9];
    material_t*     mat;

    uint            hashNext; // 1-based index
} materialbind_t;

typedef struct mtexinstnode_s {
    int             flags; // Texture instance (TEXF_*) flags.
    materialtexinst_t glTex;
    struct mtexinstnode_s* next; // Next in list of instances.
} mtexinstnode_t;

typedef struct mtex_typedata_s {
    uint            hashTable[HASH_SIZE];
} mtex_typedata_t;

typedef struct animframe_s {
    struct material_s* mat;
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

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(ListMaterials);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean mapSetup;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initedOk = false;
static uint numMaterialTexs;
static materialtex_t** materialTexs;

static int numgroups;
static animgroup_t* groups;

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
static materialnum_t numMaterialBinds, maxMaterialBinds;
static uint hashTable[HASH_SIZE];

static mtex_typedata_t mtexData[NUM_MATERIALTEX_TYPES];

// CODE --------------------------------------------------------------------

void R_MaterialsRegister(void)
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
 * somewhat-random number between 0 and HASH_SIZE.
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

    return key % HASH_SIZE;
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
            material_t*         mat;

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
 * @param idx           Index of the texture/flat/sprite/etc to search for.
 * @param groupNum      Specific MG_* material group NOT @c MG_ANY.
 * @return              Unique number of the found material, else zero.
 */
static materialnum_t getMaterialNumForIndex(uint idx,
                                            materialgroup_t groupNum)
{
    materialnum_t       i;

    // Go through the candidates.
    for(i = 0; i < numMaterialBinds; ++i)
    {
        materialbind_t*     mb = &materialBinds[i];
        material_t*         mat = mb->mat;

        if(mat->group == groupNum && mat->tex->ofTypeID == idx)
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

static animgroup_t* getAnimGroup(int number)
{
    if(--number < 0 || number >= numgroups)
        return NULL;

    return &groups[number];
}

static boolean isInAnimGroup(int groupNum, const material_t* mat)
{
    int                 i;
    animgroup_t*        group;

    if(!mat)
        return false;

    if(!(group = getAnimGroup(groupNum)))
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

static void updateMaterialTex(materialtex_t* mTex)
{
    switch(mTex->type)
    {
    case MTT_TEXTURE:
        mTex->isFromIWAD =
            (R_GetTextureDef(mTex->ofTypeID)->flags & TXDF_IWAD)? true : false;
        break;

    case MTT_FLAT:
        mTex->isFromIWAD = W_IsFromIWAD(flats[mTex->ofTypeID]->lump);
        break;

    case MTT_DDTEX:
        mTex->isFromIWAD = false; // Its definetely not.
        break;

    case MTT_SPRITE:
        mTex->isFromIWAD =
            W_IsFromIWAD(spriteTextures[mTex->ofTypeID]->lump);
        break;

    default:
        Con_Error("updateMaterialTex: Internal Error, invalid type %i.",
                  (int) mTex->type);
    }

    R_MaterialTexDelete(mTex);
}

/**
 * One time initialization of the materials list. Called during init.
 */
void R_InitMaterials(void)
{
    uint                i;

    if(initedOk)
        return; // Already been here.

    numMaterialTexs = 0;
    materialTexs = NULL;

    materialsBlockSet = Z_BlockCreate(sizeof(material_t),
                                      MATERIALS_BLOCK_ALLOC, PU_STATIC);
    materialsHead = NULL;

    materialBinds = NULL;
    numMaterialBinds = maxMaterialBinds = 0;

    // Clear the name bind hash tables.
    memset(hashTable, 0, sizeof(hashTable));
    for(i = 0; i < NUM_MATERIALTEX_TYPES; ++i)
        memset(mtexData[i].hashTable, 0, sizeof(mtexData[i].hashTable));

    initedOk = true;
}

/**
 * Release all memory acquired for the materials list.
 * Called during shutdown.
 */
void R_ShutdownMaterials(void)
{
    if(!initedOk)
        return;

    if(materialTexs)
    {
        uint                i;

        for(i = 0; i < numMaterialTexs; ++i)
        {
            materialtex_t*      mTex = materialTexs[i];
            mtexinstnode_t*     node, *next;

            node = (mtexinstnode_t*) mTex->instances;
            while(node)
            {
                next = node->next;
                Z_Free(node);
                node = next;
            }
            Z_Free(mTex);
        }

        Z_Free(materialTexs);
        materialTexs = NULL;
        numMaterialTexs = 0;
    }

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

materialnum_t R_GetNumMaterials(void)
{
    return numMaterialBinds; // Currently 1-to-1 with materials.
}

/**
 * Deletes all GL textures of materials in the specified group.
 *
 * @param group         @c MG_ANY = delete from all material groups, ELSE
 *                      The specific group of materials to delete.
 */
void R_DeleteMaterialTextures(materialgroup_t groupNum)
{
    if(groupNum != MG_ANY && !isValidMaterialGroup(groupNum))
        Con_Error("R_DeleteMaterialTextures: Internal error, "
                  "invalid materialgroup '%i'.", (int) groupNum);

    if(materialsHead)
    {
        material_t*         mat;

        mat = materialsHead;
        do
        {
            if(groupNum == MG_ANY || mat->group == groupNum)
                R_MaterialTexDelete(mat->tex);
        } while((mat = mat->globalNext));
    }
}

/**
 * Updates the minification mode of ALL registered materials.
 *
 * @param minMode       The DGL minification mode to set.
 */
void R_SetAllMaterialsMinMode(int minMode)
{
    if(materialsHead)
    {
        material_t*         mat;

        mat = materialsHead;
        do
        {
            R_MaterialSetMinMode(mat, minMode);
        } while((mat = mat->globalNext));
    }
}

/**
 * Given a name and materialtex type, search the materialtexs db for a match.
 * \assume Caller knows what it's doing; params arn't validity checked.
 *
 * @param name          Name of the materialtex to search for. Must have been
 *                      transformed to all lower case.
 * @param type          Specific MTT_* materialtex type.
 * @return              Ptr to the found materialtex_t else, @c NULL.
 */
static materialtex_t* getMaterialTexForName(const char* name, uint hash,
                                            materialtextype_t type)
{
    // Go through the candidates.
    if(mtexData[type].hashTable[hash])
    {
        materialtex_t*      mTex =
            materialTexs[mtexData[type].hashTable[hash] - 1];

        for(;;)
        {
            if(mTex->type == type && !strncmp(mTex->name, name, 8))
                return mTex;

            if(!mTex->hashNext)
                break;

            mTex = materialTexs[mTex->hashNext - 1];
        }
    }

    return 0; // Not found.
}

materialtex_t* R_MaterialTexCreate(const char* rawName, int ofTypeID,
                                   materialtextype_t type)
{
    uint                i;
    materialtex_t*      mTex;

    if(!rawName || !rawName[0])
        return NULL;

#if _DEBUG
if(type < MTT_FLAT || !(type < NUM_MATERIALTEX_TYPES))
    Con_Error("R_MaterialTexCreate: Invalid type %i.", type);
#endif

    // Check if we've already created a materialtex for this.
    for(i = 0; i < numMaterialTexs; ++i)
    {
        mTex = materialTexs[i];

        if(mTex->type == type && mTex->ofTypeID == ofTypeID)
        {
            updateMaterialTex(mTex);
            return mTex; // Yep, return it.
        }
    }

    // A new materialtex.
    mTex = Z_Malloc(sizeof(*mTex), PU_STATIC, 0);
    mTex->type = type;
    mTex->ofTypeID = ofTypeID;
    mTex->instances = NULL;
    // Prepare 'name'.
    for(i = 0; *rawName && i < 8; ++i, rawName++)
        mTex->name[i] = tolower(*rawName);
    mTex->name[i] = '\0';

    switch(type)
    {
    case MTT_TEXTURE:
        mTex->isFromIWAD =
            (R_GetTextureDef(ofTypeID)->flags & TXDF_IWAD)? true : false;
        break;

    case MTT_FLAT:
        mTex->isFromIWAD = W_IsFromIWAD(flats[ofTypeID]->lump);
        break;

    case MTT_DDTEX:
        mTex->isFromIWAD = false; // Its definetely not.
        break;

    case MTT_SPRITE:
        mTex->isFromIWAD = W_IsFromIWAD(spriteTextures[ofTypeID]->lump);
        break;

    default:
        Con_Error("R_MaterialTexCreate: Internal Error, "
                  "invalid type %i.", (int) type);
    }

    // We also hash the name for faster searching.
    {
    uint                hash = hashForName(mTex->name);
    mTex->hashNext = mtexData[type].hashTable[hash];
    mtexData[type].hashTable[hash] = numMaterialTexs + 1; // 1-based index.
    }

    /**
     * Link the new materialtex into the list of materialtexs.
     */

    // Resize the existing list.
    materialTexs =
        Z_Realloc(materialTexs, sizeof(materialtex_t*) * ++numMaterialTexs,
                  PU_STATIC);
    // Add the new materialtex;
    materialTexs[numMaterialTexs - 1] = mTex;

    return mTex;
}

materialtex_t* R_GetMaterialTex(const char* rawName, materialtextype_t type)
{
    int                 n;
    uint                hash;
    char                name[9];

    if(!rawName || !rawName[0])
        return NULL;

    // Prepare 'name'.
    for(n = 0; *rawName && n < 8; ++n, rawName++)
        name[n] = tolower(*rawName);
    name[n] = '\0';
    hash = hashForName(name);

    return getMaterialTexForName(name, hash, type);
}

materialtex_t* R_GetMaterialTexByNum(int ofTypeID, materialtextype_t type)
{
    uint                i;
    materialtex_t*      mTex;

    for(i = 0; i < numMaterialTexs; ++i)
    {
        mTex = materialTexs[i];

        if(mTex->type == type && mTex->ofTypeID == ofTypeID)
            return mTex; // Yep, return it.
    }

    return NULL;
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
 * @param mTex          Texture to use with this material.
 * @param group         MG_* material group.
 *
 * @return              The created material, ELSE @c NULL.
 */
material_t* R_MaterialCreate(const char* rawName, short width, short height,
                             byte flags, materialtex_t* mTex,
                             materialgroup_t groupNum)
{
    materialnum_t       i;
    material_t*         mat;
    int                 n;
    uint                hash;
    char                name[9];

    if(!initedOk)
        return NULL;

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(!rawName || !rawName[0] || rawName[0] == '-')
    {
#if _DEBUG
Con_Message("R_MaterialCreate: Warning, attempted to create material with "
            "NULL name\n.");
#endif
        return NULL;
    }

    if(!isValidMaterialGroup(groupNum))
    {
#if _DEBUG
Con_Message("R_MaterialCreate: Warning, attempted to create material in "
            "unknown group '%i'.\n", (int) groupNum);
        return NULL;
#endif
    }

    // Prepare 'name'.
    for(n = 0; *rawName && n < 8; ++n, rawName++)
        name[n] = tolower(*rawName);
    name[n] = '\0';
    hash = hashForName(name);

    // Check if we've already created a material for this.
    if((i = getMaterialNumForName(name, hash, groupNum)))
    {
        materialbind_t*     mb = &materialBinds[i - 1];

        mat = mb->mat;

        // Update the (possibly new) meta data.
        mat->tex = mTex;
        mat->flags = flags;
        mat->width = width;
        mat->height = height;
        mat->inAnimGroup = false;
        mat->current = mat->next = mat;
        mat->inter = 0;
        mat->decoration = NULL;
        mat->ptcGen = NULL;
        mat->detail = NULL;
        mat->envClass = MATCLASS_UNKNOWN;

        return mat; // Yep, return it.
    }

    // A new material.
    mat = Z_BlockNewElement(materialsBlockSet);
    memset(mat, 0, sizeof(*mat));
    mat->group = groupNum;
    mat->width = width;
    mat->height = height;
    mat->flags = flags;
    mat->envClass = MATCLASS_UNKNOWN;
    mat->current = mat->next = mat;
    mat->tex = mTex;

    // Link the new material into the list of materials.
    mat->globalNext = materialsHead;
    materialsHead = mat;

    // Now create a name binding for it.
    newMaterialNameBinding(mat, name, hash);

    return mat;
}

static materialtexinst_t* pickTexInstance(materialtex_t* mTex, int flags)
{
    mtexinstnode_t*     node;

    node = (mtexinstnode_t*) mTex->instances;
    while(node)
    {
        if(node->flags == flags)
            return &node->glTex;
        node = node->next;
    }

    return NULL;
}

materialtexinst_t* R_MaterialPrepare(struct material_s* mat, int flags,
                                     gltexture_t* glTex,
                                     gltexture_t* glDetailTex, byte* result)
{
    if(mat)
    {
        byte                tmpResult = 0;
        materialtex_t*      mTex = mat->tex;
        materialtexinst_t*  texInst, tempInst;
        detailtexinst_t*    detailInst;

        // Pick the instance matching the specified flags.
        texInst = pickTexInstance(mTex, flags);
        if(!texInst)
        {   // No existing suitable instance.
            // Use a temporay local instance until we are sure preparation
            // completes successfully.
            memset(&tempInst, 0, sizeof(materialtexinst_t));
            texInst = &tempInst;
        }

        switch(mTex->type)
        {
        case MTT_FLAT:
            tmpResult = GL_PrepareFlat(texInst, mTex->ofTypeID,
                                       mTex->isFromIWAD);
            break;

        case MTT_TEXTURE:
            tmpResult = GL_PrepareTexture(texInst, mTex->ofTypeID,
                                          mTex->isFromIWAD,
                                          (flags & TEXF_LOAD_AS_SKY)? true : false,
                                          (flags & TEXF_TEX_ZEROMASK)? true : false,
                                          (flags & TEXF_LOAD_AS_SKY)? true : false);
            break;

        case MTT_SPRITE:
            tmpResult = GL_PrepareSprite(texInst, mTex->ofTypeID,
                                         mTex->isFromIWAD);
            break;

        case MTT_DDTEX:
            tmpResult = GL_PrepareDDTexture(texInst, mTex->ofTypeID);
            break;

        default:
            Con_Error("R_MaterialPrepare: Internal error, invalid type %i.",
                      (int) mTex->type);
        };

        if(tmpResult)
        {
            if(texInst == &tempInst)
            {   // We have a new instance.
                mtexinstnode_t*         node;

                // Add it to the list of intances for this materialtex.
                node = Z_Malloc(sizeof(*node), PU_STATIC, 0);
                node->flags = flags;
                memcpy(&node->glTex, texInst, sizeof(materialtexinst_t));
                node->next = (mtexinstnode_t*) mTex->instances;
                mTex->instances = (void*) node;
                texInst = &node->glTex;
            }

            // We need to update the assocated enhancements.
            // Material decorations.
            mat->flags &= ~MATF_GLOW;
            if((mat->decoration = Def_GetDecoration(mat, tmpResult == 2)))
            {
                // A glowing material?
                if(mat->decoration->glow)
                    mat->flags |= MATF_GLOW;
            }

            // Surface reflection.
            {
            ded_reflection_t*   def = Def_GetReflection(mat, tmpResult == 2);

            // Make sure the shiny texture and mask has been loaded.
            if(GL_LoadReflectionMap(def))
            {
                mat->shiny.tex = def->useShiny->shinyTex;
                mat->shiny.blendMode = def->blendMode;
                mat->shiny.shininess = def->shininess;
                mat->shiny.minColor[CR] = def->minColor[CR];
                mat->shiny.minColor[CG] = def->minColor[CG];
                mat->shiny.minColor[CB] = def->minColor[CB];
                mat->shiny.maskTex =
                    (def->useMask? def->useMask->maskTex : 0);
                mat->shiny.maskWidth = def->maskWidth;
                mat->shiny.maskHeight = def->maskHeight;
            }
            else
            {
                mat->shiny.tex = 0;
            }
            }

            // Load a detail texture (if one is defined).
            {
            ded_detailtexture_t* def = Def_GetDetailTex(mat, tmpResult == 2);

            if(def)
            {
                lumpnum_t           lump =
                    W_CheckNumForName(def->detailLump.path);
                const char*         external =
                    (def->isExternal? def->detailLump.path : NULL);

                mat->detail = R_GetDetailTexture(lump, external, def->scale,
                                                 def->strength, def->maxDist);
            }
            else
            {
                mat->detail = NULL;
            }
            }

            // Get the particle generator definition for this.
            {
            ded_ptcgen_t*       def;
            int                 i, g;
            boolean             found = false;

            // The generator will be determined now.
            for(i = 0, def = defs.ptcGens; i < defs.count.ptcGens.num; ++i, def++)
            {
                materialnum_t       num;
                material_t*         defMat;

                if(!(num = R_MaterialNumForName(def->materialName,
                                                def->materialGroup)))
                    continue;
                defMat = materialBinds[num - 1].mat;

                if(def->flags & PGF_GROUP)
                {
                    // This generator is triggered by all the materials in
                    // the animation group.

                    // We only need to search if we know both the real used
                    // flat and the flat of this definition belong in an
                    // animgroup.
                    if(defMat->inAnimGroup && mat->inAnimGroup)
                    {
                        for(g = 0; g < numgroups; ++g)
                        {
                            // Precache groups don't apply.
                            if(groups[g].flags & AGF_PRECACHE)
                                continue;

                            if(isInAnimGroup(groups[g].id, defMat) &&
                               isInAnimGroup(groups[g].id, mat))
                            {
                                // Both are in this group! This def will do.
                                mat->ptcGen = def;
                                found = true;
                            }
                        }
                    }
                }

                if(mat == defMat)
                {
                    mat->ptcGen = def;
                    found = true;
                }
            }

            if(!found)
                mat->ptcGen = NULL;
            }
        }

        detailInst = NULL;
        if(r_detail && mat->detail)
            detailInst =
                GL_PrepareDetailTexture(mat->detail, mat->detail->strength);

        if(glTex)
        {
            glTex->id = (texInst? texInst->tex : 0);
            switch(mat->tex->type)
            {
            case MTT_SPRITE:
                {
                spritetex_t*        sprTex =
                    spriteTextures[mat->tex->ofTypeID];

                glTex->width = sprTex->width;
                glTex->height = sprTex->height;
                glTex->scale = 1;
                break;
                }

            case MTT_TEXTURE:
                {
                texturedef_t*       texDef =
                    R_GetTextureDef(mat->tex->ofTypeID);

                glTex->width = texDef->width;
                glTex->height = texDef->height;
                glTex->scale = 1;
                break;
                }
            case MTT_FLAT:
                glTex->width = 64;
                glTex->height = 64;
                glTex->scale = 1;
                break;

            case MTT_DDTEX:
                glTex->width = 64;
                glTex->height = 64;
                glTex->scale = 1;
                break;

            default:
                Con_Error("R_MaterialPrepare: Internal error, "
                          "invalid type %i.", (int) mat->tex->type);
                break;
            }

            if(mat->tex->type == MTT_SPRITE)
                glTex->magMode = filterSprites? DGL_LINEAR : DGL_NEAREST;
            else
                glTex->magMode = glmode[texMagMode];

            glTex->flags = 0;
            if(texInst && texInst->masked)
                glTex->flags |= GLTXF_MASKED;
        }

        if(glDetailTex)
        {
            if(r_detail && detailInst)
            {
                glDetailTex->id = detailInst->tex;
                glDetailTex->magMode = DGL_LINEAR;
                glDetailTex->width = mat->detail->width;
                glDetailTex->height = mat->detail->height;
                glDetailTex->scale = mat->detail->scale;
                glDetailTex->flags = 0;
            }
            else
            {
                glDetailTex->id = 0;
                glDetailTex->magMode = DGL_LINEAR;
                glDetailTex->width = glDetailTex->height = 0;
                glDetailTex->scale = 1;
                glDetailTex->flags = 0;
            }
        }

        if(result)
            (*result) = tmpResult;

        return texInst;
    }

    if(glTex)
    {
        glTex->id = 0;
        glTex->magMode = DGL_LINEAR;
        glTex->width = glTex->height = 64;
        glTex->scale = 1;
        glTex->flags = 0;
    }

    if(glDetailTex)
    {
        glDetailTex->id = 0;
        glDetailTex->magMode = DGL_LINEAR;
        glDetailTex->width = glDetailTex->height = 0;
        glDetailTex->scale = 1;
        glDetailTex->flags = 0;
    }

    return NULL;
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
material_t* R_GetMaterial(int ofTypeID, materialgroup_t groupNum)
{
    if(!initedOk)
        return NULL;

    if(!isValidMaterialGroup(groupNum)) // MG_ANY is considered invalid.
    {
#if _DEBUG
Con_Message("R_GetMaterial: Internal error, invalid material group '%i'\n",
            (int) groupNum);
#endif
        return NULL;
    }

    if(materialsHead)
    {
        material_t*         mat;

        mat = materialsHead;
        do
        {
            if(groupNum == mat->group && mat->tex->ofTypeID == ofTypeID)
            {
                if(mat->flags & MATF_NO_DRAW)
                   return NULL;

                return mat;
            }
        } while((mat = mat->globalNext));
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
    if(!initedOk)
        return NULL;

    if(num != 0 && num <= numMaterialBinds)
        return materialBinds[num - 1].mat; // 1-based index.

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

        for(i = 0; i < numMaterialBinds; ++i)
            if(materialBinds[i].mat == mat)
                return i + 1; // 1-based index.
    }

    return 0;
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
materialnum_t R_MaterialCheckNumForName(const char* rawName,
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
Con_Message("R_GetMaterial: Internal error, invalid material group '%i'\n",
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
 * \note2 Same as R_MaterialCheckNumForName except will log an error
 *        message if the material being searched for is not found.
 *
 * @param name          Name of the material to search for.
 * @param group         MG_* material group.
 *
 * @return              Unique identifier of the found material, else zero.
 */
materialnum_t R_MaterialNumForName(const char* name, materialgroup_t group)
{
    materialnum_t       result;

    if(!initedOk)
        return 0;

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(!name || !name[0] || name[0] == '-')
        return 0;

    result = R_MaterialCheckNumForName(name, group);

    // Not found?
    if(result == 0 && !mapSetup) // Don't announce during map setup.
        Con_Message("R_MaterialNumForName: \"%.8s\" in group %i not found!\n",
                    name, group);
    return result;
}

materialnum_t R_MaterialCheckNumForIndex(uint idx, materialgroup_t groupNum)
{
    if(!initedOk)
        return 0;

    // Caller wants a material in a specific group.
    if(!isValidMaterialGroup(groupNum))
    {
#if _DEBUG
Con_Message("R_GetMaterial: Internal error, invalid material group '%i'\n",
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
 * \note2 Sames as R_MaterialCheckNumForIndex except will log an error
 *        message if the material being searched for is not found.
 *
 * @param idx           Index of the texture/flat/sprite/etc.
 * @param group         MG_* material group.
 *
 * @return              Unique identifier of the found material, else zero.
 */
materialnum_t R_MaterialNumForIndex(uint idx, materialgroup_t group)
{
    materialnum_t       result = R_MaterialCheckNumForIndex(idx, group);

    // Not found? Don't announce during map setup or if not yet inited.
    if(result == 0 && (!mapSetup || !initedOk))
        Con_Message("R_MaterialNumForIndex: %u in group %i not found!\n",
                    idx, group);
    return result;
}

/**
 * Given a unique material identifier, lookup the associated name.
 * \note Part of the Doomsday public API.
 *
 * @param num           The unique identifier.
 *
 * @return              The associated name.
 */
const char* R_MaterialNameForNum(materialnum_t num)
{
    if(!initedOk)
        return NULL;

    if(num != 0 && num <= numMaterialBinds)
        return materialBinds[num-1].name;

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
        materialtex_t*      mTex = mat->tex;
        mtexinstnode_t*     node;

        node = (mtexinstnode_t*) mTex->instances;
        while(node)
        {
            materialtexinst_t*  inst = &node->glTex;

            if(inst->tex) // Is the texture loaded?
            {
                DGL_Bind(inst->tex);
                DGL_TexFilter(DGL_MIN_FILTER, minMode);
            }
            node = node->next;
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
 * Retrieve the decoration definition associated with the material.
 *
 * @return              The associated decoration definition, else @c NULL.
 */
const ded_decor_t* R_MaterialGetDecoration(material_t* mat)
{
    if(mat)
    {
        // Ensure we've already prepared this material.
        R_MaterialPrepare(mat->current, 0, NULL, NULL, NULL);

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
        R_MaterialPrepare(mat->current, 0, NULL, NULL, NULL);

        return mat->ptcGen;
    }

    return NULL;
}

materialclass_t R_MaterialGetClass(material_t* mat)
{
    if(mat)
    {
        if(mat->envClass == MATCLASS_UNKNOWN)
        {
            materialnum_t       matIdx = R_GetMaterialNum(mat);

            S_MaterialClassForName(R_MaterialNameForNum(matIdx), mat->group);
        }

        if(!(mat->flags & MATF_NO_DRAW))
        {
            return mat->envClass;
        }
    }

    return MATCLASS_UNKNOWN;
}

/**
 * Populate 'info' with information about the requested material.
 * \note Part of the Doomsday public API.
 *
 * @param mat           Material num.
 * @param info          Ptr to info to be populated.
 */
boolean R_MaterialGetInfo(materialnum_t num, materialinfo_t* info)
{
    material_t*         mat;

    if(!initedOk)
        return false;

    if(!info)
        Con_Error("R_MaterialGetInfo: Info paramater invalid.");

    if(!(mat = R_GetMaterialByNum(num)))
        return false;

    info->num = num;
    info->group = mat->group;
    info->width = (int) mat->width;
    info->height = (int) mat->height;
    info->flags = mat->flags;

    return true;
}

void R_MaterialSetSkyMask(materialnum_t num, boolean yes)
{
    material_t*         mat;

    if(!(mat = R_GetMaterialByNum(num)))
        return;

    if(yes)
        mat->flags |= MATF_SKYMASK;
    else
        mat->flags &= ~MATF_SKYMASK;
}

/**
 * Deletes a texture (not for rawlumptexs' etc.).
 */
void R_MaterialTexDelete(materialtex_t* mTex)
{
    if(mTex)
    {
        mtexinstnode_t*     node;

        // Delete all instances.
        node = (mtexinstnode_t*) mTex->instances;
        while(node)
        {
            materialtexinst_t*  inst = &node->glTex;

            if(inst->tex) // Is the texture loaded?
            {
                DGL_DeleteTextures(1, &inst->tex);
                inst->tex = 0;
            }
            node = node->next;
        }
    }
}

/**
 * \note Part of the Doomsday public API.
 *
 * @return              @c true, if the texture is probably not from
 *                      the original game.
 */
boolean R_MaterialIsCustom(materialnum_t num)
{
    material_t*         mat = R_GetMaterialByNum(num);

    if(mat)
        return !mat->tex->isFromIWAD;

    return true; // Yes?
}

/**
 * Prepares all resources associated with the specified material including
 * all in the same animation group.
 */
void R_MaterialPrecache2(material_t* mat)
{
    if(!mat)
        return;

    if(mat->inAnimGroup)
    {   // The material belongs in one or more animgroups.
        int                 i;

        for(i = 0; i < numgroups; ++i)
        {
            if(isInAnimGroup(groups[i].id, mat))
            {
                int                 k;

                // Precache this group.
                for(k = 0; k < groups[i].count; ++k)
                {
                    animframe_t*        frame = &groups[i].frames[k];

                    R_MaterialPrepare(frame->mat->current, 0, NULL, NULL, NULL);
                }
            }
        }

        return;
    }

    // Just this one material.
    R_MaterialPrepare(mat->current, 0, NULL, NULL, NULL);
}

/**
 * \note Part of the Doomsday public API.
 */
void R_MaterialPrecache(materialnum_t num)
{
    R_MaterialPrecache2(R_GetMaterialByNum(num));
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
    material_t*         mat;

    group = getAnimGroup(groupNum);
    if(!group)
        Con_Error("R_AddToAnimGroup: Unknown anim group '%i'\n.", groupNum);

    mat = R_GetMaterialByNum(num);
    if(!mat)
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

boolean R_IsInAnimGroup(int groupNum, materialnum_t num)
{
    return isInAnimGroup(groupNum, R_GetMaterialByNum(num));
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
    R_AnimateAnimGroups();
}

void R_AnimateAnimGroups(void)
{
    int                 i, timer, k;
    animgroup_t*        group;

    // The animation will only progress when the game is not paused.
    if(clientPaused)
        return;

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
                material_t*             real, *current, *next;

                real = group->frames[k].mat;
                current =
                    group->frames[(group->index + k) % group->count].mat;
                next =
                    group->frames[(group->index + k + 1) % group->count].mat;

                R_MaterialSetTranslation(real, current, next, 0);

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
                material_t*             mat = group->frames[k].mat;

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

static void printMaterials(materialgroup_t grp)
{
    materialnum_t       i, numDigits;

    if(!(grp < NUM_MATERIAL_GROUPS))
        return;

    if(grp == MG_ANY)
        Con_Printf("Known Materials (IDX - Name (Group) [width, height]):\n");
    else
        Con_Printf("Known Materials in Group %i (IDX - Name [width, height]):\n", grp);

    numDigits = floor(log10(abs(numMaterialBinds))) + 1;
    for(i = 0; i < numMaterialBinds; ++i)
    {
        int                 j;
        materialbind_t*     mb = &materialBinds[i];
        material_t*         mat = mb->mat;

        if(grp != MG_ANY && mat->group != grp)
            continue;

        Con_Printf(" %*lu - \"%s\"", numDigits, i, mb->name);
        if(grp == MG_ANY)
            Con_Printf(" (%i)", mat->group);
        Con_Printf(" [%i, %i]", mat->width, mat->height);
        Con_Printf("\n");

        for(j = 0; j < numgroups; ++j)
        {
            if(isInAnimGroup(groups[j].id, mat))
            {
                int                 k;

                for(k = 0; k < groups[j].count; ++k)
                {
                    animframe_t*        frame = &groups[j].frames[k];
                    materialnum_t       otherIDX =
                        getMaterialNumForIndex(frame->mat->tex->ofTypeID,
                                               frame->mat->group) - 1;
                    materialbind_t*     otherMB = &materialBinds[otherIDX];

                    Con_Printf(" > %*lu - \"%s\"\n", numDigits, otherIDX,
                               otherMB->name);
                }
            }
        }
    }
}

D_CMD(ListMaterials)
{
    materialgroup_t     grp = MG_ANY;

    if(argc > 1)
    {
        grp = atoi(argv[1]);

        if(grp < MG_FIRST || !(grp < NUM_MATERIAL_GROUPS))
        {
            Con_Printf("Invalid material group \"%s\".\n", argv[1]);
            return false;
        }
    }

    printMaterials(grp);
    return true;
}
