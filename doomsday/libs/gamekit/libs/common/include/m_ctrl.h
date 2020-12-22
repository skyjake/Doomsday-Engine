/** @file m_ctrl.h  Controls menu page and associated widgets.
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

#ifndef LIBCOMMON_MENU_CONTROLS
#define LIBCOMMON_MENU_CONTROLS
#ifdef __cplusplus

#include "hu_lib.h"

namespace common {
namespace menu {

// Control config flags.
#define CCF_NON_INVERSE         0x1
#define CCF_INVERSE             0x2
#define CCF_STAGED              0x4
#define CCF_REPEAT              0x8
#define CCF_SIDESTEP_MODIFIER   0x10
#define CCF_MULTIPLAYER         0x20

struct controlconfig_t
{
    const char *text;
    const char *bindContext;
    const char *controlName;
    const char *command;
    int flags;
};

void Hu_MenuInitControlsPage();
void Hu_MenuControlGrabDrawer(const char *niceName, float alpha);

} // namespace menu
} // namespace common

#endif // __cplusplus
#endif // LIBCOMMON_MENU_CONTROLS
