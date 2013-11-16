/** @file resourcesystem.cpp  Resource subsystem.
 *
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_platform.h"
#include "resource/resourcesystem.h"

#include "resource/compositetexture.h"
#include "resource/patch.h"
#include "resource/patchname.h"
#ifdef __CLIENT__
#  include "BitmapFont"
#  include "CompositeBitmapFont"
#endif

#include "filesys/fs_main.h"
#include "filesys/lumpindex.h"

#ifdef __CLIENT__
#  include "gl/gl_texmanager.h"
#endif

#include <de/ByteRefArray>
#include <de/Log>
#include <de/Reader>
#include <de/Time>
#include "uri.hh"
#include <de/stack.h> /// @todo remove me
#include <QList>
#include <QMap>
#include <QtAlgorithms>

using namespace de;

typedef QList<CompositeTexture *> CompositeTextures;

static QList<de::File1 *> collectPatchCompositeDefinitionFiles();

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

DENG2_PIMPL(ResourceSystem)
{
    typedef QList<de::ResourceClass *> ResourceClasses;
    NullResourceClass nullResourceClass;
    ResourceClasses resClasses;

    QList<PatchName> patchNames;
    Textures textures;

    Materials materials;

#ifdef __CLIENT__
    Fonts fonts;
#endif

    Instance(Public *i) : Base(i)
    {
        LOG_AS("ResourceSystem");
        resClasses.append(new ResourceClass("RC_PACKAGE",    "Packages"));
        resClasses.append(new ResourceClass("RC_DEFINITION", "Defs"));
        resClasses.append(new ResourceClass("RC_GRAPHIC",    "Graphics"));
        resClasses.append(new ResourceClass("RC_MODEL",      "Models"));
        resClasses.append(new ResourceClass("RC_SOUND",      "Sfx"));
        resClasses.append(new ResourceClass("RC_MUSIC",      "Music"));
        resClasses.append(new ResourceClass("RC_FONT",       "Fonts"));

        LOG_MSG("Initializing Texture collection...");
        /// @note Order here defines the ambigious-URI search order.
        textures.createScheme("Sprites");
        textures.createScheme("Textures");
        textures.createScheme("Flats");
        textures.createScheme("Patches");
        textures.createScheme("System");
        textures.createScheme("Details");
        textures.createScheme("Reflections");
        textures.createScheme("Masks");
        textures.createScheme("ModelSkins");
        textures.createScheme("ModelReflectionSkins");
        textures.createScheme("Lightmaps");
        textures.createScheme("Flaremaps");

        LOG_MSG("Initializing Material collection...");
        /// @note Order here defines the ambigious-URI search order.
        materials.createScheme("Sprites");
        materials.createScheme("Textures");
        materials.createScheme("Flats");
        materials.createScheme("System");

#ifdef __CLIENT__
        LOG_MSG("Initializing Font collection...");
        /// @note Order here defines the ambigious-URI search order.
        fonts.createScheme("System");
        fonts.createScheme("Game");
#endif
    }

    ~Instance()
    {
        qDeleteAll(resClasses);
    }

#ifdef __CLIENT__
    void clearRuntimeFonts()
    {
        fonts.scheme("Game").clear();

        GL_PruneTextureVariantSpecifications();
    }

    void clearSystemFonts()
    {
        fonts.scheme("System").clear();

        GL_PruneTextureVariantSpecifications();
    }
#endif // __CLIENT__

    void clearRuntimeTextures()
    {
        textures.scheme("Flats").clear();
        textures.scheme("Textures").clear();
        textures.scheme("Patches").clear();
        textures.scheme("Sprites").clear();
        textures.scheme("Details").clear();
        textures.scheme("Reflections").clear();
        textures.scheme("Masks").clear();
        textures.scheme("ModelSkins").clear();
        textures.scheme("ModelReflectionSkins").clear();
        textures.scheme("Lightmaps").clear();
        textures.scheme("Flaremaps").clear();

#ifdef __CLIENT__
        GL_PruneTextureVariantSpecifications();
#endif
    }

    void clearSystemTextures()
    {
        textures.scheme("System").clear();

#ifdef __CLIENT__
        GL_PruneTextureVariantSpecifications();
#endif
    }

    void deriveAllTexturesInScheme(String schemeName)
    {
        TextureScheme &scheme = textures.scheme(schemeName);

        PathTreeIterator<TextureScheme::Index> iter(scheme.index().leafNodes());
        while(iter.hasNext())
        {
            TextureManifest &manifest = iter.next();
            deriveTexture(manifest);
        }
    }

    void loadPatchNames(String lumpName)
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
            if(App_GameLoaded())
            {
                LOG_WARNING(er.asText());
            }
        }
    }

    CompositeTextures loadCompositeTextureDefs()
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
                << (newDefs.count() == archiveCount? String("all %1").arg(newDefs.count())
                                                   : String("%1 of %1").arg(newDefs.count()).arg(archiveCount))
                << NativePath(file.container().composeUri().asText()).pretty()
                << NativePath(file.composeUri().asText()).pretty();
        }

        if(!customDefs.isEmpty())
        {
            // Custom definitions were found - we must cross compare them.

            // Map the definitions for O(log n) lookup performance,
            CompositeTextureMap mappedCustomDefs;
            foreach(CompositeTexture *custom, customDefs)
            {
                mappedCustomDefs.insert(custom->percentEncodedNameRef(), custom);
            }

            // Perform reclassification of replaced texture definitions.
            for(int i = 0; i < defs.count(); ++i)
            {
                CompositeTexture *orig = defs[i];

                // Does a potential replacement exist for this original definition?
                CompositeTextureMap::const_iterator found = mappedCustomDefs.constFind(orig->percentEncodedNameRef());
                if(found == mappedCustomDefs.constEnd())
                    continue;

                // Definition 'custom' is destined to replace 'orig'.
                CompositeTexture *custom = found.value();
                bool haveReplacement = false;

                if(custom->isFlagged(CompositeTexture::Custom))
                {
                    haveReplacement = true; // Uses a custom patch.
                }
                else
                {
                    // Do the definitions differ?
                    if(custom->dimensions()        != orig->dimensions()  ||
                       custom->logicalDimensions() != orig->logicalDimensions() ||
                       custom->componentCount()    != orig->componentCount())
                    {
                        haveReplacement = true;
                    }
                    else
                    {
                        // Check the patches.
                        for(int k = 0; k < orig->componentCount(); ++k)
                        {
                            CompositeTexture::Component const &origP   = orig->components()[k];
                            CompositeTexture::Component const &customP = custom->components()[k];

                            if(origP.lumpNum() != customP.lumpNum() &&
                               origP.xOrigin() != customP.xOrigin() &&
                               origP.yOrigin() != customP.yOrigin())
                            {
                                haveReplacement = true;
                                break;
                            }
                        }
                    }
                }

                if(haveReplacement)
                {
                    custom->setFlags(CompositeTexture::Custom);

                    // Let the PWAD "copy" override the IWAD original.
                    defs.takeAt(i);
                    delete orig;

                    --i; // Process the new next definition item.
                }
            }

            /*
             * List 'defs' now contains only those definitions which are not superceeded
             * by those in the 'customDefs' list.
             */

            // Add definitions from the custom list to the end of the main set.
            defs.append(customDefs);
        }

        return defs;
    }

    /**
     * Reads patch composite texture definitions from @a file.
     *
     * @param data  The data buffer to be read.
     * @param origIndexBase  Base value for the "original index" logic.
     * @param archiveCount  Will be updated with the total number of definitions
     *                      in the file (which may not necessarily equal the total
     *                      number of definitions which are actually read).
     */
    CompositeTextures readCompositeTextureDefs(IByteArray &data,
        int origIndexBase, int& archiveCount)
    {
        LOG_AS("readCompositeTextureDefs");

        CompositeTextures result; ///< The resulting set of validated definitions.

        // The game data format determines the format of the archived data.
        CompositeTexture::ArchiveFormat format =
                (gameDataFormat == 0? CompositeTexture::DoomFormat : CompositeTexture::StrifeFormat);

        de::Reader reader(data);

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
                    result.append(def);
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

    /**
     * @param defs  Definitions to be processed.
     */
    void processCompositeTextureDefs(CompositeTextures &defs)
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

            try
            {
                TextureManifest &manifest =
                    textures.declare(uri, flags, def.logicalDimensions(), Vector2i(), def.origIndex());

                // Are we redefining an existing texture?
                if(manifest.hasTexture())
                {
                    // Yes. Destroy the existing definition (*should* exist).
                    Texture &tex = manifest.texture();
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
                else if(Texture *tex = manifest.derive())
                {
                    tex->setUserDataPointer((void *)&def);
                    continue;
                }
            }
            catch(TextureScheme::InvalidPathError const &er)
            {
                LOG_WARNING(er.asText() + ". Failed declaring texture \"%s\", ignoring.") << uri;
            }

            delete &def;
        }
    }
};

ResourceSystem::ResourceSystem() : d(new Instance(this))
{}

void ResourceSystem::timeChanged(Clock const &)
{
    // Nothing to do.
}

ResourceClass &ResourceSystem::resClass(String name)
{
    if(!name.isEmpty())
    {
        foreach(ResourceClass *resClass, d->resClasses)
        {
            if(!resClass->name().compareWithoutCase(name))
                return *resClass;
        }
    }
    return d->nullResourceClass; // Not found.
}

ResourceClass &ResourceSystem::resClass(resourceclassid_t id)
{
    if(id == RC_NULL) return d->nullResourceClass;
    if(VALID_RESOURCECLASSID(id))
    {
        return *d->resClasses.at(uint(id));
    }
    /// @throw UnknownResourceClass Attempted with an unknown id.
    throw UnknownResourceClass("ResourceSystem::toClass", QString("Invalid id '%1'").arg(int(id)));
}

void ResourceSystem::clearAllRuntimeResources()
{
#ifdef __CLIENT__
    d->clearRuntimeFonts();
#endif
    d->clearRuntimeTextures();
}

void ResourceSystem::clearAllSystemResources()
{
#ifdef __CLIENT__
    d->clearSystemFonts();
#endif
    d->clearSystemTextures();
}

Materials &ResourceSystem::materials()
{
    return d->materials;
}

Textures &ResourceSystem::textures()
{
    return d->textures;
}

void ResourceSystem::initSystemTextures()
{
    LOG_AS("ResourceSystem");

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

        d->textures.declare(de::Uri("System", Path(def.path)), Texture::Custom,
                            Vector2i(), Vector2i(), uniqueId, &resourceUri);
    }

    // Define any as yet undefined system textures.
    /// @todo Defer until necessary (manifest texture is first referenced).
    d->deriveAllTexturesInScheme("System");
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
    LumpIndex const &index  = App_FileSystem().nameIndex();
    lumpnum_t firstTexLump  = App_FileSystem().lumpNumForName("TEXTURE1");
    lumpnum_t secondTexLump = App_FileSystem().lumpNumForName("TEXTURE2");

    /*
     * Also process all other lumps named TEXTURE1/2.
     */
    for(int i = 0; i < index.size(); ++i)
    {
        de::File1 &file = index.lump(i);

        // Will this be processed anyway?
        if(i == firstTexLump) continue;
        if(i == secondTexLump) continue;

        String fileName = file.name().fileNameWithoutExtension();
        if(fileName.compareWithoutCase("TEXTURE1") &&
           fileName.compareWithoutCase("TEXTURE2"))
        {
            continue;
        }

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

void ResourceSystem::initCompositeTextures()
{
    Time begunAt;

    LOG_VERBOSE("Initializing PatchComposite textures...");

    // Load texture definitions from TEXTURE1/2 lumps.
    CompositeTextures texs = d->loadCompositeTextureDefs();
    d->processCompositeTextureDefs(texs);

    DENG_ASSERT(texs.isEmpty());

    LOG_INFO(String("R_InitCompositeTextures: Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

void ResourceSystem::initFlatTextures()
{
    de::Time begunAt;

    LOG_VERBOSE("Initializing Flat textures...");

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
            if(d->textures.has(uri)) continue;

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

            d->textures.declare(uri, flags, dimensions, origin, uniqueId, &resourceUri);
        }
    }

    // Define any as yet undefined flat textures.
    /// @todo Defer until necessary (manifest texture is first referenced).
    d->deriveAllTexturesInScheme("Flats");

    LOG_INFO(String("R_InitFlatTetxures: Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
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

void ResourceSystem::initSpriteTextures()
{
    Time begunAt;

    LOG_AS("ResourceSystem");
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

        de::Uri uri("Sprites", Path(fileName));

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
        try
        {
            d->textures.declare(uri, flags, dimensions, origin, uniqueId, &resourceUri);
            uniqueId++;
        }
        catch(TextureScheme::InvalidPathError const &er)
        {
            LOG_WARNING(er.asText() + ". Failed declaring texture \"%s\", ignoring.") << uri;
        }
    }

    while(Stack_Height(stack))
    { Stack_Pop(stack); }

    Stack_Delete(stack);

    // Define any as yet undefined sprite textures.
    /// @todo Defer until necessary (manifest texture is first referenced).
    d->deriveAllTexturesInScheme("Sprites");

    LOG_INFO(String("Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

Texture *ResourceSystem::texture(String schemeName, de::Uri const *resourceUri)
{
    if(resourceUri && !resourceUri->isEmpty())
    {
        if(!resourceUri->path().toStringRef().compareWithoutCase("-")) return 0;

        try
        {
            return &d->textures.scheme(schemeName).findByResourceUri(*resourceUri).texture();
        }
        catch(TextureManifest::MissingTextureError const &)
        {} // Ignore this error.
        catch(TextureScheme::NotFoundError const &)
        {} // Ignore this error.
    }
    return 0;
}

Texture *ResourceSystem::defineTexture(String schemeName, de::Uri const &resourceUri,
    Vector2i const &dimensions)
{
    LOG_AS("ResourceSystem::DefineTexture");

    if(resourceUri.isEmpty()) return 0;

    // Have we already created one for this?
    TextureScheme &scheme = d->textures.scheme(schemeName);
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
    try
    {
        TextureManifest &manifest = d->textures.declare(uri, Texture::Custom, dimensions,
                                                        Vector2i(), uniqueId, &resourceUri);

        /// @todo Defer until necessary (manifest texture is first referenced).
        return deriveTexture(manifest);
    }
    catch(TextureScheme::InvalidPathError const &er)
    {
        LOG_WARNING(er.asText() + ". Failed declaring texture \"%s\", ignoring.") << uri;
    }
    return 0;
}

patchid_t ResourceSystem::declarePatch(String encodedName)
{
    LOG_AS("ResourceSystem::declarePatch");

    if(encodedName.isEmpty())
    {
        LOG_DEBUG("Invalid 'name' argument, ignoring.");
        return 0;
    }

    de::Uri uri("Patches", Path(encodedName));

    // Already defined as a patch?
    try
    {
        TextureManifest &manifest = d->textures.find(uri);
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

    int uniqueId        = d->textures.scheme("Patches").count() + 1; // 1-based index.
    de::Uri resourceUri = composeLumpIndexResourceUrn(lumpNum);

    try
    {
        TextureManifest &manifest = d->textures.declare(uri, flags, dimensions, origin, uniqueId, &resourceUri);

        /// @todo Defer until necessary (manifest texture is first referenced).
        deriveTexture(manifest);

        return uniqueId;
    }
    catch(TextureScheme::InvalidPathError const &er)
    {
        LOG_WARNING(er.asText() + ". Failed declaring texture \"%s\", ignoring.") << uri;
    }
    return 0;
}

#ifdef __CLIENT__
Fonts &ResourceSystem::fonts()
{
    return d->fonts;
}

void ResourceSystem::clearFontDefinitionLinks()
{
    foreach(AbstractFont *font, d->fonts.all())
    {
        if(CompositeBitmapFont *compFont = font->maybeAs<CompositeBitmapFont>())
        {
            compFont->setDefinition(0);
        }
    }
}

AbstractFont *ResourceSystem::createFontFromDef(ded_compositefont_t const &def)
{
    LOG_AS("ResourceSystem::createFontFromDef");

    if(!def.uri) return 0;
    de::Uri const &uri = *reinterpret_cast<de::Uri *>(def.uri);

    try
    {
        // Create/retrieve a manifest for the would-be font.
        FontManifest &manifest = d->fonts.declare(uri);

        if(manifest.hasResource())
        {
            if(CompositeBitmapFont *compFont = manifest.resource().maybeAs<CompositeBitmapFont>())
            {
                /// @todo Do not update fonts here (not enough knowledge). We should
                /// instead return an invalid reference/signal and force the caller
                /// to implement the necessary update logic.
                LOG_DEBUG("A Font with uri \"%s\" already exists, returning existing.")
                    << manifest.composeUri();

                compFont->rebuildFromDef(def);
            }
            return &manifest.resource();
        }

        // A new font.
        manifest.setResource(CompositeBitmapFont::fromDef(manifest, def));
        if(manifest.hasResource())
        {
            if(verbose >= 1)
            {
                LOG_VERBOSE("New font \"%s\"")
                    << manifest.composeUri();
            }
            return &manifest.resource();
        }

        LOG_WARNING("Failed defining new Font for \"%s\", ignoring.")
            << NativePath(uri.asText()).pretty();
    }
    catch(Fonts::UnknownSchemeError const &er)
    {
        LOG_WARNING(er.asText() + ". Failed declaring font \"%s\", ignoring.")
            << NativePath(uri.asText()).pretty();
    }
    catch(FontScheme::InvalidPathError const &er)
    {
        LOG_WARNING(er.asText() + ". Failed declaring font \"%s\", ignoring.")
            << NativePath(uri.asText()).pretty();
    }

    return 0;
}

AbstractFont *ResourceSystem::createFontFromFile(de::Uri const &uri,
    String filePath)
{
    LOG_AS("ResourceSystem::createFontFromFile");

    if(!App_FileSystem().accessFile(de::Uri::fromNativePath(filePath)))
    {
        LOG_WARNING("Invalid filePath, ignoring.");
        return 0;
    }

    try
    {
        // Create/retrieve a manifest for the would-be font.
        FontManifest &manifest = d->fonts.declare(uri);

        if(manifest.hasResource())
        {
            if(BitmapFont *bmapFont = manifest.resource().maybeAs<BitmapFont>())
            {
                /// @todo Do not update fonts here (not enough knowledge). We should
                /// instead return an invalid reference/signal and force the caller
                /// to implement the necessary update logic.
                LOG_DEBUG("A Font with uri \"%s\" already exists, returning existing.")
                    << manifest.composeUri();

                bmapFont->setFilePath(filePath);
            }
            return &manifest.resource();
        }

        // A new font.
        manifest.setResource(BitmapFont::fromFile(manifest, filePath));
        if(manifest.hasResource())
        {
            if(verbose >= 1)
            {
                LOG_VERBOSE("New font \"%s\"")
                    << manifest.composeUri();
            }
            return &manifest.resource();
        }

        LOG_WARNING("Failed defining new Font for \"%s\", ignoring.")
            << NativePath(uri.asText()).pretty();
    }
    catch(Fonts::UnknownSchemeError const &er)
    {
        LOG_WARNING(er.asText() + ". Failed declaring font \"%s\", ignoring.")
            << NativePath(uri.asText()).pretty();
    }
    catch(FontScheme::InvalidPathError const &er)
    {
        LOG_WARNING(er.asText() + ". Failed declaring font \"%s\", ignoring.")
            << NativePath(uri.asText()).pretty();
    }

    return 0;
}

#endif // __CLIENT__

void ResourceSystem::consoleRegister() // static
{
    Textures::consoleRegister();
    Materials::consoleRegister();
#ifdef __CLIENT__
    Fonts::consoleRegister();
#endif
}
