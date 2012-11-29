/**
 * @file textures.h Texture Resource Collection.
 * @ingroup resource
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

/// Unique identifier associated with each texture name in the collection.
typedef int textureid_t;

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

namespace de {

class TextureMetaFile;

/**
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
    typedef class TextureMetaFile MetaFile;

    class Scheme; // forward declaration

    struct ResourceClass
    {
        /**
         * Interpret a metafile producing a new logical Texture instance..
         *
         * @param metafile  The file to be interpreted.
         * @param dimensions  Logical dimensions. Components can be @c 0 in
         *                  which case their value will be inherited from the
         *                  actual pixel dimensions of the image at load time.
         * @param flags     Texture Flags.
         * @param userData  User data to associate with the resultant texture.
         */
        static Texture *interpret(MetaFile &metafile, Size2Raw const &dimensions,
                                  Texture::Flags flags, void *userData = 0);

        /**
         * @copydoc interpret()
         */
        static Texture *interpret(MetaFile &metafile, Texture::Flags flags,
                                  void *userData = 0);
    };

    /**
     * Scheme defines a texture system subspace.
     */
    class Scheme
    {
    public:
        /// Minimum length of a symbolic name.
        static int const min_name_length = URI_MINSCHEMELENGTH;

        /// Texture metafiles within the scheme are placed into a tree.
        typedef PathTreeT<MetaFile> Index;

    public:
        /// The requested metafile could not be found in the index.
        DENG2_ERROR(NotFoundError);

    public:
        /**
         * Construct a new (empty) texture subspace scheme.
         *
         * @param symbolicName  Symbolic name of the new subspace scheme. Must
         *                      have at least @ref min_name_length characters.
         */
        explicit Scheme(String symbolicName);

        ~Scheme();

        /// @return  Symbolic name of this scheme (e.g., "ModelSkins").
        String const& name() const;

        /// @return  Total number of metafiles in the scheme.
        int size() const;

        /// @return  Total number of metafiles in the scheme. Same as @ref size().
        inline int count() const {
            return size();
        }

        /**
         * Clear all metafiles in the scheme (any GL textures which have been
         * acquired for associated textures will be released).
         */
        void clear();

        /**
         * Insert a new metafile at the given @a path into the scheme.
         * If a metafile already exists at this path, the existing metafile is
         * returned and this is a no-op.
         *
         * @param path  Virtual path for the resultant metafile.
         * @return  The (possibly newly created) metafile at @a path.
         */
        MetaFile &insertMetaFile(Path const &path);

        /**
         * Search the scheme for a metafile matching @a path.
         *
         * @return  Found metafile.
         */
        MetaFile const &find(Path const &path) const;

        /// @copydoc find()
        MetaFile &find(Path const &path);

        /**
         * Search the scheme for a metafile whose associated resource
         * URI matches @a uri.
         *
         * @return  Found metafile.
         */
        MetaFile const &findByResourceUri(Uri const &uri) const;

        /// @copydoc findByResourceUri()
        MetaFile &findByResourceUri(Uri const &uri);

        /**
         * Search the scheme for a metafile whose associated unique
         * identifier matches @a uniqueId.
         *
         * @return  Found metafile.
         */
        MetaFile const &findByUniqueId(int uniqueId) const;

        /// @copydoc findByUniqueId()
        MetaFile &findByUniqueId(int uniqueId);

        /**
         * Provides access to the metafile index for efficient traversal.
         */
        Index const &index() const;

        /// @todo Refactor away -ds
        void markUniqueIdLutDirty();

    private:
        struct Instance;
        Instance *d;
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

    /// @return  Unique identifier of the primary name for @a texture else @c NOTEXTUREID.
    textureid_t id(Texture &texture) const;

    /// @return  Unique identifier of the primary name for @a metafile else @c NOTEXTUREID.
    textureid_t idForMetaFile(MetaFile const &metafile) const;

    /// @return  Texture associated with unique identifier @a textureId else @c 0.
    Texture *toTexture(textureid_t textureId) const;

    /// @return  Scheme-unique identfier associated with the identified @a textureId.
    int uniqueId(textureid_t textureId) const;

    /// @return  Declared, percent-encoded path to this data resource,
    ///          else a "null" Uri (no scheme or path).
    Uri const &resourceUri(textureid_t textureId) const;

    /// @return  URI to this texture, percent-encoded.
    Uri composeUri(textureid_t textureId) const;

    /**
     * Find a single declared texture.
     *
     * @param search  The search term.
     * @return Found unique identifier; otherwise @c NOTEXTUREID.
     */
    MetaFile *find(Uri const &search) const;

    /**
     * Declare a texture in the collection. If a texture with the specified
     * @a uri already exists, its unique identifier is returned. If the given
     * @a resourcePath differs from that already defined for the pre-existing
     * texture, any associated Texture instance is released (any GL-textures
     * acquired for it).
     *
     * @param uri           Uri representing a path to the texture in the
     *                      virtual hierarchy.
     * @param uniqueId      Scheme-unique identifier to associate with the
     *                      texture.
     * @param resourcepath  The path to the underlying data resource.
     *
     * @return  MetaFile for this texture unless @a uri is invalid, in which
     *          case @c 0 is returned.
     */
    MetaFile *declare(Uri const &uri, int uniqueId, Uri const *resourceUri);

    /**
     * Removes a file from any indexes.
     *
     * @param metafile      Metafile to remove from the index.
     */
    void deindex(MetaFile &metafile);

    /**
     * Iterate over defined Textures in the collection making a callback for
     * each visited. Iteration ends when all textures have been visited or a
     * callback returns non-zero.
     *
     * @param nameOfScheme  If a known symbolic scheme name, only consider
     *                      textures within this scheme. Can be @ zero-length
     *                      string, in which case visit all textures.
     * @param callback      Callback function ptr.
     * @param parameters    Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int iterate(String nameOfScheme, int (*callback)(Texture &texture, void *parameters),
                void *parameters = 0) const;

    /**
     * @copydoc iterate()
     */
    inline int iterate(int (*callback)(Texture &texture, void *parameters),
                       void *parameters = 0) const {
        return iterate("", callback, parameters);
    }

    /**
     * Iterate over declared textures in the collection making a callback for
     * each visited. Iteration ends when all textures have been visited or a
     * callback returns non-zero.
     *
     * @param nameOfScheme  If a known symbolic scheme name, only consider
     *                      textures within this scheme. Can be @ zero-length
     *                      string, in which case visit all textures.
     * @param callback      Callback function ptr.
     * @param parameters    Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int iterateDeclared(String nameOfScheme, int (*callback)(MetaFile &metafile, void *parameters),
                        void* parameters = 0) const;

    /**
     * @copydoc iterate()
     */
    inline int iterateDeclared(int (*callback)(MetaFile &metafile, void *parameters),
                               void* parameters = 0) const {
        return iterateDeclared("", callback, parameters);
    }


private:
    struct Instance;
    Instance *d;
};

} // namespace de

de::Textures* App_Textures();

#endif __cplusplus

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
