/**
 * @file nativepath.h
 * File paths for the native file system.
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_NATIVEPATH_H
#define LIBDENG2_NATIVEPATH_H

#include "../Path"

namespace de {

/**
 * Manipulates paths of the native file system. Always uses the directory
 * separator characters appropriate for the native file system: any directory
 * separators present in the strings are automatically converted to the native
 * ones.
 *
 * The public interface of NativePath closely mirrors that of String, e.g.,
 * String::fileNamePath(), so that equivalent operations are provided except
 * with native separator characters.
 */
class DENG2_PUBLIC NativePath : public Path
{
public:
    /// An unknown user name was encounterd in the string. @ingroup errors
    DENG2_ERROR(UnknownUserError);

public:
    /**
     * Constructs an empty native path.
     */
    NativePath();

    /**
     * Constructs a native path from any string.
     *
     * @param str Path. Any directory separators in the path are converted to
     * native ones.
     */
    NativePath(QString const &str);

    NativePath(char const *nullTerminatedCStr);
    NativePath(char const *cStr, dsize length);

    /**
     * Assignment.
     *
     * @param str Path. Any directory separators in the path are converted to
     * native ones.
     */
    NativePath &operator = (QString const &str);

    NativePath &operator = (char const *nullTerminatedCStr);

    /**
     * Does a path concatenation on a native path. The directory separator
     * character depends on the platform. Note that if @a nativePath is an
     * absolute path, the result of the concatenation is just @a nativePath.
     *
     * @param nativePath  Native path to concatenate.
     */
    NativePath concatenatePath(NativePath const &nativePath) const;

    NativePath concatenatePath(QString const &nativePath) const;

    /// A more convenient way to invoke concatenatePath().
    NativePath operator / (NativePath const &nativePath) const;

    /**
     * Native path concatenation.
     * @param str  Path that is converted to a native path.
     * @return Concatenated path.
     */
    NativePath operator / (QString const &str) const;

    NativePath operator / (char const *nullTerminatedCStr) const;

    /// Extracts the path of the string, using native directory separators.
    NativePath fileNamePath() const;

    /**
     * Determines if the path is an absolute path.
     * @return @c true if absolute, otherwise it's a relative path.
     */
    bool isAbsolute() const;

    /**
     * Replaces symbols and shorthand in the path with the actual paths.
     * Expands the legacy native path directives '>' and '}' at the start of
     * the path, replacing them with the native base path. Handles '~' and
     * '~username' on UNIX-based platforms so that a user specific home path
     * (taken from passwd) may also be used.
     *
     * @param didExpand  If specified, this value will be set to true if
     *                   path expansion was done.
     *
     * @return Path with directives expanded.
     *
     * @see App::nativeBasePath()
     */
    NativePath expand(bool *didExpand = 0) const;

    /**
     * Forms a prettier version of the path, where commonly known paths in the
     * beginning of the NativePath are replaced with a symbol. No information
     * is lost in the transformation.
     *
     * Also handles the legacy native path directives '>' and '}', which expand
     * to the base path.
     *
     * @return Simplified version of the path. The result should be used for
     * paths appearing in messages intended for the user.
     */
    String pretty() const;

    /**
     * Converts all separator characters in the path to @a sep and returns the
     * updated path.
     *
     * @param sep  Character to use to replace all separators.
     *
     * @return Path with separators replaced.
     */
    String withSeparators(QChar sep) const;

    /**
     * Returns the current native working path.
     */
    static NativePath workPath();
};

} // namespace de

#endif // LIBDENG2_NATIVEPATH_H
