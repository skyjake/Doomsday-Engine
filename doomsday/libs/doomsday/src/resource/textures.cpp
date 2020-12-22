/** @file textures.cpp
 *
 * @authors Copyright © 2010-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/res/textures.h"
#include "doomsday/res/resources.h"
#include "doomsday/res/composite.h"
#include "doomsday/res/texturescheme.h"
#include "doomsday/res/patch.h"
#include "doomsday/res/patchname.h"
#include "doomsday/res/sprites.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/doomsdayapp.h"

#include <de/legacy/mathutil.h>
#include <de/legacy/types.h>
#include <de/legacy/stack.h>

using namespace de;

namespace res {

//uint qHash(const TextureSchemeHashKey &key)
//{
//    return key.scheme.at(2).toLower().unicode();
//}

DE_PIMPL(Textures)
, DE_OBSERVES(TextureScheme,   ManifestDefined)
, DE_OBSERVES(TextureManifest, TextureDerived)
, DE_OBSERVES(Texture,         Deletion)
{
    TextureSchemes textureSchemes;
    List<TextureScheme *> textureSchemeCreationOrder;
    Composite::ArchiveFormat compositeFormat = Composite::DoomFormat;

    /// All texture instances in the system (from all schemes).
    AllTextures textures;

    Impl(Public *i) : Base(i)
    {
        // This may be overridden later.
        TextureManifest::setTextureConstructor([] (TextureManifest &m) -> Texture * {
            return new Texture(m);
        });

        // Note: Order here defines the ambigious-URI search order.
        createTextureScheme("Sprites");
        createTextureScheme("Textures");
        createTextureScheme("Flats");
        createTextureScheme("Patches");
        createTextureScheme("System");
        createTextureScheme("Details");
        createTextureScheme("Reflections");
        createTextureScheme("Masks");
        createTextureScheme("ModelSkins");
        createTextureScheme("ModelReflectionSkins");
        createTextureScheme("Lightmaps");
        createTextureScheme("Flaremaps");
    }

    ~Impl()
    {
        clear();
    }

    void clear()
    {
        clearTextureManifests();
        clearAllTextureSchemes();
    }

    void clearTextureManifests()
    {
        textureSchemes.deleteAll();
        textureSchemes.clear();
        textureSchemeCreationOrder.clear();
    }

    void clearAllTextureSchemes()
    {
        for (const auto &scheme : textureSchemes)
        {
            scheme.second->clear();
        }
    }

    void createTextureScheme(const String &name)
    {
        DE_ASSERT(name.size() >= TextureScheme::min_name_length);

        // Create a new scheme.
        TextureScheme *newScheme = new TextureScheme(name);
        textureSchemes.insert(name, newScheme);
        textureSchemeCreationOrder << newScheme;

        // We want notification when a new manifest is defined in this scheme.
        newScheme->audienceForManifestDefined += this;
    }

    void textureSchemeManifestDefined(TextureScheme & /*scheme*/, TextureManifest &manifest) override
    {
        // We want notification when the manifest is derived to produce a texture.
        manifest.audienceForTextureDerived += this;
    }

    void textureManifestTextureDerived(TextureManifest & /*manifest*/, Texture &texture) override
    {
        // Include this new texture in the scheme-agnostic list of instances.
        textures << &texture;

        // We want notification when the texture is about to be deleted.
        texture.audienceForDeletion += this;
    }

    void textureBeingDeleted(const Texture &texture) override
    {
        textures.remove(const_cast<Texture *>(&texture));
    }

//- Texture Initialization --------------------------------------------------------------

    static List<File1 *> collectPatchCompositeDefinitionFiles()
    {
        List<File1 *> result;

        // Precedence order of definitions is defined by id tech1 which processes
        // the TEXTURE1/2 lumps in the following order:
        //
        // (last)TEXTURE2 > (last)TEXTURE1
        const LumpIndex &index  = App_FileSystem().nameIndex();
        lumpnum_t firstTexLump  = App_FileSystem().lumpNumForName("TEXTURE1");
        lumpnum_t secondTexLump = App_FileSystem().lumpNumForName("TEXTURE2");

        // Also process all other lumps named TEXTURE1/2.
        for (dint i = 0; i < index.size(); ++i)
        {
            File1 &file = index[i];

            // Will this be processed anyway?
            if (i == firstTexLump ) continue;
            if (i == secondTexLump) continue;

            String fileName = file.name().fileNameWithoutExtension();
            if (fileName.compareWithoutCase("TEXTURE1") &&
                fileName.compareWithoutCase("TEXTURE2"))
            {
                continue;
            }

            result.append(&file);
        }

        if (firstTexLump >= 0)
        {
            result.append(&index[firstTexLump]);
        }

        if (secondTexLump >= 0)
        {
            result.append(&index[secondTexLump]);
        }

        return result;
    }

    typedef List<Composite *> Composites;
    typedef List<PatchName>   PatchNames;

    static PatchNames readPatchNames(File1 &file)
    {
        LOG_AS("readPatchNames");
        PatchNames names;

        if (file.size() < 4)
        {
            LOG_RES_WARNING("File \"%s\" does not appear to be valid PNAMES data")
                    << NativePath(file.composeUri().asText()).pretty();
            return names;
        }

        ByteRefArray lumpData(file.cache(), file.size());
        de::Reader from(lumpData);

        // The data begins with the total number of patch names.
        dint32 numNames;
        from >> numNames;

        // Followed by the names (eight character ASCII strings).
        if (numNames > 0)
        {
            if (unsigned(numNames) > (file.size() - 4) / 8)
            {
                // The data appears to be truncated.
                LOG_RES_WARNING("File \"%s\" appears to be truncated (%u bytes, expected %u)")
                        << NativePath(file.composeUri().asText()).pretty()
                        << file.size() << (numNames * 8 + 4);

                // We'll only read this many names.
                numNames = (file.size() - 4) / 8;
            }

            // Read the names.
            for (int i = 0; i < numNames; ++i)
            {
                PatchName name;
                from >> name;
                names.append(name);
            }
        }

        file.unlock();

        return names;
    }

    /**
     * Reads patch composite texture definitions from @a file.
     *
     * @param file           File to be read.
     * @param origIndexBase  Base value for the "original index" logic.
     * @param archiveCount   Will be updated with the total number of definitions
     *                       in the file (which may not necessarily equal the total
     *                       number of definitions which are actually read).
     */
    Composites readCompositeTextureDefs(File1 &file,
                                        const PatchNames &patchNames,
                                        int origIndexBase,
                                        int &archiveCount) const
    {
        LOG_AS("readCompositeTextureDefs");

        Composites result; ///< The resulting set of validated definitions.

        // The game data format determines the format of the archived data.
        Composite::ArchiveFormat format = compositeFormat;

        ByteRefArray data(file.cache(), file.size());
        de::Reader reader(data);

        // First is a count of the total number of definitions.
        dint32 definitionCount;
        reader >> definitionCount;

        // Next is directory of offsets to the definitions.
        typedef KeyMap<dint32, int> Offsets;
        Offsets offsets;
        for (int i = 0; i < definitionCount; ++i)
        {
            dint32 offset;
            reader >> offset;

            // Ensure the offset is within valid range.
            if (offset < 0 || unsigned(offset) < unsigned(definitionCount) * sizeof(offset) ||
               dsize(offset) > reader.source()->size())
            {
                LOG_RES_WARNING("Ignoring definition #%i: invalid offset %i") << i << offset;
            }
            else
            {
                offsets.insert(offset, origIndexBase + i);
            }
        }

        // Seek to each offset and deserialize the definition.
        DE_FOR_EACH_CONST(Offsets, i, offsets)
        {
            // Read the next definition.
            reader.setOffset(i->first);
            Composite *def = Composite::constructFrom(reader, patchNames, format);

            // Attribute the "original index".
            def->setOrigIndex(i->second);

            // If the composite contains at least one known component image it is
            // considered valid and we will therefore produce a Texture for it.
            DE_FOR_EACH_CONST(Composite::Components, it, def->components())
            {
                if (it->lumpNum() >= 0)
                {
                    // Its valid - include in the result.
                    result.append(def);
                    def = 0;
                    break;
                }
            }

            // Failed to validate? Dump it.
            if (def) delete def;
        }

        file.unlock(); // We have now finished with this file.

        archiveCount = definitionCount;
        return result;
    }

    Composites loadCompositeTextureDefs()
    {
        LOG_AS("loadCompositeTextureDefs");

        typedef std::multimap<String, Composite *> CompositeTextureMap;

        // Load the patch names from the PNAMES lump.
        PatchNames pnames;
        try
        {
            auto &fs1 = App_FileSystem();
            pnames = readPatchNames(fs1.lump(fs1.lumpNumForName("PNAMES")));
        }
        catch (const LumpIndex::NotFoundError &er)
        {
            if (DoomsdayApp::isGameLoaded())
            {
                LOGDEV_RES_WARNING(er.asText());
            }
        }

        // If no patch names - there is no point continuing further.
        if (!pnames.count()) return Composites();

        // Collate an ordered list of all the definition files we intend to process.
        const auto defFiles = collectPatchCompositeDefinitionFiles();

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
        Composites defs, customDefs;

        // Process each definition file.
        int origIndexBase = 0;
        for (auto *file : defFiles)
        {
            //File1 &file = **i;

            LOG_RES_VERBOSE("Processing \"%s:%s\"...")
                << NativePath(file->container().composeUri().asText()).pretty()
                << NativePath(file->composeUri().asText()).pretty();

            // Buffer the file and read the next set of definitions.
            int archiveCount;
            Composites newDefs = readCompositeTextureDefs(*file, pnames, origIndexBase, archiveCount);

            // In which set do these belong?
            Composites *existingDefs =
                    (file->container().hasCustom()? &customDefs : &defs);

            if (!existingDefs->isEmpty())
            {
                // Merge with the existing definitions.
                *existingDefs += newDefs;
            }
            else
            {
                *existingDefs = newDefs;
            }

            // Maintain the original index.
            origIndexBase += archiveCount;

            // Print a summary.
            LOG_RES_MSG("Loaded %s texture definitions from \"%s:%s\"")
                << (newDefs.count() == archiveCount? Stringf("all %i", newDefs.count())
                                                   : Stringf("%i of %i", newDefs.count(), archiveCount))
                << NativePath(file->container().composeUri().asText()).pretty()
                << NativePath(file->composeUri().asText()).pretty();
        }

        if (!customDefs.isEmpty())
        {
            // Custom definitions were found - we must cross compare them.

            // Map the definitions for O(log n) lookup performance,
            CompositeTextureMap mappedCustomDefs;
            for (Composite *custom : customDefs)
            {
                mappedCustomDefs.insert(std::make_pair(custom->percentEncodedNameRef(), custom));
            }

            // Perform reclassification of replaced texture definitions.
            for (int i = 0; i < defs.count(); ++i)
            {
                Composite *orig = defs[i];

                // Does a potential replacement exist for this original definition?
                CompositeTextureMap::const_iterator found = mappedCustomDefs.find(orig->percentEncodedNameRef());
                if (found == mappedCustomDefs.end())
                    continue;

                // Definition 'custom' is destined to replace 'orig'.
                Composite *custom = found->second;
                bool haveReplacement = false;

                if (custom->isFlagged(Composite::Custom))
                {
                    haveReplacement = true; // Uses a custom patch.
                }
                else if (*orig != *custom)
                {
                    haveReplacement = true;
                }

                if (haveReplacement)
                {
                    custom->setFlags(Composite::Custom);

                    // Let the PWAD "copy" override the IWAD original.
                    defs.takeAt(i);
                    delete orig;

                    --i; // Process the new next definition item.
                }
            }

            /*
             * List 'defs' now contains only those definitions which are not
             * superceeded by those in the 'customDefs' list.
             */

            // Add definitions from the custom list to the end of the main set.
            defs += customDefs;
        }

        return defs;
    }

    void initCompositeTextures()
    {
        Time begunAt;

        LOG_RES_VERBOSE("Initializing composite textures...");

        //self().textures().textureScheme("Textures").clear();

        // Load texture definitions from TEXTURE1/2 lumps.
        Composites allDefs = loadCompositeTextureDefs();
        while (!allDefs.isEmpty())
        {
            Composite &def = *allDefs.takeFirst();
            res::Uri uri(DE_STR("Textures"), Path(def.percentEncodedName()));

            Flags flags;
            if (def.isFlagged(Composite::Custom)) flags |= Texture::Custom;

            /*
             * The id Tech 1 implementation of the texture collection has a flaw
             * which results in the first texture being used dually as a "NULL"
             * texture.
             */
            if (def.origIndex() == 0) flags |= Texture::NoDraw;

            try
            {
                TextureManifest &manifest =
                    self().declareTexture(uri, flags, def.logicalDimensions(),
                                        Vec2i(), def.origIndex());

                // Are we redefining an existing texture?
                if (manifest.hasTexture())
                {
                    // Yes. Destroy the existing definition (*should* exist).
                    Texture &tex = manifest.texture();
                    Composite *oldDef = reinterpret_cast<Composite *>(tex.userDataPointer());
                    if (oldDef)
                    {
                        tex.setUserDataPointer(0);
                        delete oldDef;
                    }

                    // Attach the new definition.
                    tex.setUserDataPointer((void *)&def);

                    continue;
                }
                // A new texture.
                else if (Texture *tex = manifest.derive())
                {
                    tex->setUserDataPointer((void *)&def);
                    continue;
                }
            }
            catch (const TextureScheme::InvalidPathError &er)
            {
                LOG_RES_WARNING("Failed declaring texture \"%s\": %s")
                        << uri << er.asText();
            }

            delete &def;
        }

        LOG_RES_VERBOSE("initCompositeTextures: Completed in %.2f seconds") << begunAt.since();
    }

    void initFlatTextures()
    {
        Time begunAt;

        LOG_RES_VERBOSE("Initializing Flat textures...");

        //self().textures().textureScheme("Flats").clear();

        const LumpIndex &index = App_FileSystem().nameIndex();
        lumpnum_t firstFlatMarkerLumpNum = index.findFirst(Path("F_START.lmp"));
        if (firstFlatMarkerLumpNum >= 0)
        {
            lumpnum_t lumpNum;
            File1 *blockContainer = 0;
            for (lumpNum = index.size(); lumpNum --> firstFlatMarkerLumpNum + 1;)
            {
                File1 &file = index[lumpNum];
                String percentEncodedName = file.name().fileNameWithoutExtension();

                if (blockContainer && blockContainer != &file.container())
                {
                    blockContainer = 0;
                }

                if (!blockContainer)
                {
                    if (!percentEncodedName.compareWithoutCase("F_END") ||
                        !percentEncodedName.compareWithoutCase("FF_END"))
                    {
                        blockContainer = &file.container();
                    }
                    continue;
                }

                if (!percentEncodedName.compareWithoutCase("F_START"))
                {
                    blockContainer = 0;
                    continue;
                }

                // Ignore extra marker lumps.
                if (!percentEncodedName.compareWithoutCase("FF_START") ||
                    !percentEncodedName.compareWithoutCase("F_END")    ||
                    !percentEncodedName.compareWithoutCase("FF_END")) continue;

                res::Uri uri(DE_STR("Flats"), Path(percentEncodedName));
                if (self().textureManifestPtr(uri)) continue;

                Flags flags;
                if (file.container().hasCustom()) flags |= Texture::Custom;

                /*
                 * Kludge Assume 64x64 else when the flat is loaded it will inherit the
                 * pixel dimensions of the graphic, which, if it has been replaced with
                 * a hires version - will be much larger than it should be.
                 *
                 * @todo Always determine size from the lowres original.
                 */
                Vec2ui dimensions(64, 64);
                Vec2i origin(0, 0);
                const int uniqueId  = lumpNum - (firstFlatMarkerLumpNum + 1);
                res::Uri resourceUri = LumpIndex::composeResourceUrn(lumpNum);

                self().declareTexture(uri, flags, dimensions, origin, uniqueId, &resourceUri);
            }
        }

        // Define any as yet undefined flat textures.
        /// @todo Defer until necessary (manifest texture is first referenced).
        self().deriveAllTexturesInScheme("Flats");

        LOG_RES_VERBOSE("Flat textures initialized in %.2f seconds") << begunAt.since();
    }

    void initSpriteTextures()
    {
        Time begunAt;

        LOG_RES_VERBOSE("Initializing Sprite textures...");

        //self().textures().textureScheme("Sprites").clear();

        dint uniqueId = 1/*1-based index*/;

        /// @todo fixme: Order here does not respect id Tech 1 logic.
        ddstack_t *stack = Stack_New();

        const LumpIndex &index = App_FileSystem().nameIndex();
        for (dint i = 0; i < index.size(); ++i)
        {
            File1 &file = index[i];
            String fileName = file.name().fileNameWithoutExtension();

            if (fileName.beginsWith('S', CaseInsensitive) && fileName.length() >= 5)
            {
                if (fileName.endsWith("_START", CaseInsensitive))
                {
                    // We've arrived at *a* sprite block.
                    Stack_Push(stack, NULL);
                    continue;
                }

                if (Stack_Height(stack) > 0 && fileName.endsWith("_END", CaseInsensitive))
                {
                    // The sprite block ends.
                    Stack_Pop(stack);
                    continue;
                }
            }

            if (!Stack_Height(stack)) continue;

            String decodedFileName = String::fromPercentEncoding(fileName);
            if (!Sprites::isValidSpriteName(decodedFileName))
            {
                LOG_RES_NOTE("Ignoring invalid sprite name '%s'") << decodedFileName;
                continue;
            }

            res::Uri const uri("Sprites", Path(fileName));

            Flags flags = 0;
            // If this is from an add-on flag it as "custom".
            if (file.container().hasCustom())
            {
                flags |= Texture::Custom;
            }

            Vec2ui dimensions;
            Vec2i origin;

            if (file.size())
            {
                // If this is a Patch read the world dimension and origin offset values.
                ByteRefArray const fileData(file.cache(), file.size());
                if (Patch::recognize(fileData))
                {
                    try
                    {
                        auto info = Patch::loadMetadata(fileData);

                        dimensions = info.logicalDimensions;
                        origin     = -info.origin;
                    }
                    catch (const IByteArray::OffsetError &)
                    {
                        LOG_RES_WARNING("File \"%s:%s\" does not appear to be a valid Patch. "
                                        "World dimension and origin offset not set for sprite \"%s\".")
                                << NativePath(file.container().composePath()).pretty()
                                << NativePath(file.composePath()).pretty()
                                << uri;
                    }
                }
                file.unlock();
            }

            const res::Uri resourceUri = LumpIndex::composeResourceUrn(i);
            try
            {
                self().declareTexture(uri, flags, dimensions, origin, uniqueId, &resourceUri);
                uniqueId++;
            }
            catch (const TextureScheme::InvalidPathError &er)
            {
                LOG_RES_WARNING("Failed declaring texture \"%s\": %s") << uri << er.asText();
            }
        }

        while (Stack_Height(stack))
        {
            Stack_Pop(stack);
        }

        Stack_Delete(stack);

        // Define any as yet undefined sprite textures.
        /// @todo Defer until necessary (manifest texture is first referenced).
        self().deriveAllTexturesInScheme("Sprites");

        LOG_RES_VERBOSE("Sprite textures initialized in %.2f seconds") << begunAt.since();
    }
};

Textures::Textures()
    : d(new Impl(this))
{}

void Textures::setCompositeArchiveFormat(Composite::ArchiveFormat format)
{
    d->compositeFormat = format;
}

void Textures::clear()
{
    d->clear();
}

void Textures::clearRuntimeTextures()
{
    // Everything except "System".
    textureScheme("Sprites").clear();
    textureScheme("Textures").clear();
    textureScheme("Flats").clear();
    textureScheme("Patches").clear();
    textureScheme("Details").clear();
    textureScheme("Reflections").clear();
    textureScheme("Masks").clear();
    textureScheme("ModelSkins").clear();
    textureScheme("ModelReflectionSkins").clear();
    textureScheme("Lightmaps").clear();
    textureScheme("Flaremaps").clear();
}

void Textures::initTextures()
{
    LOG_AS("Textures");

    d->initCompositeTextures();
    d->initFlatTextures();
    d->initSpriteTextures();
}

Textures &Textures::get() // static
{
    return Resources::get().textures();
}

TextureScheme &Textures::textureScheme(const String &name) const
{
    if (auto *ts = textureSchemePtr(name))
    {
        return *ts;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw Resources::UnknownSchemeError("Textures::textureScheme", "No scheme found matching '" + name + "'");
}

TextureScheme *Textures::textureSchemePtr(const String &name) const
{
    if (!name.isEmpty())
    {
        auto found = d->textureSchemes.find(name);
        if (found != d->textureSchemes.end())
        {
            return found->second;
        }
    }
    return nullptr;
}

bool Textures::isKnownTextureScheme(const String &name) const
{
    if (!name.isEmpty())
    {
        return d->textureSchemes.contains(name);
    }
    return false;
}

Textures::TextureSchemes const& Textures::allTextureSchemes() const
{
    return d->textureSchemes;
}

void Textures::clearAllTextureSchemes()
{
    d->clearAllTextureSchemes();
}

#if 0
bool Textures::hasTextureManifest(const res::Uri &path) const
{
    try
    {
        textureManifest(path);
        return true;
    }
    catch (const Resources::MissingResourceManifestError &)
    {}  // Ignore this error.
    return false;
}
#endif

TextureManifest &Textures::textureManifest(const res::Uri &uri) const
{
    if (auto *mft = textureManifestPtr(uri))
    {
        return *mft;
    }
    /// @throw MissingResourceManifestError Failed to locate a matching manifest.
    throw Resources::MissingResourceManifestError
            ("Textures::textureManifest",
             "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

TextureManifest *Textures::textureManifestPtr(const res::Uri &uri) const
{
    // Perform the search.
    // Is this a URN? (of the form "urn:schemename:uniqueid")
    if (!uri.scheme().compareWithoutCase("urn"))
    {
        const String pathStr = uri.path().toString();
        if (auto uIdPos = pathStr.indexOf(":"))
        {
            String schemeName = pathStr.left(uIdPos);
            dint uniqueId     = pathStr.substr(uIdPos + 1 /*skip delimiter*/).toInt();

            if (auto *ts = textureSchemePtr(schemeName))
            {
                return ts->tryFindByUniqueId(uniqueId);
            }
        }
    }
    else
    {
        // No, this is a URI.
        // Does the user want a manifest in a specific scheme?
        if (!uri.scheme().isEmpty())
        {
            if (auto *ts = textureSchemePtr(uri.scheme()))
            {
                return ts->tryFind(uri.path());
            }
        }
        else
        {
            // No, check each scheme in priority order.
            for (const TextureScheme *scheme : d->textureSchemeCreationOrder)
            {
                if (auto *tex = scheme->tryFind(uri.path()))
                {
                    return tex;
                }
            }
        }
    }
    return nullptr;
}

const Textures::AllTextures &Textures::allTextures() const
{
    return d->textures;
}

TextureManifest &Textures::declareSystemTexture(const Path &texturePath, const res::Uri &resourceUri)
{
    auto &scheme = textureScheme(DE_STR("System"));
    const dint uniqueId = scheme.count() + 1;
    return scheme.declare(texturePath,
                          Texture::Custom,
                          Vec2ui(),
                          Vec2i(),
                          uniqueId,
                          &resourceUri);
}

Texture *Textures::tryFindTextureByResourceUri(const String &schemeName, const res::Uri &resourceUri)
{
    if (!resourceUri.isEmpty())
    {
        if (resourceUri.path() == "-")
        {
            return nullptr;
        }
        if (auto *ts = textureSchemePtr(schemeName))
        {
            if (auto *mft = ts->tryFindByResourceUri(resourceUri))
            {
                return mft->texturePtr();
            }
        }
    }
    return nullptr;
}

Texture *Textures::defineTexture(const String &schemeName,
                                 const res::Uri &resourceUri,
                                 const Vec2ui &dimensions)
{
    LOG_AS("Textures::defineTexture");

    if (resourceUri.isEmpty()) return nullptr;

    // Have we already created one for this?
    TextureScheme &scheme = textureScheme(schemeName);
    if (auto *mft = scheme.tryFindByResourceUri(resourceUri))
    {
        return mft->texturePtr();
    }

    dint uniqueId = scheme.count() + 1; // 1-based index.
    if (M_NumDigits(uniqueId) > 8)
    {
        LOG_RES_WARNING("Failed declaring texture manifest in scheme %s (max:%i)")
            << schemeName << DDMAXINT;
        return nullptr;
    }

    res::Uri uri(scheme.name(), Path(Stringf("%08i", uniqueId)));
    try
    {
        TextureManifest &manifest = declareTexture(uri, Texture::Custom, dimensions,
                                                   Vec2i(), uniqueId, &resourceUri);

        /// @todo Defer until necessary (manifest texture is first referenced).
        return deriveTexture(manifest);
    }
    catch (const TextureScheme::InvalidPathError &er)
    {
        LOG_RES_WARNING("Failed declaring texture \"%s\": %s") << uri << er.asText();
    }
    return nullptr;
}

Texture *Textures::deriveTexture(TextureManifest &manifest)
{
    LOG_AS("Textures");
    Texture *tex = manifest.derive();
    if (!tex)
    {
        LOGDEV_RES_WARNING("Failed to derive a Texture for \"%s\", ignoring")
                << manifest.composeUri();
    }
    return tex;
}

void Textures::deriveAllTexturesInScheme(String schemeName)
{
    TextureScheme &scheme = textureScheme(schemeName);

    PathTreeIterator<res::TextureScheme::Index> iter(scheme.index().leafNodes());
    while (iter.hasNext())
    {
        res::TextureManifest &manifest = iter.next();
        deriveTexture(manifest);
    }
}

patchid_t Textures::declarePatch(const String &encodedName)
{
    LOG_AS("Textures::declarePatch");

    if (encodedName.isEmpty())
        return 0;

    res::Uri uri(DE_STR("Patches"), Path(encodedName));

    // Already defined as a patch?
    if (auto *mft = textureManifestPtr(uri))
    {
        /// @todo We should instead define Materials from patches and return the material id.
        return patchid_t(mft->uniqueId());
    }

    auto &fs1 = App_FileSystem();

    Path lumpPath = uri.path() + ".lmp";
    if (!fs1.nameIndex().contains(lumpPath))
    {
        LOG_RES_WARNING("Failed to locate lump for \"%s\"") << uri;
        return 0;
    }

    const lumpnum_t lumpNum = fs1.nameIndex().findLast(lumpPath);
    File1 &file = fs1.lump(lumpNum);

    Flags flags;
    if (file.container().hasCustom()) flags |= Texture::Custom;

    Vec2ui dimensions;
    Vec2i origin;

    // If this is a Patch (the format) read the world dimension and origin offset values.
    ByteRefArray fileData = ByteRefArray(file.cache(), file.size());
    if (Patch::recognize(fileData))
    {
        try
        {
            auto info = Patch::loadMetadata(fileData);

            dimensions = info.logicalDimensions;
            origin     = Vec2i(-info.origin.x, -info.origin.y);
        }
        catch (const IByteArray::OffsetError &)
        {
            LOG_RES_WARNING("File \"%s:%s\" does not appear to be a valid Patch. "
                            "World dimension and origin offset not set for patch \"%s\".")
                << NativePath(file.container().composePath()).pretty()
                << NativePath(file.composePath()).pretty()
                << uri;
        }
    }
    file.unlock();

    dint uniqueId       = textureScheme(DE_STR("Patches")).count() + 1;  // 1-based index.
    res::Uri resourceUri = LumpIndex::composeResourceUrn(lumpNum);

    try
    {
        TextureManifest &manifest = declareTexture(uri, flags, dimensions, origin,
                                                   uniqueId, &resourceUri);

        /// @todo Defer until necessary (manifest texture is first referenced).
        deriveTexture(manifest);

        return uniqueId;
    }
    catch (const TextureScheme::InvalidPathError &er)
    {
        LOG_RES_WARNING("Failed declaring texture \"%s\": %s") << uri << er.asText();
    }
    return 0;
}

} // namespace res
