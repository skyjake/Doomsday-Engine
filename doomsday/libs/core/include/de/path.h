/** @file path.h Textual path composed of segments.
 *
 * @author Copyright © 2010-2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright © 2010-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_PATH_H
#define LIBCORE_PATH_H

#include <algorithm> // std::swap

#include "de/libcore.h"
#include "de/error.h"
#include "de/log.h"
#include "de/iserializable.h"
#include "de/cstring.h"
#include "de/range.h"

namespace de {

/**
 * Special-purpose string that is always lower-case and comes with a CRC-32 hash.
 */
struct LowercaseHashString
{
    String   str;
    uint32_t hash;

    LowercaseHashString(const String &s = {})
        : str(s.lower())
        , hash(crc32(str))
    {}

    LowercaseHashString(const LowercaseHashString &) = default;

    inline LowercaseHashString &operator=(const LowercaseHashString &other)
    {
        str  = other.str;
        hash = other.hash;
        return *this;
    }

    inline bool operator<(const LowercaseHashString &other) const
    {
        return str.compare(other.str) < 0;
    }

    inline bool operator==(const LowercaseHashString &other) const
    {
        if (hash != other.hash) return false;
        return str.compare(other.str) == 0;
    }

    inline bool operator!=(const LowercaseHashString &other) const { return !(*this == other); }
};

/**
 * A textual path composed of segments. @ingroup data
 *
 * A path is a case insensitive text string that is broken down into segments.
 * Path is a generic class and as such does not provide any interpretation of
 * what the path refers to; it just parses the string and splits it into
 * segments. The user may choose any character to act as the segment separator.
 *
 * Paths are used when identifying and organizing structured data. One
 * practical example is file system paths.
 *
 * Internally, the class avoids duplicating the provided path String (i.e.,
 * string is not altered), instead relying on the implicit sharing of QString.
 */
class DE_PUBLIC Path : public ISerializable, public LogEntry::Arg::Base
{
public:
    /// Segment index was out of bounds. @ingroup errors
    DE_ERROR(OutOfBoundsError);

    /**
     * Marks a segment in the path. Makes no copy of the segments in the path,
     * only stores the location within the path where they begin and end.
     *
     * Examples:
     * - Empty path (as produced by the default constructor) => one empty segment ""
     * - Unix-style root directory "/" => two empty segments "", ""
     * - Windows-style root directory "c:/" => "c:", ""
     * - relative path "some/dir/file.ext" => "some", "dir", file.ext"
     * - Unix-style absolute path "/some/file.ext" => "", "some", file.ext"
     *
     * @see URI paths are composed of "segments":
     * http://tools.ietf.org/html/rfc3986#section-3.3
     */
    struct DE_PUBLIC Segment
    {
        Segment(const CString &string)
            : range(string)
            , _key(String(string))
        {}

        /**
         * Segments are implicitly converted to text strings.
         */
        inline operator CString() const { return range; }

        inline CString       toCString() const { return range; }
        inline const String &toLowercaseString() const { return _key.str; }
        inline Rangecc       toRange() const { return range; }

        /**
         * Determines the length of the segment in characters.
         * Same as size().
         */
        int length() const;

        /**
         * Determines the length of the segment in characters.
         * Same as length().
         */
        dsize size() const;

        /*
         * Returns a somewhat-random number in the range [0..Path::hash_range)
         * generated from the segment.
         *
         * @return  The generated hash key.
         */
//        hash_type hash() const;
        inline const LowercaseHashString &key() const { return _key; }

        bool hasWildCard() const;

        /**
         * Case insensitive equality test.
         *
         * @param other Other segment.
         *
         * @return @c true, iff the segments are equal.
         */
        bool operator==(const Segment &other) const;

        inline bool operator!=(const Segment &other) const { return !(*this == other); }

        bool operator==(const String &text) const
        {
            DE_ASSERT(text.lower() == text);
            return _key.str == text;
        }

//        bool operator==(const char *text) const
//        {
//            return range.compare(text, CaseInsensitive) == 0;
//        }

        inline bool operator!=(const String &text) const { return !(*this == text); }

        /**
         * Returns @c true if this segment is lexically less than @a other.
         * The test is case and separator insensitive.
         */
        bool operator<(const Segment &other) const;

        enum Flag { GotHashKey = 0x1, WildCardChecked = 0x2, IncludesWildCard = 0x4 };

    private:
        mutable Flags       flags;
        CString             range;
        LowercaseHashString _key;

        friend class Path;
    };

public:
    /**
     * Construct an empty Path instance.
     */
    Path();

    /**
     * Construct a path from @a path.
     *
     * @param path  Path to be parsed. The supplied string is used as-is
     *              (implicit sharing): all white space is included in the path.
     * @param sep   Character used to separate path segments in @a path.
     */
    Path(const String &path, Char sep = '/');

    Path(const CString &path, Char sep = '/');

    /**
     * Construct a path from a UTF-8 C-style string.
     *
     * @param nullTerminatedCStr  Path to be parsed. All white space is included in the path.
     * @param sep   Character used to separate path segments.
     */
    Path(const char *nullTerminatedCStr, Char sep);

    /**
     * Construct a path from a UTF-8 C-style string that uses '/' separators.
     *
     * @param nullTerminatedCStr  Path to be parsed. All white space is included in the path.
     */
    Path(const char *nullTerminatedCStr);

    /**
     * Construct a path by duplicating @a other.
     */
    Path(const Path &other);

    Path(Path &&moved);

    Path &operator=(const Path &other);

    Path &operator=(Path &&moved);

    Path &operator=(const char *pathUtf8);

    /**
     * Append a string.
     *
     * @param str  String.
     *
     * @return Path with @a str added to the end.
     *
     * @note This is a plain string append, not a path concatenation: use the /
     * operator for concatenating paths in a way that takes care of separators
     * and path relativity.
     */
    Path operator+(const String &str) const;

    /**
     * @copydoc operator+
     */
    Path operator+(const char *str) const;

    /**
     * Determines if this path is equal to @a other. The test is case
     * and separator insensitive.
     *
     * Examples:
     * - "hello/world" (sep: /) == "HELLO/World" (sep: /)
     * - "hello/world" (sep: /) == "Hello|World" (sep: |)
     *
     * @param other  Path.
     *
     * @return @c true, iff the paths are equal.
     */
    bool operator==(const Path &other) const;

    bool operator==(const char *cstr) const;

    /**
     * Determines if this path is not equal to @a other. The test is case
     * and separator insensitive.
     *
     * @param other  Path.
     *
     * @return @c true, iff the paths are not equal.
     */
    bool operator!=(const Path &other) const { return !(*this == other); }

    /**
     * Returns @c true if this path is lexically less than @a other. The test
     * is case and separator insensitive.
     */
    bool operator<(const Path &other) const;

    /**
     * Concatenate paths together. This path's separator will be used for
     * the resulting path. @see String::concatenatePath()
     *
     * @param other  Path to concatenate after this one.
     *
     * @return Concatenated path.
     */
    Path operator/(const Path &other) const;

    /**
     * Concatenate paths together. This path's separator will be used for
     * the resulting path. @see String::concatenatePath()
     *
     * @param other  Path to concatenate after this one. '/' is used as
     *               the separator.
     *
     * @return Concatenated path.
     */
    Path operator/(const String &other) const;

    Path operator/(const CString &other) const;

    Path operator/(const char *otherNullTerminatedUtf8) const;

    /**
     * Convert this path to a text string.
     */
    inline CString toCString() const { return {c_str(), c_str() + size()}; }

    inline operator String() const { return toString(); }

    /**
     * Convert this path to a text string.
     */
    String toString() const;
    
    inline std::string toStdString() const { return toString().toStdString(); }

    const char *c_str() const;

    /**
     * Returns a reference to the path as a string.
     */
    operator const char *() const;

    /**
     * Returns @c true if the path is empty; otherwise @c false.
     */
    bool isEmpty() const;

    bool isAbsolute() const;

    /// Returns the length of the path.
    int length() const;

    /// Returns the length of the path.
    dsize size() const;

    BytePos sizeb() const;

    /// Returns the first character of the path.
    Char first() const;

    /// Returns the last character of the path.
    Char last() const;

    /**
     * Clear the path.
     */
    Path &clear();

    /**
     * Assigns a new path with '/' separators.
     *
     * @param newPath  Path where '/' is the segment separator character.
     *
     * @return This Path.
     */
    Path &operator = (const String &newPath);

    /**
     * Changes the path.
     *
     * @param newPath  New path.
     * @param sep      Character used to separate path segments in @a path.
     */
    Path &set(const String &newPath, Char sep = '/');

    /**
     * Returns a copy of the path where all segment separators have been
     * replaced with a new character.
     *
     * @param sep  Character used to replace segment separators.
     *
     * @return  Path with new separators.
     */
    Path withSeparators(Char sep = '/') const;

    /**
     * Returns the character used as segment separator.
     */
    Char separator() const;

    /**
     * Adds a separator in the end of the path, if one is not there already.
     */
    void addTerminatingSeparator();

    /**
     * Returns the file name portion of the path, i.e., the last segment.
     */
    CString fileName() const;

    Block toUtf8() const;

    /**
     * Retrieve a reference to the segment at @a index. In this method the
     * segments are indexed left to right, in the same order as they appear in
     * the original textual path. There is always at least one segment (index
     * 0, the first segment).
     *
     * @note The zero-length name in UNIX-style absolute paths is also treated
     *       as a segment. For example, the path "/Users/username" has three
     *       segments ("", "Users", "username").
     *
     * @param index  Index of the segment to look up. All paths have
     *               at least one segment (even empty ones) thus index @c 0 will
     *               always be valid.
     *
     * @return  Referenced segment. Do not keep the returned reference after
     * making a change to the path.
     */
    const Segment &segment(int index) const;

    /**
     * Retrieve a reference to the segment at @a reverseIndex. In this method
     * the segments are indexed in reverse order (right to left). There is
     * always at least one segment (index 0, the last segment).
     *
     * For example, if the path is "c:/mystuff/myaddon.addon" the corresponding
     * segment map is arranged as follows:
     * <pre>
     *   [0:{myaddon.addon}, 1:{mystuff}, 2:{c:}].
     * </pre>
     *
     * @note The zero-length name in UNIX-style absolute paths is also treated
     *       as a segment. For example, the path "/Users/username" has three
     *       segments ("username", "Users", "").
     *
     * @param reverseIndex  Reverse-index of the segment to look up. All paths have
     *                      at least one segment (even empty ones) thus index @c 0 will
     *                      always be valid.
     *
     * @return  Referenced segment. Do not keep the returned reference after
     * making a change to the path.
     */
    const Segment &reverseSegment(int reverseIndex) const;

    Path subPath(const Rangei &range) const;

    Path beginningOmitted(int omittedSegmentCount = 1) const;

    Path endOmitted(int omittedSegmentCount = 1) const;

    /**
     * @return  Total number of segments in the segment map. There is always
     * at least one segment.
     */
    int segmentCount() const;

    /**
     * @return First (i.e., left-most) segment in the path. If the path is
     * empty, the returned segment is an empty, zero-length segment.
     */
    inline const Segment &firstSegment() const { return segment(0); }

    /**
     * @return  Last (i.e., right-most) segment in the path. If the path is empty,
     * the returned segment is an empty, zero-length segment.
     */
    inline const Segment &lastSegment() const { return segment(segmentCount() - 1); }

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::StringArgument; }
    String              asText()          const { return toString(); }

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    /**
     * Normalizes slashes in a string so that they are replaced with the given
     * character (defaults to forward slash).
     *
     * @param text          String where to replace separators.
     * @param replaceWith   New separator character.
     *
     * @return String with all slashes replaced with @a replaceWith.
     */
    static String normalizeString(const String &text, Char replaceWith = '/');

    /**
     * Makes a path where the given input text is first normalized so that
     * slashes are replaced with the given character (defaults to forward
     * slash).
     *
     * @param text          String where to replace separators.
     * @param replaceWith   New separator character.
     *
     * @return Path with @a replaceWith used in place of slashes.
     */
    static Path normalize(const String &text, Char replaceWith = '/');

private:
    DE_PRIVATE(d)
};

/**
 * Utility class for specifying paths that use a dot (.) as the path separator.
 * @ingroup data
 */
class DE_PUBLIC DotPath : public Path
{
public:
    DotPath(const String &path = "")        : Path(path, '.') {}
    DotPath(const char *nullTerminatedCStr) : Path(nullTerminatedCStr, '.') {}
    DotPath(const Path &other)              : Path(other) {}
    DotPath(Path &&moved)                   : Path(moved) {}
    DotPath(const DotPath &other)           : Path(other.toString(), other.separator()) {}
    DotPath(DotPath &&moved)                : Path(moved) {}

    DotPath &operator = (const char *nullTerminatedCStr) {
        return *this = DotPath(nullTerminatedCStr);
    }
    DotPath &operator = (const String &str) {
        return *this = DotPath(str);
    }
//    DotPath &operator = (const QString &str) {
//        return *this = DotPath(str);
//    }
    DotPath &operator = (const Path &other) {
        Path::operator = (other);
        return *this;
    }
    DotPath &operator = (const DotPath &other) {
        Path::operator = (other);
        return *this;
    }
    DotPath &operator = (Path &&moved) {
        Path::operator = (moved);
        return *this;
    }
    DotPath &operator = (DotPath &&moved) {
        Path::operator = (moved);
        return *this;
    }

    bool operator==(const DotPath &other) const { return Path::operator==(other); }
    bool operator!=(const DotPath &other) const { return Path::operator!=(other); }

    bool operator==(const char *cstr) const { return toString() == cstr; }
};

/**
 * Utility class for referring to a portion of an existing (immutable) path.
 * @ingroup data
 */
class DE_PUBLIC PathRef
{
public:
    PathRef(const Path &path)
        : _path(path)
        , _range(0, path.segmentCount())
    {}
    PathRef(const Path &path, const Rangei &segRange)
        : _path(path)
        , _range(segRange)
    {}

    const Path &path() const { return _path; }
    Rangei      range() const { return _range; }

    bool    isEmpty() const { return _range.isEmpty(); }
    bool    isAbsolute() const { return !isEmpty() && !firstSegment().size(); }
    PathRef subPath(const Rangei &sub) const { return PathRef(_path, sub + _range.start); }
    Path    toPath() const;

    const Path::Segment &segment(int index) const { return _path.segment(_range.start + index); }
    int                  segmentCount() const { return _range.size(); }

    inline const Path::Segment &firstSegment() const { return segment(0); }
    inline const Path::Segment &lastSegment() const { return segment(segmentCount() - 1); }

private:
    const Path &_path;
    Rangei      _range;
};

} // namespace de

#endif // LIBCORE_PATH_H
