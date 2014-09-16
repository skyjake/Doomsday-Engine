/** @file sliderwidget.h  UI widget for a graphical slider.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_UI_SLIDERWIDGET
#define LIBCOMMON_UI_SLIDERWIDGET

#include "widget.h"

namespace common {
namespace menu {

#define MNDATA_SLIDER_SLOTS             (10)
#define MNDATA_SLIDER_SCALE             (.75f)
#if __JDOOM__ || __JDOOM64__
#  define MNDATA_SLIDER_OFFSET_X        (0)
#  define MNDATA_SLIDER_OFFSET_Y        (0)
#  define MNDATA_SLIDER_PATCH_LEFT      ("M_THERML")
#  define MNDATA_SLIDER_PATCH_RIGHT     ("M_THERMR")
#  define MNDATA_SLIDER_PATCH_MIDDLE    ("M_THERMM")
#  define MNDATA_SLIDER_PATCH_HANDLE    ("M_THERMO")
#elif __JHERETIC__ || __JHEXEN__
#  define MNDATA_SLIDER_OFFSET_X        (0)
#  define MNDATA_SLIDER_OFFSET_Y        (1)
#  define MNDATA_SLIDER_PATCH_LEFT      ("M_SLDLT")
#  define MNDATA_SLIDER_PATCH_RIGHT     ("M_SLDRT")
#  define MNDATA_SLIDER_PATCH_MIDDLE    ("M_SLDMD1")
#  define MNDATA_SLIDER_PATCH_HANDLE    ("M_SLDKB")
#endif

/**
 * @defgroup mnsliderSetValueFlags  MNSlider Set Value Flags
 */
///@{
#define MNSLIDER_SVF_NO_ACTION            0x1 /// Do not call any linked action function.
///@}

/**
 * UI widget for manipulating a value with a graphical slider.
 */
struct SliderWidget : public Widget
{
public:
    float min, max;
    float _value;
    float step; // Button step.
    dd_bool floatMode; // Otherwise only integers are allowed.
    /// @todo Turn this into a property record or something.
    void *data1;
    void *data2;
    void *data3;
    void *data4;
    void *data5;

public:
    SliderWidget();
    virtual ~SliderWidget() {}

    void draw(Point2Raw const *origin);
    void updateGeometry(Page *pagePtr);
    int handleCommand(menucommand_e command);

    int thumbPos() const;

    /// @return  Current value represented by the slider.
    float value() const;

    /**
     * Change the current value represented by the slider.
     * @param flags  @ref mnsliderSetValueFlags
     * @param value  New value.
     */
    void setValue(int flags, float value);

public:
    static void loadResources();
};

int SliderWidget_CommandResponder(Widget *wi, menucommand_e command);

void CvarSliderWidget_UpdateCvar(Widget *wi, Widget::mn_actionid_t action);

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_SLIDERWIDGET
