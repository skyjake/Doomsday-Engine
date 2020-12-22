/** @file cvartogglewidget.h  Button widget for toggling cvars.
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

#ifndef LIBCOMMON_UI_CVARTOGGLEWIDGET
#define LIBCOMMON_UI_CVARTOGGLEWIDGET

#include "buttonwidget.h"
#include <functional>

namespace common {
namespace menu {

/**
 * @ingroup menu
 */
class CVarToggleWidget : public ButtonWidget
{
public:
    enum State { Up, Down };

public:
    CVarToggleWidget(const char *cvarPath, int cvarValueMask = 0,
                     const de::String &downText = "Yes",
                     const de::String &upText   = "No");
    virtual ~CVarToggleWidget() override;

    int handleCommand(menucommand_e command) override;

    void setState(State newState);
    State state() const;

    inline bool isUp()   const { return state() == Up; }
    inline bool isDown() const { return state() == Down; }

    const char *cvarPath() const;
    int cvarValueMask() const;

    void setDownText(const de::String &newDownText);
    de::String downText() const;

    void setUpText(const de::String &newUpText);
    de::String upText() const;

    void setStateChangeCallback(const std::function<void(State)> &stateChanged);

    void pageActivated() override;

private:
    DE_PRIVATE(d)
};

void CVarToggleWidget_UpdateCVar(Widget &wi, Widget::Action action);

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_CVARTOGGLEWIDGET
