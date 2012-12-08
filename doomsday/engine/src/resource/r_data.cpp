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
#include <de/Time>
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

/**
 * Compose the path to the data resource.
 * @note We do not use the lump name, instead we use the logical lump index
 * in the global LumpIndex. This is necessary because of the way id tech 1
 * manages graphic references in animations (intermediate frames are chosen
 * by their 'original indices' rather than by name).
 */
static inline de::Uri composeLumpIndexResourceUrn(lumpnum_t lumpNum)
{
    return de::Uri("LumpDir", Path(String("%1").arg(lumpNum)));
}

void R_InitSystemTextures()
{
    LOG_AS("R_InitSystemTextures");

    static String const names[] =
    {   "unknown", "missing", "bbox", "gray", "" };

    LOG_VERBOSE("Initializing System textures...");
    Textures &textures = *App_Textures();

    for(uint i = 0; !names[i].isEmpty(); ++i)
    {
        Path path = names[i];
        de::Uri uri("System", path);
        de::Uri resourcePath("Graphics", path);

        TextureManifest *manifest = textures.declare(uri, i + 1/*1-based index*/, &resourcePath);
        if(!manifest) continue; // Invalid uri?

        // Have we defined this yet?
        if(manifest->texture()) continue;

        if(!manifest->derive(de::Texture::Custom))
        {
            LOG_WARNING("Failed to define Texture for system texture \"%s\".") << uri;
        }
    }
}

/// @note Part of the Doomsday public API.
AutoStr *R_ComposePatchPath(patchid_t id)
{
    try
    {
        TextureManifest &manifest = App_Textures()->scheme("Patches").findByUniqueId(id);
        return AutoStr_FromTextStd(manifest.path().toUtf8().constData());
    }
    catch(Textures::Scheme::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    return AutoStr_NewStd();
}

/// @note Part of the Doomsday public API.
uri_s *R_ComposePatchUri(patchid_t id)
{
    try
    {
        TextureManifest &manifest = App_Textures()->scheme("Patches").findByUniqueId(id);
        return reinterpret_cast<uri_s *>(new de::Uri(manifest.composeUri()));
    }
    catch(Textures::Scheme::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    return reinterpret_cast<uri_s *>(new de::Uri());
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

    Textures &textures = *App_Textures();

    // WAD format allows characters not normally permitted in native paths.
    // To achieve uniformity we apply a percent encoding to the "raw" names.
    de::Uri uri("Patches", Path(QString(QByteArray(name, qstrlen(name)).toPercentEncoding())));

    // Already defined as a patch?
    try
    {
        TextureManifest &manifest = textures.find(uri);
        /// @todo We should instead define Materials from patches and return the material id.
        return patchid_t( manifest.uniqueId() );
    }
    catch(Textures::NotFoundError const &)
    {} // Ignore this error.

    Path lumpPath = uri.path() + ".lmp";
    lumpnum_t lumpNum = App_FileSystem()->nameIndex().lastIndexForPath(lumpPath);
    if(lumpNum < 0)
    {
        LOG_WARNING("Failed to locate lump for \"%s\", ignoring.") << uri;
        return 0;
    }

    // Compose the path to the data resource.
    de::File1 &file = App_FileSystem()->nameIndex().lump(lumpNum);
    de::Uri resourceUri = composeLumpIndexResourceUrn(lumpNum);

    int uniqueId = textures.scheme("Patches").count() + 1; // 1-based index.
    TextureManifest *manifest = textures.declare(uri, uniqueId, &resourceUri);
    if(!manifest) return 0; // Invalid uri?

    de::Texture::Flags flags;
    if(file.container().hasCustom()) flags |= de::Texture::Custom;

    // Take a copy of the current patch loading state so that future texture
    // loads will produce the same results.
    if(monochrome)               flags |= de::Texture::Monochrome;
    if(upscaleAndSharpenPatches) flags |= de::Texture::UpscaleAndSharpen;

    /// @todo fixme: Ensure this is in Patch format.
    ByteRefArray fileData = ByteRefArray(file.cache(), file.size());
    de::Reader from = de::Reader(fileData);
    PatchHeader patchHdr;
    from >> patchHdr;

    file.unlock();

    de::Texture *tex = manifest->texture();
    if(!tex)
    {
        tex = manifest->derive(patchHdr.dimensions, flags);
        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for Patch texture \"%s\".") << uri;
            return 0;
        }

        tex->setOrigin(QPoint(-patchHdr.origin.x(), -patchHdr.origin.y()));
    }
    else
    {
        tex->setOrigin(QPoint(-patchHdr.origin.x(), -patchHdr.origin.y()));
        tex->setDimensions(patchHdr.dimensions);
        tex->flags() = flags;
    }

    return uniqueId;
}

boolean R_GetPatchInfo(patchid_t id, patchinfo_t *info)
{
    LOG_AS("R_GetPatchInfo");
    if(!info) Con_Error("R_GetPatchInfo: Argument 'info' cannot be NULL.");

    memset(info, 0, sizeof(patchinfo_t));

    if(!id) return false;

    try
    {
        de::Texture *tex = App_Textures()->scheme("Patches").findByUniqueId(id).texture();
        if(!tex) return false;

        // Ensure we have up to date information about this patch.
        GL_PreparePatchTexture(reinterpret_cast<texture_s*>(tex));

        info->id = id;
        info->flags.isCustom = tex->flags().testFlag(de::Texture::Custom);

        averagealpha_analysis_t *aa = reinterpret_cast<averagealpha_analysis_t *>(tex->analysisDataPointer(TA_ALPHA));
        info->flags.isEmpty = aa && FEQUAL(aa->alpha, 0);

        info->geometry.size.width  = tex->width();
        info->geometry.size.height = tex->height();

        info->geometry.origin.x = tex->origin().x();
        info->geometry.origin.y = tex->origin().y();

        /// @todo fixme: kludge:
        info->extraOffset[0] = info->extraOffset[1] = (tex->flags().testFlag(de::Texture::UpscaleAndSharpen)? -1 : 0);
        // Kludge end.
        return true;
    }
    catch(de::Textures::Scheme::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
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
                << NativePath(file.composeUri().asText()).pretty() << lumpNum;
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
           (dsize) offset > reader.source()->size())
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

        // If the composite contains at least one known component image it is
        // considered valid and we will therefore produce a Texture for it.
        DENG2_FOR_EACH_CONST(CompositeTexture::Components, it, def->components())
        {
            if(it->lumpNum() >= 0)
            {
                // Its valid - include in the result.
                result.push_back(def);
                def = 0;
                break;
            }
        }

        // Failed to validate? Dump it.
        if(def) delete def;
    }

    archiveCount = definitionCount;
    return result;
}

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
                    else if(custom->height()          != orig->height() ||
                            custom->width()           != orig->width()  ||
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
    Textures &textures = *App_Textures();

    bool isFirst = true;
    while(!defs.isEmpty())
    {
        CompositeTexture &def = *defs.takeFirst();
        de::Uri uri("Textures", Path(def.percentEncodedName()));

        TextureManifest *manifest = textures.declare(uri, def.origIndex(), 0);
        if(manifest)
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

            de::Texture::Flags flags;
            if(def.flags().testFlag(CompositeTexture::Custom)) flags |= de::Texture::Custom;

            // Are we redefining an existing texture?
            if(de::Texture *tex = manifest->texture())
            {
                // Yes. Destroy the existing definition (*should* exist).
                CompositeTexture *oldDef = reinterpret_cast<CompositeTexture *>(tex->userDataPointer());
                if(oldDef)
                {
                    tex->setUserDataPointer(0);
                    delete oldDef;
                }

                // Reconfigure and attach the new definition.
                tex->flags() = flags;
                tex->setDimensions(def.dimensions());
                tex->setUserDataPointer((void *)&def);

                continue;
            }
            // A new texture.
            else if(de::Texture *tex = manifest->derive(def.dimensions(), flags))
            {
                tex->setUserDataPointer((void *)&def);
                continue;
            }
        }

        LOG_WARNING("Failed defining Texture for patch composite \"%s\", ignoring.") << uri;
        delete &def;
    }
}

void R_InitCompositeTextures()
{
    de::Time begunAt;

    LOG_VERBOSE("Initializing PatchComposite textures...");

    // Load texture definitions from TEXTURE1/2 lumps.
    CompositeTextures texs = loadCompositeTextureDefs();
    processCompositeTextureDefs(texs);

    DENG_ASSERT(texs.isEmpty());

    LOG_INFO(String("R_InitCompositeTextures: Done in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

static inline de::Uri composeFlatUri(String percentEncodedPath)
{
    return de::Uri("Flats", Path(percentEncodedPath.fileNameWithoutExtension()));
}

void R_InitFlatTextures()
{
    de::Time begunAt;

    LOG_VERBOSE("Initializing Flat textures...");
    Textures &textures = *App_Textures();

    LumpIndex const &index = App_FileSystem()->nameIndex();
    lumpnum_t firstFlatMarkerLumpNum = index.firstIndexForPath(Path("F_START.lmp"));
    if(firstFlatMarkerLumpNum >= 0)
    {
        lumpnum_t lumpNum;
        de::File1 *blockContainer = 0;
        for(lumpNum = index.size(); lumpNum --> firstFlatMarkerLumpNum + 1;)
        {
            de::File1 &file = index.lump(lumpNum);
            String percentEncodedName = file.name().fileNameWithoutExtension();

            if(blockContainer && blockContainer != &file.container())
            {
                blockContainer = 0;
            }

            if(!blockContainer)
            {
                if(!percentEncodedName.compareWithoutCase("F_END") ||
                   !percentEncodedName.compareWithoutCase("FF_END"))
                {
                    blockContainer = &file.container();
                }
                continue;
            }

            if(!percentEncodedName.compareWithoutCase("F_START"))
            {
                blockContainer = 0;
                continue;
            }

            // Ignore extra marker lumps.
            if(!percentEncodedName.compareWithoutCase("FF_START") ||
               !percentEncodedName.compareWithoutCase("F_END")    ||
               !percentEncodedName.compareWithoutCase("FF_END")) continue;

            de::Uri uri = composeFlatUri(percentEncodedName);

            if(!textures.has(uri)) // A new flat?
            {
                /**
                 * Kludge Assume 64x64 else when the flat is loaded it will inherit the
                 * dimensions of the texture, which, if it has been replaced with a hires
                 * version - will be much larger than it should be.
                 *
                 * @todo Always determine size from the lowres original.
                 */
                int const uniqueId = lumpNum - (firstFlatMarkerLumpNum + 1);
                de::Uri resourcePath = composeLumpIndexResourceUrn(lumpNum);
                TextureManifest *manifest = textures.declare(uri, uniqueId, &resourcePath);

                de::Texture::Flags flags;
                if(file.container().hasCustom()) flags |= de::Texture::Custom;

                if(manifest && !manifest->derive(QSize(64, 64), flags))
                {
                    LOG_WARNING("Failed defining Texture for new flat \"%s\", ignoring.") << uri;
                }
            }
        }
    }

    LOG_INFO(String("R_InitFlatTetxures: Done in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

void R_DefineSpriteTexture(TextureManifest &manifest)
{
    LOG_AS("R_DefineSpriteTexture");

    // Have we already defined this texture?
    if(manifest.texture()) return;

    // A new sprite texture.
    de::Texture *tex = manifest.derive();
    if(!tex)
    {
        LOG_WARNING("Failed to define Texture for sprite \"%s\", ignoring.")
            << manifest.composeUri();
        return;
    }

    de::Uri const &resourceUri = manifest.resourceUri();
    if(resourceUri.isEmpty()) return;

    try
    {
        lumpnum_t lumpNum = resourceUri.path().toString().toInt();
        de::File1& file = App_FileSystem()->nameIndex().lump(lumpNum);

        /// @todo fixme: Ensure this is in Patch format.
        ByteRefArray fileData = ByteRefArray(file.cache(), file.size());
        de::Reader from = de::Reader(fileData);
        PatchHeader patchHdr;
        from >> patchHdr;

        tex->setOrigin(QPoint(-patchHdr.origin.x(), -patchHdr.origin.y()));
        tex->setDimensions(patchHdr.dimensions);
        if(file.hasCustom())
            tex->flags() |= de::Texture::Custom;

        file.unlock();
    }
    catch(LumpIndex::NotFoundError const &)
    {} // Ignore this error.
}

static int defineSpriteTextureWorker(TextureManifest &manifest, void * /*parameters*/)
{
    R_DefineSpriteTexture(manifest);
    return 0; // Continue iteration.
}

/// @todo Defer until necessary (sprite is first de-referenced).
static void defineAllSpriteTextures()
{
    App_Textures()->iterateDeclared("Sprites", defineSpriteTextureWorker);
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

void R_InitSpriteTextures()
{
    de::Time begunAt;

    LOG_VERBOSE("Initializing Sprite textures...");

    int uniqueId = 1/*1-based index*/;

    /// @todo fixme: Order here does not respect id tech1 logic.
    ddstack_t *stack = Stack_New();

    LumpIndex const &index = App_FileSystem()->nameIndex();
    for(int i = 0; i < index.size(); ++i)
    {
        de::File1 &file = index.lump(i);
        String fileName = file.name().fileNameWithoutExtension();

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

        // Compose the data resource path.
        de::Uri resourceUri = composeLumpIndexResourceUrn(i);

        if(!App_Textures()->declare(de::Uri("Sprites", Path(fileName)), uniqueId, &resourceUri))
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

    LOG_INFO(String("R_InitSpriteTextures: Done in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

texture_s *R_CreateSkinTex(uri_s const *_resourceUri, boolean isShinySkin)
{
    LOG_AS("R_CreateSkinTex");

    if(!_resourceUri) return 0;
    de::Uri const &resourceUri = reinterpret_cast<de::Uri const &>(*_resourceUri);

    if(resourceUri.isEmpty()) return 0;

    Textures &textures = *App_Textures();
    Textures::Scheme &scheme = textures.scheme(isShinySkin? "ModelReflectionSkins" : "ModelSkins");

    // Have we already created one for this?
    try
    {
        de::Texture *tex = scheme.findByResourceUri(resourceUri).texture();
        if(tex) return reinterpret_cast<texture_s *>(tex);
    }
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.

    int uniqueId = scheme.count() + 1; // 1-based index
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed declaring new model skin (max:%i), ignoring.") << DDMAXINT;
        return 0;
    }

    de::Uri uri(scheme.name(), Path(String("%1").arg(uniqueId, 8, 10, QChar('0'))));

    TextureManifest *manifest = textures.declare(uri, uniqueId, &resourceUri);
    if(!manifest) return 0; // Invalid uri?

    de::Texture *tex = manifest->texture();
    if(!tex)
    {
        // Create a texture for it.
        tex = manifest->derive(de::Texture::Custom);
        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for URI \"%s\", ignoring.") << uri;
        }
    }

    return reinterpret_cast<texture_s *>(tex);
}

texture_s *R_CreateDetailTexture(uri_s const *_resourceUri)
{
    LOG_AS("R_CreateDetailTexture");

    if(!_resourceUri) return 0;
    de::Uri const &resourceUri = reinterpret_cast<de::Uri const &>(*_resourceUri);

    if(resourceUri.isEmpty()) return 0;

    Textures &textures = *App_Textures();
    Textures::Scheme &scheme = textures.scheme("Details");

    // Have we already created one for this?
    try
    {
        de::Texture *tex = scheme.findByResourceUri(resourceUri).texture();
        if(tex) return reinterpret_cast<texture_s *>(tex);
    }
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.

    int uniqueId = scheme.count() + 1; // 1-based index.
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed declaring new detail texture (max:%i).") << DDMAXINT;
        return 0;
    }

    de::Uri uri(scheme.name(), Path(String("%1").arg(uniqueId, 8, 10, QChar('0'))));
    TextureManifest *manifest = textures.declare(uri, uniqueId, &resourceUri);
    if(!manifest) return 0; // Invalid URI?

    de::Texture *tex = manifest->texture();
    if(!tex)
    {
        // Create a texture for it.
        tex = manifest->derive(de::Texture::Custom);
        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for URI \"%s\", ignoring.") << uri;
        }
    }

    return reinterpret_cast<texture_s *>(tex);
}

texture_s *R_CreateLightmap(uri_s const *_resourceUri)
{
    LOG_AS("R_CreateLightmap");

    if(!_resourceUri) return 0;
    de::Uri const &resourceUri = reinterpret_cast<de::Uri const &>(*_resourceUri);

    if(resourceUri.isEmpty()) return 0;
    if(!resourceUri.path().toStringRef().compareWithoutCase("-")) return 0;

    Textures &textures = *App_Textures();
    Textures::Scheme &scheme = textures.scheme("Lightmaps");

    // Have we already created one for this?
    try
    {
        de::Texture *tex = scheme.findByResourceUri(resourceUri).texture();
        if(tex) return reinterpret_cast<texture_s *>(tex);
    }
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.


    int uniqueId = scheme.count() + 1; // 1-based index.
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed declaring new lightmap (max:%i), ignoring.") << DDMAXINT;
        return 0;
    }

    de::Uri uri(scheme.name(), Path(String("%1").arg(uniqueId, 8, 10, QChar('0'))));

    TextureManifest *manifest = textures.declare(uri, uniqueId, &resourceUri);
    if(!manifest) return 0; // Invalid URI?

    de::Texture *tex = manifest->texture();
    if(!tex)
    {
        // Create a texture for it.
        tex = manifest->derive(de::Texture::Custom);
        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for URI \"%s\", ignoring.") << uri;
        }
    }

    return reinterpret_cast<texture_s *>(tex);
}

texture_s *R_CreateFlaremap(uri_s const *_resourceUri)
{
    LOG_AS("R_CreateFlaremap");

    if(!_resourceUri) return 0;
    de::Uri const &resourceUri = reinterpret_cast<de::Uri const &>(*_resourceUri);

    if(resourceUri.isEmpty()) return 0;
    if(!resourceUri.path().toStringRef().compareWithoutCase("-")) return 0;

    // Perhaps a "built-in" flare texture id?
    // Try to convert the id to a system flare tex constant idx
    String const &resourcePathStr = resourceUri.path().toStringRef();
    if(resourcePathStr.length() == 1 &&
       resourcePathStr.first() >= '0' && resourcePathStr.first() <= '4')
        return 0;

    Textures &textures = *App_Textures();
    Textures::Scheme &scheme = textures.scheme("Flaremaps");

    // Have we already created one for this?
    try
    {
        de::Texture *tex = scheme.findByResourceUri(resourceUri).texture();
        if(tex) return reinterpret_cast<texture_s *>(tex);
    }
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.

    int uniqueId = scheme.count() + 1; // 1-based index.
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed declaring new flare texture (max:%i), ignoring.") << DDMAXINT;
        return 0;
    }

    // Create a texture for it.
    de::Uri uri(scheme.name(), Path(String("%1").arg(uniqueId, 8, 10, QChar('0'))));

    TextureManifest *manifest = textures.declare(uri, uniqueId, &resourceUri);
    if(!manifest) return 0; // Invalid URI?

    de::Texture *tex = manifest->texture();
    if(!tex)
    {
        tex = manifest->derive(de::Texture::Custom);
        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for URI \"%s\", ignoring.") << uri;
        }
    }

    return reinterpret_cast<texture_s *>(tex);
}

texture_s *R_CreateReflectionTexture(uri_s const *_resourceUri)
{
    LOG_AS("R_CreateReflectionTexture");

    if(!_resourceUri) return 0;
    de::Uri const &resourceUri = reinterpret_cast<de::Uri const &>(*_resourceUri);

    if(resourceUri.isEmpty()) return 0;

    Textures &textures = *App_Textures();
    Textures::Scheme &scheme = textures.scheme("Reflections");

    // Have we already created one for this?
    try
    {
        de::Texture *tex = scheme.findByResourceUri(resourceUri).texture();
        if(tex) return reinterpret_cast<texture_s *>(tex);
    }
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.

    int uniqueId = scheme.count() + 1; // 1-based index.
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed declaring new shiny texture (max:%i), ignoring.") << DDMAXINT;
        return 0;
    }

    de::Uri uri(scheme.name(), Path(String("%1").arg(uniqueId, 8, 10, QChar('0'))));

    TextureManifest *manifest = textures.declare(uri, uniqueId, &resourceUri);
    if(!manifest) return 0; // Invalid URI?

    de::Texture *tex = manifest->texture();
    if(!tex)
    {
        // Create a texture for it.
        tex = manifest->derive(de::Texture::Custom);
        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for URI \"%s\", ignoring.") << uri;
        }
    }

    return reinterpret_cast<texture_s *>(tex);
}

texture_s *R_CreateMaskTexture(uri_s const *_resourceUri, Size2Raw const *dimensions)
{
    DENG_ASSERT(dimensions);
    LOG_AS("R_CreateMaskTexture");

    if(!_resourceUri) return 0;
    de::Uri const &resourceUri = reinterpret_cast<de::Uri const &>(*_resourceUri);

    if(resourceUri.isEmpty()) return 0;

    Textures &textures = *App_Textures();
    Textures::Scheme &scheme = textures.scheme("Masks");

    // Have we already created one for this?
    if(resourceUri.isEmpty()) return 0;
    try
    {
        de::Texture *tex = scheme.findByResourceUri(resourceUri).texture();
        if(tex) return reinterpret_cast<texture_s *>(tex);
    }
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.

    int uniqueId = scheme.count() + 1; // 1-based index.
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed declaring mask texture (max:%i), ignoring.") << DDMAXINT;
        return 0;
    }

    de::Uri uri(scheme.name(), Path(String("%1").arg(uniqueId, 8, 10, QChar('0'))));

    TextureManifest *manifest = textures.declare(uri, uniqueId, &resourceUri);
    if(!manifest) return 0; // Invalid URI?

    de::Texture *tex = manifest->texture();
    if(tex)
    {
        tex->setDimensions(QSize(dimensions->width, dimensions->height));
    }
    else
    {
        // Create a texture for it.
        tex = manifest->derive(QSize(dimensions->width, dimensions->height), de::Texture::Custom);
        if(!tex)
        {
            LOG_WARNING("Failed defining Texture for URI \"%s\", ignoring.") << uri;
        }
    }

    return reinterpret_cast<texture_s *>(tex);
}
