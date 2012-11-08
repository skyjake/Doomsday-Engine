/**
 * @file nativepath.h
 * File paths for the native file system. @ingroup fs
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "../String"

namespace de {

/**
 * Specialized String that is used for manipulating paths of the native file
 * system. Always uses the directory separator characters appropriate for the
 * native file system: any directory separators present in the strings are
 * automatically converted to the native ones.
 *
 * Note that some of the methods of String are overridden, for instance
 * String::fileNamePath(), so that they operate using native separator
 * characters.
 */
class NativePath : public String
{
public:
    /// An unknown user name was encounterd in the string. @ingroup errors
    DENG2_SUB_ERROR(Error, UnknownUserError);

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
    NativePath(const QString& str);

    NativePath(const char* nullTerminatedCStr);

    /**
     * Assignment.
     *
     * @param str Path. Any directory separators in the path are converted to
     * native ones.
     */
    NativePath& operator = (const QString& str);

    NativePath& operator = (const char *nullTerminatedCStr);

    /**
     * Does a path concatenation on a native path. The directory separator
     * character depends on the platform.
     *
     * @param nativePath  Native path to concatenate.
     */
    NativePath concatenatePath(const NativePath& nativePath) const;

    NativePath concatenatePath(const QString& nativePath) const;

    /// A more convenient way to concatenate().
    NativePath operator / (const NativePath& nativePath) const;

    /**
     * Native path concatenation.
     * @param str  Path that is converted to a native path.
     * @return Concatenated path.
     */
    NativePath operator / (const QString& str) const;

    NativePath operator / (const char* nullTerminatedCStr) const;

    /// Extracts the path of the string, using native directory separators.
    NativePath fileNamePath() const;

    /**
     * Expands the legacy native path directives '>' and '}' at the start of
     * the path, replacing them with the native base path.
     *
     * Handles '~' and '~username' on UNIX-based platforms so that a user
     * specific home path (taken from passwd) may also be used.
     *
     * @param didExpand  If specified, this value will be set to true if
     *                   path expansion was done.
     *
     * @return Path with directives expanded.
     *
     * @see App::nativeBasePath()
     */
    NativePath expand(bool* didExpand = 0) const;

    /**
     * Returns the current native working path.
     */
    static NativePath workPath();
};

} // namespace de

#endif // LIBDENG2_NATIVEPATH_H
