/** @file clientresources.h  Client-side resource subsystem.
 * @ingroup resource
 *
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_RESOURCES_H
#define DE_CLIENT_RESOURCES_H

#include <de/list.h>
#include <de/keymap.h>
#include <de/set.h>
#include <de/error.h>
#include <de/record.h>
#include <de/string.h>
#include <de/system.h>

#include <doomsday/defs/ded.h>
#include <doomsday/filesys/wad.h>
#include <doomsday/filesys/zip.h>
#include <doomsday/uri.h>
#include <doomsday/res/animgroup.h>
#include <doomsday/res/mapmanifest.h>
#include <doomsday/res/resources.h>
#include <doomsday/res/colorpalette.h>
#include <doomsday/res/texture.h>
#include <doomsday/res/texturescheme.h>

#include "abstractfont.h"
#include "bitmapfont.h"
#include "compositebitmapfont.h"
#include "fontscheme.h"
#include "framemodel.h"
#include "framemodeldef.h"
#include "materialvariantspec.h"
#include "rawtexture.h"

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
    /// The referenced model def was not found. @ingroup errors
    DE_ERROR(MissingModelDefError);

    /// The specified font id was invalid (out of range). @ingroup errors
    DE_ERROR(UnknownFontIdError);

    typedef de::KeyMap<de::String, de::FontScheme *> FontSchemes;
    typedef de::List<AbstractFont *> AllFonts;

    static ClientResources &get();

public:
    /**
     * Construct a new resource system, configuring all resource classes and
     * the associated resource collection schemes.
     */
    ClientResources();

    void clear() override;
    void clearAllRuntimeResources() override;
    void clearAllSystemResources() override;

    void initSystemTextures() override;

    void reloadAllResources() override;

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
    de::List<rawtex_t *> collectRawTextures() const;

    /**
     * Determines if a manifest exists for a resource on @a path.
     *
     * @return @c true, if a manifest exists; otherwise @a false.
     */
    bool hasFont(const res::Uri &path) const;

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
    de::FontManifest &fontManifest(const res::Uri &search) const;

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
    de::FontScheme &fontScheme(const de::String& name) const;

    /**
     * Returns @c true iff a Scheme exists with the symbolic @a name.
     */
    bool knownFontScheme(const de::String& name) const;

    /**
     * Returns a list of all the schemes for efficient traversal.
     */
    const FontSchemes &allFontSchemes() const;

    /**
     * Returns the total number of manifest schemes in the collection.
     */
    inline int fontSchemeCount() const { return allFontSchemes().size(); }

    /**
     * Clear all resources in all schemes.
     *
     * @see allFontSchemes(), FontScheme::clear().
     */
    inline void clearAllFontSchemes() {
        for (const auto &scheme : allFontSchemes()) {
            scheme.second->clear();
        }
    }

    /**
     * Returns a list of pointers to all the concrete resources in the collection,
     * from all schemes.
     */
    const AllFonts &allFonts() const;

    /**
     * Declare a resource in the collection, producing a (possibly new) manifest
     * for a resource which may be defined later. If a manifest with the specified
     * @a uri already exists the existing manifest will be returned.
     *
     * @param uri  Uri representing a path to the resource in the virtual hierarchy.
     *
     * @return  The associated manifest for this URI.
     */
    inline de::FontManifest &declareFont(const res::Uri &uri) {
        return fontScheme(uri.scheme()).declare(uri.path());
    }

    /**
     * Lookup the unique index attributed to the given @a modelDef.
     *
     * @return  Index of the definition; otherwise @c -1 if @a modelDef is unknown.
     */
    int indexOf(const FrameModelDef *modelDef);

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
    FrameModelDef &modelDef(int index);

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
    FrameModelDef *modelDefForState(int stateIndex, int select = 0);

    /**
     * Returns the total number of model definitions in the system.
     *
     * @see modelDef()
     */
    int modelDefCount() const;

    /// @todo Refactor away. Used for animating particle/sky models.
    void setModelDefFrame(FrameModelDef &modelDef, int frame);

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
    void releaseGLTexturesByScheme(const de::String& schemeName);

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
    const de::MaterialVariantSpec &materialSpec(MaterialContextId contextId,
                                                int          flags,
                                                byte              border,
                                                int          tClass,
                                                int          tMap,
                                                GLenum            wrapS,
                                                GLenum            wrapT,
                                                int          minFilter,
                                                int          magFilter,
                                                int          anisoFilter,
                                                bool              mipmapped,
                                                bool              gammaCorrection,
                                                bool              noStretch,
                                                bool              toAlpha);

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
    const TextureVariantSpec &textureSpec(texturevariantusagecontext_t tc,
        int flags, byte border, int tClass, int tMap, GLenum wrapS, GLenum wrapT,
        int minFilter, int magFilter, int anisoFilter,
        dd_bool mipmapped, dd_bool gammaCorrection, dd_bool noStretch, dd_bool toAlpha);

    /**
     * Prepare a TextureVariantSpecification according to usage context. If the
     * specification is incomplete suitable defaults are chosen automatically.
     *
     * @return  A rationalized and valid TextureVariantSpecification.
     */
    TextureVariantSpec &detailTextureSpec(float contrast);

    AbstractFont *newFontFromDef(const ded_compositefont_t &def);
    AbstractFont *newFontFromFile(const res::Uri &uri, const de::String& filePath);

    /**
     * Release all GL-textures for fonts in the identified scheme.
     *
     * @param schemeName  Symbolic name of the font scheme to process.
     */
    void releaseFontGLTexturesByScheme(de::String schemeName);

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
    void cache(ClientMaterial &material, const de::MaterialVariantSpec &spec,
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
    void cache(spritenum_t spriteId, const de::MaterialVariantSpec &materialSpec);

    /**
     * Process all queued material cache tasks.
     */
    void processCacheQueue();

    /**
     * Cancel all queued material cache tasks.
     */
    void purgeCacheQueue();

public:  /// @todo Should be private:
    void initModels();
    void clearAllRawTextures();
    void clearAllTextureSpecs();
    void pruneUnusedTextureSpecs();

public:
    /**
     * Register the console commands, variables, etc..., of this module.
     */
    static void consoleRegister();

private:
    DE_PRIVATE(d)
};

DE_EXTERN_C byte precacheMapMaterials, precacheSprites;

#endif // DE_CLIENT_RESOURCES_H
