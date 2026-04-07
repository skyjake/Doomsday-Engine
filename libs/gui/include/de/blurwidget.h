/** @file blurwidget.h
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_BLURWIDGET_H
#define LIBAPPFW_BLURWIDGET_H

#include "de/guiwidget.h"

namespace de {

/**
 * Utility widget for drawing blurred widget backgrounds. Many widgets can
 * share the same blurred background texture, assuming they don't overlap each
 * other.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC BlurWidget : public GuiWidget
{
public:
    BlurWidget(const String &name = String());
};

} // namespace de

#endif // LIBAPPFW_BLURWIDGET_H
