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

#ifndef __AMETHYST_CMDLINE_H__
#define __AMETHYST_CMDLINE_H__

#include "string.h"
#include <QList>

class CommandLine
{
public:
    CommandLine(int argc, char **argv);

    int count() const { return _args.size(); }
    bool exists(const String& opt) const;
    bool beginsWith(int index, const String& begin) const;
    String at(int index) const;

private:
    QList<String> _args;
};

#endif
