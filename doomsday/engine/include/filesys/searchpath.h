/**
 * @file searchpath.h
 * Search Path. @ingroup fs
 *
 * @author Copyright &copy; 2010-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_SEARCHPATH_H
#define LIBDENG_FILESYS_SEARCHPATH_H

#ifdef __cplusplus

#include <algorithm> // std::swap
#include "dd_types.h"
#include "uri.hh"

#if WIN32
#  if defined(SearchPath)
#    undef SearchPath
#  endif
#endif

namespace de {

/**
 * SearchPath is the pairing of a @ref de::Uri plus a set of flags which
 * determine how the URI should be interpreted.
 *
 * This class is intended as a convenient way to manage these two pieces
 * of closely related information as a unit.
 *
 * @ingroup fs
 */
class SearchPath : public Uri
{
public:
    /// @defgroup searchPathFlags Search Path Flags
    /// @ingroup flags
    enum Flag
    {
        /// Interpreters should not decend into branches.
        NoDescend       = 0x1
    };
    Q_DECLARE_FLAGS(Flags, Flag)

public:
    /**
     * @param _uri     Unresolved search URI (may include symbolic names or
     *                 other symbol references).
     * @param _flags   @ref searchPathFlags
     */
    SearchPath(Uri const& _uri, Flags _flags = 0);

    /**
     * Construct a copy from @a other. This is a "deep copy".
     */
    SearchPath(SearchPath const& other);

    inline SearchPath& operator = (SearchPath other) {
        Uri::swap(other);
        std::swap(flags_, other.flags_);
        return *this;
    }

    /**
     * Swaps this SearchPath with @a other.
     * @param other  SearchPath.
     */
    inline void swap(SearchPath& other) { // nothrow
        Uri::swap(other);
        std::swap(flags_, other.flags_);
    }

    /// Returns the interpretation flags for the search path.
    Flags flags() const;

    /**
     * Change interpretation flags for the search path.
     * @param flags  New flags.
     * @return  This instance.
     */
    SearchPath& setFlags(Flags flags);

private:
    Flags flags_;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(SearchPath::Flags)

} // namespace de

namespace std {
    // std::swap specialization for de::SearchPath
    template <>
    inline void swap<de::SearchPath>(de::SearchPath& a, de::SearchPath& b) {
        a.swap(b);
    }
}

#endif // __cplusplus

#endif /* LIBDENG_FILESYS_SEARCHPATH_H */
