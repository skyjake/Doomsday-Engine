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
#include <QMap>
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

typedef QList<CompositeTexture*> CompositeTextures;

/**
 * Reads patch composite texture definitions from @a file.
 *
 * @param data  The data buffer to be read.
 * @param origIndexBase  Base value for the "original index" logic.
 * @param archiveCount  Will be updated with the total number of definitions
 *                      in the file (which may not necessarily equal the total
 *                      number of definitions which are actually read).
 */
static CompositeTextures readCompositeTextureDefs(IByteArray &data,
    int origIndexBase, int& archiveCount)
{
    LOG_AS("readCompositeTextureDefs");

    CompositeTextures result; ///< The resulting set of validated definitions.

    // The game data format determines the format of the archived data.
    CompositeTexture::ArchiveFormat format =
            (gameDataFormat == 0? CompositeTexture::DoomFormat : CompositeTexture::StrifeFormat);

    de::Reader reader = de::Reader(data);

    // First is a count of the total number of definitions.
    dint32 definitionCount;
    reader >> definitionCount;

    // Next is directory of offsets to the definitions.
    typedef QMap<dint32, int> Offsets;
    Offsets offsets;
    for(int i = 0; i < definitionCount; ++i)
    {
        dint32 offset;
        reader >> offset;

        // Ensure the offset is within valid range.
        if(offset < 0 || (unsigned) offset < definitionCount * sizeof(offset) ||
           offset > reader.source()->size())
        {
            LOG_WARNING("Invalid offset %i for definition #%i, ignoring.") << offset << i;
        }
        else
        {
            offsets.insert(offset, origIndexBase + i);
        }
    }

    // Seek to each offset and deserialize the definition.
    DENG2_FOR_EACH_CONST(Offsets, i, offsets)
    {
        // Read the next definition.
        reader.setOffset(i.key());
        CompositeTexture* def = CompositeTexture::constructFrom(reader, patchNames, format);

        // Attribute the "original index".
        def->setOrigIndex(i.value());

        bool validated = true;
        DENG2_FOR_EACH_CONST(CompositeTexture::Components, it, def->components())
        {
            if(it->lumpNum() >= 0)
            {
                // The composite contains at least one known component image
                // it is therefore valid.
                validated = true;
            }
        }

        if(validated)
        {
            // Add it to the list.
            result.push_back(def);
            continue;
        }

        delete def;
    }

    archiveCount = definitionCount;
    return result;
}

/**

 */
static CompositeTextures loadCompositeTextureDefs()
{
    LOG_AS("loadCompositeTextureDefs");

    // Load the patch names from the PNAMES lump.
    loadPatchNames("PNAMES");

    // If no patch names - there is no point continuing further.
    if(!patchNames.count()) return CompositeTextures();

    // Collate an ordered list of all the definition files we intend to process.
    QList<de::File1 *> defFiles = collectPatchCompositeDefinitionFiles();

    /**
     * Definitions are read into two discreet sets.
     *
     * Older add-ons contain copies of the original games' texture definitions,
     * with their own new definitions appended on the end. However, Doomsday needs
     * to classify all definitions according to whether they originate from the
     * original game data. To achieve the correct user-expected results, we must
     * compare each definition originating from an add-on to determine whether it
     * should instead be classified as "original" data.
     */
    CompositeTextures defs, customDefs;

    // Process each definition file.
    int origIndexBase = 0;
    DENG2_FOR_EACH_CONST(QList<de::File1 *>, i, defFiles)
    {
        de::File1 &file = **i;

        LOG_VERBOSE("Processing \"%s:%s\"...")
            << NativePath(file.container().composeUri().asText()).pretty()
            << NativePath(file.composeUri().asText()).pretty();

        // Buffer the file.
        ByteRefArray dataBuffer = ByteRefArray(file.cache(), file.size());

        // Read the next set of definitions.
        int archiveCount;
        CompositeTextures newDefs = readCompositeTextureDefs(dataBuffer, origIndexBase, archiveCount);

        // We have now finished with this file.
        file.unlock();

        // In which set do these belong?
        CompositeTextures* existingDefs =
                (file.container().hasCustom()? &customDefs : &defs);

        if(!existingDefs->isEmpty())
        {
            // Merge with the existing definitions.
            existingDefs->append(newDefs);
        }
        else
        {
            *existingDefs = newDefs;
        }

        // Maintain the original index.
        origIndexBase += archiveCount;

        // Print a summary.
        LOG_INFO("Loaded %s texture definitions from \"%s:%s\".")
            << (newDefs.count() == archiveCount? "all" : String("%1 of %1").arg(newDefs.count()).arg(archiveCount))
            << NativePath(file.container().composeUri().asText()).pretty()
            << NativePath(file.composeUri().asText()).pretty();
    }

    if(customDefs.count())
    {
        // Custom definitions were found - we must cross compare them.
        for(int i = 0; i < defs.count(); ++i)
        {
            CompositeTexture *orig = defs[i];
            bool hasReplacement = false;

            for(int j = 0; j < customDefs.count(); ++j)
            {
                CompositeTexture *custom = customDefs[j];

                if(!orig->percentEncodedName().compareWithoutCase(custom->percentEncodedName()))
                {
                    // Definition 'custom' is destined to replace 'orig'.
                    if(custom->flags().testFlag(CompositeTexture::Custom))
                    {
                        hasReplacement = true; // Uses a custom patch.
                    }
                    // Do the definitions differ?
                    else if(custom->height()      != orig->height() ||
                            custom->width()       != orig->width()  ||
                            custom->componentCount()  != orig->componentCount())
                    {
                        custom->flags() |= CompositeTexture::Custom;
                        hasReplacement = true;
                    }
                    else
                    {
                        // Check the patches.
                        short k = 0;
                        while(k < orig->componentCount() && !custom->flags().testFlag(CompositeTexture::Custom))
                        {
                            CompositeTexture::Component const &origP   = orig->components()[k];
                            CompositeTexture::Component const &customP = custom->components()[k];

                            if(origP.lumpNum() != customP.lumpNum() &&
                               origP.xOrigin() != customP.xOrigin() &&
                               origP.yOrigin() != customP.yOrigin())
                            {
                                custom->flags() |= CompositeTexture::Custom;
                                hasReplacement = true;
                            }
                            else
                            {
                                k++;
                            }
                        }
                    }

                    // The non-drawable flag must pass to the replacement.
                    if(hasReplacement && orig->flags().testFlag(CompositeTexture::NoDraw))
                    {
                        custom->flags() |= CompositeTexture::NoDraw;
                    }
                    break;
                }
            }

            if(hasReplacement)
            {
                // Let the PWAD "copy" override the IWAD original.
                defs.takeAt(i);
                delete orig;

                --i; // Process the current item again.
            }
        }

        /*
         * List now contains only those definitions which are not superceeded
         * by those in the custom list.
         */

        // Add definitions from the custom list to the end of the main set.
        defs.append(customDefs);
    }

    return defs;
}

/**
 * @param defs  Definitions to be processed.
 */
static void processCompositeTextureDefs(CompositeTextures &defs)
{
    LOG_AS("processCompositeTextureDefs");
    bool isFirst = true;
    while(!defs.isEmpty())
    {
        CompositeTexture &def = *defs.takeFirst();

        de::Uri uri = de::Uri(Path(def.percentEncodedName())).setScheme("Textures");
        textureid_t texId = Textures_Declare(reinterpret_cast<uri_s *>(&uri), def.origIndex(), 0);
        if(texId != NOTEXTUREID)
        {
            /*
             * Vanilla DOOM's implementation of the texture collection has a flaw
             * which results in the first texture being used dually as a "NULL"
             * texture.
             */
            if(isFirst)
            {
                def.flags() |= CompositeTexture::NoDraw;
                isFirst = false;
            }

            // Are we redefining an existing texture?
            if(de::Texture *tex = reinterpret_cast<de::Texture *>(
                                  Textures_ToTexture(texId)))
            {
                // Yes. Destroy the existing definition (*should* exist).
                CompositeTexture *oldDef = reinterpret_cast<CompositeTexture *>(tex->userDataPointer());
                if(oldDef) delete oldDef;

                // Reconfigure and attach the new definition.
                tex->flagCustom(def.flags().testFlag(CompositeTexture::Custom));
                tex->setDimensions(def.dimensions());
                tex->setUserDataPointer((void *)&def);
                continue;
            }

            // A new texture.
            if(Textures_CreateWithDimensions(texId, def.flags().testFlag(CompositeTexture::Custom),
                                             &def.dimensions(), (void *)&def))
            {
                continue;
            }
        }

        LOG_WARNING("Failed defining Texture for patch composite \"%s\", ignoring.")
            << NativePath(uri.asText()).pretty();

        delete &def;
    }
}

void R_InitCompositeTextures()
{
    LOG_VERBOSE("Initializing PatchComposite textures...");
    uint usedTime = Timer_RealMilliseconds();

    // Load texture definitions from TEXTURE1/2 lumps.
    processCompositeTextureDefs(loadCompositeTextureDefs());

    LOG_INFO(String("R_InitPatchComposites: Done in %1 seconds.").arg(double((Timer_RealMilliseconds() - usedTime) / 1000.0f), 0, 'g', 2));
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
