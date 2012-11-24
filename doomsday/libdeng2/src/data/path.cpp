/** @file path.cpp Textual path composed of segments.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/Path"
#include "de/Reader"
#include "de/Writer"

#include <QList>
#include <cstring> // memset

namespace de {

/// Size of the fixed-size portion of the segment buffer.
static const int SEGMENT_BUFFER_SIZE = 24;

static String emptyPath;

Path::hash_type const Path::hash_range = 512;

ushort Path::Segment::hash() const
{
    // Is it time to compute the hash?
    if(!gotHashKey)
    {
        hashKey = 0;
        int op = 0;
        for(int i = 0; i < range.length(); ++i)
        {
            ushort unicode = range.at(i).toLower().unicode();
            switch(op)
            {
            case 0: hashKey ^= unicode; ++op;   break;
            case 1: hashKey *= unicode; ++op;   break;
            case 2: hashKey -= unicode;   op=0; break;
            }
        }
        hashKey %= hash_range;
        gotHashKey = true;
    }
    return hashKey;
}

bool Path::Segment::operator == (Path::Segment const &other) const
{
    return !range.compare(other.range, Qt::CaseInsensitive);
}

int Path::Segment::length() const
{
    return range.size();
}

dsize Path::Segment::size() const
{
    return range.size();
}

Path::Segment::operator String () const
{
    return toString();
}

String Path::Segment::toString() const
{
    return range.string()->mid(range.position(), range.size());
}

struct Path::Instance
{
    String path;

    /// The character in Instance::path that acts as the segment separator.
    QChar separator;

    /**
     * Total number of segments in the path. If 0, it means that the path
     * isn't parsed into segments yet -- all paths have at least one segment
     * (an empty one if nothing else).
     */
    int segmentCount;

    /**
     * Fixed-size array for the segments of the path.
     *
     * The segments array is composed of two parts: the first
     * SEGMENT_BUFFER_SIZE elements are placed into a fixed-size array which is
     * allocated along with the Instance, and additional segments are allocated
     * dynamically and linked in the extraSegments list.
     *
     * This optimized representation should mean that the majority of paths
     * can be represented without dynamically allocating memory from the heap
     * (apart from the Instance, that is).
     *
     * @note Contents of the array are not initialized to zero.
     */
    Path::Segment segments[SEGMENT_BUFFER_SIZE];

    /**
     * List of the extra segments that don't fit in segments, in reverse
     * order.
     */
    QList<Path::Segment *> extraSegments;

    Instance() : separator('/'), segmentCount(0)
    {}

    Instance(String const &p, QChar sep) : path(p), separator(sep), segmentCount(0)
    {}

    ~Instance()
    {
        clearSegments();
    }

    /**
     * Clear the segment array.
     *
     * @post The map will need to be rebuilt with parse().
     */
    void clearSegments()
    {
        while(!extraSegments.isEmpty())
        {
            delete extraSegments.takeFirst();
        }
        memset(segments, 0, sizeof(segments));
        segmentCount = 0;
    }

    /**
     * Allocate a new segment, either from the fixed-size segments array if it
     * isn't full, or dynamically from the heap.
     *
     * @return  New segment.
     */
    Path::Segment *allocSegment(QStringRef const &range)
    {
        Path::Segment *segment;
        if(segmentCount < SEGMENT_BUFFER_SIZE)
        {
            segment = segments + segmentCount;
        }
        else
        {
            // Allocate an "extra" node.
            segment = new Path::Segment();
            extraSegments.append(segment);
        }

        std::memset(segment, 0, sizeof(*segment));
        segment->range = range;

        // There is now one more segment in the map.
        segmentCount++;

        return segment;
    }

    /**
     * Build the segment array by parsing the path. when the path is modified,
     * the existing map is invalidated and needs to be remapped.
     */
    void parse()
    {
        // Already been here?
        if(segmentCount > 0) return;

        segmentCount = 0;
        extraSegments.clear();

        if(path.isEmpty())
        {
            // There always has to be at least one segment.
            allocSegment(&emptyPath);

            DENG2_ASSERT(segmentCount > 0);
            return;
        }

        QChar const *segBegin = path.constData();
        QChar const *segEnd   = path.constData() + path.length() - 1;

        // Skip over any trailing delimiters.
        for(int i = path.length();
            segEnd->unicode() && *segEnd == separator && i-- > 0;
            --segEnd) {}

        // Scan the path for segments, in reverse order.
        QChar const *from;
        forever
        {
            // Find the start of the next segment.
            for(from = segEnd; from > segBegin && !(*from == separator); from--)
            {}

            int startIndex = (*from == separator? from + 1 : from) - path.constData();
            int length = (segEnd - path.constData()) - startIndex + 1;
            allocSegment(QStringRef(&path, startIndex, length));

            // Are there no more parent directories?
            if(from == segBegin) break;

            // So far so good. Move one directory level upwards.
            // The next name ends here.
            segEnd = from - 1;
        }

        // Unix style zero-length root name?
        if(*segBegin == separator)
        {
            allocSegment(&emptyPath);
        }

        DENG2_ASSERT(segmentCount > 0);
    }

private:
    Instance &operator = (Instance const &); // no assignment
    Instance(Instance const &); // no copying
};

Path::Path() : d(new Instance)
{}

Path::Path(String const &path, QChar sep)
    : LogEntry::Arg::Base(), d(new Instance(path, sep))
{}

Path::Path(Path const &other)
    : ISerializable(), LogEntry::Arg::Base(),
      d(new Instance(other.d->path, other.d->separator))
{}

Path::~Path()
{
    delete d;
}

int Path::segmentCount() const
{
    d->parse();
    return d->segmentCount;
}

Path::Segment const &Path::segment(int index) const
{
    return reverseSegment(segmentCount() - 1 - index);
}

Path::Segment const &Path::reverseSegment(int reverseIndex) const
{
    d->parse();

    if(reverseIndex < 0 || reverseIndex >= d->segmentCount)
    {
        /// @throw OutOfBoundsError  Attempt to reference a nonexistent segment.
        throw OutOfBoundsError("Path::reverseSegment", String("Reverse index %1 is out of bounds").arg(reverseIndex));
    }

    // Is this in the static buffer?
    if(reverseIndex < SEGMENT_BUFFER_SIZE)
    {
        return d->segments[reverseIndex];
    }

    // No - an extra segment.
    return *d->extraSegments[reverseIndex - SEGMENT_BUFFER_SIZE];
}

bool Path::operator == (Path const &other) const
{
    if(this == &other) return true;

    if(d->separator == other.d->separator)
    {
        // The same separators, do a string-based test.
        return !d->path.compareWithoutCase(other.d->path);
    }

    // The separators are different, let's compare the segments.
    if(segmentCount() != other.segmentCount()) return false;
    for(int i = 0; i < d->segmentCount; ++i)
    {
        if(segment(i) != other.segment(i)) return false;
    }
    return true;
}

Path Path::operator / (Path const &other) const
{
    // Unify the separators.
    String otherPath = other.d->path;
    if(other.separator() != d->separator)
    {
        otherPath.replace(other.d->separator, d->separator);
    }

    return Path(d->path.concatenatePath(otherPath, d->separator), d->separator);
}

Path Path::operator / (QString other) const
{
    return *this / Path(other);
}

Path Path::operator / (char const *otherNullTerminatedUtf8) const
{
    return *this / Path(otherNullTerminatedUtf8);
}

String Path::toString() const
{
    return d->path;
}

String const &Path::toStringRef() const
{
    return d->path;
}

bool Path::isEmpty() const
{
    return d->path.isEmpty();
}

int Path::length() const
{
    return d->path.length();
}

dsize Path::size() const
{
    return length();
}

QChar Path::first() const
{
    return d->path.first();
}

QChar Path::last() const
{
    return d->path.last();
}

Path &Path::clear()
{
    d->path.clear();
    d->clearSegments();
    return *this;
}

Path &Path::operator = (String const &newPath)
{
    set(newPath, '/');
    return *this;
}

Path &Path::set(String const &newPath, QChar sep)
{
    d->path = newPath; // implicitly shared
    d->separator = sep;
    d->clearSegments();
    return *this;
}

Path Path::withSeparators(QChar sep) const
{
    if(sep == d->separator) return *this;

    String modPath = d->path;
    modPath.replace(d->separator, sep);
    return Path(modPath, sep);
}

QChar Path::separator() const
{
    return d->separator;
}

String Path::fileName() const
{
    if(last() == d->separator) return "";
    return lastSegment();
}

Block Path::toUtf8() const
{
    return d->path.toUtf8();
}

void Path::operator >> (Writer &to) const
{
    to << d->path.toUtf8() << d->separator.unicode();
}

void Path::operator << (Reader &from)
{
    clear();

    Block b;
    ushort sep;
    from >> b >> sep;
    set(String::fromUtf8(b), sep);
}

} // namespace de

#ifdef _DEBUG
#include <QDebug>

using namespace de;

static int Path_UnitTest()
{
    try
    {
        // Test emptiness.
        {
            Path p;
            DENG2_ASSERT(p == Path(""));
            DENG2_ASSERT(p.isEmpty());
            DENG2_ASSERT(p.segmentCount() == 1);
        }

        // Equality and copying.
        {
            Path a("some/thing");
            Path b("/other/thing");

            DENG2_ASSERT(a != b);

            Path c = a;
            DENG2_ASSERT(c == a);
            DENG2_ASSERT(c.segment(1).toString() == "thing");
            DENG2_ASSERT(c.reverseSegment(1).toString() == "some");

            b = a;
            DENG2_ASSERT(b == a);
            DENG2_ASSERT(b.segment(1).toString() == "thing");
            DENG2_ASSERT(b.reverseSegment(1).toString() == "some");
        }

        // Swapping.
        {
            Path a("a/b/c");
            Path b("d/e");

            DENG2_ASSERT(a.segmentCount() == 3);
            DENG2_ASSERT(a.reverseSegment(1).toString() == "b");

            std::swap(a, b);

            DENG2_ASSERT(a.segmentCount() == 2);
            DENG2_ASSERT(a.reverseSegment(1).toString() == "d");
            DENG2_ASSERT(b.segmentCount() == 3);
            DENG2_ASSERT(b.reverseSegment(1).toString() == "b");
        }

        // Test a Windows style path with a drive plus file path.
        {
            Path p("c:/something.ext");
            DENG2_ASSERT(p.segmentCount() == 2);

            DENG2_ASSERT(p.reverseSegment(0).length() == 13);
            DENG2_ASSERT(p.reverseSegment(0).toString() == "something.ext");

            DENG2_ASSERT(p.reverseSegment(1).length() == 2);
            DENG2_ASSERT(p.reverseSegment(1).toString() == "c:");
        }

        // Test a Unix style path with a zero-length root node name.
        {
            Path p("/something.ext");
            DENG2_ASSERT(p.segmentCount() == 2);

            DENG2_ASSERT(p.reverseSegment(0).length() == 13);
            DENG2_ASSERT(p.reverseSegment(0).toString() == "something.ext");

            DENG2_ASSERT(p.reverseSegment(1).length() == 0);
            DENG2_ASSERT(p.reverseSegment(1).toString() == "");
        }

        // Test a relative directory.
        {
            Path p("some/dir/structure/");
            DENG2_ASSERT(p.segmentCount() == 3);

            DENG2_ASSERT(p.reverseSegment(0).length() == 9);
            DENG2_ASSERT(p.reverseSegment(0).toString() == "structure");

            DENG2_ASSERT(p.reverseSegment(1).length() == 3);
            DENG2_ASSERT(p.reverseSegment(1).toString() == "dir");

            DENG2_ASSERT(p.reverseSegment(2).length() == 4);
            DENG2_ASSERT(p.reverseSegment(2).toString() == "some");
        }

        // Test the extra segments.
        {
            Path p("/30/29/28/27/26/25/24/23/22/21/20/19/18/17/16/15/14/13/12/11/10/9/8/7/6/5/4/3/2/1/0");
            DENG2_ASSERT(p.segmentCount() == 32);

            DENG2_ASSERT(p.reverseSegment(0).toString()  == "0");
            DENG2_ASSERT(p.reverseSegment(23).toString() == "23");
            DENG2_ASSERT(p.reverseSegment(24).toString() == "24");
            DENG2_ASSERT(p.reverseSegment(30).toString() == "30");
        }

        // Test separators.
        {
            Path a("a.b.c.d", '.');
            Path b("con-variable", '-');

            DENG2_ASSERT(a.segmentCount() == 4);
            DENG2_ASSERT(a.segment(1).toString() == "b");

            DENG2_ASSERT(b.segmentCount() == 2);
            DENG2_ASSERT(b.segment(0).toString() == "con");
            DENG2_ASSERT(b.segment(1).toString() == "variable");
        }

        // Test fileName().
        {
            Path p;
            Path a("hello");
            Path b("hello/world");
            Path c("hello/world/");
            Path d("hello/world/  ");

            /*
            qDebug() << p << "=>" << p.fileName();
            qDebug() << a << "=>" << a.fileName();
            qDebug() << b << "=>" << b.fileName();
            qDebug() << c << "=>" << c.fileName();
            qDebug() << d << "=>" << d.fileName();
            */

            DENG2_ASSERT(p.fileName() == String(p).fileName());
            DENG2_ASSERT(a.fileName() == String(a).fileName());
            DENG2_ASSERT(b.fileName() == String(b).fileName());
            DENG2_ASSERT(c.fileName() == String(c).fileName());
            DENG2_ASSERT(d.fileName() == String(d).fileName());
        }
    }
    catch(Error const &er)
    {
        qWarning() << er.asText();
        return false;
    }
    return true;
}

static int testResult = Path_UnitTest();

#endif // _DEBUG
