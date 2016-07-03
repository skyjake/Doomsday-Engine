/** @file sourcelinetable.cpp  Table for source paths and line numbers.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/SourceLineTable"
#include "de/PathTree"

namespace de {

static int const SOURCE_SHIFT = 17;
static int const NUMBER_MASK = 0x1ffff;

DENG2_PIMPL_NOREF(SourceLineTable)
{
    struct IdNode : public PathTree::Node
    {
        static LineId counter;

        LineId id;
        IdNode(PathTree::NodeArgs const &args)
            : PathTree::Node(args), id(++counter) {}
    };

    PathTreeT<IdNode> paths;
    QHash<duint, IdNode const *> lookup; // reverse lookup
};

SourceLineTable::LineId SourceLineTable::Impl::IdNode::counter = 0;

SourceLineTable::SourceLineTable() : d(new Impl)
{}

SourceLineTable::LineId SourceLineTable::lineId(String const &path, duint lineNumber)
{
    Path const source(path);

    auto const *node = d->paths.tryFind(source, PathTree::MatchFull | PathTree::NoBranch);
    if (!node)
    {
        node = &d->paths.insert(source);
        d->lookup[node->id] = node;
    }
    return (node->id << SOURCE_SHIFT) | lineNumber;
}

String SourceLineTable::sourceLocation(LineId sourceId) const
{
    auto const location = sourcePathAndLineNumber(sourceId);
    return QString("%1:%2").arg(location.first).arg(location.second);
}

SourceLineTable::PathAndLine SourceLineTable::sourcePathAndLineNumber(LineId sourceId) const
{
    duint const lineNumber = (NUMBER_MASK & sourceId);

    auto found = d->lookup.constFind(sourceId >> SOURCE_SHIFT);
    if (found != d->lookup.constEnd())
    {
        return PathAndLine(found.value()->path().toStringRef(), lineNumber);
    }
    return PathAndLine("", lineNumber);
}

} // namespace de
