/** @file clientresources.h  Client-side resource subsystem.
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

#ifndef DENG_CLIENT_RESOURCES_H
#define DENG_CLIENT_RESOURCES_H

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
#include <doomsday/resource/colorpalette.h>
#include <doomsday/res/Texture>
#include <doomsday/res/TextureScheme>

#include "resource/animgroup.h"
#include "resource/rawtexture.h"

#ifdef __CLIENT__
#  include "AbstractFont"
#  include "BitmapFont"
#  include "CompositeBitmapFont"
#  include "FontScheme"
#  include "MaterialVariantSpec"
#  include "resource/framemodel.h"
#  include "resource/framemodeldef.h"
#endif

class ClientMaterial;

/**
 * Subsystem for managing client-side resources.
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
class ClientResources : public Resources
{
public:
#ifdef __CLIENT__
    /// The referenced model def was not found. @ingroup errors
    DENG2_ERROR(MissingModelDefError);

    /// The specified font id was invalid (out of range). @ingroup errors
    DENG2_ERROR(UnknownFontIdError);

    typedef QMap<de::String, de::FontScheme *> FontSchemes;
    typedef QList<AbstractFont *> AllFonts;
#endif

    typedef QMap<de::dint, de::Record> SpriteSet;  ///< frame => Sprite

public:
    /**
     * Construct a new resource system, configuring all resource classes and
     * the associated resource collection schemes.
     */
    ClientResources();

    void clear() override;

    void clearAllResources();
    void clearAllRuntimeResources();
    void clearAllSystemResources();

    void addColorPalette(res::ColorPalette &newPalette, de::String const &name);

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
    de::dint indexOf(FrameModelDef const *modelDef);

    /**
     * Convenient method of looking up a concrete model resource in the collection
     * given it's unique identifier. O(1)
     *
     * @return  The associated model resource.
     */
    FrameModel &model(modelid_t id);

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
    FrameModelDef &modelDef(de::dint index);

    /**
     * Lookup a model definition by it's unique @a id. O(n)
     *
     * @return  Found model definition.
     *
     * @see hasModelDef()
     */
    FrameModelDef &modelDef(de::String id);

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
    FrameModelDef *modelDefForState(de::dint stateIndex, de::dint select = 0);

    /**
     * Returns the total number of model definitions in the system.
     *
     * @see modelDef()
     */
    de::dint modelDefCount() const;

    /// @todo Refactor away. Used for animating particle/sky models.
    void setModelDefFrame(FrameModelDef &modelDef, de::dint frame);

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

    de::AnimGroup *animGroupForTexture(res::TextureManifest const &textureManifest);

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
    void cache(ClientMaterial &material, de::MaterialVariantSpec const &spec,
               bool cacheGroups = true);

    /**
     * Cache all resources needed to visualize models using the given @a modelDef.
     */
    void cache(FrameModelDef *modelDef);

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
     * @param gameId      Identifier of the game and corresponding subfolder name within
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

#endif // DENG_CLIENT_RESOURCES_H
