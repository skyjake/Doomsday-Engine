/** @file r_data.cpp Texture Resource Registration.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <cstring>

#include <QList>
#include <QMap>
#include <de/App>
#include <de/ByteRefArray>
#include <de/LegacyCore>
#include <de/Log>
#include <de/Reader>
#include <de/String>
#include <de/Time>
#include <de/mathutil.h> // M_NumDigits()
#include <de/stack.h>

#include "de_base.h"
#include "de_filesys.h"
#include "de_resource.h"

#include "def_main.h"
#ifdef __CLIENT__
#  include "render/r_draw.h"
#endif
#include "gl/gl_tex.h"
#ifdef __CLIENT__
#  include "gl/gl_texmanager.h"
#endif
#include "uri.hh"

using namespace de;

static QList<PatchName> patchNames;

/// Ensure a texture has been derived for @a manifest.
static Texture *deriveTexture(TextureManifest &manifest)
{
    LOG_AS("deriveTexture");
    Texture *tex = manifest.derive();
    if(!tex)
    {
        LOG_WARNING("Failed to derive a Texture for \"%s\", ignoring.") << manifest.composeUri();
    }
    return tex;
}

static int defineTextureWorker(TextureManifest &manifest, void * /*parameters*/)
{
    deriveTexture(manifest);
    return 0; // Continue iteration.
}

/**
 * Compose the path to the data resource.
 * @note We do not use the lump name, instead we use the logical lump index
 * in the global LumpIndex. This is necessary because of the way id tech 1
 * manages graphic references in animations (intermediate frames are chosen
 * by their 'original indices' rather than by name).
 */
static inline de::Uri composeLumpIndexResourceUrn(lumpnum_t lumpNum)
{
    return de::Uri("LumpIndex", Path(String("%1").arg(lumpNum)));
}

Texture *R_FindTextureByResourceUri(String schemeName, de::Uri const *resourceUri)
{
    if(resourceUri && !resourceUri->isEmpty())
    {
        if(!resourceUri->path().toStringRef().compareWithoutCase("-")) return 0;

        try
        {
            return &App_Textures().scheme(schemeName).findByResourceUri(*resourceUri).texture();
        }
        catch(TextureManifest::MissingTextureError const &)
        {} // Ignore this error.
        catch(TextureScheme::NotFoundError const &)
        {} // Ignore this error.
    }
    return 0;
}

void R_InitSystemTextures()
{
    LOG_AS("R_InitSystemTextures");

    static const struct TexDef {
        String const graphicName;
        String const path;
    } texDefs[] = {
        { "unknown",    "unknown" },
        { "missing",    "missing" },
        { "bbox",       "bbox" },
        { "gray",       "gray" },
        { "mouse",      "ui/mouse" },
        { "boxcorner",  "ui/boxcorner" },
        { "boxfill",    "ui/boxfill" },
        { "boxshade",   "ui/boxshade" },
        { "hint",       "ui/hint" },
        { "logo",       "ui/logo" },
        { "background", "ui/background" },
        { "", "" }
    };

    LOG_VERBOSE("Initializing System textures...");

    for(uint i = 0; !texDefs[i].graphicName.isEmpty(); ++i)
    {
        struct TexDef const &def = texDefs[i];

        int uniqueId = i + 1/*1-based index*/;
        de::Uri resourceUri("Graphics", Path(def.graphicName));

        App_Textures().declare(de::Uri("System", Path(def.path)), Texture::Custom,
                               Vector2i(), Vector2i(), uniqueId, &resourceUri);
    }

    // Define any as yet undefined system textures.
    /// @todo Defer until necessary (manifest texture is first referenced).
    App_Textures().iterateDeclared("System", defineTextureWorker);
}

#undef R_ComposePatchPath
DENG_EXTERN_C AutoStr *R_ComposePatchPath(patchid_t id)
{
    try
    {
        TextureManifest &manifest = App_Textures().scheme("Patches").findByUniqueId(id);
        return AutoStr_FromTextStd(manifest.path().toUtf8().constData());
    }
    catch(TextureScheme::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    return AutoStr_NewStd();
}

#undef R_ComposePatchUri
DENG_EXTERN_C uri_s *R_ComposePatchUri(patchid_t id)
{
    try
    {
        TextureManifest &manifest = App_Textures().scheme("Patches").findByUniqueId(id);
        return reinterpret_cast<uri_s *>(new de::Uri(manifest.composeUri()));
    }
    catch(TextureScheme::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    return reinterpret_cast<uri_s *>(new de::Uri());
}

#undef R_DeclarePatch
DENG_EXTERN_C patchid_t R_DeclarePatch(char const *encodedName)
{
    LOG_AS("R_DeclarePatch");

    if(!encodedName || !encodedName[0])
    {
        LOG_DEBUG("Invalid 'name' argument, ignoring.");
        return 0;
    }

    Textures &textures = App_Textures();

    de::Uri uri("Patches", Path(encodedName));

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
    lumpnum_t lumpNum = App_FileSystem().nameIndex().lastIndexForPath(lumpPath);
    if(lumpNum < 0)
    {
        LOG_WARNING("Failed to locate lump for \"%s\", ignoring.") << uri;
        return 0;
    }

    de::File1 &file = App_FileSystem().nameIndex().lump(lumpNum);

    Texture::Flags flags;
    if(file.container().hasCustom()) flags |= Texture::Custom;

    Vector2i dimensions;
    Vector2i origin;

    // If this is a Patch (the format) read the world dimension and origin offset values.
    ByteRefArray fileData = ByteRefArray(file.cache(), file.size());
    if(Patch::recognize(fileData))
    {
        try
        {
            Patch::Metadata info = Patch::loadMetadata(fileData);

            dimensions = info.logicalDimensions;
            origin     = Vector2i(-info.origin.x, -info.origin.y);
        }
        catch(IByteArray::OffsetError const &)
        {
            LOG_WARNING("File \"%s:%s\" does not appear to be a valid Patch.\n"
                        "World dimension and origin offset not set for patch \"%s\".")
                << NativePath(file.container().composePath()).pretty()
                << NativePath(file.composePath()).pretty()
                << uri;
        }
    }
    file.unlock();

    int uniqueId        = textures.scheme("Patches").count() + 1; // 1-based index.
    de::Uri resourceUri = composeLumpIndexResourceUrn(lumpNum);

    TextureManifest *manifest = textures.declare(uri, flags, dimensions, origin, uniqueId, &resourceUri);
    if(!manifest) return 0; // Invalid uri?

    /// @todo Defer until necessary (manifest texture is first referenced).
    deriveTexture(*manifest);

    return uniqueId;
}

#undef R_GetPatchInfo
DENG_EXTERN_C boolean R_GetPatchInfo(patchid_t id, patchinfo_t *info)
{
    DENG2_ASSERT(info);
    LOG_AS("R_GetPatchInfo");

    std::memset(info, 0, sizeof(patchinfo_t));
    if(!id) return false;

    try
    {
        Texture &tex = App_Textures().scheme("Patches").findByUniqueId(id).texture();

#ifdef __CLIENT__
        // Ensure we have up to date information about this patch.
        texturevariantspecification_t &texSpec =
            *Rend_PatchTextureSpec(0 | (tex.isFlagged(Texture::Monochrome)        ? TSF_MONOCHROME : 0)
                                     | (tex.isFlagged(Texture::UpscaleAndSharpen) ? TSF_UPSCALE_AND_SHARPEN : 0));
        GL_PrepareTexture(tex, texSpec);
#endif

        info->id = id;
        info->flags.isCustom = tex.isFlagged(Texture::Custom);

        averagealpha_analysis_t *aa = reinterpret_cast<averagealpha_analysis_t *>(tex.analysisDataPointer(Texture::AverageAlphaAnalysis));
        info->flags.isEmpty = aa && FEQUAL(aa->alpha, 0);

        info->geometry.size.width  = tex.width();
        info->geometry.size.height = tex.height();

        info->geometry.origin.x = tex.origin().x;
        info->geometry.origin.y = tex.origin().y;

        /// @todo fixme: kludge:
        info->extraOffset[0] = info->extraOffset[1] = (tex.isFlagged(Texture::UpscaleAndSharpen)? -1 : 0);
        // Kludge end.
        return true;
    }
    catch(TextureManifest::MissingTextureError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    catch(TextureScheme::NotFoundError const &er)
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
        lumpnum_t lumpNum = App_FileSystem().lumpNumForName(lumpName);
        de::File1 &file = App_FileSystem().nameIndex().lump(lumpNum);

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
    LumpIndex const &index = App_FileSystem().nameIndex();
    lumpnum_t firstTexLump  = App_FileSystem().lumpNumForName("TEXTURE1");
    lumpnum_t secondTexLump = App_FileSystem().lumpNumForName("TEXTURE2");

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

typedef QList<CompositeTexture *> CompositeTextures;

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
                    if(custom->isFlagged(CompositeTexture::Custom))
                    {
                        hasReplacement = true; // Uses a custom patch.
                    }
                    // Do the definitions differ?
                    else if(custom->dimensions()        != orig->dimensions()  ||
                            custom->logicalDimensions() != orig->logicalDimensions() ||
                            custom->componentCount()    != orig->componentCount())
                    {
                        custom->flags() |= CompositeTexture::Custom;
                        hasReplacement = true;
                    }
                    else
                    {
                        // Check the patches.
                        short k = 0;
                        while(k < orig->componentCount() && !custom->isFlagged(CompositeTexture::Custom))
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

    while(!defs.isEmpty())
    {
        CompositeTexture &def = *defs.takeFirst();
        de::Uri uri("Textures", Path(def.percentEncodedName()));

        Texture::Flags flags;
        if(def.isFlagged(CompositeTexture::Custom)) flags |= Texture::Custom;

        /*
         * The id Tech 1 implementation of the texture collection has a flaw
         * which results in the first texture being used dually as a "NULL"
         * texture.
         */
        if(def.origIndex() == 0) flags |= Texture::NoDraw;


        TextureManifest *manifest = App_Textures().declare(uri, flags, def.logicalDimensions(), Vector2i(), def.origIndex());
        if(manifest)
        {
            // Are we redefining an existing texture?
            if(manifest->hasTexture())
            {
                // Yes. Destroy the existing definition (*should* exist).
                Texture &tex = manifest->texture();
                CompositeTexture *oldDef = reinterpret_cast<CompositeTexture *>(tex.userDataPointer());
                if(oldDef)
                {
                    tex.setUserDataPointer(0);
                    delete oldDef;
                }

                // Attach the new definition.
                tex.setUserDataPointer((void *)&def);

                continue;
            }
            // A new texture.
            else if(Texture *tex = manifest->derive())
            {
                tex->setUserDataPointer((void *)&def);
                continue;
            }
        }

        /// @todo Defer until necessary (manifest texture is first referenced).
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

void R_InitFlatTextures()
{
    de::Time begunAt;

    LOG_VERBOSE("Initializing Flat textures...");

    Textures &textures = App_Textures();

    LumpIndex const &index = App_FileSystem().nameIndex();
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

            de::Uri uri("Flats", Path(percentEncodedName));
            if(textures.has(uri)) continue;

            Texture::Flags flags;
            if(file.container().hasCustom()) flags |= Texture::Custom;

            /*
             * Kludge Assume 64x64 else when the flat is loaded it will inherit the
             * pixel dimensions of the graphic, which, if it has been replaced with
             * a hires version - will be much larger than it should be.
             *
             * @todo Always determine size from the lowres original.
             */
            Vector2i dimensions(64, 64);
            Vector2i origin(0, 0);
            int const uniqueId  = lumpNum - (firstFlatMarkerLumpNum + 1);
            de::Uri resourceUri = composeLumpIndexResourceUrn(lumpNum);

            textures.declare(uri, flags, dimensions, origin, uniqueId, &resourceUri);
        }
    }

    // Define any as yet undefined flat textures.
    /// @todo Defer until necessary (manifest texture is first referenced).
    textures.iterateDeclared("Flats", defineTextureWorker);

    LOG_INFO(String("R_InitFlatTetxures: Done in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
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

    LumpIndex const &index = App_FileSystem().nameIndex();
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

        de::Uri uri = de::Uri("Sprites", Path(fileName));

        Texture::Flags flags = 0;
        // If this is from an add-on flag it as "custom".
        if(file.container().hasCustom())
        {
            flags |= Texture::Custom;
        }

        Vector2i dimensions;
        Vector2i origin;

        // If this is a Patch read the world dimension and origin offset values.
        ByteRefArray fileData = ByteRefArray(file.cache(), file.size());
        if(Patch::recognize(fileData))
        {
            try
            {
                Patch::Metadata info = Patch::loadMetadata(fileData);

                dimensions = info.logicalDimensions;
                origin     = -info.origin;
            }
            catch(IByteArray::OffsetError const &)
            {
                LOG_WARNING("File \"%s:%s\" does not appear to be a valid Patch.\n"
                            "World dimension and origin offset not set for sprite \"%s\".")
                    << NativePath(file.container().composePath()).pretty()
                    << NativePath(file.composePath()).pretty()
                    << uri;
            }
        }
        file.unlock();

        de::Uri resourceUri = composeLumpIndexResourceUrn(i);
        TextureManifest *manifest = App_Textures().declare(uri, flags, dimensions, origin, uniqueId, &resourceUri);
        if(!manifest) continue; // Invalid uri?

        uniqueId++;
    }

    while(Stack_Height(stack))
    { Stack_Pop(stack); }

    Stack_Delete(stack);

    // Define any as yet undefined sprite textures.
    /// @todo Defer until necessary (manifest texture is first referenced).
    App_Textures().iterateDeclared("Sprites", defineTextureWorker);

    LOG_INFO(String("R_InitSpriteTextures: Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

Texture *R_DefineTexture(String schemeName, de::Uri const &resourceUri,
                         Vector2i const &dimensions)
{
    LOG_AS("R_DefineTexture");

    if(resourceUri.isEmpty()) return 0;

    // Have we already created one for this?
    TextureScheme &scheme = App_Textures().scheme(schemeName);
    try
    {
        return &scheme.findByResourceUri(resourceUri).texture();
    }
    catch(TextureManifest::MissingTextureError const &)
    {} // Ignore this error.
    catch(TextureScheme::NotFoundError const &)
    {} // Ignore this error.

    int uniqueId = scheme.count() + 1; // 1-based index.
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_WARNING("Failed declaring texture manifest in scheme %s (max:%i), ignoring.")
            << schemeName << DDMAXINT;
        return 0;
    }

    de::Uri uri(scheme.name(), Path(String("%1").arg(uniqueId, 8, 10, QChar('0'))));
    TextureManifest *manifest = App_Textures().declare(uri, Texture::Custom, dimensions,
                                                       Vector2i(), uniqueId, &resourceUri);
    if(!manifest) return 0; // Invalid URI?

    /// @todo Defer until necessary (manifest texture is first referenced).
    return deriveTexture(*manifest);
}

Texture *R_DefineTexture(String schemeName, de::Uri const &resourceUri)
{
    return R_DefineTexture(schemeName, resourceUri, Vector2i());
}
