/**
 * @file r_data.cpp
 *
 * @ingroup resource
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <QDir>
#include <QList>
#include <de/App>
#include <de/ByteRefArray>
#include <de/Log>
#include <de/Reader>
#include <de/String>
#include <de/timer.h>
#include <de/memory.h>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "de_resource.h"

#include "def_main.h"
#include "gl/gl_tex.h"
#include "gl/gl_texmanager.h"
#include "m_stack.h"
#include "m_misc.h" // For M_NumDigits()
#include "uri.hh"

using namespace de;

static QList<PatchName> patchNames;

void R_InitSystemTextures()
{
    LOG_AS("R_InitSystemTextures");

    static String const names[] =
    {   "unknown", "missing", "bbox", "gray", "" };

    LOG_VERBOSE("Initializing System textures...");
    for(uint i = 0; !names[i].isEmpty(); ++i)
    {
        Path path = names[i];
        de::Uri uri = de::Uri(path).setScheme("System");
        de::Uri resourcePath = de::Uri(path).setScheme("Graphics");

        textureid_t texId = Textures_Declare(reinterpret_cast<uri_s*>(&uri), i + 1/*1-based index*/,
                                             reinterpret_cast<uri_s*>(&resourcePath));
        if(texId == NOTEXTUREID) continue; // Invalid uri?

        // Have we defined this yet?
        de::Texture *tex = reinterpret_cast<de::Texture*>(Textures_ToTexture(texId));
        if(!tex && !Textures_Create(texId, true/*is-custom*/, 0))
        {
            LOG_WARNING("Failed to define Texture for system texture \"%s\".")
                << NativePath(uri.asText()).pretty();
        }
    }
}

/// @note Part of the Doomsday public API.
AutoStr *R_ComposePatchPath(patchid_t id)
{
    textureid_t texId = Textures_TextureForUniqueId(TS_PATCHES, id);
    if(texId == NOTEXTUREID) return AutoStr_NewStd();
    return Textures_ComposePath(texId);
}

/// @note Part of the Doomsday public API.
uri_s *R_ComposePatchUri(patchid_t id)
{
    return Textures_ComposeUri(Textures_TextureForUniqueId(TS_PATCHES, id));
}

/// @note Part of the Doomsday public API.
patchid_t R_DeclarePatch(char const *name)
{
    LOG_AS("R_DeclarePatch");

    if(!name || !name[0])
    {
        LOG_DEBUG("Invalid 'name' argument, ignoring.");
        return 0;
    }

    // WAD format allows characters not normally permitted in native paths.
    // To achieve uniformity we apply a percent encoding to the "raw" names.
    de::Uri uri = de::Uri(Path(QString(QByteArray(name, qstrlen(name)).toPercentEncoding()))).setScheme("Patches");

    // Already defined as a patch?
    textureid_t texId = Textures_ResolveUri2(reinterpret_cast<uri_s*>(&uri), true/*quiet please*/);
    if(texId)
    {
        /// @todo We should instead define Materials from patches and return the material id.
        return patchid_t( Textures_UniqueId(texId) );
    }

    Path lumpPath = uri.path() + ".lmp";
    lumpnum_t lumpNum = App_FileSystem()->nameIndex().lastIndexForPath(lumpPath);
    if(lumpNum < 0)
    {
        LOG_WARNING("Failed to locate lump for \"%s\", ignoring.")
            << NativePath(uri.asText()).pretty();
        return 0;
    }

    // Compose the path to the data resource.
    de::File1& file = App_FileSystem()->nameIndex().lump(lumpNum);
    de::Uri resourceUri = de::Uri(Path(file.name())).setScheme("Lumps");

    int uniqueId = Textures_Count(TS_PATCHES) + 1; // 1-based index.
    texId = Textures_Declare(reinterpret_cast<uri_s*>(&uri), uniqueId,
                             reinterpret_cast<uri_s*>(&resourceUri));
    if(texId == NOTEXTUREID) return 0; // Invalid uri?

    // Generate a new patch.
    patchtex_t *p = (patchtex_t*) M_Malloc(sizeof(*p));
    if(!p) Con_Error("R_DeclarePatch: Failed on allocation of %u bytes for new PatchTex.", (unsigned long) sizeof(*p));

    // Take a copy of the current patch loading state so that future texture
    // loads will produce the same results.
    p->flags = 0;
    if(monochrome)               p->flags |= PF_MONOCHROME;
    if(upscaleAndSharpenPatches) p->flags |= PF_UPSCALE_AND_SHARPEN;

    /**
     * @todo: Cannot be sure this is in Patch format until a load attempt
     *        is made. We should not read this info here!
     */
    ByteRefArray fileData = ByteRefArray(file.cache(), file.size());
    de::Reader from = de::Reader(fileData);
    PatchHeader patchHdr;
    from >> patchHdr;

    p->offX = -patchHdr.origin.x;
    p->offY = -patchHdr.origin.y;

    de::Texture *tex = reinterpret_cast<de::Texture*>(Textures_ToTexture(texId));
    if(!tex)
    {
        tex = reinterpret_cast<de::Texture*>(Textures_CreateWithDimensions(texId, file.container().hasCustom(), &patchHdr.dimensions, (void *)p));
        file.unlock();

        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for Patch texture \"%s\".")
                << NativePath(uri.asText()).pretty();
            M_Free(p);
            return 0;
        }
    }
    else
    {
        patchtex_t *oldPatch = reinterpret_cast<patchtex_t *>(tex->userDataPointer());

        tex->flagCustom(file.container().hasCustom());
        tex->setDimensions(patchHdr.dimensions);
        tex->setUserDataPointer((void *)p);

        M_Free(oldPatch);

        file.unlock();
    }

    return uniqueId;
}

boolean R_GetPatchInfo(patchid_t id, patchinfo_t *info)
{
    LOG_AS("R_GetPatchInfo");
    if(!info) Con_Error("R_GetPatchInfo: Argument 'info' cannot be NULL.");

    memset(info, 0, sizeof(patchinfo_t));

    de::Texture *tex = reinterpret_cast<de::Texture*>(Textures_ToTexture(Textures_TextureForUniqueId(TS_PATCHES, id)));
    if(tex)
    {
        patchtex_t const *pTex = reinterpret_cast<patchtex_t *>(tex->userDataPointer());
        DENG_ASSERT(pTex);

        // Ensure we have up to date information about this patch.
        GL_PreparePatchTexture(reinterpret_cast<texture_s*>(tex));

        info->id = id;
        info->flags.isCustom = tex->isCustom();

        averagealpha_analysis_t *aa = reinterpret_cast<averagealpha_analysis_t *>(tex->analysisDataPointer(TA_ALPHA));
        info->flags.isEmpty = aa && FEQUAL(aa->alpha, 0);

        info->geometry.size.width  = tex->width();
        info->geometry.size.height = tex->height();
        info->geometry.origin.x = pTex->offX;
        info->geometry.origin.y = pTex->offY;
        /// @todo fixme: kludge:
        info->extraOffset[0] = info->extraOffset[1] = (pTex->flags & PF_UPSCALE_AND_SHARPEN)? -1 : 0;
        // Kludge end.
        return true;
    }

    if(id != 0)
    {
        LOG_DEBUG("Invalid Patch id #%i, returning false.") << id;
    }
    return false;
}

static void loadPatchNames(String lumpName)
{
    LOG_AS("loadPatchNames");

    // Clear any previously exisiting names.
    patchNames.clear();

    try
    {
        lumpnum_t lumpNum = App_FileSystem()->lumpNumForName(lumpName);
        de::File1 &file = App_FileSystem()->nameIndex().lump(lumpNum);

        if(file.size() < 4)
        {
            LOG_WARNING("File \"%s\" (#%i) does not appear to be valid PNAMES data.")
                << de::NativePath(file.composeUri().asText()).pretty() << lumpNum;
            return;
        }

        ByteRefArray lumpData = ByteRefArray(file.cache(), file.size());
        de::Reader from = de::Reader(lumpData);

        // The data begins with the total number of patch names.
        dint32 numNames;
        from >> numNames;

        // Followed by the names (eight character ASCII strings).
        if(numNames > 0)
        {
            if((unsigned) numNames > (file.size() - 4) / 8)
            {
                // The data appears to be truncated.
                LOG_WARNING("File \"%s\" (#%i) appears to be truncated (%u bytes, expected %u).")
                    << NativePath(file.composeUri().asText()).pretty() << lumpNum
                    << file.size() << (numNames * 8 + 4);

                // We'll only read this many names.
                numNames = (file.size() - 4) / 8;
            }

            // Read the names.
            for(int i = 0; i < numNames; ++i)
            {
                PatchName name;
                from >> name;
                patchNames.push_back(name);
            }
        }

        file.unlock();
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
}

static QList<de::File1 *> collectPatchCompositeDefinitionFiles()
{
    QList<de::File1 *> result;

    /*
     * Precedence order of definitions is defined by id tech1 which processes
     * the TEXTURE1/2 lumps in the following order:
     *
     * (last)TEXTURE2 > (last)TEXTURE1
     */
    LumpIndex const &index = App_FileSystem()->nameIndex();
    lumpnum_t firstTexLump  = App_FileSystem()->lumpNumForName("TEXTURE1");
    lumpnum_t secondTexLump = App_FileSystem()->lumpNumForName("TEXTURE2");

    /*
     * Also process all other lumps named TEXTURE1/2.
     */
    for(int i = 0; i < index.size(); ++i)
    {
        de::File1 &file = index.lump(i);

        // Will this be processed anyway?
        if(i == firstTexLump || i == secondTexLump) continue;

        String fileName = file.name().fileNameWithoutExtension();
        if(fileName.compareWithoutCase("TEXTURE1") &&
           fileName.compareWithoutCase("TEXTURE2")) continue;

        result.push_back(&file);
    }

    if(firstTexLump >= 0)
    {
        result.push_back(&index.lump(firstTexLump));
    }

    if(secondTexLump >= 0)
    {
        result.push_back(&index.lump(secondTexLump));
    }

    return result;
}

#pragma pack(1)
struct DoomTexturePatchDef
{
    int16_t originX;
    int16_t originY;
    int16_t patch;
    int16_t stepDir;
    int16_t colorMap;
};

struct DoomTextureDef
{
    byte name[8];
    int16_t unused;
    byte scale[2]; ///< [x, y] Used by ZDoom, div 8.
    int16_t width;
    int16_t height;
    int32_t columnDirectoryPadding;
    int16_t patchCount;
    DoomTexturePatchDef patches[1];
};
#pragma pack()

/**
 * Read DOOM and Strife format texture definitions from the specified lump.
 */
static patchcompositetex_t **readDoomTextureDefLump(de::File1 &lump, int *origIndexBase,
    bool firstNull, int *numDefs)
{
    DENG_ASSERT(origIndexBase);
    LOG_AS("readTextureDefs");

    patchcompositetex_t **result = 0; // The resulting set of validated definitions.

    LOG_VERBOSE("Processing \"%s:%s\"...")
        << NativePath(lump.container().composeUri().asText()).pretty()
        << NativePath(lump.composeUri().asText()).pretty();

    // Buffer the whole lump.
    int *maptex1 = (int *) M_Malloc(lump.size());
    if(!maptex1) Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for temporary copy of archived DOOM texture definitions.", (unsigned long) lump.size());

    lump.read((uint8_t *)maptex1, 0, lump.size());

    // The data begins with a count of the total number of definitions in the lump.
    int numTexDefs = LONG(*maptex1);

    byte *validTexDefs = (byte *) M_Calloc(sizeof(*validTexDefs) * numTexDefs);
    if(!validTexDefs) Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for valid texture record list.", (unsigned long) sizeof(*validTexDefs) * numTexDefs);

    short *texDefNumPatches = (short *) M_Calloc(sizeof(*texDefNumPatches) * numTexDefs);
    if(!texDefNumPatches) Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for texture patch count record list.", (unsigned long) sizeof(*texDefNumPatches) * numTexDefs);

    /**
     * Pass #1
     * Count total number of texture and patch defs we'll need and check
     * for missing patches and any other irregularities.
     */
    int numValidTexDefs = 0;
    size_t numValidPatchRefs = 0;
    int *directory = maptex1 + 1;
    for(int i = 0; i < numTexDefs; ++i, directory++)
    {
        size_t offset = LONG(*directory);
        if(offset > lump.size())
        {
            LOG_WARNING("Invalid offset %u for definition %i, ignoring.") << offset << i;
            continue;
        }

        ByteRefArray lump = ByteRefArray((byte const *) maptex1 + offset, lump.size());
        de::Reader from   = de::Reader(lump);
        PatchCompositeTexture texDef =
                gameDataFormat == 0? PatchCompositeTexture::fromDoomFormat(from) :
                                     PatchCompositeTexture::fromStrifeFormat(from);

        texDef.validate(patchNames);

        dint16 patchIdx = 0;
        DENG2_FOR_EACH_CONST(PatchCompositeTexture::Patches, it, texDef.patches())
        {
            PatchCompositeTexture::Patch const &patchDef = *it;
            if(patchDef.lumpNum >= 0)
            {
                ++texDefNumPatches[i];
                ++numValidPatchRefs;
            }
            else
            {
                LOG_WARNING("Missing patch #%i in definition \"%s\".")
                    << patchIdx << texDef.percentEncodedNameRef();
            }
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
        result = (patchcompositetex_t **) M_Malloc(sizeof(*result) * numValidTexDefs);
        directory = maptex1 + 1;
        size_t n = 0;
        for(int i = 0; i < numTexDefs; ++i, directory++)
        {
            if(!validTexDefs[i]) continue;

            size_t offset = LONG(*directory);
            patchcompositetex_t* texDef = 0;

            // Read and create the texture def.
            DoomTextureDef *mtexture = (DoomTextureDef *) ((byte *) maptex1 + offset);

            texDef = (patchcompositetex_t *) M_Malloc(sizeof(*texDef));
            if(!texDef) Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for new PatchComposite.", (unsigned long) sizeof *texDef);

            texDef->patchCount = texDefNumPatches[i];
            texDef->flags = 0;
            texDef->origIndex = (*origIndexBase) + i;

            Str_Init(&texDef->name);
            Str_PercentEncode(Str_StripRight(Str_PartAppend(&texDef->name, (char const *) mtexture->name, 0, 8)));

            texDef->size.width  = SHORT(mtexture->width);
            texDef->size.height = SHORT(mtexture->height);

            texDef->patches = (texpatch_t *) M_Malloc(sizeof(*texDef->patches) * texDef->patchCount);
            if(!texDef->patches) Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for new TexPatch list for texture definition '%s'.", (unsigned long) sizeof *texDef->patches * texDef->patchCount, Str_Text(&texDef->name));

            DoomTexturePatchDef *mpatch = &mtexture->patches[0];
            texpatch_t *patch  = texDef->patches;
            for(short j = 0; j < SHORT(mtexture->patchCount); ++j, mpatch++)
            {
                short pnamesIndex = SHORT(mpatch->patch);
                lumpnum_t lumpNum = patchNames[pnamesIndex].lumpNum();
                if(lumpNum < 0) continue;

                patch->offX = SHORT(mpatch->originX);
                patch->offY = SHORT(mpatch->originY);
                patch->lumpNum = lumpNum;
                patch++;
            }

            /**
             * Vanilla DOOM's implementation of the texture collection has a flaw which
             * results in the first texture being used dually as a "NULL" texture.
             */
            if(firstNull && i == 0)
            {
                texDef->flags |= TXDF_NODRAW;
            }

            /// Is this a custom texture?
            int j = 0;
            while(j < texDef->patchCount && !(texDef->flags & TXDF_CUSTOM))
            {
                de::File1 &file = App_FileSystem()->nameIndex().lump(texDef->patches[j].lumpNum);
                if(file.container().hasCustom())
                    texDef->flags |= TXDF_CUSTOM;
                else
                    j++;
            }

            // Add it to the list.
            result[n++] = texDef;
        }
    }

    *origIndexBase += numTexDefs;

    LOG_INFO("Loaded %s texture definitions from \"%s:%s\".")
        << (numValidTexDefs == numTexDefs? "all" : String("%1 of %1").arg(numValidTexDefs).arg(numTexDefs))
        << NativePath(lump.container().composeUri().asText()).pretty()
        << NativePath(lump.composeUri().asText()).pretty();

    // Free all temporary storage.
    M_Free(validTexDefs);
    M_Free(texDefNumPatches);
    M_Free(maptex1);

    if(numDefs) *numDefs = numValidTexDefs;

    return result;
}

/**
 * @brief Many PWADs include new TEXTURE1/2 lumps including the IWAD
 * texture definitions, with new definitions appended. In order to correctly
 * determine whether a defined texture originates from an IWAD we must
 * compare all definitions against those in the IWAD and if matching,
 * they should be considered as IWAD resources, even though the doomtexture
 * definition does not come from an IWAD lump.
 */
static patchcompositetex_t **loadPatchCompositeDefs(int *numDefs)
{
    LOG_AS("loadPatchCompositeDefs");

    // Load the patch names from the PNAMES lump.
    loadPatchNames("PNAMES");

    // If no patch names - there is no point continuing further.
    if(!patchNames.count())
    {
        if(numDefs) *numDefs = 0;
        return 0;
    }

    // Collect a list of all definition files we intend to process.
    QList<de::File1 *> defLumps = collectPatchCompositeDefinitionFiles();

    // Process the definition files.
    patchcompositetex_t **list = 0, **listCustom = 0, ***eList = 0;
    int count = 0, countCustom = 0, *eCount = 0;

    int origIndexBase = 0;
    bool firstNull = true;
    DENG2_FOR_EACH_CONST(QList<de::File1 *>, i, defLumps)
    {
        // Read in the new texture defs.
        de::File1 &lump = **i;
        int newNumTexDefs;
        patchcompositetex_t **newTexDefs = readDoomTextureDefLump(lump, &origIndexBase,
                                                                  firstNull, &newNumTexDefs);

        bool isCustom = lump.container().hasCustom();

        eList  = (!isCustom? &list  : &listCustom);
        eCount = (!isCustom? &count : &countCustom);
        if(*eList)
        {
            // Merge with the existing doomtexturedefs.
            patchcompositetex_t **newList = (patchcompositetex_t **) M_Malloc(sizeof(*newList) * ((*eCount) + newNumTexDefs));
            if(!newList) Con_Error("loadPatchCompositeDefs: Failed on allocation of %lu bytes for merged PatchComposite list.", (unsigned long) sizeof *newList * ((*eCount) + newNumTexDefs));

            int n = 0;
            for(int i = 0; i < *eCount; ++i)
            {
                newList[n++] = (*eList)[i];
            }
            for(int i = 0; i < newNumTexDefs; ++i)
            {
                newList[n++] = newTexDefs[i];
            }

            M_Free(*eList);
            M_Free(newTexDefs);

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

    int patchCompositeTexturesCount = 0;
    patchcompositetex_t **patchCompositeTextures = 0;

    if(listCustom)
    {
        // There are custom doomtexturedefs, cross compare with the IWAD
        // originals to see if they have been changed.
        int i = 0;
        while(i < count)
        {
            patchcompositetex_t *orig = list[i];
            bool hasReplacement = false;

            for(int j = 0; j < countCustom; ++j)
            {
                patchcompositetex_t *custom = listCustom[j];

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
                            texpatch_t *origP   = orig->patches   + k;
                            texpatch_t *customP = custom->patches + k;

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
                if(orig->patches) M_Free(orig->patches);
                M_Free(orig);
                if(i < count - 1)
                    memmove(list + i, list + i + 1, sizeof(*list) * (count - i - 1));
                count--;
            }
            else
            {
                i++;
            }
        }

        // List contains only non-replaced doomtexturedefs, merge them.
        patchcompositetex_t **newList = (patchcompositetex_t **) M_Malloc(sizeof(*newList) * (count + countCustom));
        if(!newList) Con_Error("loadPatchCompositeDefs: Failed on allocation of %lu bytes for the unique PatchComposite list.", (unsigned long) sizeof(*newList) * (count + countCustom));

        int n = 0;
        for(i = 0; i < count; ++i)
        {
            newList[n++] = list[i];
        }
        for(i = 0; i < countCustom; ++i)
        {
            newList[n++] = listCustom[i];
        }

        M_Free(list);
        M_Free(listCustom);

        patchCompositeTextures = newList;
        patchCompositeTexturesCount = count + countCustom;
    }
    else
    {
        patchCompositeTextures = list;
        patchCompositeTexturesCount = count;
    }

    if(numDefs) *numDefs = patchCompositeTexturesCount;
    return patchCompositeTextures;
}

/// @note Definitions for Textures that are not created successfully
///       will be pruned from the set.
static void createTexturesForPatchCompositeDefs(patchcompositetex_t **defs, int count)
{
    DENG_ASSERT(defs);
    LOG_AS("createTexturesForPatchCompositeDefs");

    de::Uri uri;
    uri.setScheme("Textures");

    for(int i = 0; i < count; ++i)
    {
        patchcompositetex_t *pcTex = defs[i];

        uri.setPath(Str_Text(&pcTex->name));

        textureid_t texId = Textures_Declare(reinterpret_cast<uri_s *>(&uri), pcTex->origIndex, NULL);
        if(texId == NOTEXTUREID) continue; // Invalid uri?

        if(de::Texture *tex = reinterpret_cast<de::Texture *>(Textures_ToTexture(texId)))
        {
            patchcompositetex_t *oldPcTex = reinterpret_cast<patchcompositetex_t *>(tex->userDataPointer());

            tex->flagCustom(!!(pcTex->flags & TXDF_CUSTOM));
            tex->setDimensions(pcTex->size);
            tex->setUserDataPointer((void*)pcTex);

            Str_Free(&oldPcTex->name);
            if(oldPcTex->patches) M_Free(oldPcTex->patches);
            M_Free(oldPcTex);
        }
        else if(!Textures_CreateWithDimensions(texId, !!(pcTex->flags & TXDF_CUSTOM), &pcTex->size, (void *)pcTex))
        {
            LOG_WARNING("Failed defining Texture for patch composite '%s', ignoring.") << Str_Text(&pcTex->name);
            Str_Free(&pcTex->name);
            if(pcTex->patches) M_Free(pcTex->patches);
            M_Free(pcTex);
            defs[i] = 0;
        }
    }
}

void R_InitPatchCompositeTextures()
{
    LOG_VERBOSE("Initializing PatchComposite textures...");
    uint usedTime = Timer_RealMilliseconds();

    // Load texture definitions from TEXTURE1/2 lumps.
    int defsCount;
    patchcompositetex_t **defs = loadPatchCompositeDefs(&defsCount);
    if(defs)
    {
        createTexturesForPatchCompositeDefs(defs, defsCount);
        M_Free(defs);
    }

    LOG_INFO(de::String("R_InitPatchComposites: Done in %1 seconds.").arg(double((Timer_RealMilliseconds() - usedTime) / 1000.0f), 0, 'g', 2));
}

static inline de::Uri composeFlatUri(String percentEncodedPath)
{
    return de::Uri(Path(percentEncodedPath.fileNameWithoutExtension())).setScheme("Flats");
}

/**
 * Compose the path to the data resource.
 * @note We do not use the lump name, instead we use the logical lump index
 * in the global LumpIndex. This is necessary because of the way id tech 1
 * manages flat references in animations (intermediate frames are chosen by their
 * 'original indices' rather than by name).
 */
static inline de::Uri composeFlatResourceUrn(lumpnum_t lumpNum)
{
    return de::Uri(Path(String("%1").arg(lumpNum))).setScheme("LumpDir");
}

void R_InitFlatTextures()
{
    LOG_VERBOSE("Initializing Flat textures...");
    uint usedTime = Timer_RealMilliseconds();

    LumpIndex const &index = App_FileSystem()->nameIndex();
    lumpnum_t firstFlatMarkerLumpNum = index.firstIndexForPath(Path("F_START.lmp"));
    if(firstFlatMarkerLumpNum >= 0)
    {
        lumpnum_t lumpNum;
        de::File1 *blockFile = 0;
        for(lumpNum = index.size(); lumpNum --> firstFlatMarkerLumpNum + 1;)
        {
            de::File1 &lump = index.lump(lumpNum);
            String percentEncodedName = lump.name().fileNameWithoutExtension();
            de::File1 *containerFile = &lump.container();

            if(blockFile && blockFile != containerFile)
            {
                blockFile = 0;
            }

            if(!blockFile)
            {
                if(!percentEncodedName.compareWithoutCase("F_END") ||
                   !percentEncodedName.compareWithoutCase("FF_END"))
                {
                    blockFile = containerFile;
                }
                continue;
            }

            if(!percentEncodedName.compareWithoutCase("F_START"))
            {
                blockFile = 0;
                continue;
            }

            // Ignore extra marker lumps.
            if(!percentEncodedName.compareWithoutCase("FF_START") ||
               !percentEncodedName.compareWithoutCase("F_END")    ||
               !percentEncodedName.compareWithoutCase("FF_END")) continue;

            de::Uri uri = composeFlatUri(percentEncodedName);
            if(Textures_ResolveUri2(reinterpret_cast<uri_s *>(&uri), true/*quiet please*/) == NOTEXTUREID) // A new flat?
            {
                /**
                 * Kludge Assume 64x64 else when the flat is loaded it will inherit the
                 * dimensions of the texture, which, if it has been replaced with a hires
                 * version - will be much larger than it should be.
                 *
                 * @todo Always determine size from the lowres original.
                 */
                Size2Raw const size = Size2Raw(64, 64);
                int const uniqueId = lumpNum - (firstFlatMarkerLumpNum + 1);
                de::Uri resourcePath = composeFlatResourceUrn(lumpNum);
                textureid_t texId = Textures_Declare(reinterpret_cast<uri_s *>(&uri), uniqueId,
                                                     reinterpret_cast<uri_s *>(&resourcePath));
                if(!Textures_CreateWithDimensions(texId, lump.hasCustom(), &size, NULL))
                {
                    LOG_WARNING("Failed defining Texture for new flat \"%s\", ignoring.")
                        << NativePath(uri.asText()).pretty();
                }
            }
        }
    }

    LOG_INFO(String("R_InitFlatTetxures: Done in %1 seconds.").arg(double((Timer_RealMilliseconds() - usedTime) / 1000.0f), 0, 'g', 2));
}

void R_DefineSpriteTexture(textureid_t texId)
{
    LOG_AS("R_DefineSpriteTexture");

    // Have we already encountered this name?
    de::Texture *tex = reinterpret_cast<de::Texture *>(Textures_ToTexture(texId));
    if(!tex)
    {
        // A new sprite texture.
        patchtex_t *pTex = (patchtex_t *) M_Malloc(sizeof(*pTex));
        if(!pTex) Con_Error("R_InitSpriteTextures: Failed on allocation of %lu bytes for new PatchTex.", (unsigned long) sizeof(*pTex));
        pTex->offX = pTex->offY = 0; // Deferred until texture load time.

        tex = reinterpret_cast<de::Texture *>(Textures_Create(texId, 0, (void *)pTex));
        if(!tex)
        {
            M_Free(pTex);

            de::Uri *uri = reinterpret_cast<de::Uri *>(Textures_ComposeUri(texId));
            LOG_WARNING("Failed to define Texture for sprite \"%s\", ignoring.")
                << NativePath(uri->asText()).pretty();
            delete uri;
        }
    }

    de::Uri const *resourceUri = reinterpret_cast<de::Uri const *>(Textures_ResourcePath(texId));
    if(!tex || !resourceUri) return;

    try
    {
        String const &resourcePath = resourceUri->resolvedRef();
        lumpnum_t lumpNum = App_FileSystem()->nameIndex().lastIndexForPath(resourcePath);
        de::File1& file = App_FileSystem()->nameIndex().lump(lumpNum);

        ByteRefArray fileData = ByteRefArray(file.cache(), file.size());
        de::Reader from = de::Reader(fileData);
        PatchHeader patchHdr;
        from >> patchHdr;

        tex->setDimensions(patchHdr.dimensions);
        tex->flagCustom(file.hasCustom());

        file.unlock();
    }
    catch(LumpIndex::NotFoundError const &)
    {} // Ignore this error.
}

static int defineSpriteTextureWorker(textureid_t texId, void *)
{
    R_DefineSpriteTexture(texId);
    return 0; // Continue iteration.
}

/// @todo Defer until necessary (sprite is first de-referenced).
static void defineAllSpriteTextures()
{
    Textures_IterateDeclared(TS_SPRITES, defineSpriteTextureWorker);
}

/// Returns @c true iff @a name is a well-formed sprite name.
static bool validateSpriteName(String name)
{
    if(name.length() < 6) return false;

    // Character at position 5 must be a number [0..8)
    int frameOrRotationNumber = name.at(5).digitValue();
    if(frameOrRotationNumber < 0 || frameOrRotationNumber > 8) return false;

    // If defined, the character at position 7 must be a number [0..8)
    if(name.length() >= 7)
    {
        int rotationNumber = name.at(7).digitValue();
        if(rotationNumber < 0 || rotationNumber > 8) return false;
    }

    return true;
}

/**
 * Compose the path to the data resource.
 * @note We do not use the lump name, instead we use the logical lump index
 * in the global LumpIndex. This is necessary because of the way id tech 1
 * manages patch references.
 */
#if 0
static inline de::Uri composeSpriteResourceUrn(lumpnum_t lumpNum)
{
    return de::Uri(Path(String("%1").arg(lumpNum))).setScheme("LumpDir");
}
#endif

void R_InitSpriteTextures()
{
    LOG_VERBOSE("Initializing Sprite textures...");
    uint usedTime = Timer_RealMilliseconds();

    int uniqueId = 1/*1-based index*/;

    /// @todo fixme: Order here does not respect id tech1 logic.
    ddstack_t *stack = Stack_New();

    LumpIndex const &index = App_FileSystem()->nameIndex();
    for(int i = 0; i < index.size(); ++i)
    {
        de::File1 &file = index.lump(i);
        String fileName = file.name().fileNameWithoutExtension();
        textureid_t texId;

        if(fileName.beginsWith('S', Qt::CaseInsensitive) && fileName.length() >= 5)
        {
            if(fileName.endsWith("_START", Qt::CaseInsensitive))
            {
                // We've arrived at *a* sprite block.
                Stack_Push(stack, NULL);
                continue;
            }

            if(fileName.endsWith("_END", Qt::CaseInsensitive))
            {
                // The sprite block ends.
                Stack_Pop(stack);
                continue;
            }
        }

        if(!Stack_Height(stack)) continue;

        String decodedFileName = QString(QByteArray::fromPercentEncoding(fileName.toUtf8()));
        if(!validateSpriteName(decodedFileName))
        {
            LOG_WARNING("'%s' is not a valid sprite name, ignoring.") << decodedFileName;
            continue;
        }

        // Compose the resource name.
        de::Uri uri = de::Uri(Path(fileName)).setScheme("Sprites");

        // Compose the data resource path.
        //de::Uri resourcePath = composeSpriteResourceUrn(i);
        de::Uri resourcePath = de::Uri(Path(fileName)).setScheme("Lumps");

        texId = Textures_Declare(reinterpret_cast<uri_s *>(&uri), uniqueId,
                                 reinterpret_cast<uri_s *>(&resourcePath));
        if(texId == NOTEXTUREID)
        {
            continue; // Invalid uri?
        }

        uniqueId++;
    }

    while(Stack_Height(stack))
    { Stack_Pop(stack); }

    Stack_Delete(stack);

    // Define any as yet undefined sprite textures.
    defineAllSpriteTextures();

    LOG_INFO(String("R_InitSpriteTextures: Done in %1 seconds.").arg(double((Timer_RealMilliseconds() - usedTime) / 1000.0f), 0, 'g', 2));
}

texture_s *R_CreateSkinTex(uri_s const *filePath, boolean isShinySkin)
{
    if(!filePath || Str_IsEmpty(Uri_Path(filePath))) return 0;

    LOG_AS("R_CreateSkinTex");

    // Have we already created one for this?
    texture_s *tex;
    if(!isShinySkin)
    {
        tex = Textures_TextureForResourcePath(TS_MODELSKINS, filePath);
    }
    else
    {
        tex = Textures_TextureForResourcePath(TS_MODELREFLECTIONSKINS, filePath);
    }
    if(tex) return tex;

    int uniqueId = Textures_Count(isShinySkin? TS_MODELREFLECTIONSKINS : TS_MODELSKINS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed to create ModelSkin (max:%i), ignoring.") << DDMAXINT;
        return 0;
    }

    de::Uri uri = de::Uri(Path(String("%1").arg(uniqueId, 8, 10, QChar('0'))));
    uri.setScheme((isShinySkin? "ModelReflectionSkins" : "ModelSkins"));

    textureid_t texId = Textures_Declare(reinterpret_cast<uri_s *>(&uri), uniqueId, filePath);
    if(texId == NOTEXTUREID) return 0; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        // Create a texture for it.
        tex = Textures_Create(texId, true/*is-custom*/, NULL);
        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for ModelSkin \"%s\", ignoring.")
                << NativePath(uri.asText()).pretty();
            return 0;
        }
    }

    return tex;
}

texture_s *R_CreateDetailTextureFromDef(ded_detailtexture_t const *def)
{
    LOG_AS("R_CreateDetailTextureFromDef");

    if(!def->detailTex || Uri_IsEmpty(def->detailTex)) return 0;

    // Have we already created one for this?
    texture_s *tex = Textures_TextureForResourcePath(TS_DETAILS, def->detailTex);
    if(tex) return tex;

    int uniqueId = Textures_Count(TS_DETAILS) + 1; // 1-based index.
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed creating new detail texture (max:%i).") << DDMAXINT;
        return 0;
    }

    de::Uri uri = de::Uri(Path(String("%1").arg(uniqueId, 8, 10, QChar('0')))).setScheme("Details");
    textureid_t texId = Textures_Declare(reinterpret_cast<uri_s *>(&uri), uniqueId, def->detailTex);
    if(texId == NOTEXTUREID) return 0; // Invalid URI?

    tex = Textures_ToTexture(texId);
    if(!tex && !Textures_Create(texId, true/*is-custom*/, 0))
    {
        LOG_WARNING("Failed defining Texture for detail texture \"%s\", ignoring.")
            << NativePath(uri.asText()).pretty();
        return 0;
    }

    return tex;
}

texture_s *R_CreateLightMap(uri_s const *resourcePath)
{
    LOG_AS("R_CreateLightMap");

    if(!resourcePath || Uri_IsEmpty(resourcePath)) return 0;
    if(!Str_CompareIgnoreCase(Uri_Path(resourcePath), "-")) return 0;

    // Have we already created one for this?
    texture_s *tex = Textures_TextureForResourcePath(TS_LIGHTMAPS, resourcePath);
    if(tex) return tex;

    int uniqueId = Textures_Count(TS_LIGHTMAPS) + 1; // 1-based index.
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed declaring new lightmap (max:%i), ignoring.") << DDMAXINT;
        return 0;
    }

    de::Uri uri = de::Uri(Path(String("%1").arg(uniqueId, 8, 10, QChar('0')))).setScheme("Lightmaps");
    textureid_t texId = Textures_Declare(reinterpret_cast<uri_s *>(&uri), uniqueId, resourcePath);
    if(texId == NOTEXTUREID) return 0; // Invalid URI?

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        // Create a texture for it.
        tex = Textures_Create(texId, true/*is-custom*/, 0);
        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for lightmap \"%s\", ignoring.")
                << NativePath(uri.asText()).pretty();
            return 0;
        }
    }
    return tex;
}

texture_s *R_CreateFlareTexture(uri_s const *resourcePath)
{
    LOG_AS("R_CreateFlareTexture");

    if(!resourcePath || Uri_IsEmpty(resourcePath)) return 0;
    if(!Str_CompareIgnoreCase(Uri_Path(resourcePath), "-")) return 0;

    // Perhaps a "built-in" flare texture id?
    // Try to convert the id to a system flare tex constant idx
    if(Str_At(Uri_Path(resourcePath), 0) >= '0' && Str_At(Uri_Path(resourcePath), 0) <= '4' &&
      !Str_At(Uri_Path(resourcePath), 1))
        return 0;

    // Have we already created one for this?
    texture_s *tex = Textures_TextureForResourcePath(TS_FLAREMAPS, resourcePath);
    if(tex) return tex;

    int uniqueId = Textures_Count(TS_FLAREMAPS) + 1; // 1-based index.
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed declaring new flare texture (max:%i), ignoring.") << DDMAXINT;
        return 0;
    }

    // Create a texture for it.
    de::Uri uri = de::Uri(Path(String("%1").arg(uniqueId, 8, 10, QChar('0')))).setScheme("Flaremaps");
    textureid_t texId = Textures_Declare(reinterpret_cast<uri_s *>(&uri), uniqueId, resourcePath);
    if(texId == NOTEXTUREID) return 0; // Invalid URI?

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        tex = Textures_Create(texId, true/*is-custom*/, 0);
        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for flare texture \"%s\", ignoring.")
                << NativePath(uri.asText()).pretty();
            return 0;
        }
    }
    return tex;
}

texture_s *R_CreateReflectionTexture(uri_s const *resourcePath)
{
    LOG_AS("R_CreateReflectionTexture");

    if(!resourcePath || Uri_IsEmpty(resourcePath)) return 0;

    // Have we already created one for this?
    texture_s *tex = Textures_TextureForResourcePath(TS_REFLECTIONS, resourcePath);
    if(tex) return tex;

    int uniqueId = Textures_Count(TS_REFLECTIONS) + 1; // 1-based index.
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed declaring new shiny texture (max:%i), ignoring.") << DDMAXINT;
        return 0;
    }

    de::Uri uri = de::Uri(Path(String("%1").arg(uniqueId, 8, 10, QChar('0')))).setScheme("Reflections");
    textureid_t texId = Textures_Declare(reinterpret_cast<uri_s *>(&uri), uniqueId, resourcePath);
    if(texId == NOTEXTUREID) return 0; // Invalid URI?

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        // Create a texture for it.
        tex = Textures_Create(texId, true/*is-custom*/, 0);
        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for shiny texture \"%s\", ignoring.")
                << NativePath(uri.asText()).pretty();
            return 0;
        }
    }

    return tex;
}

texture_s *R_CreateMaskTexture(uri_s const *resourcePath, Size2Raw const *size)
{
    LOG_AS("R_CreateMaskTexture");

    if(!resourcePath || Uri_IsEmpty(resourcePath)) return 0;

    // Have we already created one for this?
    texture_s *tex = Textures_TextureForResourcePath(TS_MASKS, resourcePath);
    if(tex) return tex;

    int uniqueId = Textures_Count(TS_MASKS) + 1; // 1-based index.
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed declaring mask texture (max:%i), ignoring.") << DDMAXINT;
        return 0;
    }

    de::Uri uri = de::Uri(Path(String("%1").arg(uniqueId, 8, 10, QChar('0')))).setScheme("Masks");
    textureid_t texId = Textures_Declare(reinterpret_cast<uri_s *>(&uri), uniqueId, resourcePath);
    if(texId == NOTEXTUREID) return 0; // Invalid URI?

    tex = Textures_ToTexture(texId);
    if(tex)
    {
        Texture_SetDimensions(tex, size);
    }
    else
    {
        // Create a texture for it.
        tex = Textures_CreateWithDimensions(texId, true/*is-custom*/, size, 0);
        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for mask texture \"%s\", ignoring.")
                << NativePath(uri.asText()).pretty();
            return 0;
        }
    }

    return tex;
}
