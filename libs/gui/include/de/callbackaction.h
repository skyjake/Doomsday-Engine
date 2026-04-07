/** @file callbackaction.h  Action with a std::function callback.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_CALLBACKACTION_H
#define LIBAPPFW_CALLBACKACTION_H

#include "libgui.h"
#include <de/action.h>
#include <functional>

namespace de {

/**
 * Action that calls a callback function when triggered.
 */
class LIBGUI_PUBLIC CallbackAction : public Action
{
public:
    typedef std::function<void ()> Callback;

public:
    CallbackAction(Callback callback);
    void trigger() override;

private:
    Callback _func;
};

} // namespace de

#endif // LIBAPPFW_CALLBACKACTION_H
