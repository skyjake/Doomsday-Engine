/** @file resourcesystem.h  Resource subsystem.
 * @ingroup resource
 *
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef RESOURCESYSTEM_H
#define RESOURCESYSTEM_H

#include <QList>
#include <QMap>
#include <QSet>
#include <de/Error>
#include <de/Record>
#include <de/String>
#include <de/System>
#include <doomsday/defs/ded.h>
#include <doomsday/filesys/wad.h>
#include <doomsday/filesys/zip.h>
#include <doomsday/uri.h>
#include <doomsday/resource/mapmanifest.h>
#include <doomsday/resource/resources.h>

#include "resource/animgroup.h"
#include "resource/colorpalette.h"
#ifdef __CLIENT__
#  include "AbstractFont"
#  include "BitmapFont"
#  include "CompositeBitmapFont"
#  include "FontScheme"
#  include "MaterialVariantSpec"
#  include "Model"
#  include "ModelDef"
#endif

#include "Material"
#include "MaterialScheme"
#include "Texture"
#include "TextureScheme"
#include "resource/rawtexture.h"

/**
 * Logical resources; materials, packages, textures, etc...
 *
 * Resource pointers are considered @em eternal in the sense that they will
 * continue to reference the same logical resource data, even after the engine
 * is reset. Public resource identifiers (e.g., materialid_t) are similarly
 * eternal.
 *
 * Resource names (paths) are semi-independant from the resources. There may be
 * multiple names for any given resource (aliases). The only requirement is that
 * their symbolic name must be unique among resources in the same scheme.
 *
 * @par Classification
 *
 * @em Runtime resources are not loaded until precached or actually needed. They
 * may be cleared, in which case they will be reloaded when needed.
 *
 * @em System resources are loaded at startup and remain in memory all the time.
 * After clearing they must be manually reloaded.
 *
 * @par Texture resources
 *
 * @em Clearing a texture is to 'undefine' it - any names bound to it will be
 * deleted and any GL textures acquired for it are 'released'. The logical
 * Texture instance used to represent it is also deleted.
 *
 * @em Releasing a texture will leave it defined (any names bound to it will
 * persist) but any GL textures acquired for it are 'released'. Note that the
 * logical Texture instance used to represent is NOT be deleted.
 *
 * @ingroup resource
 */
class ResourceSystem : public Resources
{
public:
    /// An unknown resource scheme was referenced. @ingroup errors
    DENG2_ERROR(UnknownSchemeError);

    /// The referenced resource was not found. @ingroup errors
    DENG2_ERROR(MissingResourceError);

    /// An unknown material group was referenced. @ingroup errors
    DENG2_ERROR(UnknownMaterialGroupError);

    /// The specified material id was invalid (out of range). @ingroup errors
    DENG2_ERROR(UnknownMaterialIdError);

#ifdef __CLIENT__
    /// The referenced model def was not found. @ingroup errors
    DENG2_ERROR(MissingModelDefError);

    /// The specified font id was invalid (out of range). @ingroup errors
    DENG2_ERROR(UnknownFontIdError);
#endif

    typedef QSet<MaterialManifest *> MaterialManifestSet;
    typedef MaterialManifestSet MaterialManifestGroup; // Alias
    typedef QList<MaterialManifestGroup *> MaterialManifestGroups;

    typedef QMap<de::String, de::TextureScheme *> TextureSchemes;
    typedef QList<de::Texture *> AllTextures;

#ifdef __CLIENT__
    typedef QMap<de::String, de::FontScheme *> FontSchemes;
    typedef QList<AbstractFont *> AllFonts;
#endif

    typedef QMap<de::dint, de::Record> SpriteSet;  ///< frame => Sprite

public:
    /**
     * Construct a new resource system, configuring all resource classes and
     * the associated resource collection schemes.
     */
    ResourceSystem();

    void clearAllResources();
    void clearAllRuntimeResources();
    void clearAllSystemResources();

    /**
     * Returns @c true if a Sprite exists with given unique @a id and @a frame number.
     */
    bool hasSprite(spritenum_t id, de::dint frame);

    /**
     * Lookup a Sprite by it's unique @a id and @a frame number.
     *
     * @see hasSprite(), spritePtr()
     */
    de::Record &sprite(spritenum_t id, de::dint frame);

    /**
     * Returns a pointer to the identified Sprite.
     *
     * @see hasSprite()
     */
    inline de::Record *spritePtr(spritenum_t id, de::dint frame) {
        return hasSprite(id, frame) ? &sprite(id, frame) : nullptr;
    }

    /**
     * Returns the SpriteSet associated with the given unique @a id.
     */
    SpriteSet const &spriteSet(spritenum_t id);

    /**
     * Returns the total number of SpriteSets.
     */
    de::dint spriteCount();

    /**
     * Determines if a material exists for a @a path.
     *
     * @return @c true if a material exists; otherwise @a false.
     *
     * @see hasMaterialManifest(), MaterialManifest::hasMaterial()
     */
    inline bool hasMaterial(de::Uri const &path) const {
        if(hasMaterialManifest(path)) return materialManifest(path).hasMaterial();
        return false;
    }

    /**
     * Lookup a material resource for the specified @a path.
     *
     * @return The found material.
     *
     * @see MaterialManifest::material()
     */
    inline Material &material(de::Uri const &path) const {
        return materialManifest(path).material();
    }

    /**
     * Returns a pointer to the identified Material.
     *
     * @see hasMaterialManifest(), MaterialManifest::materialPtr()
     */
    Material *materialPtr(de::Uri const &path);

    /**
     * Determines if a manifest exists for a material on @a path.
     *
     * @return @c true if a manifest exists; otherwise @a false.
     */
    bool hasMaterialManifest(de::Uri const &path) const;

    /**
     * Look up a material manifest by its unique resource @a path.
     *
     * @param path  The path to search for.
     * @return  Found material manifest.
     */
    MaterialManifest &materialManifest(de::Uri const &path) const;

    /**
     * Look up a material manifest by its unique resource @a path.
     *
     * @param path  The path to search for.
     * @return  Found material manifest, or nullptr if not found.
     */
    MaterialManifest *materialManifestPtr(de::Uri const &path) const;

    /**
     * Lookup a manifest by unique identifier.
     *
     * @param id  Unique identifier for the manifest to be looked up. Note
     *            that @c 0 is not a valid identifier.
     *
     * @return  The associated manifest.
     */
    MaterialManifest &toMaterialManifest(materialid_t id) const;

    /**
     * Returns the total number of unique materials in the collection.
     */
    de::dint materialCount() const;

    /**
     * Returns @c true iff a MaterialScheme exists with the symbolic @a name.
     */
    bool knownMaterialScheme(de::String name) const;

    /**
     * Lookup a material resource scheme by symbolic name.
     *
     * @param name  Symbolic name of the scheme.
     * @return  MaterialScheme associated with @a name.
     *
     * @throws UnknownSchemeError If @a name is unknown.
     *
     * @see knownMaterialScheme()
     */
    de::MaterialScheme &materialScheme(de::String name) const;

    /**
     * Returns the total number of material manifest schemes in the collection.
     */
    de::dint materialSchemeCount() const;

    /**
     * Iterate through all the material resource schemes of the resource system.
     *
     * @param func  Callback to make for each MaterialScheme.
     */
    de::LoopResult forAllMaterialSchemes(std::function<de::LoopResult (de::MaterialScheme &)> func) const;

    /**
     * Clear all materials (and their manifests) in all schemes.
     *
     * @see forAllMaterialSchemes(), MaterialScheme::clear().
     */
    inline void clearAllMaterialSchemes() {
        forAllMaterialSchemes([] (de::MaterialScheme &scheme) {
            scheme.clear();
            return de::LoopContinue;
        });
        DENG2_ASSERT(materialCount() == 0); // sanity check
    }

    /**
     * Lookup a material manifest group by unique @a number.
     */
    MaterialManifestGroup &materialGroup(de::dint number) const;

    /**
     * Create a new (empty) material manifest group.
     */
    MaterialManifestGroup &newMaterialGroup();

    /**
     * Destroys all material manifest groups.
     */
    void clearAllMaterialGroups();

    /**
     * Provides a list of all material manifest groups, for efficient traversal.
     */
    MaterialManifestGroups const &allMaterialGroups() const;

    /**
     * Returns the total number of material manifest groups in the collection.
     */
    inline de::dint materialGroupCount() const { return allMaterialGroups().count(); }

    /**
     * Declare a material in the collection, producing a manifest for a logical
     * Material which will be defined later. If a manifest with the specified
     * @a uri already exists the existing manifest will be returned.
     *
     * @param uri  Uri representing a path to the material in the virtual hierarchy.
     *
     * @return  Manifest for this URI.
     */
    inline MaterialManifest &declareMaterial(de::Uri const &uri) {
        return materialScheme(uri.scheme()).declare(uri.path());
    }

    /**
     * Iterate through all the materials of the resource system.
     *
     * @param func  Callback to make for each Material.
     */
    de::LoopResult forAllMaterials(std::function<de::LoopResult (Material &)> func) const;

    /**
     * Determines if a texture exists for @a path.
     *
     * @return @c true, if a texture exists; otherwise @a false.
     *
     * @see hasTextureManifest(), TextureManifest::hasTexture()
     */
    inline bool hasTexture(de::Uri const &path) const {
        if(hasTextureManifest(path)) return textureManifest(path).hasTexture();
        return false;
    }

    /**
     * Lookup a texture resource for the specified @a path.
     *
     * @return The found texture.
     *
     * @see textureManifest(), TextureManifest::texture()
     */
    inline de::Texture &texture(de::Uri const &path) const {
        return textureManifest(path).texture();
    }

    /**
     * Returns a pointer to the identified Texture.
     *
     * @see hasTextureManifest(), TextureManifest::texturePtr()
     */
    inline de::Texture *texturePtr(de::Uri const &path) {
        if(hasTextureManifest(path)) return textureManifest(path).texturePtr();
        return NULL;
    }

    /**
     * Convenient method of searching the texture collection for a texture with
     * the specified @a schemeName and @a resourceUri.
     *
     * @param schemeName   Unique name of the scheme in which to search.
     * @param resourceUri  Path to the (image) resource to find the texture for.
     *
     * @return  The found texture; otherwise @c nullptr.
     */
    de::Texture *texture(de::String schemeName, de::Uri const &resourceUri);

    /**
     * Determines if a texture manifest exists for a declared texture on @a path.
     *
     * @return @c true, if a manifest exists; otherwise @a false.
     */
    bool hasTextureManifest(de::Uri const &path) const;

    /**
     * Find the manifest for a declared texture.
     *
     * @param search  The search term.
     * @return Found unique identifier.
     */
    de::TextureManifest &textureManifest(de::Uri const &search) const;

    /**
     * Lookup a subspace scheme by symbolic name.
     *
     * @param name  Symbolic name of the scheme.
     * @return  Scheme associated with @a name.
     *
     * @throws UnknownSchemeError If @a name is unknown.
     */
    de::TextureScheme &textureScheme(de::String name) const;

    /**
     * Returns @c true iff a Scheme exists with the symbolic @a name.
     */
    bool knownTextureScheme(de::String name) const;

    /**
     * Returns a list of all the schemes for efficient traversal.
     */
    TextureSchemes const &allTextureSchemes() const;

    /**
     * Returns the total number of manifest schemes in the collection.
     */
    inline de::dint textureSchemeCount() const {
        return allTextureSchemes().count();
    }

    /**
     * Clear all textures in all schemes.
     *
     * @see Scheme::clear().
     */
    inline void clearAllTextureSchemes() {
        foreach(de::TextureScheme *scheme, allTextureSchemes()) {
            scheme->clear();
        }
    }

    /**
     * Returns a list of all the unique texture instances in the collection,
     * from all schemes.
     */
    AllTextures const &allTextures() const;

    /**
     * Declare a texture in the collection, producing a manifest for a logical
     * Texture which will be defined later. If a manifest with the specified
     * @a uri already exists the existing manifest will be returned.
     *
     * If any of the property values (flags, dimensions, etc...) differ from
     * that which is already defined in the pre-existing manifest, any texture
     * which is currently associated is released (any GL-textures acquired for
     * it are deleted).
     *
     * @param uri           Uri representing a path to the texture in the
     *                      virtual hierarchy.
     * @param flags         Texture flags property.
     * @param dimensions    Logical dimensions property.
     * @param origin        World origin offset property.
     * @param uniqueId      Unique identifier property.
     * @param resourceUri   Resource URI property.
     *
     * @return  Manifest for this URI.
     */
    inline de::TextureManifest &declareTexture(de::Uri const &uri,
        de::Texture::Flags flags, de::Vector2i const &dimensions,
        de::Vector2i const &origin, de::dint uniqueId, de::Uri const *resourceUri = nullptr)
    {
        return textureScheme(uri.scheme())
                   .declare(uri.path(), flags, dimensions, origin, uniqueId,
                            resourceUri);
    }

    de::Texture *defineTexture(de::String schemeName, de::Uri const &resourceUri,
                               de::Vector2i const &dimensions = de::Vector2i());

    patchid_t declarePatch(de::String encodedName);

    /**
     * Returns a rawtex_t for the given lump if one already exists; otherwise @c 0.
     */
    rawtex_t *rawTexture(lumpnum_t lumpNum);

    /**
     * Get a rawtex_t data structure for a raw texture specified with a WAD lump
     * number. Allocates a new rawtex_t if it hasn't been loaded yet.
     */
    rawtex_t *declareRawTexture(lumpnum_t lumpNum);

    /**
     * Returns a list of pointers to all the raw textures in the collection.
     */
    QList<rawtex_t *> collectRawTextures() const;

#ifdef __CLIENT__
    /**
     * Determines if a manifest exists for a resource on @a path.
     *
     * @return @c true, if a manifest exists; otherwise @a false.
     */
    bool hasFont(de::Uri const &path) const;

    /**
     * Convenient method of looking up a concrete font resource in the collection
     * given it's unique identifier.
     *
     * @return  The associated font resource.
     *
     * @see toFontManifest(), FontManifest::hasResource()
     */
    inline AbstractFont &font(fontid_t id) const {
        return toFontManifest(id).resource();
    }

    /**
     * Returns the total number of resource manifests in the collection.
     */
    de::duint fontCount() const { return allFonts().count(); }

    /**
     * Find a resource manifest.
     *
     * @param search  The search term.
     * @return Found unique identifier.
     */
    de::FontManifest &fontManifest(de::Uri const &search) const;

    /**
     * Lookup a manifest by unique identifier.
     *
     * @param id  Unique identifier for the manifest to be looked up. Note
     *            that @c 0 is not a valid identifier.
     *
     * @return  The associated manifest.
     */
    de::FontManifest &toFontManifest(fontid_t id) const;

    /**
     * Lookup a subspace scheme by symbolic name.
     *
     * @param name  Symbolic name of the scheme.
     * @return  Scheme associated with @a name.
     *
     * @throws UnknownSchemeError If @a name is unknown.
     */
    de::FontScheme &fontScheme(de::String name) const;

    /**
     * Returns @c true iff a Scheme exists with the symbolic @a name.
     */
    bool knownFontScheme(de::String name) const;

    /**
     * Returns a list of all the schemes for efficient traversal.
     */
    FontSchemes const &allFontSchemes() const;

    /**
     * Returns the total number of manifest schemes in the collection.
     */
    inline de::dint fontSchemeCount() const { return allFontSchemes().count(); }

    /**
     * Clear all resources in all schemes.
     *
     * @see allFontSchemes(), FontScheme::clear().
     */
    inline void clearAllFontSchemes() {
        foreach(de::FontScheme *scheme, allFontSchemes()) {
            scheme->clear();
        }
    }

    /**
     * Returns a list of pointers to all the concrete resources in the collection,
     * from all schemes.
     */
    AllFonts const &allFonts() const;

    /**
     * Declare a resource in the collection, producing a (possibly new) manifest
     * for a resource which may be defined later. If a manifest with the specified
     * @a uri already exists the existing manifest will be returned.
     *
     * @param uri  Uri representing a path to the resource in the virtual hierarchy.
     *
     * @return  The associated manifest for this URI.
     */
    inline de::FontManifest &declareFont(de::Uri const &uri) {
        return fontScheme(uri.scheme()).declare(uri.path());
    }

    /**
     * Lookup the unique index attributed to the given @a modelDef.
     *
     * @return  Index of the definition; otherwise @c -1 if @a modelDef is unknown.
     */
    de::dint indexOf(ModelDef const *modelDef);

    /**
     * Convenient method of looking up a concrete model resource in the collection
     * given it's unique identifier. O(1)
     *
     * @return  The associated model resource.
     */
    Model &model(modelid_t id);

    /**
     * Determines if a model definition exists with the given @a id. O(n)
     *
     * @return  @c true, if a definition exists; otherwise @a false.
     *
     * @see modelDef()
     */
    bool hasModelDef(de::String id) const;

    /**
     * Retrieve a model definition by it's unique @a index. O(1)
     *
     * @return  The associated model definition.
     *
     * @see modelDefCount()
     */
    ModelDef &modelDef(de::dint index);

    /**
     * Lookup a model definition by it's unique @a id. O(n)
     *
     * @return  Found model definition.
     *
     * @see hasModelDef()
     */
    ModelDef &modelDef(de::String id);

    /**
     * Lookup a model definition for the specified mobj @a stateIndex.
     *
     * @param stateIndex  Index of the mobj state.
     * @param select      Model selector argument. There may be multiple models
     *                    for a given mobj state. The selector determines which
     *                    is used according to some external selection criteria.
     *
     * @return  Found model definition; otherwise @c nullptr.
     */
    ModelDef *modelDefForState(de::dint stateIndex, de::dint select = 0);

    /**
     * Returns the total number of model definitions in the system.
     *
     * @see modelDef()
     */
    de::dint modelDefCount() const;

    /// @todo Refactor away. Used for animating particle/sky models.
    void setModelDefFrame(ModelDef &modelDef, de::dint frame);

    /**
     * Release all GL-textures in all schemes.
     */
    void releaseAllGLTextures();

    /**
     * Release all GL-textures in schemes flagged 'runtime'.
     */
    void releaseAllRuntimeGLTextures();

    /**
     * Release all GL-textures in schemes flagged 'system'.
     */
    void releaseAllSystemGLTextures();

    /**
     * Release all GL-textures in the identified scheme.
     *
     * @param schemeName  Symbolic name of the texture scheme to process.
     */
    void releaseGLTexturesByScheme(de::String schemeName);

    /**
     * Prepare a material variant specification in accordance to the specified
     * usage context. If incomplete context information is supplied, suitable
     * default values will be chosen in their place.
     *
     * @param contextId         Usage context identifier.
     * @param flags             @ref textureVariantSpecificationFlags
     * @param border            Border size in pixels (all edges).
     * @param tClass            Color palette translation class.
     * @param tMap              Color palette translation map.
     * @param wrapS             GL texture wrap/clamp mode on the horizontal axis (texture-space).
     * @param wrapT             GL texture wrap/clamp mode on the vertical axis (texture-space).
     * @param minFilter         Logical DGL texture minification level.
     * @param magFilter         Logical DGL texture magnification level.
     * @param anisoFilter       @c -1= User preference else a logical DGL anisotropic filter level.
     * @param mipmapped         @c true= use mipmapping.
     * @param gammaCorrection   @c true= apply gamma correction to textures.
     * @param noStretch         @c true= disallow stretching of textures.
     * @param toAlpha           @c true= convert textures to alpha data.
     *
     * @return  The interned copy of the rationalized specification.
     */
    de::MaterialVariantSpec const &materialSpec(MaterialContextId contextId,
        de::dint flags, byte border, de::dint tClass, de::dint tMap, de::dint wrapS, de::dint wrapT,
        de::dint minFilter, de::dint magFilter, de::dint anisoFilter, bool mipmapped,
        bool gammaCorrection, bool noStretch, bool toAlpha);

    /**
     * Prepare a TextureVariantSpecification according to usage context. If the
     * specification is incomplete suitable defaults are chosen automatically.
     *
     * @param tc      Usage context.
     * @param flags   @ref textureVariantSpecificationFlags
     * @param border  Border size in pixels (all edges).
     * @param tClass  Color palette translation class.
     * @param tMap    Color palette translation map.
     *
     * @return  The interned copy of the rationalized specification.
     */
    TextureVariantSpec const &textureSpec(texturevariantusagecontext_t tc,
        de::dint flags, byte border, de::dint tClass, de::dint tMap, de::dint wrapS, de::dint wrapT,
        de::dint minFilter, de::dint magFilter, de::dint anisoFilter,
        dd_bool mipmapped, dd_bool gammaCorrection, dd_bool noStretch, dd_bool toAlpha);

    /**
     * Prepare a TextureVariantSpecification according to usage context. If the
     * specification is incomplete suitable defaults are chosen automatically.
     *
     * @return  A rationalized and valid TextureVariantSpecification.
     */
    TextureVariantSpec &detailTextureSpec(de::dfloat contrast);

    AbstractFont *newFontFromDef(ded_compositefont_t const &def);
    AbstractFont *newFontFromFile(de::Uri const &uri, de::String filePath);

    /**
     * Release all GL-textures for fonts in the identified scheme.
     *
     * @param schemeName  Symbolic name of the font scheme to process.
     */
    void releaseFontGLTexturesByScheme(de::String schemeName);

#endif // __CLIENT__

    /**
     * Returns the total number of animation/precache groups.
     */
    de::dint animGroupCount();

    /**
     * Destroys all the animation groups.
     */
    void clearAllAnimGroups();

    /**
     * Construct a new animation group.
     *
     * @param flags  @ref animationGroupFlags
     */
    de::AnimGroup &newAnimGroup(de::dint flags);

    /**
     * Returns the AnimGroup associated with @a uniqueId (1-based); otherwise @c 0.
     */
    de::AnimGroup *animGroup(de::dint uniqueId);

    de::AnimGroup *animGroupForTexture(de::TextureManifest const &textureManifest);

    /**
     * Returns the total number of color palettes.
     */
    de::dint colorPaletteCount() const;

    /**
     * Destroys all the color palettes.
     */
    void clearAllColorPalettes();

    /**
     * Returns the ColorPalette associated with unique @a id.
     */
    ColorPalette &colorPalette(colorpaletteid_t id) const;

    /**
     * Returns the symbolic name of the specified color @a palette. A zero-length
     * string is returned if no name is associated.
     */
    de::String colorPaletteName(ColorPalette &palette) const;

    /**
     * Returns @c true iff a ColorPalette with the specified @a name is present.
     */
    bool hasColorPalette(de::String name) const;

    /**
     * Returns the ColorPalette associated with @a name.
     *
     * @see hasColorPalette()
     */
    ColorPalette &colorPalette(de::String name) const;

    /**
     * @param newPalette  Color palette to add. Ownership of the palette is given
     *                    to the resource system.
     * @param name        Symbolic name of the color palette.
     */
    void addColorPalette(ColorPalette &newPalette, de::String const &name = de::String());

    /**
     * Returns the unique identifier of the current default color palette.
     */
    colorpaletteid_t defaultColorPalette() const;

    /**
     * Change the default color palette.
     *
     * @param newDefaultPalette  The color palette to make default.
     */
    void setDefaultColorPalette(ColorPalette *newDefaultPalette);

#ifdef __CLIENT__

    /**
     * Prepare resources for the current Map.
     */
    void cacheForCurrentMap();

    /**
     * Add a variant of @a material to the cache queue for deferred preparation.
     *
     * @param material      Base material from which to derive a context variant.
     * @param spec          Specification for the derivation of @a material.
     * @param cacheGroups   @c true= variants for all materials in any applicable
     *                      groups are desired; otherwise just specified material.
     */
    void cache(Material &material, de::MaterialVariantSpec const &spec,
               bool cacheGroups = true);

    /**
     * Cache all resources needed to visualize models using the given @a modelDef.
     */
    void cache(ModelDef *modelDef);

    /**
     * Precache resources from the set associated with the specified @a spriteId.
     *
     * @param spriteId      Unique identifier of the sprite set to cache.
     * @param materialSpec  Specification to use when caching materials.
     */
    void cache(spritenum_t spriteId, de::MaterialVariantSpec const &materialSpec);

    /**
     * Process all queued material cache tasks.
     */
    void processCacheQueue();

    /**
     * Cancel all queued material cache tasks.
     */
    void purgeCacheQueue();

#endif // __CLIENT__

    /**
     * Utility for scheduling legacy savegame conversion(s) (delegated to background Tasks).
     *
     * @param gameId      Identity key of the game and corresponding subfolder name within
     *                    save repository to output the converted savegame to. Also used for
     *                    resolving ambiguous savegame formats.
     * @param sourcePath  If a zero-length string then @em all legacy savegames located for
     *                    this game will be considered. Otherwise use the path of a single
     *                    legacy savegame file to schedule a single conversion.
     *
     * @return  @c true if one or more conversion tasks were scheduled.
     */
    bool convertLegacySavegames(de::String const &gameId, de::String const &sourcePath = "");

    /**
     * Attempt to locate a music file referenced in the given @em Music @a definition. Songs
     * can be either in external files or non-MUS lumps.
     *
     * @note Lump based music is presently handled separately!!
     *
     * @param musicDef  Music definition to find the music file for.
     *
     * @return  Absolute path to the music if found; otherwise a zero-length string.
     */
    de::String tryFindMusicFile(de::Record const &musicDef);

public:  /// @todo Should be private:
    void initTextures();
    void initSystemTextures();
    void initSprites();
#ifdef __CLIENT__
    void initModels();
#endif

    void clearAllRawTextures();

    void clearAllTextureSpecs();
    void pruneUnusedTextureSpecs();

public:
    /**
     * Register the console commands, variables, etc..., of this module.
     */
    static void consoleRegister();

    static de::String resolveSymbol(de::String const &symbol);

private:
    DENG2_PRIVATE(d)
};

DENG_EXTERN_C byte precacheMapMaterials, precacheSprites;
DENG_EXTERN_C byte texGammaLut[256];

void R_BuildTexGammaLut();

#endif  // RESOURCESYSTEM_H
