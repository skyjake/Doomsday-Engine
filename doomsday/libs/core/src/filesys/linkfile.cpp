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

#include "de/LinkFile"
#include "de/Folder"

namespace de {

DE_PIMPL(LinkFile)
{
    SafePtr<File const> target;

    Impl(Public *i)
        : Base(i)
        , target(i) {}
};

LinkFile::LinkFile(String const &name)
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

File const &LinkFile::target() const
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

Folder const *LinkFile::targetFolder() const
{
    return maybeAs<Folder>(target());
}

Folder *LinkFile::targetFolder()
{
    return maybeAs<Folder>(target());
}

void LinkFile::setTarget(File const &file)
{
    DE_GUARD(this);

    d->target.reset(&file);
}

void LinkFile::setTarget(File const *fileOrNull)
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

IIStream const &LinkFile::operator >> (IByteArray &bytes) const
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

filesys::Node const *LinkFile::tryFollowPath(PathRef const &path) const
{
    if (Folder const *folder = targetFolder())
    {
        return folder->tryFollowPath(path);
    }
    return nullptr;
}

filesys::Node const *LinkFile::tryGetChild(String const &name) const
{
    if (Folder const *folder = targetFolder())
    {
        return folder->tryGetChild(name);
    }
    return nullptr;
}

LinkFile *LinkFile::newLinkToFile(File const &file, String linkName)
{
    // Fall back to using the target's name.
    if (linkName.isEmpty()) linkName = file.name();

    LinkFile *link = new LinkFile(linkName);
    link->setTarget(file);
    link->setStatus(file.status());
    return link;
}

} // namespace de
