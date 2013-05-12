/** @file font.h  Font with metrics.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_FONT_H
#define LIBGUI_FONT_H

#include <de/libdeng2.h>
#include <de/Rule>
#include <de/Rectangle>
#include <de/String>

#include <QFont>

#include "libgui.h"

namespace de {

/**
 * Font with metrics.
 */
class LIBGUI_PUBLIC Font
{
public:
    Font();

    Font(Font const &other);

    Font(QFont const &font);

    QFont toQFont() const;

    /**
     * Determines the size of the given line of text. (0,0) is the corner of is
     * at the baseline, left edge of the line. The rectangle may extend into
     * negative coordinates.
     *
     * @param textLine  Text to measure.
     *
     * @return
     */
    Rectanglei measure(String const &textLine);

    Rule const &height() const;
    Rule const &ascent() const;
    Rule const &descent() const;
    Rule const &lineSpacing() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_FONT_H
