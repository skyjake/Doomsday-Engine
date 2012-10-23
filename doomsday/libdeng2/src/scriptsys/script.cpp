/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Script"
#include "de/Parser"
#include "de/File"

using namespace de;

Script::Script()
{}

Script::Script(const String& source)
{
    Parser().parse(source, *this);
}

Script::Script(const File& file) : _path(file.path())
{
    Parser().parse(String::fromUtf8(file), *this);
}

Script::~Script()
{}

const Statement* Script::firstStatement() const
{
    return _compound.firstStatement();
}
