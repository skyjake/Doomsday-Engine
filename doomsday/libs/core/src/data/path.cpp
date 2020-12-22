/** @file path.cpp Textual path composed of segments.
 *
 * @author Copyright © 2010-2019 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2010-2013 Daniel Swanson <danij@dengine.net>
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

#include "de/path.h"
#include "de/reader.h"
#include "de/writer.h"
#include "de/math.h"
#include "de/byterefarray.h"
#include <array>

namespace de {

//---------------------------------------------------------------------------------------

bool Path::Segment::hasWildCard() const
{
    if (flags & WildCardChecked)
    {
        return flags.testFlag(IncludesWildCard);
    }
    bool isWild = range.contains('*');
    applyFlagOperation(flags, IncludesWildCard, isWild? SetFlags : UnsetFlags);
    flags |= WildCardChecked;
    return isWild;
}

bool Path::Segment::operator==(const Segment &other) const
{
    return !range.compare(other.range, CaseInsensitive);
}

bool Path::Segment::operator<(const Segment &other) const
{
    return range.compare(other.range, CaseInsensitive) < 0;
}

int Path::Segment::length() const
{
    return int(range.size());
}

dsize Path::Segment::size() const
{
    return range.size();
}

//---------------------------------------------------------------------------------------

DE_PIMPL_NOREF(Path)
{
    static const String emptyPath;

    String        path;
    Char          separator; // Character that acts as the segment separator.
    List<Segment> segments;  // List of the segments, in reverse order.

    inline dsize segmentCount() const
    {
        return segments.size();
    }

    Impl()
        : separator('/')
    {}

    Impl(const String &p, Char sep)
        : path(p)
        , separator(sep)
    {}

    ~Impl()
    {
        clearSegments();
    }

    /**
     * Clear the segment array. The map will need to be rebuilt with parse().
     */
    void clearSegments()
    {
        segments.clear();
    }

    /**
     * Allocate a new segment, either from the fixed-size segments array if it
     * isn't full, or dynamically from the heap.
     *
     * @return  New segment.
     */
    Segment *allocSegment(const CString &range)
    {
        segments.push_back(range);
        return &segments.back();
    }

    /**
     * Build the segment array by splitting the path. When the path is modified,
     * the existing map is invalidated and needs to be remapped.
     *
     * @todo The reverse order is not really needed any more. Could just parse segment
     * left-to-right.
     */
    void parse()
    {
        // Already been here?
        if (!segments.empty()) return;

        segments.clear();

        if (path.isEmpty())
        {
            // There always has to be at least one segment.
            allocSegment(emptyPath);
        }
        else
        {
            const auto parts = path.splitRef(separator);
            for (auto p = parts.rbegin(); p != parts.rend(); ++p)
            {
                allocSegment(*p);
            }
            // We expect an empty segment in the beginning for absolute paths.
            if (path.beginsWith(separator))
            {
                allocSegment({path.c_str(), path.c_str()});
            }
        }
        DE_ASSERT(!segments.empty());
    }
};

const String Path::Impl::emptyPath;

Path::Path() : d(new Impl)
{}

Path::Path(const String &path, Char sep)
    : d(new Impl(path, sep))
{}

Path::Path(const CString &path, Char sep)
    : d(new Impl(path.toString(), sep))
{}

Path::Path(const char *nullTerminatedCStr, Char sep)
    : d(new Impl(nullTerminatedCStr, sep))
{}

Path::Path(const char *nullTerminatedCStr)
    : d(new Impl(nullTerminatedCStr, '/'))
{}

Path::Path(const Path &other)
    : d(new Impl(other.d->path, other.d->separator))
{}

Path::Path(Path &&moved)
    : d(std::move(moved.d))
{}

Path &Path::operator=(const char *pathUtf8)
{
    return *this = Path(pathUtf8);
}

Path &Path::operator=(const Path &other)
{
    d.reset(new Impl(other.d->path, other.d->separator));
    return *this;
}

Path &Path::operator=(Path &&moved)
{
    d = std::move(moved.d);
    return *this;
}

Path Path::operator+(const String &str) const
{
    return Path(d->path + str, d->separator);
}

Path Path::operator+(const char *nullTerminatedCStr) const
{
    return Path(d->path + nullTerminatedCStr, d->separator);
}

int Path::segmentCount() const
{
    d->parse();
    return d->segments.sizei();
}

const Path::Segment &Path::segment(int index) const
{
    return reverseSegment(segmentCount() - 1 - index);
}

const Path::Segment &Path::reverseSegment(int reverseIndex) const
{
    d->parse();

    if (reverseIndex < 0 || reverseIndex >= d->segments.sizei())
    {
        /// @throw OutOfBoundsError  Attempt to reference a nonexistent segment.
        throw OutOfBoundsError("Path::reverseSegment",
                               stringf("Reverse index %i is out of bounds", reverseIndex));
    }
    return d->segments[size_t(reverseIndex)];
}

Path Path::subPath(const Rangei &range) const
{
    if (range.isEmpty())
    {
        return Path("", d->separator);
    }
    if (range.size() == 1 && range.start == 0 && segment(0).size() == 0)
    {
        // It is the root.
        return Path(String(1, d->separator), d->separator);
    }
    return Path(CString(segment(range.start).range.ptr(),
                        segment(range.end - 1).range.endPtr()), d->separator);
}

Path Path::beginningOmitted(int omittedSegmentCount) const
{
    return subPath({omittedSegmentCount, segmentCount()});
}

Path Path::endOmitted(int omittedSegmentCount) const
{
    return subPath({0, segmentCount() - omittedSegmentCount});
}

bool Path::operator==(const Path &other) const
{
    if (this == &other) return true;

    if (segmentCount() != other.segmentCount()) return false;

    // If the hashes are different, the segments can't be the same.
    for (int i = 0; i < d->segments.sizei(); ++i)
    {
        if (segment(i).key().hash != other.segment(i).key().hash)
            return false;
    }

    // Probably the same, but we have to make sure by comparing
    // the textual segments.
    if (d->separator == other.d->separator)
    {
        // The same separators, do one string-based test.
        return !d->path.compareWithoutCase(other.d->path);
    }
    else
    {
        // Do a string-based test for each segment separately.
        for (int i = 0; i < d->segments.sizei(); ++i)
        {
            if (segment(i) != other.segment(i)) return false;
        }
        return true;
    }
}

bool Path::operator==(const char *cstr) const
{
    return d->path.compareWithoutCase(cstr) == 0;
}

bool Path::operator<(const Path &other) const
{
    if (d->separator == other.d->separator)
    {
        // The same separators, do one string-based test.
        return d->path.compareWithoutCase(other.d->path) < 0;
    }
    else
    {
        // Do a string-based test for each segment separately.
        for (int i = 0; i < d->segments.sizei(); ++i)
        {
            if (!(segment(i) < other.segment(i))) return false;
        }
        return true;
    }
}

Path Path::operator/(const Path &other) const
{
    // Unify the separators.
    String otherPath = other.d->path;
    if (other.d->separator != d->separator)
    {
        otherPath.replace(other.d->separator, d->separator);
    }

    return Path(d->path.concatenatePath(otherPath, d->separator), d->separator);
}

Path Path::operator/(const String &other) const
{
    return *this / Path(other);
}

Path Path::operator/(const CString &other) const
{
    return *this / String(other);
}

Path Path::operator/(const char *otherNullTerminatedUtf8) const
{
    return *this / Path(otherNullTerminatedUtf8, '/');
}

String Path::toString() const
{
    return d->path;
}

const char *Path::c_str() const
{
    return d->path.c_str();
}

Path::operator const char *() const
{
    return d->path;
}

bool Path::isEmpty() const
{
    return d->path.isEmpty();
}

bool Path::isAbsolute() const
{
    return !isEmpty() && !firstSegment().size();
}

int Path::length() const
{
    return int(d->path.size());
}

dsize Path::size() const
{
    return d->path.size();
}

BytePos Path::sizeb() const
{
    return d->path.sizeb();
}

Char Path::first() const
{
    return d->path.first();
}

Char Path::last() const
{
    return d->path.last();
}

Path &Path::clear()
{
    d->path.clear();
    d->clearSegments();
    return *this;
}

Path &Path::operator = (const String &newPath)
{
    set(newPath, '/');
    return *this;
}

Path &Path::set(const String &newPath, Char sep)
{
    d->path = newPath; // implicitly shared
    d->separator = sep;
    d->clearSegments();
    return *this;
}

Path Path::withSeparators(Char sep) const
{
    const Char curSep = d->separator;
    if (sep == curSep) return *this;

    String modPath = d->path;
    modPath.replace(d->separator, sep);
    return Path(modPath, sep);
}

Char Path::separator() const
{
    return d->separator;
}

void Path::addTerminatingSeparator()
{
    if (!isEmpty())
    {
        if (last() != d->separator)
        {
            d->path.append(d->separator);
            d->clearSegments();
        }
    }
}

CString Path::fileName() const
{
    if (last() == d->separator) return "";
    return lastSegment();
}

Block Path::toUtf8() const
{
    return d->path.toUtf8();
}

void Path::operator >> (Writer &to) const
{
    to << d->path << duint16(d->separator);
}

void Path::operator << (Reader &from)
{
    clear();

    Block b;
    duint16 sep;
    from >> b >> sep;
    set(String::fromUtf8(b), Char(uint32_t(sep)));
}

String Path::normalizeString(const String &text, Char replaceWith)
{
    String result = text;
    if (replaceWith != '/')
    {
        result.replace('/', replaceWith);
    }
    if (replaceWith != '\\')
    {
        result.replace('\\', replaceWith);
    }
    DE_ASSERT(!strchr(result, '\r'));
    return result;
}

Path Path::normalize(const String &text, Char replaceWith)
{
    return Path(normalizeString(text, replaceWith), replaceWith);
}

Path PathRef::toPath() const
{
    if (!segmentCount()) return Path(); // Empty.

    String composed = segment(0).toRange();
    for (int i = 1; i < segmentCount(); ++i)
    {
        composed += path().separator();
        composed += segment(i);
    }
    return Path(composed, path().separator());
}

} // namespace de
