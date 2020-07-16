/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/feed.h"

namespace de {

Feed::Feed()
{}

Feed::~Feed()
{}

File *Feed::createFile(const String &/*name*/)
{
    // By default feeds can't create files.
    return 0;
}

void Feed::destroyFile(const String &/*name*/)
{}

Feed *Feed::newSubFeed(const String &/*name*/)
{
    // By default feeds can't create subfeeds.
    return 0;
}

} // namespace de
