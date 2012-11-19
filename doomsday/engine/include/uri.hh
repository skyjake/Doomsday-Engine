/**
 * @file uri.hh
 * Universal Resource Identifier. @ingroup base
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

#ifndef LIBDENG2_URI_H
#define LIBDENG2_URI_H

#ifdef __cplusplus

#include <algorithm> // std::swap
#include <de/libdeng2.h>
#include <de/ISerializable>

#include "resourceclass.h"

/// Schemes must be at least this many characters.
#define DENG2_URI_MIN_SCHEME_LENGTH     2

#include <de/Error>
#include <de/Log>
#include <de/NativePath>
#include <de/String>

struct ddstring_s; // libdeng Str

namespace de {

/**
 * Assists working with URIs and maps them to engine-managed resources.
 *
 * Universal resource identifiers (URIs) are a way to identify specific
 * entities in a hierarchy.
 *
 * @todo Derive from Qt::QUrl, which implements the official URI
 * specification more fully? Do we really need all the parts of an URI for
 * our needs?
 *
 * @ingroup base
 */
class Uri : public ISerializable, public LogEntry::Arg::Base
{
    struct Instance; // needs to be friended by Uri::Segment

public:
    /// A nonexistent path segment was referenced. @ingroup errors
    DENG2_ERROR(NotSegmentError);

    /// Base class for resolve-related errors. @ingroup errors
    DENG2_ERROR(ResolveError);

    /// An unknown symbol was encountered in the embedded expression. @ingroup errors
    DENG2_SUB_ERROR(ResolveError, UnknownSymbolError);

    /// An unresolveable symbol was encountered in the embedded expression. @ingroup errors
    DENG2_SUB_ERROR(ResolveError, ResolveSymbolError);

    /// Type used to represent a path name hash key.
    typedef ushort hash_type;

    /// Range of a path name hash key; [0..hash_range)
    static hash_type const hash_range;

    /**
     * Flags for printing URIs.
     * @ingroup flags base
     */
    enum PrintFlag
    {
        OutputResolved = 0x1,           ///< Include the resolved path in the output.
        TransformPathPrettify = 0x2,    ///< Transform paths making them "pretty".
        DefaultPrintFlags = OutputResolved | TransformPathPrettify
    };
    Q_DECLARE_FLAGS(PrintFlags, PrintFlag)

    /**
     * Marks a segment in the URI's path. Makes no copy of the segments in the
     * path, only stores the location within the URI where they begin and end.
     *
     * Note that only the path is broken down to segments. The other parts of a
     * URI are not processed in this fashion.
     *
     * @see URI paths are composed of segments:
     * http://tools.ietf.org/html/rfc3986#section-3.3
     */
    struct Segment
    {
        /**
         * Returns the segment as a string.
         */
        String toString() const;

        /// @return  Length of the segment in characters.
        int length() const;

        /**
         * Returns a somewhat-random number in the range [0..Uri::hash_range)
         * generated from the segment.
         *
         * @return  The generated hash key.
         */
        hash_type hash() const;

        friend class Uri;
        friend struct Uri::Instance;

    private:
        mutable bool gotHashKey;
        mutable hash_type hashKey;
        const char* from;
        const char* to;
    };

public:
    /**
     * Construct a default (empty) Uri instance.
     */
    Uri();

    /**
     * Construct a Uri instance from @a path.
     *
     * @param path  Path to be parsed. Assumed to be in percent-encoded representation.
     *
     * @param defaultResourceClass  If no scheme is defined in @a path and this
     *      is not @c RC_NULL, ask the resource locator whether it knows of an
     *      appropriate default scheme for this class of resource.
     */
    Uri(String path, resourceclassid_t defaultResourceClass = RC_UNKNOWN, QChar delimiter = '/');

    /**
     * Construct a Uri instance by duplicating @a other.
     */
    Uri(Uri const& other);

    ~Uri();

    inline Uri& operator = (Uri other) {
        std::swap(d, other.d);
        return *this;
    }

    /**
     * Swaps this Uri with @a other.
     * @param other  Uri.
     */
    inline void swap(Uri& other) {
        std::swap(d, other.d);
    }

    bool operator == (Uri const& other) const;

    bool operator != (Uri const& other) const;

    /**
     * Constructs a Uri instance from a NativePath that refers to a file in
     * the native file system. All path directives such as '~' are
     * expanded. The resultant Uri will have an empty/zero-length scheme
     * (because file paths do not include one).
     *
     * @param path  Native path to a file in the native file system.
     */
    static Uri fromNativePath(NativePath const& path);

    /**
     * Constructs a Uri instance from a NativePath that refers to a native
     * directory. All path directives such as '~' are expanded. The
     * resultant Uri will have an empty/zero-length scheme (because file
     * paths do not include one).
     *
     * @param nativeDirPath  Native path to a directory in the native
     *                       file system.
     * @param defaultResourceClass  Default resource class.
     */
    static Uri fromNativeDirPath(NativePath const& nativeDirPath,
                                 resourceclassid_t defaultResourceClass = RC_NULL);

    /**
     * Convert this URI to a text string.
     *
     * @see asText()
     */
    operator String () const {
        return asText();
    }

    /**
     * Returns true if the path component of the URI is empty; otherwise false.
     */
    bool isEmpty() const;

    /**
     * Clear the URI returning it to an empty state.
     */
    Uri& clear();

    /**
     * Attempt to resolve this URI. Substitutes known symbolics in the possibly
     * templated path. Resulting path is a well-formed, filesys compatible path
     * (perhaps base-relative).
     *
     * @return  The resolved path.
     */
    String resolved() const;

    /**
     * @return  Scheme of the URI.
     */
    String scheme() const;

    /**
     * @return  Path of the URI.
     */
    String path() const;

    /**
     * @return  Scheme of the URI as plain text (UTF-8 encoding).
     */
    const char* schemeCStr() const;

    /**
     * @return  Path of the URI as plain text (UTF-8 encoding).
     */
    const char* pathCStr() const;

    /**
     * @return  Scheme of the URI as plain text (UTF-8 encoding).
     */
    const struct ddstring_s* schemeStr() const;

    /**
     * @return  Path of the URI as plain text (UTF-8 encoding).
     */
    const struct ddstring_s* pathStr() const;

    /**
     * Change the scheme of the URI to @a newScheme.
     */
    Uri& setScheme(String newScheme);

    /**
     * Change the path of the URI to @a newPath.
     */
    Uri& setPath(String newPath, QChar delimiter = '/');

    /**
     * Update this URI by parsing new values from the specified arguments.
     *
     * @param newUri  URI to be parsed. Assumed to be in percent-encoded representation.
     *
     * @param defaultResourceClass  If no scheme is defined in @a newUri and
     *      this is not @c RC_NULL, ask the resource locator whether it knows
     *      of an appropriate default scheme for this class of resource.
     */
    Uri& setUri(String newUri, resourceclassid_t defaultResourceClass = RC_UNKNOWN,
                QChar delimiter = '/');

    /**
     * Compose from this URI a plain-text representation. Any internal encoding
     * method or symbolic identifiers will be left unchanged in the resultant
     * string (not decoded, not resolved).
     *
     * @return  Plain-text String representation.
     */
    String compose(QChar delimiter = '/') const;

    /**
     * Retrieve the segment with index @a index. Note that segments are indexed
     * in reverse order (right to left) and NOT the autological left to right
     * order.
     *
     * For example, if the path is "c:/mystuff/myaddon.addon" the corresponding
     * segment map is arranged as follows:
     * <pre>
     *   [0:{myaddon.addon}, 1:{mystuff}, 2:{c:}].
     * </pre>
     *
     * @note The zero-length name in relative paths is also treated as a segment.
     *       For example, the path "/Users/username" has three segments.
     *       (FIXME: "/Users/username" is NOT a relative path; "Users/username" is. -jk)
     *
     * @param index  Reverse-index of the segment to lookup. All paths have
     *               at least one segment (even empty ones) thus index @c 0 will
     *               always be valid.
     *
     * @return  Referenced segment.
     */
    const Segment& segment(int index) const;

    /**
     * @return  Total number of segments in the URI segment map.
     * @see segment()
     */
    int segmentCount() const;

    /**
     * @return  First segment in the path.
     * @see segment()
     */
    inline const Segment& firstSegment() const {
        return segment(0);
    }

    /**
     * @return  Last segment in the path.
     * @see segment()
     */
    inline const Segment& lastSegment() const {
        return segment(segmentCount() - 1);
    }

    /**
     * Transform the URI into a human-friendly representation. Percent-encoded
     * symbols are decoded.
     *
     * @return  Human-friendly representation of the URI.
     */
    String asText() const;

    /**
     * Print debug ouput for the URI.
     *
     * @param indent            Number of characters to indent the print output.
     * @param flags             @ref printUriFlags
     * @param unresolvedText    Text string to be printed if not resolvable.
     */
    void debugPrint(int indent, PrintFlags flags = DefaultPrintFlags,
                    String unresolvedText = "") const;

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::STRING; }

    // Implements ISerializable.
    void operator >> (Writer& to) const;
    void operator << (Reader& from);

private:
    Instance* d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Uri::PrintFlags)

} // namespace de

namespace std {
    // std::swap specialization for de::Uri
    template <>
    inline void swap<de::Uri>(de::Uri& a, de::Uri& b) {
        a.swap(b);
    }
}

#endif // __cplusplus

#endif // LIBDENG2_URI_H
