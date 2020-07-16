/** @file linkfile.cpp  Symbolic link.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/linkfile.h"
#include "de/folder.h"

namespace de {

DE_PIMPL(LinkFile)
{
    SafePtr<File const> target;

    Impl(Public *i)
        : Base(i)
        , target(i) {}
};

LinkFile::LinkFile(const String &name)
    : File(name)
    , d(new Impl(this))
{}

LinkFile::~LinkFile()
{
    DE_GUARD(this);

    DE_NOTIFY(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();

    deindex();
}

const File &LinkFile::target() const
{
    DE_GUARD(this);

    if (d->target)
    {
        return *d->target;
    }
    return File::target();
}

File &LinkFile::target()
{
    DE_GUARD(this);

    if (d->target)
    {
        return *const_cast<File *>(d->target.get());
    }
    return File::target();
}

const Folder *LinkFile::targetFolder() const
{
    return maybeAs<Folder>(target());
}

Folder *LinkFile::targetFolder()
{
    return maybeAs<Folder>(target());
}

void LinkFile::setTarget(const File &file)
{
    DE_GUARD(this);

    d->target.reset(&file);
}

void LinkFile::setTarget(const File *fileOrNull)
{
    DE_GUARD(this);

    d->target.reset(fileOrNull);
}

bool LinkFile::isBroken() const
{
    return &target() == this;
}

String LinkFile::describe() const
{
    DE_GUARD(this);

    if (!isBroken())
    {
        DE_GUARD_FOR(target(), G);
        return "link to " + target().description();
    }
    return "broken link";
}

const IIStream &LinkFile::operator >> (IByteArray &bytes) const
{
    if (!isBroken())
    {
        target() >> bytes;
        return *this;
    }
    else
    {
        return File::operator >> (bytes);
    }
}

const filesys::Node *LinkFile::tryFollowPath(const PathRef &path) const
{
    if (const Folder *folder = targetFolder())
    {
        return folder->tryFollowPath(path);
    }
    return nullptr;
}

const filesys::Node *LinkFile::tryGetChild(const String &name) const
{
    if (const Folder *folder = targetFolder())
    {
        return folder->tryGetChild(name);
    }
    return nullptr;
}

LinkFile *LinkFile::newLinkToFile(const File &file, String linkName)
{
    // Fall back to using the target's name.
    if (linkName.isEmpty()) linkName = file.name();

    LinkFile *link = new LinkFile(linkName);
    link->setTarget(file);
    link->setStatus(file.status());
    return link;
}

} // namespace de
