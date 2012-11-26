/**\file r_data.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Data Structures and Constants for Refresh
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_network.h"
#include "de_graphics.h"
#include "de_resource.h"
#include "de_misc.h"
#include "de_audio.h" // For texture, environmental audio properties.

#include "render/r_things.h"
#include "render/r_sky.h"

// MACROS ------------------------------------------------------------------

#define RAWTEX_HASH_SIZE    128
#define RAWTEX_HASH(x)      (rawtexhash + (((unsigned) x) & (RAWTEX_HASH_SIZE - 1)))

// TYPES -------------------------------------------------------------------

typedef char patchname_t[9];

typedef struct rawtexhash_s {
    rawtex_t*     first;
} rawtexhash_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean mapSetup; // We are currently setting up a map.

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte precacheSkins = true;
byte precacheMapMaterials = true;
byte precacheSprites = true;

int gameDataFormat; // Use a game-specifc data format where applicable.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static rawtexhash_t rawtexhash[RAWTEX_HASH_SIZE];

// CODE --------------------------------------------------------------------

/// @note Part of the Doomsday public API.
int R_TextureUniqueId2(const Uri* uri, boolean quiet)
{
    textureid_t texId = Textures_ResolveUri2(uri, quiet);
    if(texId != NOTEXTUREID)
    {
        return Textures_UniqueId(texId);
    }
    if(!quiet)
    {
        AutoStr* path = Uri_ToString(uri);
        Con_Message("Warning: Unknown Texture \"%s\"\n", Str_Text(path));
    }
    return -1;
}

/// @note Part of the Doomsday public API.
int R_TextureUniqueId(const Uri* uri)
{
    return R_TextureUniqueId2(uri, false);
}

void R_InitSystemTextures(void)
{
    static const struct texdef_s {
        const char* texPath;
        const char* resourcePath; ///< Percent-encoded.
    } defs[] = {
        { "unknown", "Graphics:unknown" },
        { "missing", "Graphics:missing" },
        { "bbox",    "Graphics:bbox" },
        { "gray",    "Graphics:gray" },
        { NULL, NULL }
    };
    Uri* uri = Uri_New(), *resourcePath = Uri_New();
    textureid_t texId;
    Texture* tex;
    uint i;

    VERBOSE( Con_Message("Initializing System textures...\n") )

    Uri_SetScheme(uri, TS_SYSTEM_NAME);
    for(i = 0; defs[i].texPath; ++i)
    {
        Uri_SetPath(uri, defs[i].texPath);
        Uri_SetUri(resourcePath, defs[i].resourcePath);

        texId = Textures_Declare(uri, i+1/*1-based index*/, resourcePath);
        if(texId == NOTEXTUREID) continue; // Invalid uri?

        // Have we defined this yet?
        tex = Textures_ToTexture(texId);
        if(!tex && !Textures_Create(texId, true/*is-custom*/, NULL))
        {
            AutoStr* path = Uri_ToString(uri);
            Con_Message("Warning: Failed defining Texture for System texture \"%s\"\n", Str_Text(path));
        }
    }
    Uri_Delete(resourcePath);
    Uri_Delete(uri);
}

static textureid_t findPatchTextureIdByName(const char* encodedName)
{
    textureid_t texId;
    Uri* uri;
    assert(encodedName && encodedName[0]);

    uri = Uri_NewWithPath2(encodedName, RC_NULL);
    Uri_SetScheme(uri, TS_PATCHES_NAME);
    texId = Textures_ResolveUri2(uri, true/*quiet please*/);
    Uri_Delete(uri);
    return texId;
}

/// @note Part of the Doomsday public API.
patchid_t R_DeclarePatch(const char* name)
{
    const doompatch_header_t* patch;
    struct file1_s* file;
    Uri* uri, *resourcePath;
    int lumpIdx;
    ddstring_t encodedName;
    lumpnum_t lumpNum;
    textureid_t texId;
    Texture* tex;
    int uniqueId;
    patchtex_t* p;

    if(!name || !name[0])
    {
#if _DEBUG
        Con_Message("Warning:R_DeclarePatch: Invalid 'name' argument, ignoring.\n");
#endif
        return 0;
    }

    Str_Init(&encodedName);
    Str_PercentEncode(Str_Set(&encodedName, name));

    // Already defined as a patch?
    texId = findPatchTextureIdByName(Str_Text(&encodedName));
    if(texId)
    {
        Str_Free(&encodedName);
        /// \todo We should instead define Materials from patches and return the material id.
        return (patchid_t)Textures_UniqueId(texId);
    }

    lumpNum = F_LumpNumForName(name);
    if(lumpNum < 0)
    {
#if _DEBUG
        Con_Message("Warning:R_DeclarePatch: Failed to locate lump for patch '%s'.\n", name);
#endif
        return 0;
    }

    // Compose the resource name
    uri = Uri_NewWithPath2(TS_PATCHES_NAME":", RC_NULL);
    Uri_SetPath(uri, Str_Text(&encodedName));
    Str_Free(&encodedName);

    // Compose the path to the data resource.
    resourcePath = Uri_NewWithPath2("Lumps:", RC_NULL);
    Uri_SetPath(resourcePath, Str_Text(F_LumpName(lumpNum)));

    uniqueId = Textures_Count(TS_PATCHES)+1; // 1-based index.
    texId = Textures_Declare(uri, uniqueId, resourcePath);
    Uri_Delete(resourcePath);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return 0; // Invalid uri?

    // Generate a new patch.
    p = (patchtex_t*)malloc(sizeof *p);
    if(!p)
        Con_Error("R_DeclarePatch: Failed on allocation of %lu bytes for new PatchTex.", (unsigned long) sizeof *p);
    // Take a copy of the current patch loading state so that future texture
    // loads will produce the same results.
    p->flags = 0;
    if(monochrome)               p->flags |= PF_MONOCHROME;
    if(upscaleAndSharpenPatches) p->flags |= PF_UPSCALE_AND_SHARPEN;

    /**
     * @todo: Cannot be sure this is in Patch format until a load attempt
     * is made. We should not read this info here!
     */
    file = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
    patch = (const doompatch_header_t*) F_CacheLump(file, lumpIdx);
    p->offX = -SHORT(patch->leftOffset);
    p->offY = -SHORT(patch->topOffset);

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        Size2Raw size;
        size.width  = SHORT(patch->width);
        size.height = SHORT(patch->height);
        tex = Textures_CreateWithSize(texId, F_LumpIsCustom(lumpNum), &size, (void*)p);
        F_UnlockLump(file, lumpIdx);

        if(!tex)
        {
            Con_Message("Warning: Failed defining Texture for Patch texture \"%s\"\n", name);
            free(p);
            return 0;
        }
    }
    else
    {
        patchtex_t* oldPatch = Texture_UserDataPointer(tex);
        Size2Raw size;

        size.width  = SHORT(patch->width);
        size.height = SHORT(patch->height);

        Texture_FlagCustom(tex, F_LumpIsCustom(lumpNum));
        Texture_SetSize(tex, &size);
        Texture_SetUserDataPointer(tex, (void*)p);

        free(oldPatch);

        F_UnlockLump(file, lumpIdx);
    }

    return uniqueId;
}

boolean R_GetPatchInfo(patchid_t id, patchinfo_t* info)
{
    Texture* tex;
    if(!info)
        Con_Error("R_GetPatchInfo: Argument 'info' cannot be NULL.");

    memset(info, 0, sizeof *info);
    tex = Textures_ToTexture(Textures_TextureForUniqueId(TS_PATCHES, id));
    if(tex)
    {
        const patchtex_t* pTex = (patchtex_t*)Texture_UserDataPointer(tex);
        assert(tex);

        // Ensure we have up to date information about this patch.
        GL_PreparePatchTexture(tex);

        info->id = id;
        info->flags.isCustom = Texture_IsCustom(tex);

        { averagealpha_analysis_t* aa = (averagealpha_analysis_t*)Texture_AnalysisDataPointer(tex, TA_ALPHA);
        info->flags.isEmpty = aa && FEQUAL(aa->alpha, 0);
        }

        info->geometry.size.width = Texture_Width(tex);
        info->geometry.size.height = Texture_Height(tex);
        info->geometry.origin.x = pTex->offX;
        info->geometry.origin.y = pTex->offY;
        /// @kludge:
        info->extraOffset[0] = info->extraOffset[1] = (pTex->flags & PF_UPSCALE_AND_SHARPEN)? -1 : 0;
        // Kludge end.
        return true;
    }
    if(id != 0)
    {
#if _DEBUG
        Con_Message("Warning:R_GetPatchInfo: Invalid Patch id #%i.\n", id);
#endif
    }
    return false;
}

/// @note Part of the Doomsday public API.
Uri* R_ComposePatchUri(patchid_t id)
{
    return Textures_ComposeUri(Textures_TextureForUniqueId(TS_PATCHES, id));
}

/// @note Part of the Doomsday public API.
AutoStr* R_ComposePatchPath(patchid_t id)
{
    textureid_t texId = Textures_TextureForUniqueId(TS_PATCHES, id);
    if(texId == NOTEXTUREID) return AutoStr_NewStd();
    return Textures_ComposePath(texId);
}

rawtex_t** R_CollectRawTexs(int* count)
{
    rawtex_t* r, **list;
    int i, num;

    // First count the number of patchtexs.
    for(num = 0, i = 0; i < RAWTEX_HASH_SIZE; ++i)
    {
        for(r = rawtexhash[i].first; r; r = r->next)
            num++;
    }

    // Tell this to the caller.
    if(count) *count = num;

    // Allocate the array, plus one for the terminator.
    list = Z_Malloc(sizeof(r) * (num + 1), PU_APPSTATIC, NULL);

    // Collect the pointers.
    for(num = 0, i = 0; i < RAWTEX_HASH_SIZE; ++i)
    {
        for(r = rawtexhash[i].first; r; r = r->next)
            list[num++] = r;
    }

    // Terminate.
    list[num] = NULL;

    return list;
}

rawtex_t* R_FindRawTex(lumpnum_t lumpNum)
{
    rawtex_t* i;

    if(-1 == lumpNum || lumpNum >= F_LumpCount())
    {
#if _DEBUG
        Con_Message("Warning:R_FindRawTex: LumpNum #%i out of bounds (%i), returning NULL.\n",
                lumpNum, F_LumpCount());
#endif
        return NULL;
    }

    for(i = RAWTEX_HASH(lumpNum)->first; i; i = i->next)
    {
        if(i->lumpNum == lumpNum)
            return i;
    }
    return 0;
}

rawtex_t* R_GetRawTex(lumpnum_t lumpNum)
{
    rawtexhash_t* hash = 0;
    rawtex_t* r;

    if(-1 == lumpNum || lumpNum >= F_LumpCount())
    {
#if _DEBUG
        Con_Message("Warning:R_GetRawTex: LumpNum #%i out of bounds (%i), returning NULL.\n",
                lumpNum, F_LumpCount());
#endif
        return NULL;
    }

    // Check if this lumpNum has already been loaded as a rawtex.
    r = R_FindRawTex(lumpNum);
    if(r) return r;

    // Hmm, this is an entirely new rawtex.
    r = Z_Calloc(sizeof(*r), PU_REFRESHRAW, 0);
    Str_Copy(Str_Init(&r->name), F_LumpName(lumpNum));
    r->lumpNum = lumpNum;

    // Link to the hash.
    hash = RAWTEX_HASH(lumpNum);
    r->next = hash->first;
    hash->first = r;

    return r;
}

void R_InitRawTexs(void)
{
    memset(rawtexhash, 0, sizeof(rawtexhash));
}

void R_UpdateRawTexs(void)
{
    int i;
    rawtex_t* rawTex;
    for(i = 0; i < RAWTEX_HASH_SIZE; ++i)
    for(rawTex = rawtexhash[i].first; NULL != rawTex; rawTex = rawTex->next)
    {
        Str_Free(&rawTex->name);
    }

    Z_FreeTags(PU_REFRESHRAW, PU_REFRESHRAW);
    R_InitRawTexs();
}

static patchname_t* loadPatchNames(lumpnum_t lumpNum, int* num)
{
    int lumpIdx;
    struct file1_s* file = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
    size_t lumpSize = F_LumpLength(lumpNum);
    patchname_t* names, *name;
    const uint8_t* lump;
    int i, numNames;

    if(lumpSize < 4)
    {
        AutoStr* path = F_ComposeLumpPath(file, lumpIdx);
        Con_Message("Warning:loadPatchNames: \"%s\"(#%i) is not valid PNAMES data.\n",
                    F_PrettyPath(Str_Text(path)), lumpNum);

        if(num) *num = 0;
        return NULL;
    }

    lump = F_CacheLump(file, lumpIdx);
    numNames = LONG(*((const int*) lump));
    if(numNames <= 0)
    {
        F_UnlockLump(file, lumpIdx);

        if(num) *num = 0;
        return NULL;
    }

    if((unsigned)numNames > (lumpSize - 4) / 8)
    {
        // Lump appears to be truncated.
        AutoStr* path = F_ComposeLumpPath(file, lumpIdx);
        Con_Message("Warning:loadPatchNames: Patch '%s'(#%i) is truncated (%lu bytes, expected %lu).\n",
                    F_PrettyPath(Str_Text(path)), lumpNum, (unsigned long) lumpSize,
                    (unsigned long) (numNames * 8 + 4));

        numNames = (int) ((lumpSize - 4) / 8);
    }

    names = (patchname_t*) calloc(1, sizeof(*names) * numNames);
    if(!names)
        Con_Error("loadPatchNames: Failed on allocation of %lu bytes for patch name list.", (unsigned long) sizeof(*names) * numNames);

    name = names;
    for(i = 0; i < numNames; ++i)
    {
        /// @todo Some filtering of invalid characters wouldn't go amiss...
        strncpy(*name, (const char*) (lump + 4 + i * 8), 8);
        name++;
    }

    F_UnlockLump(file, lumpIdx);

    if(num) *num = numNames;
    return names;
}

/**
 * Read DOOM and Strife format texture definitions from the specified lump.
 */
static patchcompositetex_t** readDoomTextureDefLump(lumpnum_t lumpNum,
    patchname_t* patchNames, int numPatchNames, int* origIndexBase, boolean firstNull, int* numDefs)
{
#pragma pack(1)
typedef struct {
    int16_t         originX;
    int16_t         originY;
    int16_t         patch;
    int16_t         stepDir;
    int16_t         colorMap;
} mappatch_t;

typedef struct {
    byte            name[8];
    int16_t         unused;
    byte            scale[2]; // [x, y] Used by ZDoom, div 8.
    int16_t         width;
    int16_t         height;
    //void          **columnDirectory;  // OBSOLETE
    int32_t         columnDirectoryPadding;
    int16_t         patchCount;
    mappatch_t      patches[1];
} maptexture_t;

// strifeformat texture definition variants
typedef struct {
    int16_t         originX;
    int16_t         originY;
    int16_t         patch;
} strifemappatch_t;

typedef struct {
    byte            name[8];
    int16_t         unused;
    byte            scale[2]; // [x, y] Used by ZDoom, div 8.
    int16_t         width;
    int16_t         height;
    int16_t         patchCount;
    strifemappatch_t patches[1];
} strifemaptexture_t;
#pragma pack()

typedef struct {
    lumpnum_t lumpNum;
    struct {
        char processed:1; /// @c true if this patch has already been searched for.
    } flags;
} patchinfo_t;

    patchcompositetex_t** texDefs = NULL;
    size_t lumpSize, offset, n, numValidPatchRefs;
    int numTexDefs, numValidTexDefs;
    struct file1_s* file;
    int* directory, *maptex1;
    short* texDefNumPatches;
    patchinfo_t* patchInfo;
    byte* validTexDefs;
    int i, lumpIdx;
    assert(patchNames && origIndexBase);

    patchInfo = (patchinfo_t*)calloc(1, sizeof(*patchInfo) * numPatchNames);
    if(!patchInfo)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for patch info.", (unsigned long) sizeof *patchInfo * numPatchNames);

    lumpSize = F_LumpLength(lumpNum);
    maptex1 = (int*) malloc(lumpSize);
    if(!maptex1)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for temporary copy of archived DOOM texture definitions.", (unsigned long) lumpSize);

    file = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
    F_ReadLumpSection(file, lumpIdx, (uint8_t*)maptex1, 0, lumpSize);

    numTexDefs = LONG(*maptex1);

    VERBOSE(
        AutoStr* path = F_ComposeLumpPath(file, lumpIdx);
        Con_Message("  Processing \"%s\"...\n", F_PrettyPath(Str_Text(path)));
    )

    validTexDefs = (byte*) calloc(1, sizeof(*validTexDefs) * numTexDefs);
    if(!validTexDefs)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for valid texture record list.", (unsigned long) sizeof(*validTexDefs) * numTexDefs);

    texDefNumPatches = (short*) calloc(1, sizeof(*texDefNumPatches) * numTexDefs);
    if(!texDefNumPatches)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for texture patch count record list.", (unsigned long) sizeof(*texDefNumPatches) * numTexDefs);

    /**
     * Pass #1
     * Count total number of texture and patch defs we'll need and check
     * for missing patches and any other irregularities.
     */
    numValidTexDefs = 0;
    numValidPatchRefs = 0;
    directory = maptex1 + 1;
    for(i = 0; i < numTexDefs; ++i, directory++)
    {
        offset = LONG(*directory);
        if(offset > lumpSize)
        {
            AutoStr* path = F_ComposeLumpPath(file, lumpIdx);
            Con_Message("Warning: Invalid offset %lu for definition %i in \"%s\", ignoring.\n", (unsigned long) offset, i, F_PrettyPath(Str_Text(path)));
            continue;
        }

        if(gameDataFormat == 0)
        {   // DOOM format.
            maptexture_t* mtexture = (maptexture_t*) ((byte*) maptex1 + offset);
            short j, n, patchCount = SHORT(mtexture->patchCount);

            n = 0;
            if(patchCount > 0)
            {
                mappatch_t* mpatch = &mtexture->patches[0];

                for(j = 0; j < patchCount; ++j, mpatch++)
                {
                    short patchNum = SHORT(mpatch->patch);
                    patchinfo_t* pinfo;

                    if(patchNum < 0 || patchNum >= numPatchNames)
                    {
                        Con_Message("Warning: Invalid Patch %i in texture definition \"%s\", ignoring.\n", (int) patchNum, mtexture->name);
                        continue;
                    }
                    pinfo = patchInfo + patchNum;

                    if(!pinfo->flags.processed)
                    {
                        pinfo->lumpNum = F_LumpNumForName(*(patchNames + patchNum));
                        pinfo->flags.processed = true;
                        if(-1 == pinfo->lumpNum)
                        {
                            Con_Message("Warning: Failed to locate Patch \"%s\", ignoring.\n", *(patchNames + patchNum));
                        }
                    }

                    if(-1 == pinfo->lumpNum)
                    {
                        Con_Message("Warning: Missing Patch %i in texture definition \"%s\", ignoring.\n", (int) j, mtexture->name);
                        continue;
                    }
                    ++n;
                }
            }
            else
            {
                Con_Message("Warning: Invalid patch count %i in texture definition \"%s\", ignoring.\n", (int) patchCount, mtexture->name);
            }

            texDefNumPatches[i] = n;
            numValidPatchRefs += n;
        }
        else if(gameDataFormat == 3)
        {   // Strife format.
            strifemaptexture_t* smtexture = (strifemaptexture_t *) ((byte *) maptex1 + offset);
            short j, n, patchCount = SHORT(smtexture->patchCount);

            n = 0;
            if(patchCount > 0)
            {
                strifemappatch_t* smpatch = &smtexture->patches[0];
                for(j = 0; j < patchCount; ++j, smpatch++)
                {
                    short patchNum = SHORT(smpatch->patch);
                    patchinfo_t* pinfo;

                    if(patchNum < 0 || patchNum >= numPatchNames)
                    {
                        Con_Message("Warning: Invalid Patch #%i in texture definition \"%s\", ignoring.\n", (int) patchNum, smtexture->name);
                        continue;
                    }
                    pinfo = patchInfo + patchNum;

                    if(!pinfo->flags.processed)
                    {
                        pinfo->lumpNum = F_LumpNumForName(*(patchNames + patchNum));
                        pinfo->flags.processed = true;
                        if(-1 == pinfo->lumpNum)
                        {
                            Con_Message("Warning: Failed locating Patch \"%s\", ignoring.\n", *(patchNames + patchNum));
                        }
                    }

                    if(-1 == pinfo->lumpNum)
                    {
                        Con_Message("Warning: Missing patch #%i in texture definition \"%s\", ignoring.\n", (int) j, smtexture->name);
                        continue;
                    }
                    ++n;
                }
            }
            else
            {
                Con_Message("Warning: Invalid patch count %i in texture definition \"%s\", ignoring.\n", (int) patchCount, smtexture->name);
            }

            texDefNumPatches[i] = n;
            numValidPatchRefs += n;
        }

        if(texDefNumPatches[i] > 0)
        {
            // This is a valid texture definition.
            validTexDefs[i] = true;
            numValidTexDefs++;
        }
    }

    if(numValidTexDefs > 0 && numValidPatchRefs > 0)
    {
        /**
         * Pass #2
         * There is at least one valid texture def in this lump so convert
         * to the internal format.
         */

        // Build the texturedef index.
        texDefs = (patchcompositetex_t**)malloc(sizeof(*texDefs) * numValidTexDefs);
        directory = maptex1 + 1;
        n = 0;
        for(i = 0; i < numTexDefs; ++i, directory++)
        {
            patchcompositetex_t* texDef;
            short j;

            if(!validTexDefs[i]) continue;

            offset = LONG(*directory);

            // Read and create the texture def.
            if(gameDataFormat == 0)
            {   // DOOM format.
                texpatch_t* patch;
                mappatch_t* mpatch;
                maptexture_t* mtexture = (maptexture_t*) ((byte*) maptex1 + offset);

                texDef = (patchcompositetex_t*) malloc(sizeof(*texDef));
                if(!texDef)
                    Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for new PatchComposite.", (unsigned long) sizeof *texDef);

                texDef->patchCount = texDefNumPatches[i];
                texDef->flags = 0;
                texDef->origIndex = (*origIndexBase) + i;

                Str_Init(&texDef->name);
                Str_PercentEncode(Str_StripRight(Str_PartAppend(&texDef->name, (const char*) mtexture->name, 0, 8)));

                texDef->size.width = SHORT(mtexture->width);
                texDef->size.height = SHORT(mtexture->height);

                texDef->patches = (texpatch_t*)malloc(sizeof(*texDef->patches) * texDef->patchCount);
                if(!texDef->patches)
                    Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for new TexPatch list for texture definition '%s'.", (unsigned long) sizeof *texDef->patches * texDef->patchCount, Str_Text(&texDef->name));

                mpatch = &mtexture->patches[0];
                patch = texDef->patches;
                for(j = 0; j < SHORT(mtexture->patchCount); ++j, mpatch++)
                {
                    short patchNum = SHORT(mpatch->patch);

                    if(patchNum < 0 || patchNum >= numPatchNames ||
                       patchInfo[patchNum].lumpNum == -1)
                        continue;

                    patch->offX = SHORT(mpatch->originX);
                    patch->offY = SHORT(mpatch->originY);
                    patch->lumpNum = patchInfo[patchNum].lumpNum;
                    patch++;
                }
            }
            else if(gameDataFormat == 3)
            {   // Strife format.
                texpatch_t* patch;
                strifemappatch_t* smpatch;
                strifemaptexture_t* smtexture = (strifemaptexture_t*) ((byte*) maptex1 + offset);

                texDef = (patchcompositetex_t*) malloc(sizeof(*texDef));
                if(!texDef)
                    Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for new PatchComposite.", (unsigned long) sizeof *texDef);

                texDef->patchCount = texDefNumPatches[i];
                texDef->flags = 0;
                texDef->origIndex = (*origIndexBase) + i;

                Str_Init(&texDef->name);
                Str_PercentEncode(Str_StripRight(Str_PartAppend(&texDef->name, (const char*) smtexture->name, 0, 8)));

                texDef->size.width = SHORT(smtexture->width);
                texDef->size.height = SHORT(smtexture->height);

                texDef->patches = (texpatch_t*)malloc(sizeof(*texDef->patches) * texDef->patchCount);
                if(!texDef->patches)
                    Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for new TexPatch list for texture definition '%s'.", (unsigned long) sizeof *texDef->patches * texDef->patchCount, Str_Text(&texDef->name));

                smpatch = &smtexture->patches[0];
                patch = texDef->patches;
                for(j = 0; j < SHORT(smtexture->patchCount); ++j, smpatch++)
                {
                    short patchNum = SHORT(smpatch->patch);

                    if(patchNum < 0 || patchNum >= numPatchNames ||
                       patchInfo[patchNum].lumpNum == -1)
                        continue;

                    patch->offX = SHORT(smpatch->originX);
                    patch->offY = SHORT(smpatch->originY);
                    patch->lumpNum = patchInfo[patchNum].lumpNum;
                    patch++;
                }
            }
            else
            {
                Con_Error("R_ReadTextureDefs: Invalid gameDataFormat=%i.", gameDataFormat);
                exit(1); // Unreachable.
            }

            /**
             * Vanilla DOOM's implementation of the texture collection has a flaw which
             * results in the first texture being used dually as a "NULL" texture.
             */
            if(firstNull && i == 0)
                texDef->flags |= TXDF_NODRAW;

            /// Is this a custom texture?
            j = 0;
            while(j < texDef->patchCount && !(texDef->flags & TXDF_CUSTOM))
            {
                if(F_LumpIsCustom(texDef->patches[j].lumpNum))
                    texDef->flags |= TXDF_CUSTOM;
                else
                    j++;
            }

            // Add it to the list.
            texDefs[n++] = texDef;
        }
    }

    *origIndexBase += numTexDefs;

    VERBOSE2( Con_Message("  Loaded %i of %i definitions.\n", numValidTexDefs, numTexDefs) )

    // Free all temporary storage.
    free(validTexDefs);
    free(texDefNumPatches);
    free(maptex1);
    free(patchInfo);

    if(numDefs) *numDefs = numValidTexDefs;

    return texDefs;
}

static patchcompositetex_t** loadPatchCompositeDefs(int* numDefs)
{
    int patchCompositeTexturesCount = 0;
    patchcompositetex_t** patchCompositeTextures = NULL;

    patchcompositetex_t** list = 0, **listCustom = 0, ***eList = 0;
    lumpnum_t pnames, firstTexLump, secondTexLump;
    int count = 0, countCustom = 0, *eCount = 0;
    int i, numLumps, numPatchNames, origIndexBase;
    patchcompositetex_t** newTexDefs;
    boolean firstNull, isCustom;
    patchname_t* patchNames;
    int newNumTexDefs;
    int defLumpsSize;
    lumpnum_t* defLumps;

    pnames = F_LumpNumForName("PNAMES");
    if(-1 == pnames)
    {
        if(numDefs) *numDefs = 0;
        return NULL;
    }

    // Load the patch names from the PNAMES lump.
    patchNames = loadPatchNames(pnames, &numPatchNames);
    if(!patchNames)
    {
        Con_Message("Warning:loadPatchCompositeDefs: Unexpected error occured loading PNAMES.\n");
        if(numDefs) *numDefs = 0;
        return NULL;
    }

    /*
     * Precedence order of definitions is defined by id tech1 which processes
     * the TEXTURE1/2 lumps in the following order:
     *
     * (last)TEXTURE2 > (last)TEXTURE1
     */
    defLumpsSize = 0;
    defLumps = NULL;
    firstTexLump = W_CheckLumpNumForName2("TEXTURE1", true/*quiet please*/);
    secondTexLump = W_CheckLumpNumForName2("TEXTURE2", true/*quiet please*/);

    /*
     * Also process all other lumps named TEXTURE1/2.
     */
    numLumps = F_LumpCount();
    for(i = 0; i < numLumps; ++i)
    {
        ddstring_t const* lumpName;

        // Will this be processed anyway?
        if(i == firstTexLump || i == secondTexLump) continue;

        lumpName = F_LumpName(i);
        if(strnicmp(Str_Text(lumpName), "TEXTURE1", 8) &&
           strnicmp(Str_Text(lumpName), "TEXTURE2", 8)) continue;

        defLumps = (lumpnum_t*) realloc(defLumps, sizeof *defLumps * ++defLumpsSize);
        if(!defLumps) Con_Error("loadPatchCompositeDefs: Failed on (re)allocation of %lu bytes enlarging definition lump list.", (unsigned long) sizeof *defLumps * defLumpsSize);
        defLumps[defLumpsSize-1] = i;
    }

    if(firstTexLump >= 0)
    {
        defLumps = (lumpnum_t*)realloc(defLumps, sizeof *defLumps * ++defLumpsSize);
        if(!defLumps)
            Con_Error("loadPatchCompositeDefs: Failed on (re)allocation of %lu bytes enlarging definition lump list.", (unsigned long) sizeof *defLumps * defLumpsSize);
        defLumps[defLumpsSize-1] = firstTexLump;
    }

    if(secondTexLump >= 0)
    {
        defLumps = (lumpnum_t*)realloc(defLumps, sizeof *defLumps * ++defLumpsSize);
        if(!defLumps)
            Con_Error("loadPatchCompositeDefs: Failed on (re)allocation of %lu bytes enlarging definition lump list.", (unsigned long) sizeof *defLumps * defLumpsSize);
        defLumps[defLumpsSize-1] = secondTexLump;
    }

    /*
     * Many PWADs include new TEXTURE1/2 lumps including the IWAD doomtexture
     * definitions, with new definitions appended. In order to correctly
     * determine whether a defined texture originates from an IWAD we must
     * compare all definitions against those in the IWAD and if matching,
     * they should be considered as IWAD resources, even though the doomtexture
     * definition does not come from an IWAD lump.
     */
    origIndexBase = 0;
    firstNull = true;
    for(i = 0; i < defLumpsSize; ++i)
    {
        lumpnum_t lumpNum = defLumps[i];

        // Read in the new texture defs.
        newTexDefs = readDoomTextureDefLump(lumpNum, patchNames, numPatchNames, &origIndexBase, firstNull, &newNumTexDefs);

        isCustom = F_LumpIsCustom(lumpNum);
        eList  = (!isCustom? &list  : &listCustom);
        eCount = (!isCustom? &count : &countCustom);
        if(*eList)
        {
            patchcompositetex_t** newList;
            size_t n;
            int i;

            // Merge with the existing doomtexturedefs.
            newList = (patchcompositetex_t**)malloc(sizeof *newList * ((*eCount) + newNumTexDefs));
            if(!newList)
                Con_Error("loadPatchCompositeDefs: Failed on allocation of %lu bytes for merged PatchComposite list.", (unsigned long) sizeof *newList * ((*eCount) + newNumTexDefs));

            n = 0;
            for(i = 0; i < *eCount; ++i)
                newList[n++] = (*eList)[i];
            for(i = 0; i < newNumTexDefs; ++i)
                newList[n++] = newTexDefs[i];

            free(*eList);
            free(newTexDefs);

            *eList = newList;
            *eCount += newNumTexDefs;
        }
        else
        {
            *eList = newTexDefs;
            *eCount = newNumTexDefs;
        }

        // No more "not-drawn" textures.
        firstNull = false;
    }

    if(listCustom)
    {
        // There are custom doomtexturedefs, cross compare with the IWAD
        // originals to see if they have been changed.
        patchcompositetex_t** newList;
        size_t n;

        i = 0;
        while(i < count)
        {
            patchcompositetex_t* orig = list[i];
            boolean hasReplacement = false;
            int j;

            for(j = 0; j < countCustom; ++j)
            {
                patchcompositetex_t* custom = listCustom[j];

                if(!Str_CompareIgnoreCase(&orig->name, Str_Text(&custom->name)))
                {
                    // This is a newer version of an IWAD doomtexturedef.
                    if(custom->flags & TXDF_CUSTOM)
                    {
                        hasReplacement = true; // Uses a non-IWAD patch.
                    }
                    // Do the definitions differ?
                    else if(custom->size.height != orig->size.height ||
                            custom->size.width  != orig->size.width  ||
                            custom->patchCount  != orig->patchCount)
                    {
                        custom->flags |= TXDF_CUSTOM;
                        hasReplacement = true;
                    }
                    else
                    {
                        // Check the patches.
                        short k = 0;
                        while(k < orig->patchCount && !(custom->flags & TXDF_CUSTOM))
                        {
                            texpatch_t* origP   = orig->patches  + k;
                            texpatch_t* customP = custom->patches + k;

                            if(origP->lumpNum != customP->lumpNum &&
                               origP->offX != customP->offX &&
                               origP->offY != customP->offY)
                            {
                                custom->flags |= TXDF_CUSTOM;
                                hasReplacement = true;
                            }
                            else
                            {
                                k++;
                            }
                        }
                    }

                    // The non-drawable flag must pass to the replacement.
                    if(hasReplacement && (orig->flags & TXDF_NODRAW))
                        custom->flags |= TXDF_NODRAW;
                    break;
                }
            }

            if(hasReplacement)
            {
                // Let the PWAD "copy" override the IWAD original.
                Str_Free(&orig->name);
                if(orig->patches) free(orig->patches);
                free(orig);
                if(i < count - 1)
                    memmove(list + i, list + i + 1, sizeof *list * (count - i - 1));
                count--;
            }
            else
            {
                i++;
            }
        }

        // List contains only non-replaced doomtexturedefs, merge them.
        newList = (patchcompositetex_t**)malloc(sizeof *newList * (count + countCustom));
        if(!newList)
            Con_Error("loadPatchCompositeDefs: Failed on allocation of %lu bytes for the unique PatchComposite list.", (unsigned long) sizeof *newList * (count + countCustom));

        n = 0;
        for(i = 0; i < count; ++i)
            newList[n++] = list[i];
        for(i = 0; i < countCustom; ++i)
            newList[n++] = listCustom[i];

        free(list);
        free(listCustom);

        patchCompositeTextures = newList;
        patchCompositeTexturesCount = count + countCustom;
    }
    else
    {
        patchCompositeTextures = list;
        patchCompositeTexturesCount = count;
    }

    if(defLumps) free(defLumps);

    // We are finished with the patch names now.
    free(patchNames);

    if(numDefs) *numDefs = patchCompositeTexturesCount;
    return patchCompositeTextures;
}

/// \note Definitions for Textures that are not created successfully will be pruned from the set.
static void createTexturesForPatchCompositeDefs(patchcompositetex_t** defs, int count)
{
    Uri* uri = Uri_New();
    textureid_t texId;
    Texture* tex;
    int i;
    assert(defs);

    Uri_SetScheme(uri, TS_TEXTURES_NAME);
    for(i = 0; i < count; ++i)
    {
        patchcompositetex_t* pcTex = defs[i];

        Uri_SetPath(uri, Str_Text(&pcTex->name));

        texId = Textures_Declare(uri, pcTex->origIndex, NULL);
        if(texId == NOTEXTUREID) continue; // Invalid uri?

        tex = Textures_ToTexture(texId);
        if(tex)
        {
            patchcompositetex_t* oldPcTex = Texture_UserDataPointer(tex);

            Texture_FlagCustom(tex, !!(pcTex->flags & TXDF_CUSTOM));
            Texture_SetSize(tex, &pcTex->size);
            Texture_SetUserDataPointer(tex, (void*)pcTex);

            Str_Free(&oldPcTex->name);
            if(oldPcTex->patches) free(oldPcTex->patches);
            free(oldPcTex);
        }
        else if(!Textures_CreateWithSize(texId, !!(pcTex->flags & TXDF_CUSTOM), &pcTex->size, (void*)pcTex))
        {
            Con_Message("Warning: Failed defining Texture for new patch composite '%s', ignoring.\n", Str_Text(&pcTex->name));
            Str_Free(&pcTex->name);
            if(pcTex->patches) free(pcTex->patches);
            free(pcTex);
            defs[i] = NULL;
        }
    }
    Uri_Delete(uri);
}

void R_InitPatchComposites(void)
{
    uint startTime = (verbose >= 2? Timer_RealMilliseconds() : 0);
    patchcompositetex_t** defs;
    int defsCount;

    VERBOSE( Con_Message("Initializing PatchComposite textures...\n") )

    // Load texture definitions from TEXTURE1/2 lumps.
    defs = loadPatchCompositeDefs(&defsCount);
    if(defs)
    {
        createTexturesForPatchCompositeDefs(defs, defsCount);
        free(defs);
    }

    VERBOSE2( Con_Message("R_InitPatchComposites: Done in %.2f seconds.\n", (Timer_RealMilliseconds() - startTime) / 1000.0f) );
}

/// @todo Do this in the lump directory where we can make use of the hash!
static lumpnum_t firstLumpWithName(char const* lumpName)
{
    if(lumpName && lumpName[0])
    {
        lumpnum_t lumpNum, numLumps = F_LumpCount();
        for(lumpNum = 0; lumpNum < numLumps; ++lumpNum)
        {
            if(!Str_CompareIgnoreCase(F_LumpName(lumpNum), lumpName))
                return lumpNum;
        }
    }
    return -1;
}

static Uri* composeFlatUri(char const* lumpName)
{
    Uri* uri;
    AutoStr* flatName = AutoStr_NewStd();
    F_FileName(flatName, lumpName);
    uri = Uri_NewWithPath2(TS_FLATS_NAME":", RC_NULL);
    Uri_SetPath(uri, Str_Text(flatName));
    return uri;
}

/**
 * Compose the path to the data resource.
 * @note We do not use the lump name, instead we use the logical lump index
 * in the global LumpIndex. This is necessary because of the way id tech 1
 * manages flat references in animations (intermediate frames are chosen by their
 * 'original indices' rather than by name).
 */
static Uri* composeFlatResourceUrn(lumpnum_t lumpNum)
{
    Uri* uri;
    char name[9];
    dd_snprintf(name, 9, "%i", lumpNum);
    uri = Uri_NewWithPath2("LumpDir:", RC_NULL);
    Uri_SetPath(uri, name);
    return uri;
}

void R_InitFlatTextures(void)
{
    uint startTime = (verbose >= 2? Timer_RealMilliseconds() : 0);
    lumpnum_t firstFlatMarkerLumpNum;

    VERBOSE( Con_Message("Initializing Flat textures...\n") )

    firstFlatMarkerLumpNum = firstLumpWithName("F_START.lmp");
    if(firstFlatMarkerLumpNum >= 0)
    {
        lumpnum_t lumpNum, numLumps = F_LumpCount();
        struct file1_s* blockFile = 0;
        for(lumpNum = numLumps; lumpNum --> firstFlatMarkerLumpNum + 1;)
        {
            ddstring_t const* lumpName = F_LumpName(lumpNum);
            struct file1_s* lumpFile = F_FindFileForLumpNum(lumpNum);

            if(blockFile && blockFile != lumpFile)
            {
                blockFile = 0;
            }

            if(!blockFile)
            {
                if(!Str_CompareIgnoreCase(lumpName, "F_END.lmp") ||
                   !Str_CompareIgnoreCase(lumpName, "FF_END.lmp"))
                {
                    blockFile = lumpFile;
                }
                continue;
            }

            if(!Str_CompareIgnoreCase(lumpName, "F_START.lmp"))
            {
                blockFile = 0;
                continue;
            }

            // Ignore extra marker lumps.
            if(!Str_CompareIgnoreCase(lumpName, "FF_START.lmp") ||
               !Str_CompareIgnoreCase(lumpName, "F_END.lmp")    || !Str_CompareIgnoreCase(lumpName, "FF_END.lmp")) continue;

            {
            Uri* uri = composeFlatUri(Str_Text(lumpName));
            if(Textures_ResolveUri2(uri, true/*quiet please*/) == NOTEXTUREID) // A new flat?
            {
                /**
                 * Kludge Assume 64x64 else when the flat is loaded it will inherit the
                 * dimensions of the texture, which, if it has been replaced with a hires
                 * version - will be much larger than it should be.
                 *
                 * @todo Always determine size from the lowres original.
                 */
                Size2Raw const size = { 64, 64 };
                int const uniqueId = lumpNum - (firstFlatMarkerLumpNum + 1);
                Uri* resourcePath = composeFlatResourceUrn(lumpNum);
                textureid_t texId = Textures_Declare(uri, uniqueId, resourcePath);
                if(!Textures_CreateWithSize(texId, F_LumpIsCustom(lumpNum), &size, NULL))
                {
                    AutoStr* path = Uri_ToString(uri);
                    Con_Message("Warning: Failed defining Texture for new flat '%s', ignoring.\n", Str_Text(path));
                }
                Uri_Delete(resourcePath);
            }
            Uri_Delete(uri);
            }
        }
    }

    VERBOSE2( Con_Message("R_InitFlatTextures: Done in %.2f seconds.\n", (Timer_RealMilliseconds() - startTime) / 1000.0f) );
}

static boolean validSpriteName(const ddstring_t* name)
{
    if(!name || Str_Length(name) < 5) return false;
    if(!Str_At(name, 4) || !Str_At(name, 5) || (Str_At(name, 6) && !Str_At(name, 7))) return false;
    // Indices 5 and 7 must be numbers (0-8).
    if(Str_At(name, 5) < '0' || Str_At(name, 5) > '8') return false;
    if(Str_At(name, 7) && (Str_At(name, 7) < '0' || Str_At(name, 7) > '8')) return false;
    // Its good!
    return true;
}

void R_DefineSpriteTexture(textureid_t texId)
{
    const Uri* resourceUri = Textures_ResourcePath(texId);
    Texture* tex = Textures_ToTexture(texId);

    // Have we already encountered this name?
    if(!tex)
    {
        // A new sprite texture.
        patchtex_t* pTex = (patchtex_t*)malloc(sizeof *pTex);
        if(!pTex)
            Con_Error("R_InitSpriteTextures: Failed on allocation of %lu bytes for new PatchTex.", (unsigned long) sizeof *pTex);
        pTex->offX = pTex->offY = 0; // Deferred until texture load time.

        tex = Textures_Create(texId, 0, (void*)pTex);
        if(!tex)
        {
            Uri* uri = Textures_ComposeUri(texId);
            AutoStr* path = Uri_ToString(uri);
            Con_Message("Warning: Failed defining Texture for \"%s\", ignoring.\n", Str_Text(path));
            Uri_Delete(uri);
            free(pTex);
        }
    }

    if(tex && resourceUri)
    {
        AutoStr* resourcePath = Uri_Resolved(resourceUri);
        lumpnum_t lumpNum = F_LumpNumForName(Str_Text(resourcePath));
        int lumpIdx;
        struct file1_s* file = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
        const doompatch_header_t* patch = (const doompatch_header_t*) F_CacheLump(file, lumpIdx);
        Size2Raw size;

        size.width  = SHORT(patch->width);
        size.height = SHORT(patch->height);
        Texture_SetSize(tex, &size);

        Texture_FlagCustom(tex, F_LumpIsCustom(lumpNum));

        F_UnlockLump(file, lumpIdx);
    }
}

int RIT_DefineSpriteTexture(textureid_t texId, void* paramaters)
{
    R_DefineSpriteTexture(texId);
    return 0; // Continue iteration.
}

/// @todo Defer until necessary (sprite is first de-referenced).
static void defineAllSpriteTextures(void)
{
    Textures_IterateDeclared(TS_SPRITES, RIT_DefineSpriteTexture);
}

void R_InitSpriteTextures(void)
{
    uint startTime = (verbose >= 2? Timer_RealMilliseconds() : 0);
    int i, numLumps, uniqueId = 1/*1-based index*/;
    ddstring_t spriteName, decodedSpriteName;
    Uri* uri, *resourcePath;
    ddstack_t* stack;

    VERBOSE( Con_Message("Initializing Sprite textures...\n") )

    uri = Uri_NewWithPath2(TS_SPRITES_NAME":", RC_NULL);
    resourcePath = Uri_NewWithPath2("Lumps:", RC_NULL);

    Str_Init(&spriteName);
    Str_Init(&decodedSpriteName);

    stack = Stack_New();
    numLumps = F_LumpCount();

    /// @todo Order here does not respect id tech1 logic.
    for(i = 0; i < numLumps; ++i)
    {
        ddstring_t const* lumpName = F_LumpName((lumpnum_t)i);
        textureid_t texId;

        if(Str_At(lumpName, 0) == 'S' && Str_Length(lumpName) >= 5)
        {
            if(!strnicmp(Str_Text(lumpName) + 1, "_START", 6) ||
               !strnicmp(Str_Text(lumpName) + 2, "_START", 6))
            {
                // We've arrived at *a* sprite block.
                Stack_Push(stack, NULL);
                continue;
            }
            else if(!strnicmp(Str_Text(lumpName) + 1, "_END", 4) ||
                    !strnicmp(Str_Text(lumpName) + 2, "_END", 4))
            {
                // The sprite block ends.
                Stack_Pop(stack);
                continue;
            }
        }

        if(!Stack_Height(stack)) continue;

        F_FileName(&spriteName, Str_Text(lumpName));

        Str_Set(&decodedSpriteName, Str_Text(&spriteName));
        Str_PercentDecode(&decodedSpriteName);
        if(!validSpriteName(&decodedSpriteName)) continue;

        // Compose the resource name.
        Uri_SetPath(uri, Str_Text(&spriteName));

        // Compose the data resource path.
        Uri_SetPath(resourcePath, Str_Text(lumpName));

        texId = Textures_Declare(uri, uniqueId, resourcePath);
        if(texId == NOTEXTUREID) continue; // Invalid uri?
        uniqueId++;
    }

    while(Stack_Height(stack)) { Stack_Pop(stack); }
    Stack_Delete(stack);

    Uri_Delete(resourcePath);
    Uri_Delete(uri);
    Str_Free(&decodedSpriteName);
    Str_Free(&spriteName);

    // Define any as yet undefined sprite textures.
    defineAllSpriteTextures();

    VERBOSE2( Con_Message("R_InitSpriteTextures: Done in %.2f seconds.\n", (Timer_RealMilliseconds() - startTime) / 1000.0f) )
}

Texture* R_CreateSkinTex(const Uri* filePath, boolean isShinySkin)
{
    textureid_t texId;
    Texture* tex;
    char name[9];
    int uniqueId;
    Uri* uri;

    if(!filePath || Str_IsEmpty(Uri_Path(filePath))) return 0;

    // Have we already created one for this?
    if(!isShinySkin)
    {
        tex = R_FindModelSkinForResourcePath(filePath);
    }
    else
    {
        tex = R_FindModelReflectionSkinForResourcePath(filePath);
    }
    if(tex) return tex;

    uniqueId = Textures_Count(isShinySkin? TS_MODELREFLECTIONSKINS : TS_MODELSKINS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
#if _DEBUG
        Con_Message("Warning: Failed creating SkinName (max:%i), ignoring.\n", DDMAXINT);
#endif
        return NULL;
    }

    dd_snprintf(name, 9, "%0*i", 8, uniqueId);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, (isShinySkin? TS_MODELREFLECTIONSKINS_NAME : TS_MODELSKINS_NAME));

    texId = Textures_Declare(uri, uniqueId, filePath);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return NULL; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        // Create a texture for it.
        tex = Textures_Create(texId, true/*is-custom*/, NULL);
        if(!tex)
        {
            Con_Message("Warning: Failed defining Texture for ModelSkin '%s', ignoring.\n", name);
            return NULL;
        }
    }

    return tex;
}

static boolean expandSkinName(ddstring_t* foundPath, const char* skin, const char* modelfn)
{
    boolean found = false;

    DENG_ASSERT(foundPath && skin && skin[0]);

    // Try the "first choice" directory first.
    if(modelfn)
    {
        // The "first choice" directory is that in which the model file resides.
        directory_t* mydir = Dir_FromText(modelfn);
        AutoStr* path = Str_Appendf(AutoStr_NewStd(), "%s%s", mydir->path, skin);
        Uri* searchPath = Uri_NewWithPath2(Str_Text(path), RC_NULL);

        found = F_FindPath(RC_GRAPHIC, searchPath, foundPath);

        Uri_Delete(searchPath);
        Dir_Delete(mydir);
    }

    if(!found)
    {
        AutoStr* path = Str_Appendf(AutoStr_NewStd(), "Models:%s", skin);
        Uri* searchPath = Uri_NewWithPath(Str_Text(path));

        found = F_FindPath(RC_GRAPHIC, searchPath, foundPath);

        Uri_Delete(searchPath);
    }

    return found;
}

Texture* R_RegisterModelSkin(ddstring_t* foundPath, const char* skin, const char* modelfn, boolean isShinySkin)
{
    Texture* tex = NULL;
    if(skin && skin[0])
    {
        ddstring_t buf;
        if(!foundPath)
            Str_Init(&buf);

        if(expandSkinName(foundPath ? foundPath : &buf, skin, modelfn))
        {
            Uri* uri = Uri_NewWithPath2(foundPath ? Str_Text(foundPath) : Str_Text(&buf), RC_NULL);
            tex = R_CreateSkinTex(uri, isShinySkin);
            Uri_Delete(uri);
        }

        if(!foundPath)
            Str_Free(&buf);
    }
    return tex;
}

static int findModelSkinForResourcePathWorker(textureid_t texId, void* paramaters)
{
    const Uri* resourcePath = Textures_ResourcePath(texId);
    if(Uri_Equality(resourcePath, (const Uri*)paramaters))
    {
        return (int)texId;
    }
    return 0; // Continue iteration.
}

Texture* R_FindModelSkinForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path))) return NULL;
    result = Textures_IterateDeclared2(TS_MODELSKINS, findModelSkinForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

Texture* R_FindModelReflectionSkinForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path))) return NULL;
    result = Textures_IterateDeclared2(TS_MODELREFLECTIONSKINS, findModelSkinForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

void R_UpdateData(void)
{
    R_UpdateRawTexs();
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

boolean R_IsAllowedDecoration(ded_decor_t* def, boolean hasExternal, boolean isCustom)
{
    if(hasExternal) return (def->flags & DCRF_EXTERNAL) != 0;
    if(!isCustom)   return (def->flags & DCRF_NO_IWAD ) == 0;
    return (def->flags & DCRF_PWAD) != 0;
}

boolean R_IsAllowedReflection(ded_reflection_t* def, boolean hasExternal, boolean isCustom)
{
    if(hasExternal) return (def->flags & REFF_EXTERNAL) != 0;
    if(!isCustom)   return (def->flags & REFF_NO_IWAD ) == 0;
    return (def->flags & REFF_PWAD) != 0;
}

boolean R_IsAllowedDetailTex(ded_detailtexture_t* def, boolean hasExternal, boolean isCustom)
{
    if(hasExternal) return (def->flags & DTLF_EXTERNAL) != 0;
    if(!isCustom)   return (def->flags & DTLF_NO_IWAD ) == 0;
    return (def->flags & DTLF_PWAD) != 0;
}

static boolean isInList(void** list, size_t len, void* elm)
{
    size_t              n;

    if(!list || !elm || len == 0)
        return false;

    for(n = 0; n < len; ++n)
        if(list[n] == elm)
            return true;

    return false;
}

int findSpriteOwner(thinker_t* th, void* context)
{
    mobj_t* mo = (mobj_t*) th;
    spritedef_t* sprDef = (spritedef_t*) context;

    if(mo->type >= 0 && mo->type < defs.count.mobjs.num)
    {
        //// @todo Optimize: traverses the entire state list!
        int i;
        for(i = 0; i < defs.count.states.num; ++i)
        {
            if(stateOwners[i] != &mobjInfo[mo->type])
                continue;

            if(&sprites[states[i].sprite] == sprDef)
                return true; // Found an owner!
        }
    }

    return false; // Keep looking...
}

void R_CacheSpritesForState(int stateIndex, const materialvariantspecification_t* spec)
{
    spritedef_t* sprDef;
    state_t* state;
    int j;

    if(stateIndex < 0 || stateIndex >= defs.count.states.num) return;
    if(!spec) return;

    state = &states[stateIndex];
    sprDef = &sprites[state->sprite];

    for(j = 0; j < sprDef->numFrames; ++j)
    {
        spriteframe_t* sprFrame = &sprDef->spriteFrames[j];
        int k;
        for(k = 0; k < 8; ++k)
        {
            Materials_Precache(sprFrame->mats[k], spec, true);
        }
    }
}

/// @note Part of the Doomsday public API.
void R_PrecacheMobjNum(int num)
{
    const materialvariantspecification_t* spec;
    int i;

    if(novideo || !((useModels && precacheSkins) || precacheSprites)) return;
    if(num < 0 || num >= defs.count.mobjs.num) return;

    spec = Materials_VariantSpecificationForContext(MC_SPRITE, 0, 1, 0, 0,
                                                    GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                                    1, -2, -1, true, true, true, false);

    /// @todo Optimize: Traverses the entire state list!
    for(i = 0; i < defs.count.states.num; ++i)
    {
        if(stateOwners[i] != &mobjInfo[num]) continue;

        Models_CacheForState(i);

        if(precacheSprites)
        {
            R_CacheSpritesForState(i, spec);
        }
        /// @todo What about sounds?
    }
}

void R_PrecacheForMap(void)
{
    // Don't precache when playing demo.
    if(isDedicated || playback) return;

    // Precaching from 100 to 200.
    Con_SetProgress(100);

    if(precacheMapMaterials)
    {
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
        uint i, j;

        for(i = 0; i < NUM_SIDEDEFS; ++i)
        {
            SideDef* side = SIDE_PTR(i);

            if(side->SW_middlematerial)
                Materials_Precache(side->SW_middlematerial, spec, true);

            if(side->SW_topmaterial)
                Materials_Precache(side->SW_topmaterial, spec, true);

            if(side->SW_bottommaterial)
                Materials_Precache(side->SW_bottommaterial, spec, true);
        }

        for(i = 0; i < NUM_SECTORS; ++i)
        {
            Sector* sec = SECTOR_PTR(i);
            if(!sec->lineDefCount) continue;
            for(j = 0; j < sec->planeCount; ++j)
            {
                Materials_Precache(sec->SP_planematerial(j), spec, true);
            }
        }
    }

    if(precacheSprites)
    {
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_SPRITE, 0, 1, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 1, -2, -1, true, true, true, false);
        int i;

        for(i = 0; i < numSprites; ++i)
        {
            spritedef_t* sprDef = &sprites[i];

            if(GameMap_IterateThinkers(theMap, gx.MobjThinker, 0x1/* All mobjs are public*/,
                                       findSpriteOwner, sprDef))
            {
                // This sprite is used by some state of at least one mobj.
                int j;

                // Precache all the frames.
                for(j = 0; j < sprDef->numFrames; ++j)
                {
                    spriteframe_t* sprFrame = &sprDef->spriteFrames[j];
                    int k;
                    for(k = 0; k < 8; ++k)
                        Materials_Precache(sprFrame->mats[k], spec, true);
                }
            }
        }
    }

     // Sky models usually have big skins.
    R_SkyPrecache();

    // Precache model skins?
    if(useModels && precacheSkins)
    {
        // All mobjs are public.
        GameMap_IterateThinkers(theMap, gx.MobjThinker, 0x1, Models_CacheForMobj, NULL);
    }
}

Texture* R_CreateDetailTextureFromDef(const ded_detailtexture_t* def)
{
    textureid_t texId;
    Texture* tex;
    char name[9];
    int uniqueId;
    Uri* uri;

    if(!def->detailTex || Uri_IsEmpty(def->detailTex)) return 0;

    // Have we already created one for this?
    tex = R_FindDetailTextureForResourcePath(def->detailTex);
    if(tex) return tex;

    uniqueId = Textures_Count(TS_DETAILS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
        Con_Message("Warning: failed to create new detail texture (max:%i).\n", DDMAXINT);
        return NULL;
    }

    dd_snprintf(name, 9, "%0*i", 8, uniqueId);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TS_DETAILS_NAME);

    texId = Textures_Declare(uri, uniqueId, def->detailTex);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return NULL; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(!tex && !Textures_Create(texId, true/*is-custom*/, NULL))
    {
        Con_Message("Warning: Failed defining Texture for DetailTexture '%s', ignoring.\n", name);
        return NULL;
    }

    return tex;
}

static int findDetailTextureForResourcePathWorker(textureid_t texId, void* paramaters)
{
    const Uri* resourcePath = Textures_ResourcePath(texId);
    if(Uri_Equality(resourcePath, (const Uri*)paramaters))
    {
        return (int)texId;
    }
    return 0; // Continue iteration.
}

Texture* R_FindDetailTextureForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path))) return NULL;
    result = Textures_IterateDeclared2(TS_DETAILS, findDetailTextureForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

Texture* R_CreateLightMap(const Uri* resourcePath)
{
    textureid_t texId;
    Texture* tex;
    char name[9];
    int uniqueId;
    Uri* uri;

    if(!resourcePath || Uri_IsEmpty(resourcePath) || !Str_CompareIgnoreCase(Uri_Path(resourcePath), "-"))
        return NULL;

    // Have we already created one for this?
    tex = R_FindLightMapForResourcePath(resourcePath);
    if(tex) return tex;

    uniqueId = Textures_Count(TS_LIGHTMAPS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
        Con_Message("Warning: Failed declaring new LightMap (max:%i), ignoring.\n", DDMAXINT);
        return NULL;
    }

    dd_snprintf(name, 9, "%0*i", 8, uniqueId);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TS_LIGHTMAPS_NAME);

    texId = Textures_Declare(uri, uniqueId, resourcePath);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return NULL; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        // Create a texture for it.
        tex = Textures_Create(texId, true/*is-custom*/, NULL);
        if(!tex)
        {
            Con_Message("Warning: Failed defining Texture for LightMap '%s', ignoring.\n", name);
            return NULL;
        }
    }
    return tex;
}

static int findLightMapTextureForResourcePathWorker(textureid_t texId, void* paramaters)
{
    const Uri* resourcePath = Textures_ResourcePath(texId);
    if(Uri_Equality(resourcePath, (const Uri*)paramaters))
    {
        return (int)texId;
    }
    return 0; // Continue iteration.
}

Texture* R_FindLightMapForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path)) || !Str_CompareIgnoreCase(Uri_Path(path), "-")) return NULL;

    result = Textures_IterateDeclared2(TS_LIGHTMAPS, findLightMapTextureForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

Texture* R_CreateFlareTexture(const Uri* resourcePath)
{
    textureid_t texId;
    Texture* tex;
    char name[9];
    int uniqueId;
    Uri* uri;

    if(!resourcePath || Uri_IsEmpty(resourcePath) || !Str_CompareIgnoreCase(Uri_Path(resourcePath), "-"))
        return NULL;

    // Perhaps a "built-in" flare texture id?
    // Try to convert the id to a system flare tex constant idx
    if(Str_At(Uri_Path(resourcePath), 0) >= '0' && Str_At(Uri_Path(resourcePath), 0) <= '4' && !Str_At(Uri_Path(resourcePath), 1))
        return NULL;

    // Have we already created one for this?
    tex = R_FindFlareTextureForResourcePath(resourcePath);
    if(tex) return tex;

    uniqueId = Textures_Count(TS_FLAREMAPS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
        Con_Message("Warning: Failed declaring new FlareTex (max:%i), ignoring.\n", DDMAXINT);
        return NULL;
    }

    // Create a texture for it.
    dd_snprintf(name, 9, "%0*i", 8, uniqueId);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TS_FLAREMAPS_NAME);

    texId = Textures_Declare(uri, uniqueId, resourcePath);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return NULL; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        tex = Textures_Create(texId, true/*is-custom*/, NULL);
        if(!tex)
        {
            Con_Message("Warning: Failed defining Texture for flare texture '%s', ignoring.\n", name);
            return NULL;
        }
    }
    return tex;
}

static int findFlareTextureForResourcePathWorker(textureid_t texId, void* paramaters)
{
    const Uri* resourcePath = Textures_ResourcePath(texId);
    if(Uri_Equality(resourcePath, (const Uri*)paramaters))
    {
        return (int)texId;
    }
    return 0; // Continue iteration.
}

Texture* R_FindFlareTextureForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path)) || !Str_CompareIgnoreCase(Uri_Path(path), "-")) return NULL;

    result = Textures_IterateDeclared2(TS_FLAREMAPS, findFlareTextureForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

Texture* R_CreateReflectionTexture(const Uri* resourcePath)
{
    textureid_t texId;
    Texture* tex;
    char name[9];
    int uniqueId;
    Uri* uri;

    if(!resourcePath || Uri_IsEmpty(resourcePath)) return 0;

    // Have we already created one for this?
    tex = R_FindReflectionTextureForResourcePath(resourcePath);
    if(tex) return tex;

    uniqueId = Textures_Count(TS_REFLECTIONS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
        Con_Message("Warning: Failed declaring new ShinyTex (max:%i), ignoring.\n", DDMAXINT);
        return NULL;
    }

    dd_snprintf(name, 9, "%0*i", 8, uniqueId);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TS_REFLECTIONS_NAME);

    texId = Textures_Declare(uri, uniqueId, resourcePath);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return NULL; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        // Create a texture for it.
        tex = Textures_Create(texId, true/*is-custom*/, NULL);
        if(!tex)
        {
            Con_Message("Warning: Failed defining Texture for shiny texture '%s', ignoring.\n", name);
            return NULL;
        }
    }

    return tex;
}

static int findReflectionTextureForResourcePathWorker(textureid_t texId, void* paramaters)
{
    const Uri* resourcePath = Textures_ResourcePath(texId);
    if(Uri_Equality(resourcePath, (const Uri*)paramaters))
    {
        return (int)texId;
    }
    return 0; // Continue iteration.
}

Texture* R_FindReflectionTextureForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path))) return NULL;

    result = Textures_IterateDeclared2(TS_REFLECTIONS, findReflectionTextureForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

Texture* R_CreateMaskTexture(const Uri* resourcePath, const Size2Raw* size)
{
    textureid_t texId;
    Texture* tex;
    char name[9];
    int uniqueId;
    Uri* uri;

    if(!resourcePath || Uri_IsEmpty(resourcePath)) return 0;

    // Have we already created one for this?
    tex = R_FindMaskTextureForResourcePath(resourcePath);
    if(tex) return tex;

    uniqueId = Textures_Count(TS_MASKS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
        Con_Message("Warning: Failed declaring Mask texture (max:%i), ignoring.\n", DDMAXINT);
        return NULL;
    }

    dd_snprintf(name, 9, "%0*i", 8, uniqueId);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TS_MASKS_NAME);

    texId = Textures_Declare(uri, uniqueId, resourcePath);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return NULL; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(tex)
    {
        Texture_SetSize(tex, size);
    }
    else
    {
        // Create a texture for it.
        tex = Textures_CreateWithSize(texId, true/*is-custom*/, size, NULL);
        if(!tex)
        {
            AutoStr* path = Uri_ToString(resourcePath);
            Con_Message("Warning: Failed defining Texture for mask texture \"%s\"\n", F_PrettyPath(Str_Text(path)));
            return NULL;
        }
    }

    return tex;
}

static int findMaskTextureForResourcePathWorker(textureid_t texId, void* paramaters)
{
    const Uri* resourcePath = Textures_ResourcePath(texId);
    if(Uri_Equality(resourcePath, (const Uri*)paramaters))
    {
        return (int)texId;
    }
    return 0; // Continue iteration.
}

Texture* R_FindMaskTextureForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path))) return NULL;

    result = Textures_IterateDeclared2(TS_MASKS, findMaskTextureForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

font_t* R_CreateFontFromFile(Uri* uri, char const* resourcePath)
{
    fontschemeid_t schemeId;
    fontid_t fontId;
    int uniqueId;
    font_t* font;

    if(!uri || !resourcePath || !resourcePath[0] || !F_Access(resourcePath))
    {
#if _DEBUG
        Con_Message("Warning:R_CreateFontFromFile: Invalid Uri or ResourcePath reference, ignoring.\n");
        if(resourcePath) Con_Message("  Resource path: %s\n", resourcePath);
#endif
        return NULL;
    }

    schemeId = Fonts_ParseScheme(Str_Text(Uri_Scheme(uri)));
    if(!VALID_FONTSCHEMEID(schemeId))
    {
        AutoStr* path = Uri_ToString(uri);
        Con_Message("Warning: Invalid font scheme in Font Uri \"%s\", ignoring.\n", Str_Text(path));
        return NULL;
    }

    uniqueId = Fonts_Count(schemeId)+1; // 1-based index.
    fontId = Fonts_Declare(uri, uniqueId/*, resourcePath*/);
    if(fontId == NOFONTID) return NULL; // Invalid uri?

    // Have we already encountered this name?
    font = Fonts_ToFont(fontId);
    if(font)
    {
        Fonts_RebuildFromFile(font, resourcePath);
    }
    else
    {
        // A new font.
        font = Fonts_CreateFromFile(fontId, resourcePath);
        if(!font)
        {
            AutoStr* path = Uri_ToString(uri);
            Con_Message("Warning: Failed defining new Font for \"%s\", ignoring.\n", Str_Text(path));
        }
    }
    return font;
}

font_t* R_CreateFontFromDef(ded_compositefont_t* def)
{
    fontschemeid_t schemeId;
    fontid_t fontId;
    int uniqueId;
    font_t* font;

    if(!def || !def->uri)
    {
#if _DEBUG
        Con_Message("Warning: R_CreateFontFromDef: Invalid Definition or Uri reference, ignoring.\n");
#endif
        return NULL;
    }

    schemeId = Fonts_ParseScheme(Str_Text(Uri_Scheme(def->uri)));
    if(!VALID_FONTSCHEMEID(schemeId))
    {
        AutoStr* path = Uri_ToString(def->uri);
        Con_Message("Warning: Invalid URI scheme in font definition \"%s\", ignoring.\n", Str_Text(path));
        return NULL;
    }

    uniqueId = Fonts_Count(schemeId)+1; // 1-based index.
    fontId = Fonts_Declare(def->uri, uniqueId);
    if(fontId == NOFONTID) return NULL; // Invalid uri?

    // Have we already encountered this name?
    font = Fonts_ToFont(fontId);
    if(font)
    {
        Fonts_RebuildFromDef(font, def);
    }
    else
    {
        // A new font.
        font = Fonts_CreateFromDef(fontId, def);
        if(!font)
        {
            AutoStr* path = Uri_ToString(def->uri);
            Con_Message("Warning: Failed defining new Font for \"%s\", ignoring.\n", Str_Text(path));
        }
    }
    return font;
}
