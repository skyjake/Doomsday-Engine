/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * r_data.c: Data Structures and Constants for Refresh
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_audio.h" // For texture, environmental audio properties.

// MACROS ------------------------------------------------------------------

#define PATCH_HASH_SIZE  128
#define PATCH_HASH(x)    (patchhash + (((unsigned) x) & (PATCH_HASH_SIZE - 1)))

// TYPES -------------------------------------------------------------------

typedef struct patchhash_s {
    patch_t*        first;
} patchhash_t;

typedef struct {
    boolean         inUse;
    boolean         type; // If true, data is rcolor else rvertex.
    uint            num;
    void*           data;
} rendpolydata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

int gameDataFormat; // use a game-specifc data format where applicable

extern boolean levelSetup; // we are currently setting up a level

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte precacheSkins = true;
byte precacheSprites = false;

patchhash_t patchhash[PATCH_HASH_SIZE];

int numTextures;
texture_t** textures;

int numFlats;
flat_t** flats;

int numSpriteTextures;
spritetex_t** spriteTextures;

// Raw screens.
rawtex_t* rawtextures;
uint numrawtextures;
int numgroups;
animgroup_t* groups;

// Glowing textures are always rendered fullbright.
int glowingTextures = true;

byte rendInfoRPolys = 0;

// Skinnames will only *grow*. It will never be destroyed, not even
// at resets. The skin textures themselves will be deleted, though.
// This is because we want to have permanent ID numbers for skins,
// and the ID numbers are the same as indices to the skinNames array.
// Created in r_model.c, when registering the skins.
int numSkinNames;
skintex_t* skinNames;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static unsigned int numrendpolys = 0;
static unsigned int maxrendpolys = 0;
static rendpolydata_t** rendPolys;

// CODE --------------------------------------------------------------------

void R_InfoRendVerticesPool(void)
{
    uint                i;

    if(!rendInfoRPolys)
        return;

    Con_Printf("RP Count: %-4i\n", numrendpolys);

    for(i = 0; i < numrendpolys; ++i)
    {
        Con_Printf("RP: %-4u %c (vtxs=%u t=%c)\n", i,
                   (rendPolys[i]->inUse? 'Y':'N'), rendPolys[i]->num,
                   (rendPolys[i]->type? 'c' : 'v'));
    }
}

/**
 * Called at the start of each level.
 */
void R_InitRendVerticesPool(void)
{
    rvertex_t*          rvertices;
    rcolor_t*           rcolors;

    numrendpolys = maxrendpolys = 0;
    rendPolys = NULL;

    rvertices = R_AllocRendVertices(24);
    rcolors = R_AllocRendColors(24);

    // Mark unused.
    R_FreeRendVertices(rvertices);
    R_FreeRendColors(rcolors);
}

/**
 * Retrieves a batch of rvertex_t.
 * Possibly allocates new if necessary.
 *
 * @param num           The number of verts required.
 *
 * @return              Ptr to array of rvertex_t
 */
rvertex_t* R_AllocRendVertices(uint num)
{
    unsigned int        idx;
    boolean             found = false;

    for(idx = 0; idx < maxrendpolys; ++idx)
    {
        if(rendPolys[idx]->inUse || rendPolys[idx]->type)
            continue;

        if(rendPolys[idx]->num >= num)
        {
            // Use this one.
            rendPolys[idx]->inUse = true;
            return (rvertex_t*) rendPolys[idx]->data;
        }
        else if(rendPolys[idx]->num == 0)
        {
            // There is an unused one but we haven't allocated verts yet.
            numrendpolys++;
            found = true;
            break;
        }
    }

    if(!found)
    {
        // We may need to allocate more.
        if(++numrendpolys > maxrendpolys)
        {
            uint                i, newCount;
            rendpolydata_t*     newPolyData, *ptr;

            maxrendpolys = (maxrendpolys > 0? maxrendpolys * 2 : 8);

            rendPolys =
                Z_Realloc(rendPolys, sizeof(rendpolydata_t*) * maxrendpolys,
                          PU_LEVEL);

            newCount = maxrendpolys - numrendpolys + 1;

            newPolyData =
                Z_Malloc(sizeof(rendpolydata_t) * newCount, PU_LEVEL, 0);

            ptr = newPolyData;
            for(i = numrendpolys-1; i < maxrendpolys; ++i, ptr++)
            {
                ptr->inUse = false;
                ptr->num = 0;
                rendPolys[i] = ptr;
            }
        }
        idx = numrendpolys - 1;
    }

    rendPolys[idx]->inUse = true;
    rendPolys[idx]->type = false;
    rendPolys[idx]->num = num;
    rendPolys[idx]->data =
        Z_Malloc(sizeof(rvertex_t) * num, PU_LEVEL, 0);

    return (rvertex_t*) rendPolys[idx]->data;
}

/**
 * Retrieves a batch of rcolor_t.
 * Possibly allocates new if necessary.
 *
 * @param num           The number of verts required.
 *
 * @return              Ptr to array of rcolor_t
 */
rcolor_t* R_AllocRendColors(uint num)
{
    unsigned int        idx;
    boolean             found = false;

    for(idx = 0; idx < maxrendpolys; ++idx)
    {
        if(rendPolys[idx]->inUse || !rendPolys[idx]->type)
            continue;

        if(rendPolys[idx]->num >= num)
        {
            // Use this one.
            rendPolys[idx]->inUse = true;
            return (rcolor_t*) rendPolys[idx]->data;
        }
        else if(rendPolys[idx]->num == 0)
        {
            // There is an unused one but we haven't allocated verts yet.
            numrendpolys++;
            found = true;
            break;
        }
    }

    if(!found)
    {
        // We may need to allocate more.
        if(++numrendpolys > maxrendpolys)
        {
            uint                i, newCount;
            rendpolydata_t*     newPolyData, *ptr;

            maxrendpolys = (maxrendpolys > 0? maxrendpolys * 2 : 8);

            rendPolys =
                Z_Realloc(rendPolys, sizeof(rendpolydata_t*) * maxrendpolys,
                          PU_LEVEL);

            newCount = maxrendpolys - numrendpolys + 1;

            newPolyData =
                Z_Malloc(sizeof(rendpolydata_t) * newCount, PU_LEVEL, 0);

            ptr = newPolyData;
            for(i = numrendpolys-1; i < maxrendpolys; ++i, ptr++)
            {
                ptr->inUse = false;
                ptr->num = 0;
                rendPolys[i] = ptr;
            }
        }
        idx = numrendpolys - 1;
    }

    rendPolys[idx]->inUse = true;
    rendPolys[idx]->type = true;
    rendPolys[idx]->num = num;
    rendPolys[idx]->data =
        Z_Malloc(sizeof(rcolor_t) * num, PU_LEVEL, 0);

    return (rcolor_t*) rendPolys[idx]->data;
}

/**
 * Doesn't actually free anything. Instead, mark them as unused ready for
 * the next time a batch of rendvertex_t is needed.
 *
 * @param vertices      Ptr to array of rvertex_t to mark unused.
 */
void R_FreeRendVertices(rvertex_t* rvertices)
{
    uint                i;

    if(!rvertices)
        return;

    for(i = 0; i < numrendpolys; ++i)
    {
        if(rendPolys[i]->data == rvertices)
        {
            rendPolys[i]->inUse = false;
            return;
        }
    }
#if _DEBUG
    Con_Message("R_FreeRendPoly: Dangling poly ptr!\n");
#endif
}

/**
 * Doesn't actually free anything. Instead, mark them as unused ready for
 * the next time a batch of rendvertex_t is needed.
 *
 * @param vertices      Ptr to array of rcolor_t to mark unused.
 */
void R_FreeRendColors(rcolor_t* rcolors)
{
    uint                i;

    if(!rcolors)
        return;

    for(i = 0; i < numrendpolys; ++i)
    {
        if(rendPolys[i]->data == rcolors)
        {
            rendPolys[i]->inUse = false;
            return;
        }
    }
#if _DEBUG
    Con_Message("R_FreeRendPoly: Dangling poly ptr!\n");
#endif
}

void R_ShutdownData(void)
{
    R_ShutdownMaterials();
}

/**
 * Returns a NULL-terminated array of pointers to all the patches.
 * The array must be freed with Z_Free.
 */
patch_t **R_CollectPatches(int *count)
{
    int                 i, num;
    patch_t            *p, **array;

    // First count the number of patches.
    for(num = 0, i = 0; i < PATCH_HASH_SIZE; ++i)
        for(p = patchhash[i].first; p; p = p->next)
            num++;

    // Tell this to the caller.
    if(count)
        *count = num;

    // Allocate the array, plus one for the terminator.
    array = Z_Malloc(sizeof(patch_t *) * (num + 1), PU_STATIC, NULL);

    // Collect the pointers.
    for(num = 0, i = 0; i < PATCH_HASH_SIZE; ++i)
        for(p = patchhash[i].first; p; p = p->next)
            array[num++] = p;

    // Terminate.
    array[num] = NULL;

    return array;
}

/**
 * Returns a patch_t* for the given lump, if one already exists.
 */
patch_t *R_FindPatch(lumpnum_t lump)
{
    patch_t            *i;
    patchhash_t        *hash = PATCH_HASH(lump);

    for(i = hash->first; i; i = i->next)
        if(i->lump == lump)
        {
            return i;
        }

    return NULL;
}

/**
 * Returns a rawtex_t* for the given lump, if one already exists.
 */
rawtex_t *R_FindRawTex(lumpnum_t lump)
{
    uint                i;

    for(i = 0; i < numrawtextures; ++i)
        if(rawtextures[i].lump == lump)
        {
            return &rawtextures[i];
        }

    return NULL;
}

/**
 * Get a rawtex_t data structure for a raw texture specified with a WAD lump
 * number.  Allocates a new rawtex_t if it hasn't been loaded yet.
 */
rawtex_t *R_GetRawTex(lumpnum_t lump)
{
    rawtex_t           *r;

    if(lump >= numLumps)
    {
        Con_Error("R_GetPatch: lump = %i out of bounds (%i).\n",
                  lump, numLumps);
    }

    r = R_FindRawTex(lump);
    // Check if this lump has already been loaded as a rawtex.
    if(r)
        return r;

    // Hmm, this is an entirely new rawtex.
    rawtextures = M_Realloc(rawtextures, sizeof(rawtex_t) * ++numrawtextures);
    r = &rawtextures[numrawtextures - 1];

    r->lump = lump;
    r->tex = r->tex2 = 0;
    r->info.width = r->info2.width = 0;
    r->info.height = r->info2.height = 0;
    r->info.masked = r->info2.masked = 0;

    return r;
}

/**
 * Get a patch_t data structure for a patch specified with a WAD lump
 * number.  Allocates a new patch_t if it hasn't been loaded yet.
 */
patch_t *R_GetPatch(lumpnum_t lump)
{
    patch_t            *p = 0;
    patchhash_t        *hash = 0;

    if(lump >= numLumps)
    {
        Con_Error("R_GetPatch: lump = %i out of bounds (%i).\n",
                  lump, numLumps);
    }

    p = R_FindPatch(lump);

    if(!lump)
        return NULL;

    // Check if this lump has already been loaded as a patch.
    if(p)
        return p;

    // Hmm, this is an entirely new patch.
    p = Z_Calloc(sizeof(patch_t), PU_PATCH, NULL);
    hash = PATCH_HASH(lump);

    // Link to the hash.
    p->next = hash->first;
    hash->first = p;

    // Init the new one.
    p->lump = lump;
    return p;
}

/**
 * Create a new animation group. Returns the group number.
 * This function is exported and accessible from DLLs.
 */
int R_CreateAnimGroup(int flags)
{
    animgroup_t        *group;

    // Allocating one by one is inefficient, but it doesn't really matter.
    groups =
        Z_Realloc(groups, sizeof(animgroup_t) * (numgroups + 1), PU_STATIC);

    // Init the new group.
    group = groups + numgroups;
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
            animgroup_t    *group = &groups[i];
            Z_Free(group->frames);
        }

        Z_Free(groups);
        groups = NULL;
        numgroups = 0;
    }
}

animgroup_t *R_GetAnimGroup(int number)
{
    if(--number < 0 || number >= numgroups)
        return NULL;
    return groups + number;
}

/**
 * This function is exported and accessible from DLLs.
 */
void R_AddToAnimGroup(int groupNum, const char *name, materialtype_t type,
                      int tics, int randomTics)
{
    animgroup_t        *group;
    animframe_t        *frame;
    int                 number;
    material_t         *mat;

    if(!name || !name[0])
        return;

    group = R_GetAnimGroup(groupNum);
    if(!group)
        Con_Error("R_AddToAnimGroup: Unknown anim group %i.", groupNum);

    number = R_CheckMaterialNumForName(name, type);

    if(number < 0)
    {
        Con_Message("R_AddToAnimGroup: No type %i material named '%s'.",
                    (int) type, name);
        return;
    }

    mat = R_GetMaterial(number, type);
    // Mark the material as being in an animgroup.
    mat->inGroup = true;

    // Allocate a new animframe.
    group->frames =
        Z_Realloc(group->frames, sizeof(animframe_t) * ++group->count,
                  PU_STATIC);

    frame = group->frames + group->count - 1;

    frame->mat = mat;
    frame->tics = tics;
    frame->random = randomTics;
}

boolean R_IsInAnimGroup(int groupNum, materialtype_t type, int number)
{
    int                 i;
    animgroup_t        *group = R_GetAnimGroup(groupNum);
    material_t         *mat = R_GetMaterial(number, type);

    if(!group)
        return false;

    // Is it in there?
    for(i = 0; i < group->count; ++i)
    {
        animframe_t        *frame = &group->frames[i];

        if(frame->mat == mat)
            return true;
    }

    return false;
}

/**
 * Initialize an entire animation using the data in the definition.
 */
void R_InitAnimGroup(ded_group_t *def)
{
    int                 i, ofTypeIDX;
    int                 groupNumber = -1;

    for(i = 0; i < def->count.num; ++i)
    {
        ded_group_member_t *gm = &def->members[i];

        ofTypeIDX =
            R_CheckMaterialNumForName(gm->name, (materialtype_t) gm->type);

        if(ofTypeIDX < 0)
            continue;

        // Only create a group when the first texture is found.
        if(groupNumber == -1)
        {
            // Create a new animation group.
            groupNumber = R_CreateAnimGroup(def->flags);
        }

        R_AddToAnimGroup(groupNumber, gm->name, gm->type,
                         gm->tics, gm->randomTics);
    }
}

/**
 * All animation groups are reseted back to their original state.
 * Called when setting up a map.
 */
void R_ResetAnimGroups(void)
{
    int                 i;
    animgroup_t        *group;

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

/**
 * Initializes the texture list with the textures from the world map.
 */
void R_InitTextures(void)
{
    strifemaptexture_t *smtexture;
    maptexture_t *mtexture;
    texture_t *texture;
    strifemappatch_t *smpatch;
    mappatch_t *mpatch;
    texpatch_t *patch;
    int     i, j;
    int    *maptex, *maptex2, *maptex1;
    char    name[9], *names, *name_p;
    int    *patchlookup;
    int     nummappatches;
    size_t  offset, maxoff, maxoff2;
    int     numtextures1, numtextures2;
    int    *directory;
    char    buf[64];

    // Load the patch names from the PNAMES lump.
    name[8] = 0;
    names = W_CacheLumpName("PNAMES", PU_REFRESHTEX);
    nummappatches = LONG(*((int *) names));
    name_p = names + 4;
    patchlookup = M_Malloc(nummappatches * sizeof(*patchlookup));
    for(i = 0; i < nummappatches; ++i)
    {
        strncpy(name, name_p + i * 8, 8);
        patchlookup[i] = W_CheckNumForName(name);
    }
    Z_Free(names);

    // Load the texture definitions from TEXTURE1/2.
    maptex = maptex1 = W_CacheLumpName("TEXTURE1", PU_REFRESHTEX);
    numtextures1 = LONG(*maptex);
    maxoff = W_LumpLength(W_GetNumForName("TEXTURE1"));
    directory = maptex + 1;

    if(W_CheckNumForName("TEXTURE2") != -1)
    {
        maptex2 = W_CacheLumpName("TEXTURE2", PU_REFRESHTEX);
        numtextures2 = LONG(*maptex2);
        maxoff2 = W_LumpLength(W_GetNumForName("TEXTURE2"));
    }
    else
    {
        maptex2 = NULL;
        numtextures2 = 0;
        maxoff2 = 0;
    }
    numTextures = numtextures1 + numtextures2;

    // \fixme Surely not all of these are still needed?
    textures = Z_Malloc(numTextures * sizeof(texture_t*), PU_REFRESHTEX, 0);

    sprintf(buf, "R_Init: Initializing %i textures...", numTextures);

    for(i = 0; i < numTextures; ++i, directory++)
    {
        if(i == numtextures1)
        {                       // Start looking in second texture file.
            maptex = maptex2;
            maxoff = maxoff2;
            directory = maptex + 1;
        }

        offset = LONG(*directory);
        if(offset > maxoff)
            Con_Error("R_InitTextures: bad texture directory");

        if(gameDataFormat == 0)
        {
            mtexture = (maptexture_t *) ((byte *) maptex + offset);

            texture = textures[i] =
                Z_Calloc(sizeof(texture_t) +
                        sizeof(texpatch_t) * (SHORT(mtexture->patchCount) - 1),
                        PU_REFRESHTEX, 0);
            texture->info.width = SHORT(mtexture->width);
            texture->info.height = SHORT(mtexture->height);

            texture->flags = 0;
            if(mtexture->masked)
                texture->flags |= TXF_MASKED;
            /**
             * DOOM.EXE had a bug in the way textures were managed
             * resulting in the first texture being used dually as a
             * "NULL" texture.
             */
            if(i == 0)
                texture->flags |= TXF_NO_DRAW;

            texture->patchCount = SHORT(mtexture->patchCount);

            memcpy(texture->name, mtexture->name, 8);

            mpatch = &mtexture->patches[0];
            patch = &texture->patches[0];

            for(j = 0; j < texture->patchCount; ++j, mpatch++, patch++)
            {
                strncpy(name, name_p + SHORT(mpatch->patch) * 8, 8);
                patch->originX = SHORT(mpatch->originX);
                patch->originY = SHORT(mpatch->originY);
                patch->patch = patchlookup[SHORT(mpatch->patch)];
                if(patch->patch == -1)
                {
                    Con_Error("R_InitTextures: Missing patch \"%s\" in texture %s",
                          name, texture->name);
                }
            }

        }
        else if(gameDataFormat == 3)
        {   // strife format
            smtexture = (strifemaptexture_t *) ((byte *) maptex + offset);

            texture = textures[i] =
                Z_Calloc(sizeof(texture_t) +
                        sizeof(texpatch_t) * (SHORT(smtexture->patchCount) - 1),
                        PU_REFRESHTEX, 0);
            texture->info.width = SHORT(smtexture->width);
            texture->info.height = SHORT(smtexture->height);

            texture->flags = 0;
            /**
             * STRIFE.EXE had a bug in the way textures were managed
             * resulting in the first texture being used dually as a
             * "NULL" texture.
             */
            if(i == 0)
                texture->flags |= TXF_NO_DRAW;
            texture->patchCount = SHORT(smtexture->patchCount);

            memcpy(texture->name, smtexture->name, 8);

            smpatch = &smtexture->patches[0];
            patch = &texture->patches[0];

            for(j = 0; j < texture->patchCount; ++j, smpatch++, patch++)
            {
                patch->originX = SHORT(smpatch->originX);
                patch->originY = SHORT(smpatch->originY);
                patch->patch = patchlookup[SHORT(smpatch->patch)];
                if(patch->patch == -1)
                {
                    Con_Error("R_InitTextures: Missing patch in texture %s",
                          texture->name);
                }
            }
        }
    }

    Z_Free(maptex1);
    if(maptex2)
        Z_Free(maptex2);

    // Create a material for every texture.
    for(i = 0; i < numTextures; ++i)
        R_MaterialCreate(textures[i]->name, i, MAT_TEXTURE);

    for(i = 0; i < numTextures; ++i)
    {
        // Determine the texture's material type.
        textures[i]->materialClass =
            S_MaterialClassForName(R_MaterialNameForNum(i, MAT_TEXTURE), MAT_TEXTURE);
    }

    M_Free(patchlookup);
}

/**
 * Returns the new flat index.
 */
static int R_NewFlat(lumpnum_t lump)
{
    int                 i;
    flat_t            **newlist, *ptr;

    for(i = 0; i < numFlats; i++)
    {
        // Is this lump already entered?
        if(flats[i]->lump == lump)
            return i;

        // Is this a known identifer? Newer idents overide old.
        if(!strnicmp(lumpInfo[flats[i]->lump].name, lumpInfo[lump].name, 8))
        {
            flats[i]->lump = lump;
            return i;
        }
    }

    newlist = Z_Malloc(sizeof(flat_t*) * ++numFlats, PU_REFRESHTEX, 0);
    if(numFlats > 1)
    {
        for(i = 0; i < numFlats -1; ++i)
            newlist[i] = flats[i];

        Z_Free(flats);
    }
    flats = newlist;
    ptr = flats[numFlats - 1] = Z_Calloc(sizeof(flat_t), PU_REFRESHTEX, 0);
    ptr->lump = lump;
    memcpy(ptr->name, lumpInfo[lump].name, 8);
    return numFlats - 1;
}

void R_InitFlats(void)
{
    int                 i;
    boolean             inFlatBlock;

    numFlats = 0;

    inFlatBlock = false;
    for(i = 0; i < numLumps; ++i)
    {
        char               *name = lumpInfo[i].name;

        if(!strnicmp(name, "F_START", 7))
        {
            // We've arrived at *a* flat block.
            inFlatBlock = true;
            continue;
        }
        else if(!strnicmp(name, "F_END", 5))
        {
            // The flat block ends.
            inFlatBlock = false;
            continue;
        }

        if(!inFlatBlock)
            continue;

        R_NewFlat(i);
    }

    /**
     * DOOM.exe had a bug in the way textures were managed resulting
     * in the first flat being used dually as a "NULL" texture.
     */
    if(numFlats > 0)
        flats[0]->flags |= TXF_NO_DRAW;

    // Create a material for every flat.
    for(i = 0; i < numFlats; ++i)
        R_MaterialCreate(flats[i]->name, i, MAT_FLAT);

    for(i = 0; i < numFlats; ++i)
    {
        // Determine the flat's material type.
        flats[i]->materialClass =
            S_MaterialClassForName(R_MaterialNameForNum(i, MAT_FLAT), MAT_FLAT);
    }
}

void R_InitSpriteTextures(void)
{
    lumppatch_t        *patch;
    spritetex_t        *sprTex;
    int                 i;
    char                buf[64];

    sprintf(buf, "R_InitSpriteTextures: Initializing %i sprites...",
            numSpriteTextures);

    for(i = 0; i < numSpriteTextures; ++i)
    {
        sprTex = spriteTextures[i];

        patch = (lumppatch_t *) W_CacheLumpNum(sprTex->lump, PU_CACHE);
        sprTex->info.width = SHORT(patch->width);
        sprTex->info.height = SHORT(patch->height);
        sprTex->info.offsetX = SHORT(patch->leftOffset);
        sprTex->info.offsetY = SHORT(patch->topOffset);
        memset(&sprTex->info.detail, 0, sizeof(sprTex->info.detail));
        sprTex->info.masked = 1;
        sprTex->info.modFlags = 0;
    }
}

/**
 * @return              The new sprite index number.
 */
int R_NewSpriteTexture(lumpnum_t lump, material_t **matP)
{
    spritetex_t       **newList, *ptr;
    int                 i;
    material_t         *mat;

    // Is this lump already entered?
    for(i = 0; i < numSpriteTextures; ++i)
        if(spriteTextures[i]->lump == lump)
        {
            mat = R_GetMaterial(i, MAT_SPRITE);
            if(matP)
                *matP = mat;
            return i;
        }

    newList = Z_Malloc(sizeof(spritetex_t*) * ++numSpriteTextures, PU_SPRITE, 0);
    if(numSpriteTextures > 1)
    {
        for(i = 0; i < numSpriteTextures -1; ++i)
            newList[i] = spriteTextures[i];

        Z_Free(spriteTextures);
    }

    spriteTextures = newList;
    ptr = spriteTextures[numSpriteTextures - 1] =
        Z_Calloc(sizeof(spritetex_t), PU_SPRITE, 0);
    ptr->lump = lump;

    // Create a new material for this sprite.
    mat = R_MaterialCreate(W_LumpName(lump), numSpriteTextures - 1,
                           MAT_SPRITE);
    if(matP)
        *matP = mat;

    return numSpriteTextures - 1;
}

void R_ExpandSkinName(char *skin, char *expanded, const char *modelfn)
{
    directory_t         mydir;

    // The "first choice" directory.
    Dir_FileDir(modelfn, &mydir);

    // Try the "first choice" directory first.
    strcpy(expanded, mydir.path);
    strcat(expanded, skin);
    if(!F_Access(expanded))
    {
        // Try the whole model path.
        if(!R_FindModelFile(skin, expanded))
        {
            expanded[0] = 0;
            return;
        }
    }
}

/**
 * Registers a new skin name.
 */
int R_RegisterSkin(char *skin, const char *modelfn, char *fullpath)
{
    const char         *formats[3] = { ".png", ".tga", ".pcx" };
    char                buf[256];
    char                fn[256];
    char               *ext;
    int                 i, idx = -1;

    // Has a skin name been provided?
    if(!skin[0])
        return -1;

    // Find the extension, or if there isn't one, add it.
    strcpy(fn, skin);
    ext = strrchr(fn, '.');
    if(!ext)
    {
        strcat(fn, ".png");
        ext = strrchr(fn, '.');
    }

    // Try PNG, TGA, PCX.
    for(i = 0; i < 3 && idx < 0; ++i)
    {
        strcpy(ext, formats[i]);
        R_ExpandSkinName(fn, fullpath ? fullpath : buf, modelfn);
        idx = R_GetSkinTexIndex(fullpath ? fullpath : buf);
    }

    return idx;
}

skintex_t *R_GetSkinTex(const char *skin)
{
    int                 i;
    skintex_t          *st;
    char                realPath[256];

    if(!skin[0])
        return NULL;

    // Convert the given skin file to a full pathname.
    // \fixme Why is this done here and not during init??
    _fullpath(realPath, skin, 255);

    for(i = 0; i < numSkinNames; ++i)
        if(!stricmp(skinNames[i].path, realPath))
            return skinNames + i;

    // We must allocate a new skintex_t.
    skinNames = M_Realloc(skinNames, sizeof(*skinNames) * ++numSkinNames);
    st = skinNames + (numSkinNames - 1);
    strcpy(st->path, realPath);
    st->tex = 0; // Not yet prepared.

    if(verbose)
    {
        Con_Message("SkinTex: %s => %li\n", M_PrettyPath(skin),
                    (long) (st - skinNames));
    }
    return st;
}

skintex_t *R_GetSkinTexByIndex(int id)
{
    if(id < 0 || id >= numSkinNames)
        return NULL;

    return &skinNames[id];
}

int R_GetSkinTexIndex(const char *skin)
{
    skintex_t          *sk = R_GetSkinTex(skin);

    if(!sk)
        return -1;

    return sk - skinNames;
}

void R_DeleteSkinTextures(void)
{
extern void DGL_DeleteTextures(int num, const DGLuint *names);

    int                 i;

    for(i = 0; i < numSkinNames; ++i)
    {
        DGL_DeleteTextures(1, &skinNames[i].tex);
        skinNames[i].tex = 0;
    }
}

/**
 * This is called at final shutdown.
 */
void R_DestroySkins(void)
{
    M_Free(skinNames);
    skinNames = 0;
    numSkinNames = 0;
}

void R_UpdateTexturesAndFlats(void)
{
    Z_FreeTags(PU_REFRESHTEX, PU_REFRESHTEX);
    R_MarkMaterialsForUpdating();
    R_InitTextures();
    R_InitFlats();
    R_InitSkyMap();
}

void R_InitPatches(void)
{
    memset(patchhash, 0, sizeof(patchhash));
}

void R_UpdatePatches(void)
{
    Z_FreeTags(PU_PATCH, PU_PATCH);
    memset(patchhash, 0, sizeof(patchhash));
    R_InitPatches();
}

/**
 * Locates all the lumps that will be used by all views.
 * Must be called after W_Init.
 */
void R_InitData(void)
{
    R_InitMaterials();
    R_InitTextures();
    R_InitFlats();
    R_InitPatches();
    Cl_InitTranslations();
}

void R_UpdateData(void)
{
    R_UpdateTexturesAndFlats();
    R_UpdatePatches();
    Cl_InitTranslations();
}

void R_InitTranslationTables(void)
{
    int                 i;
    byte               *transLump;

    // Allocate translation tables
    translationTables = Z_Malloc(256 * 3 * ( /*DDMAXPLAYERS*/ 8 - 1) + 255, PU_REFRESHTRANS, 0);

    translationTables = (byte *) (((long) translationTables + 255) & ~255);

    for(i = 0; i < 3 * ( /*DDMAXPLAYERS*/ 8 - 1); ++i)
    {
        // If this can't be found, it's reasonable to expect that the game dll
        // will initialize the translation tables as it wishes.
        if(W_CheckNumForName("trantbl0") < 0)
            break;

        transLump = W_CacheLumpNum(W_GetNumForName("trantbl0") + i, PU_STATIC);
        memcpy(translationTables + i * 256, transLump, 256);
        Z_Free(transLump);
    }
}

void R_UpdateTranslationTables(void)
{
    Z_FreeTags(PU_REFRESHTRANS, PU_REFRESHTRANS);
    R_InitTranslationTables();
}

/**
 * @return              @c true, if the given light decoration definition
 *                      is valid.
 */
boolean R_IsValidLightDecoration(const ded_decorlight_t *lightDef)
{
    return (lightDef &&
            (lightDef->color[0] != 0 || lightDef->color[1] != 0 ||
             lightDef->color[2] != 0));
}

/**
 * @return              @c true, if the given decoration works under the
 *                      specified circumstances.
 */
boolean R_IsAllowedDecoration(ded_decor_t *def, int index, boolean hasExternal)
{
    if(hasExternal)
    {
        return (def->flags & DCRF_EXTERNAL) != 0;
    }

    // Is it probably an original texture?
    if(!R_IsCustomMaterial(index, (def->isTexture? MAT_TEXTURE : MAT_FLAT)))
        return !(def->flags & DCRF_NO_IWAD);

    return (def->flags & DCRF_PWAD) != 0;
}

/**
 * Prepares the specified patch.
 */
void R_PrecachePatch(lumpnum_t num)
{
    GL_PreparePatch(num, NULL);
}

static boolean isInList(void **list, size_t len, void *elm)
{
    size_t              n;

    if(!list || !elm || len == 0)
        return false;

    for(n = 0; n < len; ++n)
        if(list[n] == elm)
            return true;

    return false;
}

static boolean precacheResourcesForMobjs(thinker_t* th, void* context)
{
    if(P_IsMobjThinker(th->function))
    {
        // Precache all the skins for the mobj.
        R_PrecacheSkinsForMobj((mobj_t *) th);
    }

    return true; // Continue iteration.
}

/**
 * Prepare all relevant skins, textures, flats and sprites.
 * Doesn't unload anything, though (so that if there's enough
 * texture memory it will be used more efficiently). That much trust
 * is placed in the GL/D3D drivers. The prepared textures are also bound
 * here once so they should be ready for use ASAP.
 */
void R_PrecacheLevel(void)
{
    uint            i, j;
    size_t          n;
    sector_t       *sec;
    sidedef_t      *side;
    material_t     *mat;
    float           starttime;
    material_t    **matPresent;

    // Don't precache when playing demo.
    if(isDedicated || playback)
        return;

    // Precaching from 100 to 200.
    Con_SetProgress(100);

    starttime = Sys_GetSeconds();

    // Precache all materials used on world surfaces.
    matPresent = M_Calloc(numMaterials);
    n = 0;

    for(i = 0; i < numSideDefs; ++i)
    {
        side = SIDE_PTR(i);

        mat = side->SW_topmaterial;
        if(mat && !isInList((void**) matPresent, n, mat))
            matPresent[n++] = mat;

        mat = side->SW_middlematerial;
        if(mat && !isInList((void**) matPresent, n, mat))
            matPresent[n++] = mat;

        mat = side->SW_bottommaterial;
        if(mat && !isInList((void**) matPresent, n, mat))
            matPresent[n++] = mat;
    }

    for(i = 0; i < numSectors; ++i)
    {
        sec = SECTOR_PTR(i);

        for(j = 0; j < sec->planeCount; ++j)
        {
            mat = sec->SP_planematerial(j);
            if(mat && !isInList((void**) matPresent, n, mat))
                matPresent[n++] = mat;
        }
    }

    if(precacheSprites)
    {
        int                 i;

        for(i = 0; i < numSprites; ++i)
        {
            int                 j;
            spritedef_t        *sprDef = &sprites[i];

            for(j = 0; j < sprDef->numFrames; ++j)
            {
                int                 k;
                spriteframe_t      *sprFrame = &sprDef->spriteFrames[j];

                for(k = 0; k < 8; ++k)
                {
                    mat = sprFrame->mats[k];
                    if(mat && !isInList((void**) matPresent, n, mat))
                        matPresent[n++] = mat;
                }
            }
        }
    }

    i = 0;
    while(i < numMaterials && matPresent[i])
        R_PrecacheMaterial(matPresent[i++]);

    // We are done with list of used materials.
    M_Free(matPresent);
    matPresent = NULL;

    // \fixme Precache sky textures!

    // Precache skins?
    if(useModels && precacheSkins)
    {
        P_IterateThinkers(NULL, precacheResourcesForMobjs, NULL);
    }

    // Update progress.

    // Sky models usually have big skins.
    R_PrecacheSky();

    if(verbose)
    {
        Con_Message("Precaching took %.2f seconds.\n",
                    Sys_GetSeconds() - starttime);
    }

    // Done!
    //Con_Progress(100, PBARF_SET);
}

void R_AnimateAnimGroups(void)
{
    int                 i, timer, k;
    animgroup_t        *group;

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

            // Update texture/flat translations.
            for(k = 0; k < group->count; ++k)
            {
                material_t         *real, *current, *next;

                real = group->frames[k].mat;
                current =
                    group->frames[(group->index + k) % group->count].mat;
                next =
                    group->frames[(group->index + k + 1) % group->count].mat;

                R_SetMaterialTranslation(real, current, next, 0);

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
                material_t         *mat = group->frames[k].mat;

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
 * If necessary and possible, generate an RGB lightmap texture for the
 * decoration's light sources.
 */
void R_GenerateDecorMap(ded_decor_t *def)
{
    int                 i, count;

    for(i = 0, count = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
    {
        if(!R_IsValidLightDecoration(def->lights + i))
            continue;
        count++;
    }

#if 0
    if(count > 1 || !def->is_texture)
    {
        def->pregen_lightmap = 10 /*dltexname */ ;
    }
#endif
}
