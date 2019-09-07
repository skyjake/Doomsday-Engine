/** @file inputbindingwidget.h  UI widget for manipulating input bindings.
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

#ifndef LIBCOMMON_UI_INPUTBINDINGWIDGET
#define LIBCOMMON_UI_INPUTBINDINGWIDGET

#include "widget.h"

namespace common {
namespace menu {

struct controlconfig_t;

/**
 * Bindings visualizer widget.
 *
 * @ingroup menu
 */
class InputBindingWidget : public Widget
{
public:
    controlconfig_t *binds;

public:
    InputBindingWidget();
    virtual ~InputBindingWidget() override;

    void draw() const override;
    void updateGeometry() override;
    int handleEvent_Privileged(const event_t &event) override;
    int handleCommand(menucommand_e command) override;

    const char *controlName() const;
    de::String bindContext() const;

    void pageActivated() override;

private:
    DE_PRIVATE(d)
};

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_INPUTBINDINGWIDGET
