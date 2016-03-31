/** @file linkfile.cpp  Symbolic link.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

DENG2_PIMPL(LinkFile)
{
    SafePtr<File const> target;

    Instance(Public *i)
        : Base(i)
        , target(i) {}
};

LinkFile::LinkFile(String const &name)
    : File(name)
    , d(new Instance(this))
{}

LinkFile::~LinkFile()
{
    DENG2_GUARD(this);

    DENG2_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();

    deindex();
}

File const &LinkFile::target() const
{
    DENG2_GUARD(this);

    if(d->target)
    {
        return *d->target;
    }
    return File::target();
}

File &LinkFile::target()
{
    DENG2_GUARD(this);

    if(d->target)
    {
        return *const_cast<File *>(d->target.get());
    }
    return File::target();
}

Folder const *LinkFile::targetFolder() const
{
    return target().maybeAs<Folder>();
}

Folder *LinkFile::targetFolder()
{
    return target().maybeAs<Folder>();
}

void LinkFile::setTarget(File const &file)
{
    DENG2_GUARD(this);

    d->target.reset(&file);
}

bool LinkFile::isBroken() const
{
    return &target() == this;
}

String LinkFile::describe() const
{
    DENG2_GUARD(this);

    if(!isBroken())
    {
        DENG2_GUARD_FOR(target(), G);
        return "link to " + target().description();
    }
    return "broken link";
}

filesys::Node const *LinkFile::tryFollowPath(PathRef const &path) const
{
    if(Folder const *folder = targetFolder())
    {
        return folder->tryFollowPath(path);
    }
    return nullptr;
}

filesys::Node const *LinkFile::tryGetChild(String const &name) const
{
    if(Folder const *folder = targetFolder())
    {
        return folder->tryGetChild(name);
    }
    return nullptr;
}

LinkFile *LinkFile::newLinkToFile(File const &file, String linkName)
{
    // Fall back to using the target's name.
    if(linkName.isEmpty()) linkName = file.name();

    LinkFile *link = new LinkFile(linkName);
    link->setTarget(file);
    link->setStatus(file.status());
    return link;
}

} // namespace de
