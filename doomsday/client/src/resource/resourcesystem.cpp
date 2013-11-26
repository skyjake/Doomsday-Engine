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

#ifdef __CLIENT__
#  include "clientapp.h"
#endif

#include "con_main.h"
#include "def_main.h"

#include "resource/colorpalette.h"
#include "resource/compositetexture.h"
#include "resource/patch.h"
#include "resource/patchname.h"
#include "resource/models.h"
#ifdef __CLIENT__
#  include "BitmapFont"
#  include "CompositeBitmapFont"
#endif

#include "filesys/fs_main.h"
#include "filesys/lumpindex.h"

#ifdef __CLIENT__
#  include "gl/gl_tex.h"
#  include "gl/gl_texmanager.h"
#  include "render/rend_particle.h" // Rend_ParticleReleaseSystemTextures
#  include "sys_system.h" // novideo

// For smart caching logics:
#  include "network/net_demo.h" // playback
#  include "render/rend_main.h" // Rend_MapSurfaceMaterialSpec
#  include "render/billboard.h" // Rend_SpriteMaterialSpec
#  include "render/sky.h"

#  include "world/world.h"
#  include "world/map.h"
#  include "world/p_object.h"
#  include "world/thinkers.h"
#  include "Surface"
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

#ifdef __CLIENT__
static int hashDetailTextureSpec(detailvariantspecification_t const &spec)
{
    return (spec.contrast * (1/255.f) * DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR + .5f);
}

static variantspecification_t &configureTextureSpec(variantspecification_t &spec,
    texturevariantusagecontext_t tc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    DENG2_ASSERT(tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc));

    flags &= ~TSF_INTERNAL_MASK;

    spec.context         = tc;
    spec.flags           = flags;
    spec.border          = (flags & TSF_UPSCALE_AND_SHARPEN)? 1 : border;
    spec.mipmapped       = mipmapped;
    spec.wrapS           = wrapS;
    spec.wrapT           = wrapT;
    spec.minFilter       = de::clamp(-1, minFilter, spec.mipmapped? 3:1);
    spec.magFilter       = de::clamp(-3, magFilter, 1);
    spec.anisoFilter     = de::clamp(-1, anisoFilter, 4);
    spec.gammaCorrection = gammaCorrection;
    spec.noStretch       = noStretch;
    spec.toAlpha         = toAlpha;

    if(tClass || tMap)
    {
        spec.flags      |= TSF_HAS_COLORPALETTE_XLAT;
        spec.tClass      = de::max(0, tClass);
        spec.tMap        = de::max(0, tMap);
    }

    return spec;
}

static detailvariantspecification_t &configureDetailTextureSpec(
    detailvariantspecification_t &spec, float contrast)
{
    int const quantFactor = DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR;

    spec.contrast = 255 * de::clamp<int>(0, contrast * quantFactor + .5f, quantFactor) * (1 / float(quantFactor));
    return spec;
}

/// @c TST_DETAIL type specifications are stored separately into a set of
/// buckets. Bucket selection is determined by their quantized contrast value.
#define DETAILVARIANT_CONTRAST_HASHSIZE     (DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR+1)

#endif // __CLIENT__

DENG2_PIMPL(ResourceSystem)
, DENG2_OBSERVES(TextureScheme,   ManifestDefined)
, DENG2_OBSERVES(TextureManifest, TextureDerived)
, DENG2_OBSERVES(Texture,         Deletion)
#ifdef __CLIENT__
, DENG2_OBSERVES(FontScheme,      ManifestDefined)
, DENG2_OBSERVES(FontManifest,    Deletion)
, DENG2_OBSERVES(AbstractFont,    Deletion)
, DENG2_OBSERVES(ColorPalette,    ColorTableChange)
#endif
{
    typedef QList<de::ResourceClass *> ResourceClasses;
    ResourceClasses resClasses;
    NullResourceClass nullResourceClass;

    typedef QList<AnimGroup *> AnimGroups;
    AnimGroups animGroups;

    typedef QList<PatchName> PatchNames;
    PatchNames patchNames;

    typedef QMap<colorpaletteid_t, ColorPalette *> ColorPalettes;
    ColorPalettes colorPalettes;

    typedef QMap<String, ColorPalette *> ColorPaletteNames;
    ColorPaletteNames colorPaletteNames;

    colorpaletteid_t defaultColorPalette;

    /// System subspace schemes containing the textures.
    TextureSchemes textureSchemes;
    QList<TextureScheme *> textureSchemeCreationOrder;
    /// All texture instances in the system (from all schemes).
    AllTextures textures;

    Materials materials;

    struct SpriteGroup {
        SpriteSet sprites;

        ~SpriteGroup() {
            qDeleteAll(sprites);
        }
    };
    typedef QMap<spritenum_t, SpriteGroup> SpriteGroups;
    SpriteGroups spriteGroups;

#ifdef __CLIENT__
    /// System subspace schemes containing the manifests/resources.
    FontSchemes fontSchemes;
    QList<FontScheme *> fontSchemeCreationOrder;

    AllFonts fonts; ///< From all schemes.
    uint fontManifestCount; ///< Total number of font manifests (in all schemes).

    uint fontManifestIdMapSize;
    FontManifest **fontManifestIdMap; ///< Index with fontid_t-1

    typedef QList<TextureVariantSpec *> TextureSpecs;
    TextureSpecs textureSpecs;
    TextureSpecs detailTextureSpecs[DETAILVARIANT_CONTRAST_HASHSIZE];
#endif

    Instance(Public *i)
        : Base(i)
        , defaultColorPalette(0)
#ifdef __CLIENT__
        , fontManifestCount(0)
        , fontManifestIdMapSize(0)
        , fontManifestIdMap(0)
#endif
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

        LOG_MSG("Initializing Material collection...");
        /// @note Order here defines the ambigious-URI search order.
        materials.createScheme("Sprites");
        materials.createScheme("Textures");
        materials.createScheme("Flats");
        materials.createScheme("System");

#ifdef __CLIENT__
        LOG_MSG("Initializing Font collection...");
        /// @note Order here defines the ambigious-URI search order.
        createFontScheme("System");
        createFontScheme("Game");
#endif
    }

    ~Instance()
    {
        qDeleteAll(resClasses);
        self.clearAllAnimGroups();
#ifdef __CLIENT__
        self.clearAllFontSchemes();
        clearFontManifests();
#endif
        self.clearAllTextureSchemes();
        clearTextureManifests();
    }

    void clearTextureManifests()
    {
        qDeleteAll(textureSchemes);
        textureSchemes.clear();
        textureSchemeCreationOrder.clear();
    }

    void createTextureScheme(String name)
    {
        DENG2_ASSERT(name.length() >= TextureScheme::min_name_length);

        // Create a new scheme.
        TextureScheme *newScheme = new TextureScheme(name);
        textureSchemes.insert(name.toLower(), newScheme);
        textureSchemeCreationOrder.push_back(newScheme);

        // We want notification when a new manifest is defined in this scheme.
        newScheme->audienceForManifestDefined += this;
    }

#ifdef __CLIENT__
    void clearFontManifests()
    {
        qDeleteAll(fontSchemes);
        fontSchemes.clear();
        fontSchemeCreationOrder.clear();

        // Clear the manifest index/map.
        if(fontManifestIdMap)
        {
            M_Free(fontManifestIdMap); fontManifestIdMap = 0;
            fontManifestIdMapSize = 0;
        }
        fontManifestCount = 0;
    }

    void createFontScheme(String name)
    {
        DENG2_ASSERT(name.length() >= FontScheme::min_name_length);

        // Create a new scheme.
        FontScheme *newScheme = new FontScheme(name);
        fontSchemes.insert(name.toLower(), newScheme);
        fontSchemeCreationOrder.push_back(newScheme);

        // We want notification when a new manifest is defined in this scheme.
        newScheme->audienceForManifestDefined += this;
    }
#endif

    SpriteGroup *spriteGroup(spritenum_t spriteId)
    {
        SpriteGroups::iterator found = spriteGroups.find(spriteId);
        if(found != spriteGroups.end())
        {
            return &found.value();
        }
        return 0;
    }

    SpriteGroup &newSpriteGroup(spritenum_t spriteId)
    {
        DENG2_ASSERT(!spriteGroup(spriteId)); // sanity check.
        return spriteGroups.insert(spriteId, SpriteGroup()).value();
    }

#ifdef __CLIENT__
    void clearRuntimeFonts()
    {
        self.fontScheme("Game").clear();

        self.pruneUnusedTextureSpecs();
    }

    void clearSystemFonts()
    {
        self.fontScheme("System").clear();

        self.pruneUnusedTextureSpecs();
    }

    TextureVariantSpec &linkTextureSpec(TextureVariantSpec *spec)
    {
        DENG2_ASSERT(spec != 0);

        switch(spec->type)
        {
        case TST_GENERAL:
            textureSpecs.append(spec);
            break;
        case TST_DETAIL: {
            int hash = hashDetailTextureSpec(spec->detailVariant);
            detailTextureSpecs[hash].append(spec);
            break; }
        }

        return *spec;
    }

    TextureVariantSpec *findTextureSpec(TextureVariantSpec const &tpl, bool canCreate)
    {
        // Do we already have a concrete version of the template specification?
        switch(tpl.type)
        {
        case TST_GENERAL: {
            foreach(TextureVariantSpec *varSpec, textureSpecs)
            {
                if(*varSpec == tpl)
                {
                    return varSpec;
                }
            }
            break; }

        case TST_DETAIL: {
            int hash = hashDetailTextureSpec(tpl.detailVariant);
            foreach(TextureVariantSpec *varSpec, detailTextureSpecs[hash])
            {
                if(*varSpec == tpl)
                {
                    return varSpec;
                }

            }
            break; }
        }

        // Not found, can we create?
        if(canCreate)
        {
            return &linkTextureSpec(new TextureVariantSpec(tpl));
        }

        return 0;
    }

    TextureVariantSpec *textureSpec(texturevariantusagecontext_t tc, int flags,
        byte border, int tClass, int tMap, int wrapS, int wrapT, int minFilter,
        int magFilter, int anisoFilter, boolean mipmapped, boolean gammaCorrection,
        boolean noStretch, boolean toAlpha)
    {
        static TextureVariantSpec tpl;
        tpl.type = TST_GENERAL;

        configureTextureSpec(tpl.variant, tc, flags, border, tClass, tMap, wrapS,
            wrapT, minFilter, magFilter, anisoFilter, mipmapped, gammaCorrection,
            noStretch, toAlpha);

        // Retrieve a concrete version of the rationalized specification.
        return findTextureSpec(tpl, true);
    }

    TextureVariantSpec *detailTextureSpec(float contrast)
    {
        static TextureVariantSpec tpl;

        tpl.type = TST_DETAIL;
        configureDetailTextureSpec(tpl.detailVariant, contrast);
        return findTextureSpec(tpl, true);
    }

    bool textureSpecInUse(TextureVariantSpec const &spec)
    {
        foreach(Texture *texture, textures)
        foreach(TextureVariant *variant, texture->variants())
        {
            if(&variant->spec() == &spec)
            {
                return true; // Found one; stop.
            }
        }
        return false;
    }

    int pruneUnusedTextureSpecs(TextureSpecs &list)
    {
        int numPruned = 0;
        QMutableListIterator<TextureVariantSpec *> it(list);
        while(it.hasNext())
        {
            TextureVariantSpec *spec = it.next();
            if(!textureSpecInUse(*spec))
            {
                it.remove();
                delete &spec;
                numPruned += 1;
            }
        }
        return numPruned;
    }

    int pruneUnusedTextureSpecs(texturevariantspecificationtype_t specType)
    {
        switch(specType)
        {
        case TST_GENERAL: return pruneUnusedTextureSpecs(textureSpecs);
        case TST_DETAIL: {
            int numPruned = 0;
            for(int i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
            {
                numPruned += pruneUnusedTextureSpecs(detailTextureSpecs[i]);
            }
            return numPruned; }
        }
        return 0;
    }

    void clearAllTextureSpecs()
    {
        qDeleteAll(textureSpecs);
        textureSpecs.clear();

        for(int i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
        {
            qDeleteAll(detailTextureSpecs[i]);
            detailTextureSpecs[i].clear();
        }
    }
#endif // __CLIENT__

    void clearRuntimeTextures()
    {
        self.textureScheme("Flats").clear();
        self.textureScheme("Textures").clear();
        self.textureScheme("Patches").clear();
        self.textureScheme("Sprites").clear();
        self.textureScheme("Details").clear();
        self.textureScheme("Reflections").clear();
        self.textureScheme("Masks").clear();
        self.textureScheme("ModelSkins").clear();
        self.textureScheme("ModelReflectionSkins").clear();
        self.textureScheme("Lightmaps").clear();
        self.textureScheme("Flaremaps").clear();

#ifdef __CLIENT__
        self.pruneUnusedTextureSpecs();
#endif
    }

    void clearSystemTextures()
    {
        self.textureScheme("System").clear();

#ifdef __CLIENT__
        self.pruneUnusedTextureSpecs();
#endif
    }

    void deriveAllTexturesInScheme(String schemeName)
    {
        TextureScheme &scheme = self.textureScheme(schemeName);

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
                    self.declareTexture(uri, flags, def.logicalDimensions(),
                                        Vector2i(), def.origIndex());

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

    /// Observes TextureScheme ManifestDefined.
    void textureSchemeManifestDefined(TextureScheme & /*scheme*/, TextureManifest &manifest)
    {
        // We want notification when the manifest is derived to produce a texture.
        manifest.audienceForTextureDerived += this;
    }

    /// Observes TextureManifest TextureDerived.
    void textureManifestTextureDerived(TextureManifest & /*manifest*/, Texture &texture)
    {
        // Include this new texture in the scheme-agnostic list of instances.
        textures.push_back(&texture);

        // We want notification when the texture is about to be deleted.
        texture.audienceForDeletion += this;
    }

    /// Observes Texture Deletion.
    void textureBeingDeleted(Texture const &texture)
    {
        textures.removeOne(const_cast<Texture *>(&texture));
    }

#ifdef __CLIENT__
    /// Observes FontScheme ManifestDefined.
    void fontSchemeManifestDefined(FontScheme & /*scheme*/, FontManifest &manifest)
    {
        // We want notification when the manifest is derived to produce a resource.
        //manifest.audienceForFontDerived += this;

        // We want notification when the manifest is about to be deleted.
        manifest.audienceForDeletion += this;

        // Acquire a new unique identifier for the manifest.
        fontid_t const id = ++fontManifestCount; // 1-based.
        manifest.setUniqueId(id);

        // Add the new manifest to the id index/map.
        if(fontManifestCount > fontManifestIdMapSize)
        {
            // Allocate more memory.
            fontManifestIdMapSize += 32;
            fontManifestIdMap = (FontManifest **) M_Realloc(fontManifestIdMap, sizeof(*fontManifestIdMap) * fontManifestIdMapSize);
        }
        fontManifestIdMap[fontManifestCount - 1] = &manifest;
    }

#if 0
    /// Observes FontManifest FontDerived.
    void fontManifestFontDerived(FontManifest & /*manifest*/, AbstractFont &font)
    {
        // Include this new font in the scheme-agnostic list of instances.
        fonts.push_back(&font);

        // We want notification when the font is about to be deleted.
        font.audienceForDeletion += this;
    }
#endif

    /// Observes FontManifest Deletion.
    void fontManifestBeingDeleted(FontManifest const &manifest)
    {
        fontManifestIdMap[manifest.uniqueId() - 1 /*1-based*/] = 0;

        // There will soon be one fewer manifest in the system.
        fontManifestCount -= 1;
    }

    /// Observes AbstractFont Deletion.
    void fontBeingDeleted(AbstractFont const &font)
    {
        fonts.removeOne(const_cast<AbstractFont *>(&font));
    }

    /// Observes ColorPalette ColorTableChange
    void colorPaletteColorTableChanged(ColorPalette &colorPalette)
    {
        self.releaseGLTexturesFor(colorPalette);
    }

#endif // __CLIENT__
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
    throw UnknownResourceClassError("ResourceSystem::toClass", QString("Invalid id '%1'").arg(int(id)));
}

void ResourceSystem::clearAllResources()
{
    clearAllRuntimeResources();
    clearAllSystemResources();
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

int ResourceSystem::spriteCount()
{
    return d->spriteGroups.count();
}

bool ResourceSystem::hasSprite(spritenum_t spriteId, int frame)
{
    if(Instance::SpriteGroup *group = d->spriteGroup(spriteId))
    {
        if(frame >= 0 && frame < group->sprites.count())
        {
            return true;
        }
    }
    return false;
}

ResourceSystem::SpriteSet const &ResourceSystem::spriteSet(spritenum_t spriteId)
{
    if(Instance::SpriteGroup *group = d->spriteGroup(spriteId))
    {
        return group->sprites;
    }
    throw MissingSpriteError("ResourceSystem::spriteSet", "Invalid sprite id " + String::number(spriteId));
}

void ResourceSystem::clearAllSprites()
{
    d->spriteGroups.clear();
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

        declareTexture(de::Uri("System", Path(def.path)), Texture::Custom,
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

    LOG_INFO(String("ResourceSystem::initCompositeTextures: Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
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
            if(hasTexture(uri)) continue;

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

            declareTexture(uri, flags, dimensions, origin, uniqueId, &resourceUri);
        }
    }

    // Define any as yet undefined flat textures.
    /// @todo Defer until necessary (manifest texture is first referenced).
    d->deriveAllTexturesInScheme("Flats");

    LOG_INFO(String("ResourceSystem::initFlatTextures: Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
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
            declareTexture(uri, flags, dimensions, origin, uniqueId, &resourceUri);
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
            return &textureScheme(schemeName).findByResourceUri(*resourceUri).texture();
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
    TextureScheme &scheme = textureScheme(schemeName);
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
        TextureManifest &manifest = declareTexture(uri, Texture::Custom, dimensions,
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
        TextureManifest &manifest = findTexture(uri);
        /// @todo We should instead define Materials from patches and return the material id.
        return patchid_t( manifest.uniqueId() );
    }
    catch(MissingManifestError const &)
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

    int uniqueId        = textureScheme("Patches").count() + 1; // 1-based index.
    de::Uri resourceUri = composeLumpIndexResourceUrn(lumpNum);

    try
    {
        TextureManifest &manifest = declareTexture(uri, flags, dimensions, origin,
                                                   uniqueId, &resourceUri);

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

TextureScheme &ResourceSystem::textureScheme(String name) const
{
    LOG_AS("ResourceSystem::textureScheme");
    if(!name.isEmpty())
    {
        TextureSchemes::iterator found = d->textureSchemes.find(name.toLower());
        if(found != d->textureSchemes.end())
        {
            return **found;
        }
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw UnknownSchemeError("ResourceSystem::textureScheme", "No scheme found matching '" + name + "'");
}

bool ResourceSystem::knownTextureScheme(String name) const
{
    if(!name.isEmpty())
    {
        return d->textureSchemes.contains(name.toLower());
    }
    return false;
}

ResourceSystem::TextureSchemes const& ResourceSystem::allTextureSchemes() const
{
    return d->textureSchemes;
}

bool ResourceSystem::hasTexture(de::Uri const &path) const
{
    try
    {
        findTexture(path);
        return true;
    }
    catch(MissingManifestError const &)
    {} // Ignore this error.
    return false;
}

TextureManifest &ResourceSystem::findTexture(de::Uri const &uri) const
{
    LOG_AS("ResourceSystem::findTexture");

    // Perform the search.
    // Is this a URN? (of the form "urn:schemename:uniqueid")
    if(!uri.scheme().compareWithoutCase("urn"))
    {
        String const &pathStr = uri.path().toStringRef();
        int uIdPos = pathStr.indexOf(':');
        if(uIdPos > 0)
        {
            String schemeName = pathStr.left(uIdPos);
            int uniqueId      = pathStr.mid(uIdPos + 1 /*skip delimiter*/).toInt();

            try
            {
                return textureScheme(schemeName).findByUniqueId(uniqueId);
            }
            catch(TextureScheme::NotFoundError const &)
            {} // Ignore, we'll throw our own...
        }
    }
    else
    {
        // No, this is a URI.
        String const &path = uri.path();

        // Does the user want a manifest in a specific scheme?
        if(!uri.scheme().isEmpty())
        {
            try
            {
                return textureScheme(uri.scheme()).find(path);
            }
            catch(TextureScheme::NotFoundError const &)
            {} // Ignore, we'll throw our own...
        }
        else
        {
            // No, check each scheme in priority order.
            foreach(TextureScheme *scheme, d->textureSchemeCreationOrder)
            {
                try
                {
                    return scheme->find(path);
                }
                catch(TextureScheme::NotFoundError const &)
                {} // Ignore, we'll throw our own...
            }
        }
    }

    /// @throw MissingManifestError Failed to locate a matching manifest.
    throw MissingManifestError("ResourceSystem::findTexture", "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

ResourceSystem::AllTextures const &ResourceSystem::allTextures() const
{
    return d->textures;
}

#ifdef __CLIENT__

static int releaseGLTexture(TextureVariant &variant, TextureVariantSpec *spec = 0)
{
    if(!spec || spec == &variant.spec())
    {
        variant.release();
        if(spec) return true; // We're done.
    }
    return 0; // Continue iteration.
}

void ResourceSystem::releaseAllSystemGLTextures()
{
    if(novideo) return;

    LOG_VERBOSE("Releasing System textures...");

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    ClientApp::renderSystem().clearDrawLists();

    GL_ReleaseAllLightingSystemTextures();
    GL_ReleaseAllFlareTextures();

    releaseGLTexturesByScheme("System");
    Rend_ParticleReleaseSystemTextures();
    releaseFontGLTexturesByScheme("System");

    pruneUnusedTextureSpecs();
}

void ResourceSystem::releaseAllRuntimeGLTextures()
{
    if(novideo) return;

    LOG_VERBOSE("Releasing Runtime textures...");

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    ClientApp::renderSystem().clearDrawLists();

    // texture-wrapped GL textures; textures, flats, sprites...
    releaseGLTexturesByScheme("Flats");
    releaseGLTexturesByScheme("Textures");
    releaseGLTexturesByScheme("Patches");
    releaseGLTexturesByScheme("Sprites");
    releaseGLTexturesByScheme("Details");
    releaseGLTexturesByScheme("Reflections");
    releaseGLTexturesByScheme("Masks");
    releaseGLTexturesByScheme("ModelSkins");
    releaseGLTexturesByScheme("ModelReflectionSkins");
    releaseGLTexturesByScheme("Lightmaps");
    releaseGLTexturesByScheme("Flaremaps");
    GL_ReleaseTexturesForRawImages();

    Rend_ParticleReleaseExtraTextures();
    releaseFontGLTexturesByScheme("Game");

    pruneUnusedTextureSpecs();
}

void ResourceSystem::releaseAllGLTextures()
{
    releaseAllRuntimeGLTextures();
    releaseAllSystemGLTextures();
}

void ResourceSystem::releaseGLTexturesFor(ColorPalette const &colorPalette)
{
    foreach(Texture *texture, d->textures)
    {
        colorpalette_analysis_t *cp = reinterpret_cast<colorpalette_analysis_t *>(texture->analysisDataPointer(Texture::ColorPaletteAnalysis));
        if(cp && cp->paletteId == colorpaletteid_t(colorPalette.id()))
        {
            releaseGLTexturesFor(*texture);
        }
    }
}

void ResourceSystem::releaseGLTexturesFor(TextureVariant &tex)
{
    releaseGLTexture(tex);
}

void ResourceSystem::releaseGLTexturesFor(Texture &texture)
{
    foreach(TextureVariant *variant, texture.variants())
    {
        releaseGLTexture(*variant);
    }
}

void ResourceSystem::releaseGLTexturesFor(Texture &texture, TextureVariantSpec &spec)
{
    foreach(TextureVariant *variant, texture.variants())
    {
        if(releaseGLTexture(*variant, &spec))
        {
            break;
        }
    }
}

void ResourceSystem::releaseGLTexturesByScheme(String schemeName)
{
    if(schemeName.isEmpty()) return;

    PathTreeIterator<TextureScheme::Index> iter(textureScheme(schemeName).index().leafNodes());
    while(iter.hasNext())
    {
        TextureManifest &manifest = iter.next();
        if(manifest.hasTexture())
        {
            releaseGLTexturesFor(manifest.texture());
        }
    }
}

void ResourceSystem::clearAllTextureSpecs()
{
    d->clearAllTextureSpecs();
}

void ResourceSystem::pruneUnusedTextureSpecs()
{
    if(Sys_IsShuttingDown()) return;

    int numPruned = 0;
    numPruned += d->pruneUnusedTextureSpecs(TST_GENERAL);
    numPruned += d->pruneUnusedTextureSpecs(TST_DETAIL);

#ifdef DENG_DEBUG
    LOG_VERBOSE("Pruned %i unused texture variant %s.")
        << numPruned << (numPruned == 1? "specification" : "specifications");
#endif
}

TextureVariantSpec &ResourceSystem::textureSpec(texturevariantusagecontext_t tc,
    int flags, byte border, int tClass, int tMap, int wrapS, int wrapT, int minFilter,
    int magFilter, int anisoFilter, boolean mipmapped, boolean gammaCorrection,
    boolean noStretch, boolean toAlpha)
{
    TextureVariantSpec *tvs =
        d->textureSpec(tc, flags, border, tClass, tMap, wrapS, wrapT, minFilter,
                       magFilter, anisoFilter, mipmapped, gammaCorrection,
                       noStretch, toAlpha);

#ifdef DENG_DEBUG
    if(tClass || tMap)
    {
        DENG2_ASSERT(tvs->variant.flags & TSF_HAS_COLORPALETTE_XLAT);
        DENG2_ASSERT(tvs->variant.tClass == tClass);
        DENG2_ASSERT(tvs->variant.tMap == tMap);
    }
#endif

    return *tvs;
}

TextureVariantSpec &ResourceSystem::detailTextureSpec(float contrast)
{
    return *d->detailTextureSpec(contrast);
}

FontScheme &ResourceSystem::fontScheme(String name) const
{
    LOG_AS("ResourceSystem::fontScheme");
    if(!name.isEmpty())
    {
        FontSchemes::iterator found = d->fontSchemes.find(name.toLower());
        if(found != d->fontSchemes.end())
        {
            return **found;
        }
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw UnknownSchemeError("ResourceSystem::fontScheme", "No scheme found matching '" + name + "'");
}

bool ResourceSystem::knownFontScheme(String name) const
{
    if(!name.isEmpty())
    {
        return d->fontSchemes.contains(name.toLower());
    }
    return false;
}

ResourceSystem::FontSchemes const &ResourceSystem::allFontSchemes() const
{
    return d->fontSchemes;
}

bool ResourceSystem::hasFont(de::Uri const &path) const
{
    try
    {
        findFont(path);
        return true;
    }
    catch(MissingManifestError const &)
    {} // Ignore this error.
    return false;
}

FontManifest &ResourceSystem::findFont(de::Uri const &uri) const
{
    LOG_AS("ResourceSystem::findFont");

    // Perform the search.
    // Is this a URN? (of the form "urn:schemename:uniqueid")
    if(!uri.scheme().compareWithoutCase("urn"))
    {
        String const &pathStr = uri.path().toStringRef();
        int uIdPos = pathStr.indexOf(':');
        if(uIdPos > 0)
        {
            String schemeName = pathStr.left(uIdPos);
            int uniqueId      = pathStr.mid(uIdPos + 1 /*skip delimiter*/).toInt();

            try
            {
                return fontScheme(schemeName).findByUniqueId(uniqueId);
            }
            catch(FontScheme::NotFoundError const &)
            {} // Ignore, we'll throw our own...
        }
    }
    else
    {
        // No, this is a URI.
        String const &path = uri.path();

        // Does the user want a manifest in a specific scheme?
        if(!uri.scheme().isEmpty())
        {
            try
            {
                return fontScheme(uri.scheme()).find(path);
            }
            catch(FontScheme::NotFoundError const &)
            {} // Ignore, we'll throw our own...
        }
        else
        {
            // No, check each scheme in priority order.
            foreach(FontScheme *scheme, d->fontSchemeCreationOrder)
            {
                try
                {
                    return scheme->find(path);
                }
                catch(FontScheme::NotFoundError const &)
                {} // Ignore, we'll throw our own...
            }
        }
    }

    /// @throw MissingManifestError Failed to locate a matching manifest.
    throw MissingManifestError("ResourceSystem::findFont", "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

FontManifest &ResourceSystem::toFontManifest(fontid_t id) const
{
    if(id > 0 && id <= d->fontManifestCount)
    {
        duint32 idx = id - 1; // 1-based index.
        if(d->fontManifestIdMap[idx])
        {
            return *d->fontManifestIdMap[idx];
        }

        // Internal bookeeping error.
        DENG2_ASSERT(false);
    }

    /// @throw UnknownIdError The specified manifest id is invalid.
    throw UnknownFontIdError("ResourceSystem::toFontManifest", QString("Invalid font ID %1, valid range [1..%2)").arg(id).arg(d->fontManifestCount + 1));
}

ResourceSystem::AllFonts const &ResourceSystem::allFonts() const
{
    return d->fonts;
}

void ResourceSystem::clearFontDefinitionLinks()
{
    foreach(AbstractFont *font, d->fonts)
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
        FontManifest &manifest = declareFont(uri);

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
    catch(UnknownSchemeError const &er)
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
        FontManifest &manifest = declareFont(uri);

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
    catch(UnknownSchemeError const &er)
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

void ResourceSystem::releaseFontGLTexturesByScheme(String schemeName)
{
    if(schemeName.isEmpty()) return;

    PathTreeIterator<FontScheme::Index> iter(fontScheme(schemeName).index().leafNodes());
    while(iter.hasNext())
    {
        FontManifest &manifest = iter.next();
        if(manifest.hasResource())
        {
            manifest.resource().glDeinit();
        }
    }
}

#endif // __CLIENT__

void ResourceSystem::clearAllAnimGroups()
{
    qDeleteAll(d->animGroups);
    d->animGroups.clear();
}

int ResourceSystem::animGroupCount()
{
    return d->animGroups.count();
}

AnimGroup *ResourceSystem::animGroup(int uniqueId)
{
    LOG_AS("ResourceSystem::animGroup");
    if(uniqueId > 0 && uniqueId <= d->animGroups.count())
    {
        return d->animGroups.at(uniqueId - 1);
    }
    LOG_DEBUG("Invalid group #%i, returning NULL.") << uniqueId;
    return 0;
}

AnimGroup &ResourceSystem::newAnimGroup(int flags)
{
    LOG_AS("ResourceSystem");
    int const uniqueId = d->animGroups.count() + 1; // 1-based.
    // Allocating one by one is inefficient but it doesn't really matter.
    d->animGroups.append(new AnimGroup(uniqueId, flags));
    return *d->animGroups.last();
}

struct SpriteFrameDef
{
    byte frame[2];
    byte rotation[2];
    Material *mat;
};

struct SpriteDef
{
    String name;
    typedef QList<SpriteFrameDef> Frames;
    Frames frames;
};

// Tempory storage, used when reading sprite definitions.
typedef QList<SpriteDef> SpriteDefs;

/**
 * In DOOM, a sprite frame is a patch texture contained in a lump existing
 * between the S_START and S_END marker lumps (in WAD) whose lump name matches
 * the following pattern:
 *
 *      NAME|A|R(A|R) (for example: "TROOA0" or "TROOA2A8")
 *
 * NAME: Four character name of the sprite.
 * A: Animation frame ordinal 'A'... (ASCII).
 * R: Rotation angle 0...8
 *    0 : Use this frame for ALL angles.
 *    1...8 : Angle of rotation in 45 degree increments.
 *
 * The second set of (optional) frame and rotation characters instruct that the
 * same sprite frame is to be used for an additional frame but that the sprite
 * patch should be flipped horizontally (right to left) during the loading phase.
 *
 * Sprite rotation 0 is facing the viewer, rotation 1 is one angle turn CLOCKWISE
 * around the axis. This is not the same as the angle, which increases counter
 * clockwise (protractor).
 */
static SpriteDefs generateSpriteDefs()
{
    SpriteDefs spriteDefs;

    PathTreeIterator<TextureScheme::Index> iter(App_ResourceSystem().textureScheme("Sprites").index().leafNodes());
    while(iter.hasNext())
    {
        TextureManifest &manifest = iter.next();

        // Have we already encountered this name?
        String spriteName = QString(QByteArray::fromPercentEncoding(manifest.path().toUtf8()));

        SpriteDef *spriteDef = 0;
        for(int i = 0; i < spriteDefs.count(); ++i)
        {
            SpriteDef const &cand = spriteDefs[i];
            if(!cand.name.compareWithoutCase(spriteName, 4))
            {
                spriteDef = &spriteDefs[i];
            }
        }

        if(!spriteDef)
        {
            // An entirely new sprite.
            spriteDefs.append(SpriteDef());
            spriteDef = &spriteDefs.last();

            spriteDef->name = spriteName.left(4);
        }

        // Add the frame(s).
        int const frameNumber    = spriteName.at(4).toUpper().unicode() - QChar('A').unicode() + 1;
        int const rotationNumber = spriteName.at(5).digitValue();

        SpriteFrameDef *frame = 0;
        for(int i = 0; i < spriteDef->frames.count(); ++i)
        {
            SpriteFrameDef &cand = spriteDef->frames[i];
            if(cand.frame[0]    == frameNumber &&
               cand.rotation[0] == rotationNumber)
            {
                frame = &cand;
                break;
            }
        }

        if(!frame)
        {
            // A new frame.
            spriteDef->frames.append(SpriteFrameDef());
            frame = &spriteDef->frames.last();
        }

        frame->mat         = &App_Materials().find(de::Uri("Sprites", manifest.path())).material();
        frame->frame[0]    = frameNumber;
        frame->rotation[0] = rotationNumber;

        // Not mirrored?
        if(spriteName.length() >= 8)
        {
            frame->frame[1]    = spriteName.at(6).toUpper().unicode() - QChar('A').unicode() + 1;
            frame->rotation[1] = spriteName.at(7).digitValue();
        }
        else
        {
            frame->frame[1]    = 0;
            frame->rotation[1] = 0;
        }
    }

    return spriteDefs;
}

void ResourceSystem::initSprites()
{
    Time begunAt;

    LOG_AS("ResourceSystem");
    LOG_MSG("Building sprites...");

    clearAllSprites();

    SpriteDefs defs = generateSpriteDefs();

    if(!defs.isEmpty())
    {
        //int spriteNameCount = de::max(defs.count(), countSprNames.num);
        //groups.reserve(spriteNameCount);

        // Build the final sprites.
        int customIdx = 0;
        foreach(SpriteDef const &def, defs)
        {
            spritenum_t spriteId = Def_GetSpriteNum(def.name.toUtf8().constData());
            if(spriteId == -1)
            {
                spriteId = countSprNames.num + customIdx++;
            }

            Instance::SpriteGroup &group = d->newSpriteGroup(spriteId);

            //std::memset(sprTemp, -1, sizeof(sprTemp));
            Sprite sprTemp[128];

            int maxSprite = -1;
            foreach(SpriteFrameDef const &frameDef, def.frames)
            {
                int frame = frameDef.frame[0] - 1;
                DENG2_ASSERT(frame >= 0);
                if(frame < 128)
                {
                    sprTemp[frame].newViewAngle(frameDef.mat, frameDef.rotation[0], false);
                    if(frame > maxSprite)
                    {
                        maxSprite = frame;
                    }
                }

                if(frameDef.frame[1])
                {
                    frame = frameDef.frame[1] - 1;
                    DENG2_ASSERT(frame >= 0);
                    if(frame < 128)
                    {
                        sprTemp[frame].newViewAngle(frameDef.mat, frameDef.rotation[1], true);
                        if(frame > maxSprite)
                        {
                            maxSprite = frame;
                        }
                    }
                }
            }
            ++maxSprite;

            /*for(int k = 0; k < maxSprite; ++k)
            {
                if(int(sprTemp[k]._rotate) == -1) // Ahem!
                {
                    // No rotations were found for that frame at all.
                    Error("initSprites", QString("No patches found for %1 frame %2")
                                              .arg(def.name).arg(char(k + 'A')));
                }

                if(sprTemp[k]._rotate)
                {
                    // Must have all 8 frames.
                    for(int rotation = 0; rotation < 8; ++rotation)
                    {
                        if(!sprTemp[k]._mats[rotation])
                        {
                            Error("initSprites", QString("Sprite %1 frame %2 is missing rotations")
                                                      .arg(def.name).arg(char(k + 'A')));
                        }
                    }
                }
            }*/

            for(int k = 0; k < maxSprite; ++k)
            {
                group.sprites.append(new Sprite(sprTemp[k]));
            }
        }
    }

    LOG_INFO(String("Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

#ifdef __CLIENT__
void ResourceSystem::cacheSpriteSet(spritenum_t spriteId, MaterialVariantSpec const &spec)
{
    if(Instance::SpriteGroup *group = d->spriteGroup(spriteId))
    {
        foreach(Sprite *sprite, group->sprites)
        foreach(SpriteViewAngle const &viewAngle, sprite->viewAngles())
        {
            if(Material *material = viewAngle.material)
            {
                cacheMaterial(*material, spec);
            }
        }
    }
}
#endif

void ResourceSystem::clearAllColorPalettes()
{
    d->colorPaletteNames.clear();

    qDeleteAll(d->colorPalettes);
    d->colorPalettes.clear();

    d->defaultColorPalette = 0;
}

int ResourceSystem::colorPaletteCount() const
{
    return d->colorPalettes.count();
}

ColorPalette &ResourceSystem::colorPalette(colorpaletteid_t id) const
{
    // Choose the default palette?
    if(!id)
    {
        id = d->defaultColorPalette;
    }

    Instance::ColorPalettes::const_iterator found = d->colorPalettes.find(id);
    if(found != d->colorPalettes.end())
    {
        return *found.value();
    }
    /// @throw MissingColorPaletteError An unknown/invalid id was specified.
    throw MissingColorPaletteError("ResourceSystem::colorPalette", QString("Invalid id %1").arg(id));
}

String ResourceSystem::colorPaletteName(ColorPalette &palette) const
{
    QList<String> names = d->colorPaletteNames.keys(&palette);
    if(!names.isEmpty())
    {
        return names.first();
    }
    return String();
}

bool ResourceSystem::hasColorPalette(String name) const
{
    return d->colorPaletteNames.contains(name);
}

ColorPalette &ResourceSystem::colorPalette(String name) const
{
    Instance::ColorPaletteNames::const_iterator found = d->colorPaletteNames.find(name);
    if(found != d->colorPaletteNames.end())
    {
        return *found.value();
    }
    /// @throw MissingColorPaletteError An unknown name was specified.
    throw MissingColorPaletteError("ResourceSystem::colorPalette", "Unknown name '" + name + "'");
}

void ResourceSystem::addColorPalette(ColorPalette &newPalette, String const &name)
{
    // Do we already own this palette?
    Instance::ColorPalettes::const_iterator found = d->colorPalettes.find(newPalette.id());
    if(found != d->colorPalettes.end())
    {
        return;
    }

    d->colorPalettes.insert(newPalette.id(), &newPalette);

#ifdef __CLIENT__
    // Observe changes to the color table so we can schedule texture updates.
    newPalette.audienceForColorTableChange += d;
#endif

    if(!name.isEmpty())
    {
        d->colorPaletteNames.insert(name, &newPalette);
    }

    // If this is the first palette automatically set it as the default.
    if(d->colorPalettes.count() == 1)
    {
        d->defaultColorPalette = newPalette.id();
    }
}

colorpaletteid_t ResourceSystem::defaultColorPalette() const
{
    return d->defaultColorPalette;
}

void ResourceSystem::setDefaultColorPalette(ColorPalette *newDefaultPalette)
{
    d->defaultColorPalette = newDefaultPalette? newDefaultPalette->id() : 0;
}

#ifdef __CLIENT__

void ResourceSystem::restartAllMaterialAnimations()
{
    foreach(Material *material, d->materials.all())
    foreach(MaterialAnimation *animation, material->animations())
    {
        animation->restart();
    }
}

static int findSpriteOwner(thinker_t *th, void *context)
{
    mobj_t *mo = reinterpret_cast<mobj_t *>(th);
    int const sprite = *static_cast<int *>(context);

    if(mo->type >= 0 && mo->type < defs.count.mobjs.num)
    {
        /// @todo optimize: traverses the entire state list!
        for(int i = 0; i < defs.count.states.num; ++i)
        {
            if(stateOwners[i] != &mobjInfo[mo->type])
            {
                continue;
            }

            state_t *state = Def_GetState(i);
            DENG2_ASSERT(state != 0);

            if(state->sprite == sprite)
            {
                return true; // Found one.
            }
        }
    }

    return false; // Continue iteration.
}

void ResourceSystem::cacheForCurrentMap()
{
    // Don't precache when playing a demo (why not? -ds).
    if(playback) return;

    Map &map = App_World().map();

    if(precacheMapMaterials)
    {
        MaterialVariantSpec const &spec = Rend_MapSurfaceMaterialSpec();

        foreach(Line *line, map.lines())
        for(int i = 0; i < 2; ++i)
        {
            LineSide &side = line->side(i);
            if(!side.hasSections()) continue;

            if(side.middle().hasMaterial())
                cacheMaterial(side.middle().material(), spec);

            if(side.top().hasMaterial())
                cacheMaterial(side.top().material(), spec);

            if(side.bottom().hasMaterial())
                cacheMaterial(side.bottom().material(), spec);
        }

        foreach(Sector *sector, map.sectors())
        {
            // Skip sectors with no line sides as their planes will never be drawn.
            if(!sector->sideCount()) continue;

            foreach(Plane *plane, sector->planes())
            {
                if(plane->surface().hasMaterial())
                    cacheMaterial(plane->surface().material(), spec);
            }
        }
    }

    if(precacheSprites)
    {
        MaterialVariantSpec const &spec = Rend_SpriteMaterialSpec();

        for(int i = 0; i < App_ResourceSystem().spriteCount(); ++i)
        {
            if(map.thinkers().iterate(reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                                      0x1/*mobjs are public*/,
                                      findSpriteOwner, &i))
            {
                // This sprite is used by some state of at least one mobj.
                cacheSpriteSet(i, spec);
            }
        }
    }

     // Sky models usually have big skins.
    Sky_Cache();

    // Precache model skins?
    if(useModels && precacheSkins)
    {
        // All mobjs are public.
        map.thinkers().iterate(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1,
                               Models_CacheForMobj);
    }
}

void ResourceSystem::processCacheQueue()
{
    d->materials.processCacheQueue();
}

void ResourceSystem::purgeCacheQueue()
{
    d->materials.purgeCacheQueue();
}

void ResourceSystem::cacheMaterial(Material &material, de::MaterialVariantSpec const &spec,
    bool cacheGroups)
{
    d->materials.cache(material, spec, cacheGroups);
}

#endif // __CLIENT__

template <typename ManifestType>
static bool pathBeginsWithComparator(ManifestType const &manifest, void *context)
{
    Path const *path = reinterpret_cast<Path*>(context);
    /// @todo Use PathTree::Node::compare()
    return manifest.path().toStringRef().beginsWith(*path, Qt::CaseInsensitive);
}

/**
 * @todo This logic should be implemented in de::PathTree -ds
 */
template <typename SchemeType, typename ManifestType>
static int collectManifestsInScheme(SchemeType const &scheme,
    bool (*predicate)(ManifestType const &manifest, void *context), void *context,
    QList<ManifestType *> *storage = 0)
{
    int count = 0;
    PathTreeIterator<SchemeType::Index> iter(scheme.index().leafNodes());
    while(iter.hasNext())
    {
        ManifestType &manifest = iter.next();
        if(predicate(manifest, context))
        {
            count += 1;
            if(storage) // Store mode?
            {
                storage->push_back(&manifest);
            }
        }
    }
    return count;
}

static QList<TextureManifest *> collectTextureManifests(TextureScheme *scheme,
    Path const &path, QList<TextureManifest *> *storage = 0)
{
    int count = 0;

    if(scheme)
    {
        // Consider resources in the specified scheme only.
        count += collectManifestsInScheme<TextureScheme, TextureManifest>(*scheme,
                    pathBeginsWithComparator, (void *)&path, storage);
    }
    else
    {
        // Consider resources in any scheme.
        foreach(TextureScheme *scheme, App_ResourceSystem().allTextureSchemes())
        {
            count += collectManifestsInScheme<TextureScheme, TextureManifest>(*scheme,
                        pathBeginsWithComparator, (void *)&path, storage);
        }
    }

    // Are we done?
    if(storage) return *storage;

    // Collect and populate.
    QList<TextureManifest *> result;
    if(count == 0) return result;

#ifdef DENG2_QT_4_7_OR_NEWER
    result.reserve(count);
#endif
    return collectTextureManifests(scheme, path, &result);
}

#ifdef __CLIENT__
static QList<FontManifest *> collectFontManifests(FontScheme *scheme,
    Path const &path, QList<FontManifest *> *storage = 0)
{
    int count = 0;

    if(scheme)
    {
        // Consider resources in the specified scheme only.
        count += collectManifestsInScheme<FontScheme, FontManifest>(*scheme,
                    pathBeginsWithComparator, (void *)&path, storage);
    }
    else
    {
        // Consider resources in any scheme.
        foreach(FontScheme *scheme, App_ResourceSystem().allFontSchemes())
        {
            count += collectManifestsInScheme<FontScheme, FontManifest>(*scheme,
                        pathBeginsWithComparator, (void *)&path, storage);
        }
    }

    // Are we done?
    if(storage) return *storage;

    // Collect and populate.
    QList<FontManifest *> result;
    if(count == 0) return result;

#ifdef DENG2_QT_4_7_OR_NEWER
    result.reserve(count);
#endif
    return collectFontManifests(scheme, path, &result);
}
#endif // __CLIENT__

/**
 * Decode and then lexicographically compare the two manifest paths,
 * returning @c true if @a is less than @a b.
 */
template <typename ManifestType>
static bool compareManifestPathsAssending(ManifestType const *a, ManifestType const *b)
{
    String pathA(QString(QByteArray::fromPercentEncoding(a->path().toUtf8())));
    String pathB(QString(QByteArray::fromPercentEncoding(b->path().toUtf8())));
    return pathA.compareWithoutCase(pathB) < 0;
}

/**
 * @param scheme    Texture subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Texture path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printTextureIndex2(TextureScheme *scheme, Path const &like,
    de::Uri::ComposeAsTextFlags composeUriFlags)
{
    QList<TextureManifest *> found = collectTextureManifests(scheme, like);
    if(found.isEmpty()) return 0;

    bool const printSchemeName = !(composeUriFlags & de::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known textures";
    if(!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if(!like.isEmpty())
        heading += " like \"" _E(b) + like.toStringRef() + _E(.) "\"";
    LOG_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index key.
    qSort(found.begin(), found.end(), compareManifestPathsAssending<TextureManifest>);
    int numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    foreach(TextureManifest *manifest, found)
    {
        String info = String("%1: %2%3")
                        .arg(idx, numFoundDigits)
                        .arg(manifest->hasTexture()? _E(1) : _E(2))
                        .arg(manifest->description(composeUriFlags));

        LOG_MSG("  " _E(>)) << info;
        idx++;
    }

    return found.count();
}

static void printTextureIndex(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printTextureIndex2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        LOG_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if(App_ResourceSystem().knownTextureScheme(search.scheme()))
    {
        printTotal = printTextureIndex2(&App_ResourceSystem().textureScheme(search.scheme()),
                                        search.path(), flags | de::Uri::OmitScheme);
        LOG_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach(TextureScheme *scheme, App_ResourceSystem().allTextureSchemes())
        {
            int numPrinted = printTextureIndex2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if(numPrinted)
            {
                LOG_MSG(_E(R));
                printTotal += numPrinted;
            }
        }
    }
    LOG_MSG("Found " _E(b) "%i" _E(.) " %s.") << printTotal << (printTotal == 1? "texture" : "textures in total");
}

#ifdef __CLIENT__

/**
 * @param scheme    Resource subspace scheme being printed. Can be @c NULL in
 *                  which case resources are printed from all schemes.
 * @param like      Resource path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printFontIndex2(FontScheme *scheme, Path const &like,
    de::Uri::ComposeAsTextFlags composeUriFlags)
{
    QList<FontManifest *> found = collectFontManifests(scheme, like);
    if(found.isEmpty()) return 0;

    bool const printSchemeName = !(composeUriFlags & de::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known fonts";
    if(!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if(!like.isEmpty())
        heading += " like \"" _E(b) + like.toStringRef() + _E(.) "\"";
    LOG_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index.
    qSort(found.begin(), found.end(), compareManifestPathsAssending<FontManifest>);
    int numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    foreach(FontManifest *manifest, found)
    {
        String info = String("%1: %2%3" _E(.))
                        .arg(idx, numFoundDigits)
                        .arg(manifest->hasResource()? _E(1) : _E(2))
                        .arg(manifest->description(composeUriFlags));

        LOG_MSG("  " _E(>)) << info;
        idx++;
    }

    return found.count();
}

static void printFontIndex(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printFontIndex2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        LOG_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if(App_ResourceSystem().knownFontScheme(search.scheme()))
    {
        printTotal = printFontIndex2(&App_ResourceSystem().fontScheme(search.scheme()),
                                     search.path(), flags | de::Uri::OmitScheme);
        LOG_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach(FontScheme *scheme, App_ResourceSystem().allFontSchemes())
        {
            int numPrinted = printFontIndex2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if(numPrinted)
            {
                LOG_MSG(_E(R));
                printTotal += numPrinted;
            }
        }
    }
    LOG_MSG("Found " _E(b) "%i" _E(.) " %s.") << printTotal << (printTotal == 1? "font" : "fonts in total");
}

#endif // __CLIENT__

static bool isKnownTextureSchemeCallback(String name)
{
    return App_ResourceSystem().knownTextureScheme(name);
}

#ifdef __CLIENT__
static bool isKnownFontSchemeCallback(String name)
{
    return App_ResourceSystem().knownFontScheme(name);
}
#endif

D_CMD(ListTextures)
{
    DENG2_UNUSED(src);

    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1, &isKnownTextureSchemeCallback);

    if(!search.scheme().isEmpty() &&
       !App_ResourceSystem().knownTextureScheme(search.scheme()))
    {
        LOG_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    printTextureIndex(search);
    return true;
}

#ifdef __CLIENT__
D_CMD(ListFonts)
{
    DENG2_UNUSED(src);

    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1, &isKnownFontSchemeCallback);
    if(!search.scheme().isEmpty() &&
       !App_ResourceSystem().knownFontScheme(search.scheme()))
    {
        LOG_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    printFontIndex(search);
    return true;
}
#endif // __CLIENT__

#ifdef DENG_DEBUG
D_CMD(PrintTextureStats)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_MSG(_E(1) "Texture Statistics:");
    foreach(TextureScheme *scheme, App_ResourceSystem().allTextureSchemes())
    {
        TextureScheme::Index const &index = scheme->index();

        uint const count = index.count();
        LOG_MSG("Scheme: %s (%u %s)")
            << scheme->name() << count << (count == 1? "texture" : "textures");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif // DENG_DEBUG

#if defined(__CLIENT__) && defined(DENG_DEBUG)
D_CMD(PrintFontStats)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_MSG(_E(1) "Font Statistics:");
    foreach(FontScheme *scheme, App_ResourceSystem().allFontSchemes())
    {
        FontScheme::Index const &index = scheme->index();

        uint const count = index.count();
        LOG_MSG("Scheme: %s (%u %s)")
            << scheme->name() << count << (count == 1? "font" : "fonts");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif // __CLIENT__ && DENG_DEBUG

void ResourceSystem::consoleRegister() // static
{
    C_CMD("listtextures",   "ss",   ListTextures)
    C_CMD("listtextures",   "s",    ListTextures)
    C_CMD("listtextures",   "",     ListTextures)

#ifdef DENG_DEBUG
    C_CMD("texturestats",   NULL,   PrintTextureStats)
#endif

#ifdef __CLIENT__
    C_CMD("listfonts",      "ss",   ListFonts)
    C_CMD("listfonts",      "s",    ListFonts)
    C_CMD("listfonts",      "",     ListFonts)

#  ifdef DENG_DEBUG
    C_CMD("fontstats",      NULL,   PrintFontStats)
#  endif
#endif

    Texture::consoleRegister();
    Materials::consoleRegister();
}
