/**
 * @file error.cpp
 * Exceptions. @ingroup errors
 *
 * @authors Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/escapeparser.h"
#include "de/string.h"
#include "../src/core/logtextstyle.h"

namespace de {

Error::Error(const std::string &where, const std::string &message)
    : std::runtime_error(stringf("%s(in " _E(m) "%s" _E(.) ")" _E(.) " %s",
                         TEXT_STYLE_SECTION,
                         where.c_str(),
                         message.c_str()))
    , _name("")
{}

std::string Error::name() const
{
    if (!_name.size()) return "Error";
    return _name;
}

std::string Error::asText() const
{
    return stringf(
                "%s[%s]" _E(.) " %s", TEXT_STYLE_SECTION, _name.c_str(), std::runtime_error::what());
}

void Error::warnPlainText() const
{
    warning("%s", asPlainText().c_str());
}

std::string Error::asPlainText() const
{
    EscapeParser esc;
    esc.parse(asText());
    return esc.plainText().toStdString();
}

void Error::setName(const std::string &name)
{
    if (_name.size()) _name += "_";
    _name += name;
}

} // namespace de
