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

namespace de {

DENG2_PIMPL_NOREF(Script)
{
    Compound compound;

    /// File path where the script was loaded. Will be visible in the namespace
    /// of the process executing the script.
    String path;
};

Script::Script() : d(new Instance)
{}

Script::Script(String const &source) : d(new Instance)
{
    Parser().parse(source, *this);
}

Script::Script(File const &file) : d(new Instance)
{
    d->path = file.path();
    Parser().parse(String::fromUtf8(Block(file)), *this);
}

void Script::parse(String const &source)
{
    d->compound.clear();
    Parser().parse(source, *this);
}

void Script::setPath(String const &path)
{
    d->path = path;
}

String const &Script::path() const
{
    return d->path;
}

Statement const *Script::firstStatement() const
{
    return d->compound.firstStatement();
}

Compound &Script::compound()
{
    return d->compound;
}

} // namespace de

