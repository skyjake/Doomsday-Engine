/** @file nativepath.h File paths for the native file system.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_NATIVEPATH_H
#define LIBCORE_NATIVEPATH_H

#include "de/path.h"

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
 *
 * @ingroup fs
 */
class DE_PUBLIC NativePath : public Path
{
public:
    /// An unknown user name was encounterd in the string. @ingroup errors
    DE_ERROR(UnknownUserError);

    /// Creating a directory failed. @ingroup errors
    DE_ERROR(CreateDirError);

public:
    /**
     * Constructs an empty native path.
     */
    NativePath();

    NativePath(const NativePath &other);
    NativePath(NativePath &&moved);

    /**
     * Constructs a native path from any string.
     *
     * @param str Path. Any directory separators in the path are converted to
     * native ones.
     */
    NativePath(const String &str);

    NativePath(const CString &str);

    NativePath(const Path &path);

    NativePath(const char *nullTerminatedCStr);
    NativePath(const char *cStr, dsize length);

    /**
     * Assignment.
     *
     * @param str Path. Any directory separators in the path are converted to
     * native ones.
     */
    NativePath &operator=(const String &str);

    NativePath &operator=(NativePath &&moved);
    NativePath &operator=(const NativePath &other);
    NativePath &operator=(const char *nullTerminatedCStr);

    /**
     * Does a path concatenation on a native path. The directory separator
     * character depends on the platform. Note that if @a nativePath is an
     * absolute path, the result of the concatenation is just @a nativePath.
     *
     * @param nativePath  Native path to concatenate.
     */
    NativePath concatenatePath(const NativePath &nativePath) const;

    NativePath concatenatePath(const String &nativePath) const;

    /// A more convenient way to invoke concatenatePath().
    NativePath operator / (const NativePath &nativePath) const;

    /**
     * Native path concatenation.
     * @param str  Path that is converted to a native path.
     * @return Concatenated path.
     */
    NativePath operator / (const String &str) const;

    NativePath operator / (const char *nullTerminatedCStr) const;

    /// Extracts the path of the string, using native directory separators.
    NativePath fileNamePath() const;

    /**
     * Determines if the path is an absolute path.
     * @return @c true if absolute, otherwise it's a relative path.
     */
    bool isAbsolute() const;

    bool isRelative() const;

    bool isDirectory() const;

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
    String withSeparators(Char sep) const;

    bool exists() const;

    bool isReadable() const;

    inline void create() const { createPath(*this); }
    inline bool destroy() const { return destroyPath(*this); }

    /**
     * Deletes the native file at the path.
     * @return @c true on success.
     */
    bool remove() const;

public:
    /**
     * Returns the current native working path.
     */
    static NativePath workPath();

    /**
     * Sets the current native working path.
     *
     * @param cwd  Working path.
     *
     * @return  @c true iff successfully changed the current working path.
     */
    static bool setWorkPath(const NativePath &cwd);

    static NativePath homePath();

    /**
     * Determines whether a native path exists.
     *
     * @param nativePath  Path to check.
     *
     * @return @c true if the path exists, @c false otherwise.
     */
    static bool exists(const NativePath &nativePath);

    /**
     * Creates a native directory relative to the current working directory.
     *
     * @param nativePath  Native directory to create.
     */
    static void createPath(const NativePath &nativePath);

    static bool destroyPath(const NativePath &nativePath);

    /**
     * Returns the native path separator character.
     */
    static Char separator();
};

} // namespace de

#endif // LIBCORE_NATIVEPATH_H
