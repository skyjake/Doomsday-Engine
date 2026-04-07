/** @file searchpath.cpp SearchPath
 *
 * @authors Copyright &copy; 2010-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2010-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/filesys/searchpath.h"

namespace res {

SearchPath::SearchPath(const res::Uri &_uri, Flags _flags)
    : Uri(_uri), flags_(_flags)
{}

SearchPath::SearchPath(const SearchPath &other)
    : Uri(other), flags_(other.flags_)
{}

Flags SearchPath::flags() const
{
    return flags_;
}

SearchPath &SearchPath::setFlags(Flags newFlags)
{
    flags_ = newFlags;
    return *this;
}

} // namespace res
