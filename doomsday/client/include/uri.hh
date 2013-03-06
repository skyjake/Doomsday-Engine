/** @file uri.hh Universal Resource Identifier.
 * @ingroup base
 *
 * @author Copyright &copy; 2010-2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2010-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_URI_H
#define LIBDENG2_URI_H

#ifdef __cplusplus

#include "resourceclass.h"

/// Schemes must be at least this many characters.
#define DENG2_URI_MIN_SCHEME_LENGTH     2

#include <de/Log>
#include <de/Error>
#include <de/NativePath>
#include <de/String>

struct ddstring_s; // libdeng Str

namespace de {

/**
 * Assists working with URIs and maps them to engine-managed resources.
 * @ingroup base
 *
 * Uri is derived from Path. It augments Path with schemes and path symbolics.
 *
 * Universal resource identifiers (URIs) are a way to identify specific
 * entities in a hierarchy.
 */
class Uri : public LogEntry::Arg::Base
{
public:
    /// Base class for resolve-related errors. @ingroup errors
    DENG2_ERROR(ResolveError);

    /// An unknown symbol was encountered in the embedded expression. @ingroup errors
    DENG2_SUB_ERROR(ResolveError, UnknownSymbolError);

    /// An unresolveable symbol was encountered in the embedded expression. @ingroup errors
    DENG2_SUB_ERROR(ResolveError, ResolveSymbolError);

    /**
     * Flags determining the composition of textual representation:
     */
    enum ComposeAsTextFlag
    {
        OmitScheme      = 0x1, ///< Exclude the scheme.
        OmitPath        = 0x2, ///< Exclude the path.
        DecodePath      = 0x4, ///< Decode percent-endcoded characters in the path.

        DefaultComposeAsTextFlags = 0
    };
    Q_DECLARE_FLAGS(ComposeAsTextFlags, ComposeAsTextFlag)

public:
    /**
     * Construct an empty Uri instance.
     */
    Uri();

    /**
     * Construct a Uri instance from a text string.
     *
     * @param percentEncoded  String to be parsed. Assumed to be in
     *                        percent-encoded representation.
     *
     * @param defaultResClass Default scheme. Determines the scheme for the Uri
     *                        if one is not specified in @a percentEncoded.
     *                        @c RC_UNKNOWN: resource locator guesses an
     *                        appropriate scheme for this type of file.
     *
     * @param sep             Character used to separate path segments
     *                        in @a path.
     */
    Uri(String const &percentEncoded, resourceclassid_t defaultResClass = RC_UNKNOWN, QChar sep = '/');

    /**
     * Construct a Uri from a textual scheme and a path.
     *
     * @param scheme  Scheme for the Uri.
     * @param path    Path for the Uri.
     */
    Uri(String const &scheme, Path const &path);

    /**
     * Construct a Uri instance from a path. Note that Path instances can
     * never contain a scheme as a prefix, so @a resClass is mandatory.
     *
     * @param resClass  Scheme for the URI. @c RC_UNKNOWN: resource locator
     *                  guesses an appropriate scheme for this.
     *
     * @param path      Path of the URI.
     */
    Uri(resourceclassid_t resClass, Path const &path);

    /**
     * Construct a Uri instance from a path without a scheme.
     *
     * @param path      Path of the URI.
     */
    Uri(Path const &path);

    /**
     * Construct a Uri instance from a UTF-8 C-style text string, using RC_UNKNOWN
     * as the default resource class.
     *
     * @param nullTerminatedCStr  String to be parsed. Assumed to be in
     *                            percent-encoded representation.
     */
    Uri(char const *nullTerminatedCStr);

    /**
     * Construct a Uri instance by duplicating @a other.
     */
    Uri(Uri const &other);

    ~Uri();

    inline Uri &operator = (Uri other) {
        std::swap(d, other.d);
        return *this;
    }

    /**
     * Swaps this Uri with @a other.
     * @param other  Uri.
     */
    inline void swap(Uri &other) {
        std::swap(d, other.d);
    }

    bool operator == (Uri const &other) const;

    bool operator != (Uri const &other) const {
        return !(*this == other);
    }

    /**
     * Constructs a Uri instance from a NativePath that refers to a file in
     * the native file system. All path directives such as '~' are
     * expanded. The resultant Uri will have an empty (zero-length) scheme
     * (because file paths do not include one).
     *
     * @param path  Native path to a file in the native file system.
     * @param defaultResourceClass  Default resource class.
     */
    static Uri fromNativePath(NativePath const &path,
                              resourceclassid_t defaultResourceClass = RC_NULL);

    /**
     * Constructs a Uri instance from a NativePath that refers to a native
     * directory. All path directives such as '~' are expanded. The
     * resultant Uri will have an empty (zero-length) scheme (because file
     * paths do not include one).
     *
     * @param nativeDirPath  Native path to a directory in the native
     *                       file system.
     * @param defaultResourceClass  Default resource class.
     */
    static Uri fromNativeDirPath(NativePath const &nativeDirPath,
                                 resourceclassid_t defaultResourceClass = RC_NULL);

    /**
     * Construct a Uri instance from a user supplied, variable-length list of UTF-8
     * C-style text string arguments.
     *
     * @param argv         The arguments to be interpreted. All of which are assumed to
     *                     be in a @em non-percent-encoded representation.
     *
     *                     Supported forms (where <> denote keyword component names):
     *                      - [0: "<scheme>:<path>"]
     *                      - [0: "<scheme>"]              (if @a knownScheme set)
     *                      - [0: "<path>"]
     *                      - [0: "<scheme>", 1: "<path>"]
     *
     * @param argc         The number of elements in @a argv.
     *
     * @param knownScheme  Callback function used to identify known scheme names when
     *                     attempting to resolve ambiguous cases (only the one argument
     *                     is provided.
     */
    static Uri fromUserInput(char **argv, int argc, bool (*knownScheme) (String name) = 0);

    /**
     * Convert this URI to a text string.
     *
     * @see asText()
     */
    operator String () const {
        return asText();
    }

    /**
     * Determines if the URI's path is empty.
     */
    bool isEmpty() const;

    /**
     * Clear the URI returning it to an empty state.
     */
    Uri &clear();

    /**
     * Attempt to resolve this URI. Substitutes known symbolics in the possibly
     * templated path. Resulting path is a well-formed, filesys compatible path
     * (perhaps base-relative).
     *
     * @return  The resolved path.
     */
    String resolved() const;

    String const &resolvedRef() const;

    /**
     * @return Scheme of the URI.
     */
    String const &scheme() const;

    /**
     * @return Path of the URI.
     */
    Path const &path() const;

    /**
     * @return Scheme of the URI as plain text (UTF-8 encoding).
     */
    char const *schemeCStr() const;

    /**
     * @return  Path of the URI as plain text (UTF-8 encoding).
     */
    char const *pathCStr() const;

    /**
     * @return  Scheme of the URI as plain text (UTF-8 encoding).
     */
    struct ddstring_s const *schemeStr() const;

    /**
     * @return  Path of the URI as plain text (UTF-8 encoding).
     */
    struct ddstring_s const *pathStr() const;

    /**
     * Change the scheme of the URI to @a newScheme.
     */
    Uri &setScheme(String newScheme);

    /**
     * Change the path of the URI to @a newPath.
     *
     * @param newPath  New path for the URI.
     */
    Uri &setPath(Path const &newPath);

    /**
     * Change the path of the URI to @a newPath.
     *
     * @param newPath  New path for the URI.
     * @param sep      Character used to separate path segments in @a path.
     */
    Uri &setPath(String newPath, QChar sep = '/');

    Uri &setPath(char const *newPathUtf8, char sep = '/');

    /**
     * Update this URI by parsing new values from the specified arguments.
     *
     * @param newUri  URI to be parsed. Assumed to be in percent-encoded representation.
     *
     * @param defaultResourceClass  If no scheme is defined in @a newUri and
     *      this is not @c RC_NULL, ask the resource locator whether it knows
     *      of an appropriate default scheme for this class of resource.
     *
     * @param sep  Character used to separate path segments in @a path.
     */
    Uri &setUri(String newUri, resourceclassid_t defaultResourceClass = RC_UNKNOWN, QChar sep = '/');

    /**
     * Compose from this URI a plain-text representation. Any symbolic identifiers
     * will be left unchanged in the resultant string (not resolved).
     *
     * @param compositionFlags  Flags determining how the textual representation
     *                          is composited.
     * @param sep  Character to use to replace path segment separators.
     *
     * @return  Plain-text String representation.
     */
    String compose(ComposeAsTextFlags compositionFlags = DefaultComposeAsTextFlags,
                   QChar sep = '/') const;

    /**
     * Transform the URI into a human-friendly representation. Percent-encoded
     * symbols are decoded.
     *
     * @return  Human-friendly representation of the URI.
     *
     * Same as @code compose(DefaultComposeAsTextFlags | DecodePath); @endcode
     */
    String asText() const;

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::STRING; }

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Uri::ComposeAsTextFlags)

} // namespace de

namespace std {
    // std::swap specialization for de::Uri
    template <>
    inline void swap<de::Uri>(de::Uri &a, de::Uri &b) {
        a.swap(b);
    }
}

#endif // __cplusplus

#endif // LIBDENG2_URI_H
