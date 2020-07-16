/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/scripting/script.h"
#include "de/scripting/parser.h"
#include "de/file.h"

namespace de {

DE_PIMPL_NOREF(Script)
{
    Compound compound;

    /// File path where the script was loaded. Will be visible in the namespace
    /// of the process executing the script.
    String path;
};

Script::Script() : d(new Impl)
{}

Script::Script(const String &source) : d(new Impl)
{
    Parser().parse(source, *this);
}

Script::Script(const File &file) : d(new Impl)
{
    d->path = file.path();
    Parser().parse(String::fromUtf8(Block(file)), *this);
}

void Script::parse(const String &source)
{
    d->compound.clear();
    Parser().parse(source, *this);
}

void Script::setPath(const String &path)
{
    d->path = path;
}

const String &Script::path() const
{
    return d->path;
}

const Statement *Script::firstStatement() const
{
    return d->compound.firstStatement();
}

Compound &Script::compound()
{
    return d->compound;
}

} // namespace de

