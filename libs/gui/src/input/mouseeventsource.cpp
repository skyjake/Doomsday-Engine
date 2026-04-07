/** @file mouseeventsource.cpp
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/mouseeventsource.h"

namespace de {

DE_PIMPL_NOREF(MouseEventSource)
{
    DE_PIMPL_AUDIENCE(MouseStateChange)
    DE_PIMPL_AUDIENCE(MouseEvent)
};

DE_AUDIENCE_METHOD(MouseEventSource, MouseStateChange)
DE_AUDIENCE_METHOD(MouseEventSource, MouseEvent)

MouseEventSource::MouseEventSource() : d(new Impl)
{}

} // namespace de
