/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/file.h"
#include "de/app.h"
#include "de/fs.h"

using namespace de;

File::File(const std::string& fileName)
    : parent_(0), originFeed_(0), name_(fileName)
{}

File::~File()
{
    fileSystem().deindex(*this);
}

FS& File::fileSystem()
{
    return App::app().fileSystem();
}

const String File::path() const
{
    String thePath = name();
    for(Folder* i = parent_; i; i = i->parent_)
    {
        thePath = i->name().concatenatePath(thePath);
    }
    return "/" + thePath;
}
        
const File& File::source() const
{
    return *this;
}

File& File::source()
{
    return *this;
}
        
File::Size File::size() const
{
    return 0;
}

void File::get(Offset at, Byte* values, Size count) const
{
    if(at >= size() || at + count > size())
    {
        throw OffsetError("File::get", "Out of range");
    }
}

void File::set(Offset at, const Byte* values, Size count)
{
    throw ReadOnlyError("File::set", "File can only be read");
}
