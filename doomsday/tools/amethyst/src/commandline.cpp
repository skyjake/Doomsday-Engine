/*
 * Copyright (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "commandline.h"
#include <QDebug>

CommandLine::CommandLine(int argc, char **argv)
{
    for(int i = 0; i < argc; i++) _args << argv[i];
}

bool CommandLine::exists(const String& opt) const
{
    return _args.contains(opt);
}

String CommandLine::at(int index) const
{
    if(index < 0 || index >= count()) return "";
    return _args[index];
}

bool CommandLine::beginsWith(int index, const String& begin) const
{
    return at(index).startsWith(begin);
}
