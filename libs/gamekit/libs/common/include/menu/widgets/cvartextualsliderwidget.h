/** @file cvartextualsliderwidget.h  UI widget for a textual slider.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_UI_CVARTEXTUALSLIDERWIDGET
#define LIBCOMMON_UI_CVARTEXTUALSLIDERWIDGET

#include <de/string.h>
#include "cvarsliderwidget.h"

namespace common {
namespace menu {

/**
 * UI widget for manipulating a console variable with a textual slider.
 *
 * @ingroup menu
 */
class CVarTextualSliderWidget : public CVarSliderWidget
{
public:
    CVarTextualSliderWidget(const char *cvarPath, float min = 0.0f, float max = 1.0f,
                            float step = 0.1f, bool floatMode = true);
    virtual ~CVarTextualSliderWidget();

    void draw() const;
    void updateGeometry();

    CVarTextualSliderWidget &setEmptyText(const de::String &newEmptyText);
    de::String emptyText() const;

    CVarTextualSliderWidget &setOnethSuffix(const de::String &newOnethSuffix);
    de::String onethSuffix() const;

    CVarTextualSliderWidget &setNthSuffix(const de::String &newNthSuffix);
    de::String nthSuffix() const;

private:
    DE_PRIVATE(d)
};

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_CVARTEXTUALSLIDERWIDGET
