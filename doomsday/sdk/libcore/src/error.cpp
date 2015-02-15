/**
 * @file error.cpp
 * Exceptions. @ingroup errors
 *
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/error.h"
//#include <iostream>

namespace de {

Error::Error(QString const &where, QString const &message)
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

void Error::setName(QString const &name)
{
    if(_name.size()) _name += "_";
    _name += name.toStdString();
}

} // namespace de
