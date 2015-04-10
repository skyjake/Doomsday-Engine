/** @file canvas_macx.mm  OS X specific Canvas routines.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/gui/canvas_macx.h"
#include <AppKit/AppKit.h>

namespace de {
    
void Canvas_OSX_EnableFullScreenMode(Canvas *canvas)
{
    NSView *view = (NSView *) canvas->winId();
    [[view window] setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
}

} // namespace de
