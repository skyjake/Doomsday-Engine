/** @file textures.h Texture Resource Collection.
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2010-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_RESOURCE_TEXTURES_H
#define LIBDENG_RESOURCE_TEXTURES_H

#include "uri.h"

/// Special value used to signify an invalid texture id.
#define NOTEXTUREID             0

#ifdef __cplusplus

#include <QList>
#include <de/Error>
#include <de/Path>
#include <de/String>
#include <de/PathTree>
#include <de/size.h>
#include "resource/texture.h"
#include "resource/texturemanifest.h"
#include "resource/texturescheme.h"

namespace de {

/**
 * Specialized resource collection for a set of logical textures.
 * @ingroup resource
 *
 * @em Clearing a texture is to 'undefine' it - any names bound to it will be
 * deleted and any GL textures acquired for it are 'released'. The logical
 * Texture instance used to represent it is also deleted.
 *
 * @em Releasing a texture will leave it defined (any names bound to it will
 * persist) but any GL textures acquired for it are 'released'. Note that the
 * logical Texture instance used to represent is NOT be deleted.
 *
 * Thus there are two general states for textures in the collection:
 *
 *   A) Declared but not defined.
 *   B) Declared and defined.
 */
class Textures
{
public:
    typedef class TextureManifest Manifest;
    typedef class TextureScheme Scheme;

    struct ResourceClass
    {
        /**
         * Interpret a manifest producing a new logical Texture instance..
         *
         * @param manifest  The manifest to be interpreted.
         * @param flags     Texture Flags.
         * @param userData  User data to associate with the resultant texture.
         */
        static Texture *interpret(Manifest &manifest, Texture::Flags flags,
                                  void *userData = 0);

        /**
         * @copydoc interpret()
         * @param dimensions  Logical dimensions. Components can be @c 0 in
         *                  which case their value will be inherited from the
         *                  actual pixel dimensions of the image at load time.
         */
        static Texture *interpret(Manifest &manifest, Size2Raw const &dimensions,
                                  Texture::Flags flags, void *userData = 0);
    };

    /// Texture system subspace schemes.
    typedef QList<Scheme*> Schemes;

public:
    /// An unknown scheme was referenced. @ingroup errors
    DENG2_ERROR(UnknownSchemeError);

public:
    /**
     * Constructs a new texture resource collection.
     */
    Textures();

    virtual ~Textures();

    /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

    /**
     * Lookup a subspace scheme by symbolic name.
     *
     * @param name  Symbolic name of the scheme.
     * @return  Scheme associated with @a name.
     *
     * @throws UnknownSchemeError If @a name is unknown.
     */
    Scheme &scheme(String name) const;

    /**
     * Create a new subspace scheme.
     *
     * @param name      Unique symbolic name of the new scheme. Must be at
     *                  least @c Scheme::min_name_length characters long.
     */
    Scheme &createScheme(String name);

    /**
     * Returns @c true iff a Scheme exists with the symbolic @a name.
     */
    bool knownScheme(String name) const;

    /**
     * Returns a list of all the schemes for efficient traversal.
     */
    Schemes const &allSchemes() const;

    /**
     * Clear all textures in all schemes.
     * @see Scheme::clear().
     */
    inline void clearAllSchemes()
    {
        Schemes schemes = allSchemes();
        DENG2_FOR_EACH(Schemes, i, schemes){ (*i)->clear(); }
    }

    /// @return  Total number of unique Textures in the collection.
    int size() const;

    /// @return  Total number of unique Textures in the collection. Same as @ref size()
    inline int count() const {
        return size();
    }

    /**
     * Removes the manifest from any indexes.
     *
     * @param manifest      Manifest to remove from the index.
     */
    void deindex(Manifest &manifest);

    /**
     * Find a single declared texture.
     *
     * @param search  The search term.
     * @return Found unique identifier; otherwise @c NOTEXTUREID.
     */
    Manifest *find(Uri const &search) const;

    /**
     * Declare a texture in the collection, producing a manifest for a logical
     * Texture which will be defined later. If a manifest with the specified
     * @a uri already exists the existing manifest will be returned.
     *
     * If either the unique id or the @a resourceUri differs from that which
     * is already defined in pre-existing manifest, any associated logical
     * Texture instance is released (any GL-textures acquired for it).
     *
     * @param uri           Uri representing a path to the texture in the
     *                      virtual hierarchy.
     * @param uniqueId      Scheme-unique identifier to associate with the
     *                      texture.
     * @param resourceUri   Uri to the associated data resource.
     *
     * @return  Manifest for this URI; otherwise @c 0 if @a uri is invalid.
     */
    Manifest *declare(Uri const &uri, int uniqueId, Uri const *resourceUri);

    /**
     * Iterate over defined Textures in the collection making a callback for
     * each visited. Iteration ends when all textures have been visited or a
     * callback returns non-zero.
     *
     * @param callback      Callback function ptr.
     * @param parameters    Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    inline int iterate(int (*callback)(Texture &texture, void *parameters),
                       void *parameters = 0) const {
        return iterate("", callback, parameters);
    }

    /**
     * @copydoc iterate()
     * @param nameOfScheme  If a known symbolic scheme name, only consider
     *                      textures within this scheme. Can be @ zero-length
     *                      string, in which case visit all textures.
     */
    int iterate(String nameOfScheme, int (*callback)(Texture &texture, void *parameters),
                void *parameters = 0) const;

    /**
     * Iterate over declared textures in the collection making a callback for
     * each visited. Iteration ends when all textures have been visited or a
     * callback returns non-zero.
     *
     * @param callback      Callback function ptr.
     * @param parameters    Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    inline int iterateDeclared(int (*callback)(Manifest &manifest, void *parameters),
                               void* parameters = 0) const {
        return iterateDeclared("", callback, parameters);
    }

    /**
     * @copydoc iterate()
     * @param nameOfScheme  If a known symbolic scheme name, only consider
     *                      textures within this scheme. Can be @ zero-length
     *                      string, in which case visit all textures.
     */
    int iterateDeclared(String nameOfScheme, int (*callback)(Manifest &manifest, void *parameters),
                        void* parameters = 0) const;

private:
    struct Instance;
    Instance *d;

public:
    /*
     * Here follows legacy interface methods awaiting removal -ds
     */

    /// @return  Unique identifier of the primary name for @a manifest else @c NOTEXTUREID.
    /// @deprecated Texture ids are now obsolete. Reference/point-to the manifest instead.
    textureid_t idForManifest(Manifest const &manifest) const;

    /// @return  Texture associated with unique identifier @a textureId else @c 0.
    /// @deprecated Texture ids are now obsolete. Reference/point-to the manifest instead.
    Texture *toTexture(textureid_t textureId) const;
};

} // namespace de

de::Textures* App_Textures();

#endif // __cplusplus

/*
 * C wrapper API
 */

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize this module. Cannot be re-initialized, must shutdown first.
void Textures_Init(void);

/// Shutdown this module.
void Textures_Shutdown(void);

int Textures_UniqueId2(Uri const *uri, boolean quiet);
int Textures_UniqueId(Uri const *uri/*, quiet = false */);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_TEXTURES_H */
