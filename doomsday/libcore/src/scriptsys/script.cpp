/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Script"
#include "de/Parser"
#include "de/File"

using namespace de;

Script::Script()
{}

Script::Script(String const &source)
{
    Parser().parse(source, *this);
}

Script::Script(File const &file) : _path(file.path())
{
    Parser().parse(String::fromUtf8(Block(file)), *this);
}

Script::~Script()
{}

void Script::parse(String const &source)
{
    _compound.clear();
    Parser().parse(source, *this);
}

Statement const *Script::firstStatement() const
{
    return _compound.firstStatement();
}
