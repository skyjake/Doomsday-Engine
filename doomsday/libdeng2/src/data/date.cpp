/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Date"

using namespace de;

Date::Date()
{}

Date::Date(Time const &time) : Time(time)
{}

Time Date::asTime() const
{
    return *this;
}

String Date::asText() const
{
    String result;
    QTextStream os(&result);
    os << *this;
    return result;
}

QTextStream &de::operator << (QTextStream &os, Date const &d)
{
    os << d.asDateTime().toString("yyyy-MM-dd");
    return os;
}
