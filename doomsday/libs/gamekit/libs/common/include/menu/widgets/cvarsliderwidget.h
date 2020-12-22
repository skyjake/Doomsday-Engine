/** @file cvarsliderwidget.h  UI widget for manipulating a cvar with a graphical slider.
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

#ifndef LIBCOMMON_UI_CVARSLIDERWIDGET
#define LIBCOMMON_UI_CVARSLIDERWIDGET

#include "sliderwidget.h"

namespace common {
namespace menu {

/**
 * UI widget for manipulating a console variable with a graphical slider.
 *
 * @ingroup menu
 */
class CVarSliderWidget : public SliderWidget
{
public:
    explicit CVarSliderWidget(const char *cvarPath, float min = 0.0f, float max = 1.0f,
                              float step = 0.1f, bool floatMode = true);
    virtual ~CVarSliderWidget();

    const char *cvarPath() const;

private:
    const char *_cvarPath;
};

void CVarSliderWidget_UpdateCVar(Widget &wi, Widget::Action action);

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_SLIDERWIDGET
