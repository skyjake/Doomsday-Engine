/**
 * @file error.cpp
 * Exceptions. @ingroup core
 *
 * @todo Update the fields above as appropriate.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de/error.h"
//#include <iostream>

namespace de {

Error::Error(const QString& where, const QString& message)
    : std::runtime_error(("(" + where + ") " + message).toStdString()), _name("")
{
    //std::cerr << "Constructed Error " << this << ": " << asText().toAscii().constData() << "\n";
}

Error::~Error() throw()
{
    //std::cerr << "destroying  Error " << this << ": " << asText().toAscii().constData() << "\n";
}

QString Error::name() const
{
    if(!_name.size()) return "Error";
    return QString::fromStdString(_name);
}

QString Error::asText() const
{
    return "[" + name() + "] " + std::runtime_error::what();
}

void Error::setName(const QString& name)
{
    if(_name.size()) _name += "_";
    _name += name.toStdString();
}

} // namespace de
