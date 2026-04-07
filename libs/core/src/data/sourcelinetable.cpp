/** @file sourcelinetable.cpp  Table for source paths and line numbers.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/sourcelinetable.h"
#include "de/pathtree.h"
#include "de/hash.h"

#include <atomic>

namespace de {

static const int SOURCE_SHIFT = 17;
static const int NUMBER_MASK = 0x1ffff;

DE_PIMPL_NOREF(SourceLineTable), public Lockable
{
    struct IdNode : public PathTree::Node
    {
        static std::atomic_uint counter;

        LineId id;
        IdNode(const PathTree::NodeArgs &args)
            : PathTree::Node(args), id(++counter) {}
    };

    PathTreeT<IdNode> paths;
    Hash<duint, const IdNode *> lookup; // reverse lookup
};

std::atomic_uint SourceLineTable::Impl::IdNode::counter;

SourceLineTable::SourceLineTable() : d(new Impl)
{}

SourceLineTable::LineId SourceLineTable::lineId(const String &path, duint lineNumber)
{
    Path const source(path);

    DE_GUARD(d);

    const auto *node = d->paths.tryFind(source, PathTree::MatchFull | PathTree::NoBranch);
    if (!node)
    {
        node = &d->paths.insert(source);
        d->lookup[node->id] = node;
    }
    return (node->id << SOURCE_SHIFT) | lineNumber;
}

String SourceLineTable::sourceLocation(LineId sourceId) const
{
    const auto location = sourcePathAndLineNumber(sourceId);
    return Stringf("%s:%u", location.first.c_str(), location.second);
}

SourceLineTable::PathAndLine SourceLineTable::sourcePathAndLineNumber(LineId sourceId) const
{
    const duint lineNumber = (NUMBER_MASK & sourceId);

    DE_GUARD(d);

    auto found = d->lookup.find(sourceId >> SOURCE_SHIFT);
    if (found != d->lookup.end())
    {
        return PathAndLine(found->second->path(), lineNumber);
    }
    return PathAndLine("", lineNumber);
}

} // namespace de
