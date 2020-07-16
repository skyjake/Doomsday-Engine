/** @file globalshortcuts.cpp  Global keyboard shortkeys.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "globalshortcuts.h"
#include "testapp.h"

#include <de/keyevent.h>

using namespace de;

DE_PIMPL_NOREF(GlobalShortcuts)
{};

GlobalShortcuts::GlobalShortcuts()
    : Widget("shortcuts"), d(new Impl)
{}

bool GlobalShortcuts::handleEvent(const Event &event)
{
    if (event.isKeyDown())
    {
        const KeyEvent &key = event.as<KeyEvent>();
        if (key.modifiers().testFlag(KeyEvent::Control) && key.ddKey() == 'q')
        {
            TestApp::app().quit();
            return true;
        }
    }
    return false;
}
