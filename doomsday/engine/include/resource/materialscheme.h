/** @file materialscheme.h Material Scheme.
 *
 * @author Copyright &copy; 2010-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_MATERIALSCHEME_H
#define LIBDENG_RESOURCE_MATERIALSCHEME_H

#include <de/PathTree>
#include "api_uri.h"
#include "resource/materialbind.h"

namespace de {

    /**
     * A material system subspace.
     * @ingroup resource
     */
    class MaterialScheme
    {
    public:
        /// Minimum length of a symbolic name.
        static int const min_name_length = URI_MINSCHEMELENGTH;

        /// Binds within the scheme are placed into a tree.
        typedef PathTreeT<MaterialBind> Index;

    public:
        /// The requested bind could not be found in the index.
        DENG2_ERROR(NotFoundError);

    public:
        /**
         * Construct a new (empty) material subspace scheme.
         *
         * @param symbolicName  Symbolic name of the new subspace scheme. Must
         *                      have at least @ref min_name_length characters.
         */
        explicit MaterialScheme(String symbolicName);

        ~MaterialScheme();

        /// @return  Symbolic name of this scheme (e.g., "Flats").
        String const &name() const;

        /// @return  Total number of binds in the scheme.
        int size() const;

        /// @return  Total number of binds in the scheme. Same as @ref size().
        inline int count() const {
            return size();
        }

        /**
         * Clear all binds in the scheme.
         */
        void clear();

        /**
         * Insert a new material bind at the given @a path into the scheme.
         * If a bind already exists at this path, the existing bind is
         * returned and this is a no-op.
         *
         * @param path  Virtual path for the resultant bind.
         * @return  The (possibly newly created) bind at @a path.
         */
        MaterialBind &insertBind(Path const &path, materialid_t id);

        /**
         * Search the scheme for a bind matching @a path.
         *
         * @return  Found bind.
         */
        MaterialBind const &find(Path const &path) const;

        /// @copydoc find()
        MaterialBind &find(Path const &path);

        /**
         * Provides access to the bind index for efficient traversal.
         */
        Index const &index() const;

    private:
        struct Instance;
        Instance *d;
    };

} // namespace de

#endif /* LIBDENG_RESOURCE_MATERIALSCHEME_H */
