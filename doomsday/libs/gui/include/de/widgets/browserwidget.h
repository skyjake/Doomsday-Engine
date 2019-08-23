/** @file browserwidget.h  Browser for tree data.
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_BROWSERWIDGET_H
#define LIBAPPFW_BROWSERWIDGET_H

#include <de/GuiWidget>

namespace de {

/**
 * Browser for tree data.
 *
 * @ingroup widgets
 */
class LIBGUI_PUBLIC BrowserWidget : public GuiWidget
{
public:
    BrowserWidget(const String &name = {});

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_BROWSERWIDGET_H
