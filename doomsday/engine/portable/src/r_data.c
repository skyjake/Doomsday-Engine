/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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

/*
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

// MACROS ------------------------------------------------------------------

#define FLAT_HASH_SIZE  128
#define FLAT_HASH(x)    (flathash + (((unsigned) x) & (FLAT_HASH_SIZE - 1)))

// TYPES -------------------------------------------------------------------

typedef struct flathash_s {
    flat_t *first;
} flathash_t;

typedef struct {
    boolean     inUse;
    unsigned int numVerts;
    rendpoly_t  poly;
} rendpolydata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

int gamedataformat; // use a game-specifc data format where applicable

extern boolean levelSetup; // we are currently setting up a level

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte    precacheSkins = true;
byte    precacheSprites = false;

lumptexinfo_t *lumptexinfo = 0;
int     numlumptexinfo = 0;
flathash_t flathash[FLAT_HASH_SIZE];
int     firstpatch, lastpatch, numpatches;
int     numtextures;
texture_t **textures;
translation_t *texturetranslation;  // for global animation
int     numgroups;
animgroup_t *groups;

// Glowing textures are always rendered fullbright.
int     glowingTextures = true;

byte rendInfoRPolys = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static unsigned int numrendpolys = 0;
static unsigned int maxrendpolys = 0;
static rendpolydata_t **rendPolys;

// CODE --------------------------------------------------------------------

void R_InfoRendPolys(void)
{
    unsigned int i;

    if(!rendInfoRPolys)
        return;

    Con_Printf("RP Count: %-4i\n", numrendpolys);

    for(i = 0; i < numrendpolys; ++i)
    {
        Con_Printf("RP: %-4i %c %c (vtxs=%i)\n", i,
                   rendPolys[i]->inUse? 'Y':'N',
                   rendPolys[i]->poly.isWall? 'w':'p',
                   rendPolys[i]->numVerts);
    }
}

/**
 * Called at the start of each level.
 */
void R_InitRendPolyPool(void)
{
    int         i;
    rendpoly_t *p;

    numrendpolys = maxrendpolys = 0;
    rendPolys = NULL;

    // Allocate the common ones to get us started.
    p = R_AllocRendPoly(RP_QUAD, true, 4); // wall
    R_FreeRendPoly(p); // mark unused.

    // sprites/models use rendpolys with 1/2 vtxs to unify lighting.
    for(i = 1; i < 16; ++i)
    {
        p = R_AllocRendPoly(i < 3? RP_NONE:RP_FLAT, false, i);
        R_FreeRendPoly(p); // mark unused.
    }
}

/**
 * Re-uses existing rendpolys whenever possible, there are a few conditions
 * which prevent this:
 *
 * There is no unused rendpoly which:
 * a) has enough vertices.
 * b) matches the "isWall" specification.
 *
 * @param numverts  The number of verts required.
 * @param isWall    <code>true</code>= wall data is required.
 *
 * @return          Ptr to a suitable rendpoly.
 */
static rendpoly_t *R_NewRendPoly(unsigned int numverts, boolean isWall)
{
    unsigned int    idx;
    rendpoly_t     *p;
    boolean         found = false;

    for(idx = 0; idx < maxrendpolys; ++idx)
    {
        if(rendPolys[idx]->inUse)
            continue;

        if(rendPolys[idx]->numVerts == numverts &&
           rendPolys[idx]->poly.isWall == isWall)
        {
            // Use this one.
            rendPolys[idx]->inUse = true;
            return &rendPolys[idx]->poly;
        }
        else if(rendPolys[idx]->numVerts == 0)
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
            unsigned int i, newCount;
            rendpolydata_t *newPolyData, *ptr;

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
                ptr->numVerts = 0;
                rendPolys[i] = ptr;
            }
        }
        idx = numrendpolys - 1;
    }

    p = &rendPolys[idx]->poly;
    rendPolys[idx]->inUse = true;
    rendPolys[idx]->numVerts = numverts;

    p->numvertices = numverts;
    p->vertices = Z_Malloc(sizeof(rendpoly_vertex_t) * p->numvertices,
                           PU_LEVEL, 0);
    p->isWall = isWall;

    if(p->isWall) // Its a wall so allocate the wall data.
        p->wall = Z_Malloc(sizeof(rendpoly_wall_t), PU_LEVEL, 0);
    else
        p->wall = NULL;

    return p;
}

/**
 * Retrieves a suitable rendpoly. Possibly allocates a new one if necessary.
 *
 * @param type      The type of the poly to create.
 * @param isWall    <code>true</code>= wall data is required for this poly.
 * @param numverts  The number of verts required.
 *
 * @return          Ptr to a suitable rendpoly.
 */
rendpoly_t *R_AllocRendPoly(rendpolytype_t type, boolean isWall,
                            unsigned int numverts)
{
    rendpoly_t *poly = R_NewRendPoly(numverts, isWall);

    poly->type = type;

    poly->flags = 0;
    poly->texoffx = 0;
    poly->texoffy = 0;
    poly->interpos = 0;
    poly->lights = 0;
    poly->decorlightmap = 0;
    poly->sector = 0;
    poly->blendmode = BM_NORMAL;
    poly->tex.id = curtex = GL_PrepareDDTexture(DDT_UNKNOWN);
    poly->tex.detail = texdetail;
    poly->tex.height = texh;
    poly->tex.width = texw;
    poly->tex.masked = texmask;

    return poly;
}

/**
 * Doesn't actually free anything. Instead, mark it as unused ready for the
 * next time a rendpoly with this number of verts is needed.
 *
 * @param poly      Ptr to the poly to mark unused.
 */
void R_FreeRendPoly(rendpoly_t *poly)
{
    unsigned int i;

    if(!poly)
        return;

    for(i = 0; i < numrendpolys; ++i)
    {
        if(&rendPolys[i]->poly == poly)
        {
            rendPolys[i]->inUse = false;
            return;
        }
    }
#if _DEBUG
    Con_Message("R_FreeRendPoly: Dangling poly ptr!\n");
#endif
}

void R_MemcpyRendPoly(rendpoly_t *dest, rendpoly_t *src)
{
    unsigned int i;

    if(!dest || !src)
        return;

    memcpy(&dest->tex, &src->tex, sizeof(gltexture_t));
    memcpy(&dest->intertex, &src->intertex, sizeof(gltexture_t));
    if(dest->wall && src->wall)
        memcpy(&dest->wall, &src->wall, sizeof(rendpoly_wall_t));
    dest->texoffx = src->texoffx;
    dest->texoffy = src->texoffy;
    dest->flags = src->flags;
    dest->interpos = src->interpos;
    dest->sector = src->sector;
    dest->blendmode = src->interpos;
    dest->lights = src->lights;
    dest->decorlightmap = src->decorlightmap;
    dest->type = src->type;
    for(i = 0; i < dest->numvertices; ++i)
        memcpy(&dest->vertices[i], &src->vertices[i], sizeof(rendpoly_vertex_t));
}

void R_ShutdownData(void)
{
}

/**
 * Returns a NULL-terminated array of pointers to all the flats.
 * The array must be freed with Z_Free.
 */
flat_t **R_CollectFlats(int *count)
{
    int     i, num;
    flat_t *f, **array;

    // First count the number of flats.
    for(num = 0, i = 0; i < FLAT_HASH_SIZE; ++i)
        for(f = flathash[i].first; f; f = f->next)
            num++;

    // Tell this to the caller.
    if(count)
        *count = num;

    // Allocate the array, plus one for the terminator.
    array = Z_Malloc(sizeof(flat_t *) * (num + 1), PU_STATIC, NULL);

    // Collect the pointers.
    for(num = 0, i = 0; i < FLAT_HASH_SIZE; ++i)
        for(f = flathash[i].first; f; f = f->next)
            array[num++] = f;

    // Terminate.
    array[num] = NULL;

    return array;
}

/**
 * Returns a flat_t* for the given lump, if one already exists.
 */
flat_t *R_FindFlat(int lumpnum)
{
    flat_t *i;
    flathash_t *hash = FLAT_HASH(lumpnum);

    for(i = hash->first; i; i = i->next)
        if(i->lump == lumpnum)
        {
            return i;
        }

    return NULL;
}

/**
 * Get a flat_t data structure for a flat specified with a WAD lump
 * number.  Allocates a new flat_t if it hasn't been loaded yet.
 */
flat_t *R_GetFlat(int lumpnum)
{
    flat_t *f = 0;
    flathash_t *hash = 0;

    if(lumpnum >= numlumps)
    {
        Con_Error("R_GetFlat: lumpnum = %i out of bounds (%i).\n", 
                  lumpnum, numlumps);
    }
    
    f = R_FindFlat(lumpnum);
    
    if(!lumpnum)
        return NULL;

    // Check if this lump has already been loaded as a flat.
    if(f)
        return f;

    // Hmm, this is an entirely new flat.
    f = Z_Calloc(sizeof(flat_t), PU_FLAT, NULL);
    hash = FLAT_HASH(lumpnum);

    // Link to the hash.
    f->next = hash->first;
    hash->first = f;

    // Init the new one.
    f->lump = lumpnum;
    f->translation.current = f->translation.next = lumpnum;
    memset(f->color.rgb, 0xff, 3);
    return f;
}

int R_SetFlatTranslation(int flat, int translateTo)
{
    flat_t *f = R_GetFlat(flat);
    int     old = f->translation.current;

    f->translation.current = f->translation.next = translateTo;
    f->translation.inter = 0;
    return old;
}

int R_SetTextureTranslation(int tex, int translateTo)
{
    int     old = texturetranslation[tex].current;

    texturetranslation[tex].current = texturetranslation[tex].next =
        translateTo;
    texturetranslation[tex].inter = 0;
    return old;
}

/**
 * Create a new animation group. Returns the group number.
 * This function is exported and accessible from DLLs.
 */
int R_CreateAnimGroup(int type, int flags)
{
    animgroup_t *group;

    // Allocating one by one is inefficient, but it doesn't really matter.
    groups =
        Z_Realloc(groups, sizeof(animgroup_t) * (numgroups + 1), PU_STATIC);

    // Init the new group.
    group = groups + numgroups;
    memset(group, 0, sizeof(*group));

    // The group number is (index + 1).
    group->id = ++numgroups;
    group->flags = flags;

    if(type == DD_TEXTURE)
        group->flags |= AGF_TEXTURE;
    if(type == DD_FLAT)
        group->flags |= AGF_FLAT;

    return group->id;
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
void R_AddToAnimGroup(int groupNum, const char *name, int tics, int randomTics)
{
    animgroup_t *group;
    animframe_t *frame;
    int         number;

    if(!name || !name[0])
        return;

    group = R_GetAnimGroup(groupNum);
    if(!group)
        Con_Error("R_AddToAnimGroup: Unknown anim group %i.", groupNum);

    if(group->flags & AGF_TEXTURE)
        number = R_CheckTextureNumForName(name);
    else
        number = W_CheckNumForName(name);

    if(number < 0)
    {
        Con_Message("R_AddToAnimGroup: Unknown %s '%s'.",
                    ((group->flags & AGF_TEXTURE)? "Texture" : "Flat"), name);
        return;
    }

    // Allocate a new animframe.
    group->frames =
        Z_Realloc(group->frames, sizeof(animframe_t) * ++group->count,
                  PU_STATIC);

    frame = group->frames + group->count - 1;

    frame->number = number;
    frame->tics = tics;
    frame->random = randomTics;

    // Mark the texture/flat as belonging to some animgroup.
    if(group->flags & AGF_TEXTURE)
    {
        textures[number]->ingroup = true;
    }
    else
    {
        R_GetFlat(number)->ingroup = true;
    }
}

boolean R_IsInAnimGroup(int groupNum, int type, int number)
{
    animgroup_t *group = R_GetAnimGroup(groupNum);
    int     i;

    if(!group)
        return false;

    if((type == DD_TEXTURE && !(group->flags & AGF_TEXTURE)) ||
       (type == DD_FLAT && !(group->flags & AGF_FLAT)))
    {
        // Not the right type.
        return false;
    }

    // Is it in there?
    for(i = 0; i < group->count; ++i)
    {
        if(group->frames[i].number == number)
            return true;
    }
    return false;
}

/**
 * Initialize an entire animation using the data in the definition.
 */
void R_InitAnimGroup(ded_group_t *def)
{
    int     i;
    int     groupNumber = 0;
    int     type, number;

    type = (def->is_texture ? DD_TEXTURE : DD_FLAT);

    for(i = 0; i < def->count.num; ++i)
    {
        if(def->is_texture)
        {
            number = R_CheckTextureNumForName(def->members[i].name);
        }
        else
        {
            number = W_CheckNumForName(def->members[i].name);
        }
        if(number < 0)
            continue;

        // Only create a group when the first texture is found.
        if(!groupNumber)
        {
            // Create a new animation group.
            groupNumber = R_CreateAnimGroup(type, def->flags);
        }

        R_AddToAnimGroup(groupNumber, def->members[i].name,
                         def->members[i].tics, def->members[i].random_tics);
    }
}

/**
 * All animation groups are reseted back to their original state.
 * Called when setting up a map.
 */
void R_ResetAnimGroups(void)
{
    int     i;
    animgroup_t *group;

    for(i = 0, group = groups; i < numgroups; ++i, group++)
    {
        // The Precache groups are not intended for animation.
        if((group->flags & AGF_PRECACHE) || !group->count)
            continue;

        group->timer = 0;
        group->maxtimer = 1;

        // The anim group should start from the first step using the
        // correct timings.
        group->index = group->count - 1;
    }

    // This'll get every group started on the first step.
    R_AnimateAnimGroups();
}

/**
 * Assigns switch texture pairs (SW1/SW2) to their own texture precaching
 * groups. This'll allow them to be precached at the same time.
 */
void R_InitSwitchAnimGroups(void)
{
    int     i, k, group;

    for(i = 0; i < numtextures; ++i)
    {
        // Is this a switch texture?
        if(strnicmp(textures[i]->name, "SW1", 3))
            continue;

        // Find the corresponding SW2.
        for(k = 0; k < numtextures; ++k)
        {
            // Could this be it?
            if(strnicmp(textures[k]->name, "SW2", 3))
                continue;

            if(!strnicmp(textures[k]->name + 3, textures[i]->name + 3, 5))
            {
                // Create a non-animating group for these.
                group = R_CreateAnimGroup(DD_TEXTURE, AGF_PRECACHE);
                R_AddToAnimGroup(group, textures[i]->name, 0, 0);
                R_AddToAnimGroup(group, textures[k]->name, 0, 0);
                break;
            }
        }
    }
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

    // Load the map texture definitions from TEXTURE1/2.
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
    numtextures = numtextures1 + numtextures2;

    // FIXME: Surely not all of these are still needed?
    textures = Z_Malloc(numtextures * 4, PU_REFRESHTEX, 0);

    sprintf(buf, "R_Init: Initializing %i textures...", numtextures);

    for(i = 0; i < numtextures; ++i, directory++)
    {
        //Con_Progress(1, PBARF_DONTSHOW);

        if(i == numtextures1)
        {                       // Start looking in second texture file.
            maptex = maptex2;
            maxoff = maxoff2;
            directory = maptex + 1;
        }

        offset = LONG(*directory);
        if(offset > maxoff)
            Con_Error("R_InitTextures: bad texture directory");

        if(gamedataformat == 0)
        {
            mtexture = (maptexture_t *) ((byte *) maptex + offset);

            texture = textures[i] =
                Z_Calloc(sizeof(texture_t) +
                        sizeof(texpatch_t) * (SHORT(mtexture->patchcount) - 1),
                        PU_REFRESHTEX, 0);
            texture->width = SHORT(mtexture->width);
            texture->height = SHORT(mtexture->height);

            texture->flags = mtexture->masked ? TXF_MASKED : 0;

            texture->patchcount = SHORT(mtexture->patchcount);

            memcpy(texture->name, mtexture->name, 8);

            mpatch = &mtexture->patches[0];
            patch = &texture->patches[0];

            for(j = 0; j < texture->patchcount; ++j, mpatch++, patch++)
            {
                strncpy(name, name_p + SHORT(mpatch->patch) * 8, 8);
                patch->originx = SHORT(mpatch->originx);
                patch->originy = SHORT(mpatch->originy);
                patch->patch = patchlookup[SHORT(mpatch->patch)];
                if(patch->patch == -1)
                {
                    Con_Error("R_InitTextures: Missing patch \"%s\" in texture %s",
                          name, texture->name);
                }
            }

        }
        else if(gamedataformat == 3)
        {   // strife format
            smtexture = (strifemaptexture_t *) ((byte *) maptex + offset);

            texture = textures[i] =
                Z_Calloc(sizeof(texture_t) +
                        sizeof(texpatch_t) * (SHORT(smtexture->patchcount) - 1),
                        PU_REFRESHTEX, 0);
            texture->width = SHORT(smtexture->width);
            texture->height = SHORT(smtexture->height);

            texture->flags = 0;
            texture->patchcount = SHORT(smtexture->patchcount);

            memcpy(texture->name, smtexture->name, 8);

            smpatch = &smtexture->patches[0];
            patch = &texture->patches[0];

            for(j = 0; j < texture->patchcount; ++j, smpatch++, patch++)
            {
                patch->originx = SHORT(smpatch->originx);
                patch->originy = SHORT(smpatch->originy);
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

    //Con_HideProgress();

    // Translation table for global animation.
    texturetranslation =
        Z_Malloc(sizeof(translation_t) * (numtextures + 1), PU_REFRESHTEX, 0);
    for(i = 0; i < numtextures; ++i)
    {
        texturetranslation[i].current = texturetranslation[i].next = i;
        texturetranslation[i].inter = 0;
    }

    M_Free(patchlookup);

    // Assign switch texture pairs (SW1/SW2) to their own texture groups.
    // This'll allow them to be precached at the same time.
    R_InitSwitchAnimGroups();
}

void R_UpdateTextures(void)
{
    Z_FreeTags(PU_REFRESHTEX, PU_REFRESHTEX);
    R_InitTextures();
}

int R_GraphicResourceFlags(resourceclass_t rclass, int picid)
{
    if(picid < 0)
        return 0;
  
    switch(rclass)
    {
    case RC_TEXTURE:  // picid is a texture id
        if(picid >= numtextures)
        {
            Con_Error("R_GraphicResourceFlags: RC_TEXTURE, picid %i out of bounds.\n",
                      picid);                      
        }
        picid = texturetranslation[picid].current;
        if(picid < 0)
            return 0;

        return textures[picid]->flags;

    case RC_FLAT:  // picid is a lumpnum
        {
        flat_t *fl = R_GetFlat(picid);

        if(!fl)
            return 0;

        return fl->flags;
        }
        break;

    default:
        return 0;
    };
}

void R_InitFlats(void)
{
    memset(flathash, 0, sizeof(flathash));
}

void R_UpdateFlats(void)
{
    Z_FreeTags(PU_FLAT, PU_FLAT);
    memset(flathash, 0, sizeof(flathash));
    R_InitFlats();
}

void R_InitLumpTexInfo(void)
{
    if(lumptexinfo)
        Z_Free(lumptexinfo);

    // Allocate one info per lump. Rather a waste...
    numlumptexinfo = numlumps;
    lumptexinfo = Z_Calloc(sizeof(*lumptexinfo) * numlumps, PU_STATIC, 0);
}

/**
 * Locates all the lumps that will be used by all views.
 * Must be called after W_Init.
 */
void R_InitData(void)
{
    R_InitTextures();
    R_InitFlats();
    R_InitLumpTexInfo();
    Cl_InitTranslations();
}

void R_UpdateData(void)
{
    R_UpdateTextures();
    R_UpdateFlats();
    R_InitLumpTexInfo();
    Cl_InitTranslations();
}

void R_InitTranslationTables(void)
{
    int     i;
    byte   *transLump;

    // Allocate translation tables
    translationtables = Z_Malloc(256 * 3 * ( /*MAXPLAYERS*/ 8 - 1) + 255, PU_REFRESHTRANS, 0);

    translationtables = (byte *) (((long) translationtables + 255) & ~255);

    for(i = 0; i < 3 * ( /*MAXPLAYERS*/ 8 - 1); ++i)
    {
        // If this can't be found, it's reasonable to expect that the game dll
        // will initialize the translation tables as it wishes.
        if(W_CheckNumForName("trantbl0") < 0)
            break;

        transLump = W_CacheLumpNum(W_GetNumForName("trantbl0") + i, PU_STATIC);
        memcpy(translationtables + i * 256, transLump, 256);
        Z_Free(transLump);
    }
}

void R_UpdateTranslationTables(void)
{
    Z_FreeTags(PU_REFRESHTRANS, PU_REFRESHTRANS);
    R_InitTranslationTables();
}

int R_FlatNumForName(const char *name)
{
    int     i;
    char    namet[9];

    i = W_CheckNumForName(name);

    if(i == -1 && !levelSetup) // dont announce during level setup
    {
        namet[8] = 0;
        memcpy(namet, name, 8);
        Con_Message("R_FlatNumForName: %.8s not found!\n", namet);
    }
    return i;                   //R_GetFlatIndex(i);//i - firstflat;
}

int R_CheckTextureNumForName(const char *name)
{
    int     i;

    if(name[0] == '-')          // no texture marker
        return 0;

    for(i = 0; i < numtextures; ++i)
        if(!strncasecmp(textures[i]->name, name, 8))
            return i;

    return -1;
}

int R_TextureNumForName(const char *name)
{
    int     i;

    i = R_CheckTextureNumForName(name);

    if(i == -1 && !levelSetup)  // dont announce during level setup
        Con_Message("R_TextureNumForName: %.8s not found!\n", name);
    return i;
}

const char *R_TextureNameForNum(int num)
{
    if(num < 0 || num > numtextures - 1)
        return NULL;
    return textures[num]->name;
}

/**
 * Returns true if the texture is probably not from the original game.
 */
boolean R_IsCustomTexture(int texture)
{
    int     i, lump;

    // First check the texture definitions.
    lump = W_CheckNumForName("TEXTURE1");
    if(lump >= 0 && !W_IsFromIWAD(lump))
        return true;

    lump = W_CheckNumForName("TEXTURE2");
    if(lump >= 0 && !W_IsFromIWAD(lump))
        return true;

    // Go through the patches.
    for(i = 0; i < textures[texture]->patchcount; ++i)
    {
        if(!W_IsFromIWAD(textures[texture]->patches[i].patch))
            return true;
    }

    // This is most likely from the original game data.
    return false;
}

/**
 * Returns true if the given light decoration definition is valid.
 */
boolean R_IsValidLightDecoration(ded_decorlight_t *lightDef)
{
    return (lightDef->color[0] != 0 || lightDef->color[1] != 0 ||
        lightDef->color[2] != 0);
}

/**
 * Returns true if the given decoration works under the specified
 * circumstances.
 */
boolean R_IsAllowedDecoration(ded_decor_t *def, int index, boolean hasExternal)
{
    if(hasExternal)
    {
        return (def->flags & DCRF_EXTERNAL) != 0;
    }

    if(def->is_texture)
    {
        // Is it probably an original texture?
        if(!R_IsCustomTexture(index))
            return !(def->flags & DCRF_NO_IWAD);
    }
    else
    {
        if(W_IsFromIWAD(index))
            return !(def->flags & DCRF_NO_IWAD);
    }

    return (def->flags & DCRF_PWAD) != 0;
}

/**
 * Prepares the specified flat and all the other flats in the same
 * animation group. Has the consequence that all lumps inside the
 * F_START...F_END block obtain a flat_t record.
 */
void R_PrecacheFlat(int num)
{
    int     i, k;
    flat_t *f = R_GetFlat(num);

    if(f->ingroup)
    {
        // The flat belongs in one or more animgroups.
        for(i = 0; i < numgroups; ++i)
        {
            if(R_IsInAnimGroup(groups[i].id, DD_FLAT, num))
            {
                // Precache this group.
                for(k = 0; k < groups[i].count; ++k)
                    GL_PrepareFlat(groups[i].frames[k].number);
            }
        }
    }
    else
    {
        GL_PrepareFlat(num);
    }
}

/**
 * Prepares the specified texture and all the other textures in the
 * same animation group.
 */
void R_PrecacheTexture(int num)
{
    int     i, k;

    if(textures[num]->ingroup)
    {
        // The texture belongs in one or more animgroups.
        for(i = 0; i < numgroups; ++i)
        {
            if(R_IsInAnimGroup(groups[i].id, DD_TEXTURE, num))
            {
                // Precache this group.
                for(k = 0; k < groups[i].count; ++k)
                    GL_PrepareTexture(groups[i].frames[k].number);
            }
        }
    }
    else
    {
        // Just this one texture.
        GL_PrepareTexture(num);
    }
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
    char   *texturepresent;
    char   *spritepresent = NULL;
    uint    i, j;
    int     k, lump, mocount;
    thinker_t *th;
    sector_t *sec;
    side_t *side;
    float   starttime;

    // Don't precache when playing demo.
    if(isDedicated || playback)
    {
        return;
    }

    // Precaching from 100 to 200.
    Con_SetProgress(100);

    starttime = Sys_GetSeconds();

    // Precache textures.
    texturepresent = M_Malloc(numtextures);
    memset(texturepresent, 0, numtextures);

    for(i = 0; i < numsides; ++i)
    {
        side = SIDE_PTR(i);

        if(side->SW_toppic != -1)
        {
            if(side->SW_topisflat)
                R_PrecacheFlat(side->SW_toppic);
            else
                texturepresent[side->SW_toppic] = 1;
        }

        if(side->SW_middlepic != -1)
        {
            if(side->SW_middleisflat)
                R_PrecacheFlat(side->SW_middlepic);
            else
                texturepresent[side->SW_middlepic] = 1;
        }

        if(side->SW_bottompic != -1)
        {
            if(side->SW_bottomisflat)
                R_PrecacheFlat(side->SW_bottompic);
            else
                texturepresent[side->SW_bottompic] = 1;
        }
    }

    // Precache flats.
    for(i = 0; i < numsectors; ++i)
    {
        sec = SECTOR_PTR(i);

        for(j = 0; j < sec->planecount; ++j)
        {
            if(sec->planes[j]->surface.isflat)
                R_PrecacheFlat(sec->planes[j]->surface.texture);
            else
                texturepresent[sec->planes[j]->surface.texture] = 1;
        }
        
        /*if(i % SAFEDIV(numsectors, 10) == 0)
            Con_Progress(1, PBARF_DONTSHOW);*/
    }
    
    // Update progress.

    // FIXME: Precache sky textures!

    for(k = 0; k < numtextures; ++k)
        if(texturepresent[k])
        {
            R_PrecacheTexture(k);
            /*if(k % SAFEDIV(numtextures, 10) == 0)
                Con_Progress(1, PBARF_DONTSHOW);*/
        }
            
    // Update progress.

    // Precache sprites.
    if(precacheSprites)
    {
        spritepresent = M_Malloc(numSprites);
        memset(spritepresent, 0, numSprites);
    }

    if(precacheSprites || (useModels && precacheSkins))
        for(th = thinkercap.next, mocount = 0; th != &thinkercap;
            th = th->next)
        {
            if(th->function != gx.MobjThinker)
                continue;

            if(precacheSprites)
                spritepresent[((mobj_t *) th)->sprite] = 1;

            mocount++;
        }


    // Precache skins?
    if(useModels && precacheSkins)
    {
        for(k = 0, th = thinkercap.next; th != &thinkercap; th = th->next)
        {
            if(th->function != gx.MobjThinker)
                continue;
            // Advance progress bar.
            /*if(++k % SAFEDIV(mocount, 10) == 0)
                Con_Progress(2, PBARF_DONTSHOW);*/
            // Precache all the skins for the mobj.
            R_PrecacheSkinsForMobj((mobj_t *) th);
        }
    }

    // Update progress.
            
    // Sky models usually have big skins.
    R_PrecacheSky();

    if(precacheSprites)
    {
        int     s, f, l;
        spriteframe_t *sf;

        for(s = 0; s < numSprites; ++s)
        {
            /*
            if(s % SAFEDIV(numSprites, 10) == 0)
                Con_Progress(1, PBARF_DONTSHOW);*/

            if(!spritepresent[s] || !useModels)
                continue;

            for(f = 0; f < sprites[s].numframes; ++f)
            {
                sf = &sprites[s].spriteframes[f];

                for(l = 0; l < 8; ++l)
                {
                    lump = spritelumps[sf->lump[l]]->lump;
                    GL_BindTexture(GL_PrepareSprite(sf->lump[l], 0));
                }
            }
        }

        M_Free(spritepresent);
    }

    M_Free(texturepresent);

    if(verbose)
    {
        Con_Message("Precaching took %.2f seconds.\n",
                    Sys_GetSeconds() - starttime);
    }

    // Done!
    //Con_Progress(100, PBARF_SET);
}

translation_t *R_GetTranslation(boolean isTexture, int number)
{
    if(isTexture)
    {
        return texturetranslation + number;
    }
    else
    {
        return &R_GetFlat(number)->translation;
    }
}

void R_AnimateAnimGroups(void)
{
    animgroup_t *group;
    translation_t *xlat;
    int     i, timer, k;
    boolean isTexture;

    // The animation will only progress when the game is not paused.
    if(clientPaused)
        return;

    for(i = 0, group = groups; i < numgroups; ++i, group++)
    {
        // The Precache groups are not intended for animation.
        if((group->flags & AGF_PRECACHE) || !group->count)
            continue;

        isTexture = (group->flags & AGF_TEXTURE) != 0;

        if(--group->timer <= 0)
        {
            // Advance to next frame.
            group->index = (group->index + 1) % group->count;
            timer = group->frames[group->index].tics;

            if(group->frames[group->index].random)
            {
                timer += M_Random() % (group->frames[group->index].random + 1);
            }
            group->timer = group->maxtimer = timer;

            // Update texture/flat translations.
            for(k = 0; k < group->count; ++k)
            {
                int     real, current, next;

                real = group->frames[k].number;
                current =
                    group->frames[(group->index + k) % group->count].number;
                next =
                    group->frames[(group->index + k + 1) %
                                  group->count].number;
/*
#ifdef _DEBUG
if(isTexture)
    Con_Printf("real=%i cur=%i next=%i\n", real, current, next);
#endif
*/
                xlat = R_GetTranslation(isTexture, real);
                xlat->current = current;
                xlat->next = next;
                xlat->inter = 0;

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
                xlat = R_GetTranslation(isTexture, group->frames[k].number);

                if(group->flags & AGF_SMOOTH)
                {
                    xlat->inter = 1 - group->timer / (float) group->maxtimer;
                }
                else
                {
                    xlat->inter = 0;
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
    int     i, count;

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
